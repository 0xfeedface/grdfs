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
void materialize_results(__global term_id* results,
                         __global term_id* subjectResults,
                         __global uint2* result_info,
                         __constant uint2* successor_info,
                         __constant term_id* successors,
                         __constant term_id* subjects,
                         __constant bucket_info* bucket_infos,
                         __constant bucket_entry* buckets,
                         const uint tableSize)
{
  size_t globx = get_global_id(0);

  const term_id subject = subjects[globx];

  /* result.s0 : index into successors or UINT_MAX */
  /* result.s1 : index into results  */
  const uint2 result = result_info[globx];

  if (result.s0 < UINT_MAX) {
    /* succ.s0 : number of successors */
    /* succ.s1 : accumulated number of successors (i.e. index into successors)  */
    const uint2 succ = successor_info[result.s0];

    // construct entailed objects (i.e successors)
    for (uint i = 0; i < succ.s0; ++i) {
      ulong object = successors[succ.s1 + i];
      ulong hash = hash_triple(subject, object) % tableSize;
      uint index = bucket_infos[hash].start;
      bool entail = true;
      if (index < UINT_MAX) {
        uint size = bucket_infos[hash].size;
        for (uint i = index; i < (index + size); ++i) {
          bucket_entry e = buckets[i];
          if (e.subject == subject && e.object == object) {
            entail = false;
            break;
          }
        }
      }
      if (entail) {
        results[result.s1 + i] = object;
        subjectResults[result.s1 + i] = subject;
      }
    }
  }
}
