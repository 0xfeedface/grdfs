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

#include <unordered_set>
#include <queue>

typedef std::queue<term_id> TermQueue;

void NativeReasoner::addTriple(triple t) {
  Reasoner::addTriple(t);
  
  // store in inverse map
  if (t.predicate == subClassOf_) {
    scPairs_[t.subject].insert(t.object);
    scPairsInverse_[t.object].insert(t.subject);
  } else if (t.predicate == subPropertyOf_) {
    spPairs_[t.subject].insert(t.object);
    spPairsInverse_[t.object].insert(t.subject);
  }
}

void NativeReasoner::computeClosure_Boost() {
  using namespace boost;
  
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
}

void NativeReasoner::computeClosure_InverseAdjacency(const TermMap& adjacentNodes,
                                                     const TermMap& adjacentNodesInverse) {
  TermMap closure;
  TermQueue nodes;
  
  // determine leaf nodes
  for (auto it(std::begin(adjacentNodesInverse)); it != std::end(adjacentNodesInverse); ++it) {
    if (adjacentNodes.find(it->first) == std::end(adjacentNodes)) {
      nodes.push(it->first);
    }
  }
  
  if (!nodes.size()) {
    // graph contains cycles
    std::cout << "Cannot calculate transitive closure on non-DAG.\n";
    return;
  }
  
  // process nodes, starting with leafs
  while (nodes.size()) {
    term_id currentNode = nodes.front();
    nodes.pop();

    auto parents = adjacentNodesInverse.find(currentNode);
    for (auto parent_it(std::begin(parents->second)); parent_it != std::end(parents->second); ++parent_it) {
      closure[*parent_it].insert(currentNode);
      // add the children of the current node as children of the parent node
      for (auto children_it(std::begin(closure[currentNode])); children_it != std::end(closure[currentNode]); ++children_it) {
        closure[*parent_it].insert(*children_it);
      }
      // push updated parent node to the queue, if it is not the last already and if it has children
      auto parentsChildren(adjacentNodesInverse.find(*parent_it));
      if ((parentsChildren != std::end(adjacentNodesInverse)) && (parentsChildren->second.size()) && (nodes.back() != *parent_it)) {
        nodes.push(*parent_it);
      }
    }
  }
}

void NativeReasoner::computeClosure() {
//  Reasoner::computeClosure();
  if (scTerms_.size()) {
    computeClosure_Boost();
//    computeClosure_InverseAdjacency(scPairs_, scPairsInverse_);
  }
  
//  if (spTerms_.size()) {
//    computeClosure_InverseAdjacency(spPairs_, spPairsInverse_);
//  }
}
