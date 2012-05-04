typedef ulong term_id;

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
void phase1b(__constant term_id* input,   /* predicates */
             __global uint2* results,      /* matched predicate index and number of successors */
             __constant term_id* schema,   /* subjects of subPropertyOf statements */
             __constant uint2* successor_info,  /* number of successors + accumulated for each schema subject */
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
    result.s1 = successor_info[result.s0].s0;
  }

  results[globx] = result;
}

__kernel
void phase1c(__global term_id* results,
             __global uint2* result_info,
             __constant uint2* successor_info,
             __constant term_id* successors)
{
  size_t globx = get_global_id(0);

  /* result.s0 : index into successors or UINT_MAX
   * result.s1 : index into results */
  const uint2 result = result_info[globx];

  if (result.s0 < UINT_MAX) {
    /* succ.s0 : number of successors
     * succ.s1 : accumulated number of successors (i.e. index into successors) */
    const uint2 succ = successor_info[result.s0];

    // construct entailed objects (i.e successors)
    for (uint i = 0; i < succ.s0; ++i) {
      results[result.s1 + i] = successors[succ.s1 + i];
    }
  }
}
