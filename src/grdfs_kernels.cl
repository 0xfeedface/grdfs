typedef ulong term_id;

typedef struct {
  uint start;
  ushort size;
  ushort free;
} bucket_info;

typedef struct {
  ulong subject;
  ulong object;
} bucket_entry;

__constant ulong kMul = 0x9ddfea08eb382d69UL;
__constant ulong kLiteralMask = (1UL << 63);

#define ROT(val, shift) (((val) >> shift) | ((val) << (64 - shift)))
#define BITS sizeof(term_id) * 8
#define WAVEFRONT_SIZE 64
#define GROUP_SIZE 256
#define NUM_WAVEFRONTS (GROUP_SIZE / WAVEFRONT_SIZE)
#define SEL(cond) (uint)(cond ? 0xffffffff : 0x0)


ulong hash_triple(ulong subject, ulong object);
void scan(__local uint*, __local uint*, const uint);
void max_scan(__local uint*, __local uint*, const uint);
void sort(__local term_id*, __local term_id*, __local uint*, __local uint*, const uint, const uint, uint);

////////////////////////////////////////////////////////////////////////////////

__kernel
void phase1(__global term_id* input,   /* predicates */
            __global term_id* results,   /* matched predicates */
            __constant term_id* schema,  /* subjects of subPropertyOf statements */
            const uint schema_size)
{
  size_t globx = get_global_id(0);

  // the property to be searched for
  term_id p = input[globx];

  // perform binary search on schema vector
  int lower = 0;
  int upper = schema_size - 1;
  term_id result = 0;
  while (lower <= upper) {
    int mid = ((uint)lower + (uint)upper) >> 0x1;
    term_id curr = schema[mid];

    if (curr < p) {
      lower = mid + 1;
    } else if (curr > p) {
      upper = mid - 1;
    } else {
      result = curr;
      break;
    }
  }

  results[globx] = result;
}

////////////////////////////////////////////////////////////////////////////////

__kernel
void count_results_hashed(__global uint2* results,
                          __global term_id* input,
                          const uint actual_size,
                          __global bucket_info* schema_bucket_infos,
                          __global term_id* schema_buckets,
                          const uint hash_table_size)
{
  size_t globx = get_global_id(0);
  if (globx < actual_size) {
    // the property to be searched for
    term_id p = input[globx];
    uint bucket = (uint)hash_triple(p, 0UL) & (hash_table_size - 1);
    uint2 result = { UINT_MAX, 0 };
    bucket_info info = schema_bucket_infos[bucket];

    if (info.start < UINT_MAX) {
      uint i = 0;
      while (i < info.size) {
        term_id t = schema_buckets[info.start + i];
        uint size = (t & 0xffff000000000000) >> 48;
        ulong value = t & 0x0000ffffffffffff;
        if (size) {
          if (p == value) {
            result.s0 = info.start + i;
            result.s1 = size;
            // printf((const char*)"%u : %u\n", p, size);
          }
        }
        // jump over successors to next entry
        i += size + 1;
      }
    }
    results[globx] = result;
  }
}

////////////////////////////////////////////////////////////////////////////////

ulong hash_triple(ulong subject, ulong object)
{
  ulong b = ROT(object + 16, 16);
  ulong c = (subject ^ b) * kMul;

  c ^= (c >> 47);
  b = (b ^ c) * kMul;
  b ^= (b >> 47);
  b *= kMul;

  return b ^ object;
}

////////////////////////////////////////////////////////////////////////////////

__kernel
void count_results(__constant term_id* input,
                   __global uint2* results,
                   __constant term_id* schema,
                   __constant uint2* schema_successor_info,
                   const uint schema_size)
{
  size_t globx = get_global_id(0);

  // the property to be searched for
  term_id p = input[globx];

  // perform binary search on schema vector
  int lower = 0;
  int upper = schema_size - 1;
  uint2 result = {UINT_MAX, 0};
  while (lower <= upper) {
    int mid = ((uint)lower + (uint)upper) >> 0x1;
    term_id curr = schema[mid];

    if (curr < p) {
      lower = mid + 1;
    } else if (curr > p) {
      upper = mid - 1;
    } else {
      result.s0 = mid;
      break;
    }
  }

  if (result.s0 < UINT_MAX) {
    result.s1 = schema_successor_info[result.s0].s0;
  }

  results[globx] = result;
}

