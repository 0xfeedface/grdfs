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

__constant ulong kMul = 0x9ddfea08eb382d69ULL;
__constant ulong kLiteralMask = (1UL << 63);

#define ROT(val, shift) (((val) >> shift) | ((val) << (64 - shift)))
/* #define ROT(val, shift) (rotate(val, shift)) */

ulong hash_triple(ulong subject, ulong object);

__kernel
void phase1(__constant term_id* input,   /* predicates */
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

__kernel
void count_results_hashed(__global uint2* results,
                          __constant term_id* input,
                          __constant bucket_info* schema_bucket_infos,
                          __constant term_id* schema_buckets,
                          const uint hash_table_size)
{
  size_t globx = get_global_id(0);
  // the property to be searched for
  term_id p = input[globx];
  uint bucket = (uint)hash_triple(p, 0UL) % hash_table_size;

  uint2 result = {UINT_MAX, 0};

  bucket_info info = schema_bucket_infos[bucket];

  if (info.start < UINT_MAX) {
    uint i = 0;
    while (i < info.size) {
      term_id t = schema_buckets[info.start + i];
      ushort size = (t & 0xffff000000000000) >> 48;
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

__kernel
void materialize_results(__global term_id* result_objects,
                         __global term_id* result_subjects,
                         __global uint2* result_info, /* const, but shared with other kernel */
                         __constant uint2* local_result_info,
                         __constant term_id* schema_buckets,
                         __constant term_id* subjects,
                         __constant bucket_info* bucket_infos,
                         __constant bucket_entry* buckets,
                         const uint tableSize)
{
  size_t globx = get_global_id(0);

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
      ulong hash = hash_triple(subject, object) % tableSize;
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
        result_objects[globx] = object;
        result_subjects[globx] = subject;
      }
    }
  }
}
