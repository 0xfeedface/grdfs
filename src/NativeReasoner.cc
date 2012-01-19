//
//  NativeReasoner.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-11.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include "NativeReasoner.h"

#include <boost/graph/transitive_closure.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_utility.hpp>

void NativeReasoner::computeClosure() {
  using namespace boost;
  
  if (scTerms_.size()) {
    TermVector indexedTerms;
    std::map<term_id, TermVector::size_type> termsIndexes;
    // copy from the set to the vector so we get indices
    std::copy(std::begin(scTerms_), std::end(scTerms_), std::back_inserter(indexedTerms));
    
    // transitive closure w/ boost
    typedef property <vertex_name_t, term_id> Name;
    typedef property <vertex_index_t, std::size_t, Name> Index;
    typedef adjacency_list <listS, listS, directedS, Index> graph_t;
    typedef graph_traits <graph_t>::vertex_descriptor vertex_t;
    std::vector<vertex_t> verts;
    graph_t g;
    
    // create vertex list
    for (std::vector<term_id>::size_type i(0); i != indexedTerms.size(); ++i) {
      term_id val(indexedTerms[i]);
      termsIndexes[val] = i;
      verts.push_back(add_vertex(Index(i, Name(val)), g));
    }
    
    // create boost graph
    for (std::vector<so_pair>::iterator it(std::begin(scTriples_)); it != std::end(scTriples_); ++it) {
      size_t sidx = termsIndexes[it->subject];
      size_t oidx = termsIndexes[it->object];
      // add to boost graph
      add_edge(verts[sidx], verts[oidx], g);
    }
    
    adjacency_list <> tc;
    transitive_closure(g, tc);
    
    print_graph(g);
    std::cout << std::endl;
    print_graph(tc);
  }
}