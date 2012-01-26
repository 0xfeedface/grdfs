#define FIN CL_UINT_MAX

typedef ulong term_id;

__kernel void transitive_closure(__global uint * vertex_list,
                                 __global uint * adjacency_list,
                                 __global uint * thread_ids,
                                 __global uint * results,
                                 const uint gpass,
                                 const uint gstep) {
  
  size_t gid = get_global_id(0);
  size_t tid = thread_ids[gid];
  
  // cache locally
  uint pass = gpass;
  uint step = gstep;
  
  uint adj_index = vertex_list[tid];
  uint adj_index_next = vertex_list[tid + 1];
  
  /*
  if (pass < (adj_index_next - adj_index)) {
    uint pass_vertex = adjacency_list[vertex + pass];
    uint step_adj_index = vertex_list[pass_vertex];
    uint step_adj_index_next = vertex_list[pass_vertex + 1];
    
    if (step < (step_adj_index_next - step_adj_index)) {
      results[tid] = adjacency_list[step_adj_index + step];
    } else {
      results[tid] = FIN;
    }
  } else {
    results[tid] = FIN;
  }
  */
}