typedef ulong term_id;

__kernel void transitivity(__global uint * reachabilityBuffer,
                           const uint width,
                           const uint pass) {
  
  size_t x = get_local_id(0);
  size_t y = get_group_id(0);
  
  size_t k = pass;
  unsigned yXwidth = y * width;
  
  bool reachableYK = reachabilityBuffer[yXwidth + k];
  bool reachableKX = reachabilityBuffer[k * width + x];
  
  if (reachableYK && reachableKX) {
    reachabilityBuffer[yXwidth + x] = 1;
  }
}
