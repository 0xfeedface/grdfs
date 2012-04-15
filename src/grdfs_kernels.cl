typedef ulong term_id;

__kernel
void phase1(__constant term_id * input,  /* predicates */
            __global term_id * results,  /* matched predicates */
            __constant term_id * schema, /* subjects of subPropertyOf statements */
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