////////////////////////////////////////////////////////////////////////////////

__kernel
void materialize_results(__global uint* result_objects,
                         __global uint* result_subjects,
                         __global uint2* result_info, // const, but shared with other kernel
                         __global uint2* local_result_info,
                         __global term_id* schema_buckets,
                         __global term_id* subjects,
                         const uint actual_size,
                         __global bucket_info* bucket_infos,
                         __global bucket_entry* buckets,
                         const uint tableSize)
{
  size_t globx = get_global_id(0);
  if (globx < actual_size) {
    // linfo.s0 : subject index
    // linfo.s1 : local successor offset
    const uint2 linfo = local_result_info[globx];
    const term_id subject = subjects[linfo.s0];

    // do not entail triples with literal subjects
    if (!(subject & kLiteralMask)) {
      // result.s0 : index into schema buckets or UINT_MAX
      // result.s1 : index into results
      const uint2 result = result_info[linfo.s0];

      if (result.s0 < UINT_MAX) {
        // construct entailed object (i.e successors)
        bool entail = true;
        ulong object = schema_buckets[result.s0 + linfo.s1 + 1];
        ulong hash = hash_triple(subject, object) & (tableSize - 1);
        uint index = bucket_infos[hash].start;

        if (index < UINT_MAX) {
          uint size = bucket_infos[hash].size;
          for (uint j = index; j < (index + size); ++j) {
            bucket_entry e = buckets[j];
            if (e.subject == subject && e.object == object) {
              entail = false;
              break;
            }
          }
        }

        if (entail) {
          result_objects[globx] = (uint)(object & 0x00000000ffffffff);
          result_subjects[globx] = (uint)(subject & 0x00000000ffffffff);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

// perform additive scan (i.e. prefix sum)
void scan(__local uint * buffer, __local uint* accum, const uint locx)
{
    const uint lane = locx & (WAVEFRONT_SIZE - 1);

    #pragma unroll
    for (uint i = 1; i < WAVEFRONT_SIZE; i = i << 0x1) {
        barrier(CLK_LOCAL_MEM_FENCE);
        if (lane >= i) { buffer[locx] += buffer[locx - i]; }
    }

#if (NUM_WAVEFRONTS > 1)
    const uint waveID = (locx >> 6);
    barrier(CLK_LOCAL_MEM_FENCE);
    if (locx < NUM_WAVEFRONTS) {
        uint total = buffer[(locx + 1) * WAVEFRONT_SIZE - 1];
        accum[locx] = total;

        // scan over partial sums
        for (uint j = 1; j < NUM_WAVEFRONTS; j = j << 0x1) {
            if (lane >= j) { accum[locx] += accum[locx - j]; }
        }

        accum[locx] -= total;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    // add previous sums to scans of wavefronts
    buffer[locx] += accum[waveID];
    barrier(CLK_LOCAL_MEM_FENCE);
#endif
}

////////////////////////////////////////////////////////////////////////////////

// perform max scan
void max_scan(__local uint * buffer, __local uint* accum, const uint locx)
{
    const uint lane = locx & (WAVEFRONT_SIZE - 1);

    #pragma unroll
    for (uint i = 1; i < WAVEFRONT_SIZE; i = i << 0x1) {
        barrier(CLK_LOCAL_MEM_FENCE);
        if (lane >= i) { buffer[locx] = max(buffer[locx], buffer[locx - i]); }
    }

#if (NUM_WAVEFRONTS > 1)
    const uint waveID = (locx >> 6);
    barrier(CLK_LOCAL_MEM_FENCE);
    if (locx < NUM_WAVEFRONTS) {
        uint total = buffer[(locx + 1) * WAVEFRONT_SIZE - 1];
        accum[locx] = total;

        // scan over partial sums
        for (uint j = 1; j < NUM_WAVEFRONTS; j = j << 0x1) {
            if (lane >= j) { accum[locx] = max(accum[locx], accum[locx - j]); }
        }

        accum[locx] -= total;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    // add previous sums to scans of wavefronts
    buffer[locx] += accum[waveID];
    barrier(CLK_LOCAL_MEM_FENCE);
#endif
}

////////////////////////////////////////////////////////////////////////////////

void sort(__local term_id* buffer0, __local term_id* buffer1,
     __local uint* scan_buffer, __local uint* accum, const uint bits,
     const uint locx, uint component)
{
  // determine number of trailing non-zero bits
  uint bit_flag, index, new_index, zero_count, zeros_before;
  term_id value, value0, value1;

  barrier(CLK_LOCAL_MEM_FENCE);
  for (uint bit = 0; bit < bits; ++bit) {
    value = (component == 0) ? buffer0[locx] : buffer1[locx];

    value0 = buffer0[locx];
    value1 = buffer1[locx];

    // set the bit flag to 1 if the current bit is not set
    bit_flag = scan_buffer[locx] = 1U - (uint)((value >> bit) & 0x1);
    barrier(CLK_LOCAL_MEM_FENCE);

    // perform scan over bit flags to determine new indices
    scan(scan_buffer, accum, locx);

    zeros_before = scan_buffer[locx];

    // copy to index vector on those positions where current bit is not set
    index = bit_flag ? (zeros_before - 1) : 0U;

    // determined new index for where current bit is set
    bit_flag = 1U - bit_flag;
    zero_count = scan_buffer[GROUP_SIZE - 1];
    new_index = (GROUP_SIZE - locx) - zero_count + zeros_before;

    // copy to index vector on those positions where current bit is set
    index = bit_flag ? (GROUP_SIZE - new_index) : index;

    // scatter values to their new indices
    buffer0[index] = value0;
    buffer1[index] = value1;

    barrier(CLK_LOCAL_MEM_FENCE);
  }
}

////////////////////////////////////////////////////////////////////////////////

__kernel
void deduplication(__global uint* objects,
                   __global uint* subjects)
{
  const uint globx  = get_global_id(0);
  const uint locx   = get_local_id(0);

  __local term_id buffer0[GROUP_SIZE];
  __local term_id buffer1[GROUP_SIZE];
  __local uint scan_buffer[GROUP_SIZE];
  __local uint accum[NUM_WAVEFRONTS];

  term_id value0 = buffer0[locx] = subjects[globx];
  term_id value1 = buffer1[locx] = objects[globx];

  // determine maximum significant bits per work group
  scan_buffer[locx] = (uint)BITS - clz(value0);
  max_scan(scan_buffer, accum, locx);
  __local uint significant_bits;
  if (locx == 0) {
    significant_bits = scan_buffer[GROUP_SIZE - 1];
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  // sort by second component
  sort(buffer0, buffer1, scan_buffer, accum, significant_bits, locx, 1);

  // determine maximum significant bits per work group
  scan_buffer[locx] = (uint)BITS - clz(value1);
  max_scan(scan_buffer, accum, locx);
  if (locx == 0) {
    significant_bits = scan_buffer[GROUP_SIZE - 1];
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  // sort by first component
  sort(buffer0, buffer1, scan_buffer, accum, significant_bits, locx, 0);

  value0 = buffer0[locx];
  value1 = buffer1[locx];

  // mark adjacent duplicates
  uint flag = ((locx > 0) && (value0 == buffer0[locx - 1]) && (value1 == buffer1[locx - 1]));
  scan_buffer[locx] = flag;
  barrier(CLK_LOCAL_MEM_FENCE);

  // scan over duplicate markings, yielding the number of duplicates before a given index
  scan(scan_buffer, accum, locx);

  // zero-initialize
  buffer0[locx] = 0;
  buffer1[locx] = 0;
  barrier(CLK_LOCAL_MEM_FENCE);
  
  // shift values by amount of duplicates before to the left
  if (flag == 0) {
    uint displacement = scan_buffer[locx];
    buffer0[locx - displacement] = value0;
    buffer1[locx - displacement] = value1;
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  subjects[globx] = buffer0[locx] & 0x00000000ffffffff;
  objects[globx] = buffer1[locx] & 0x00000000ffffffff;
}

