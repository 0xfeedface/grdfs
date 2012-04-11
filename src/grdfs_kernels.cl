#define FIN CL_UINT_MAX
typedef ulong term_id;
__constant term_id literal_mask = (1UL << (sizeof(term_id) * 8 - 1));

__kernel
void phase1(__global term_id * input,   /* predicates */
            __global term_id * results, /* matched predicates */
            __global term_id * schema,  /* subjects of subPropertyOf statements */
            const uint schema_size)
{
  size_t globx = get_global_id(0);
  size_t locx  = get_local_id(0);

  // the property to be searched for
  term_id p = input[globx];

  // perform binary search on schema vector
  int lower = 0;
  int upper = schema_size - 1;
  term_id result = 0;
  while (lower <= upper) {
    int mid = ((uint)lower + (uint)upper) >> 1;
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
void phase2(__global term_id * input,
            __global term_id * phase1_results,
            __global term_id * results,
            __global term_id * schema,
            const uint schema_size)
{
}
