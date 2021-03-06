//
//  NativeReasoner.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-11.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include "NativeReasoner.hh"

#include <queue>
#include <iostream>

typedef std::queue<term_id> TermQueue;

void NativeReasoner::computeClosure_InverseAdjacency(const TermMap& adjacentNodes,
    const TermMap& adjacentNodesInverse)
{
  TermMap closure;
  TermQueue nodes;
  std::unordered_map<term_id, bool> pendingNodes;

  // determine leaf nodes
  auto it(std::begin(adjacentNodesInverse));
  for (; it != std::end(adjacentNodesInverse); ++it) {
    if (adjacentNodes.find(it->first) == std::end(adjacentNodes)) {
      nodes.push(it->first);
    }
  }

  if (!nodes.size()) {
    // graph contains cycles
    std::cout << "Cannot calculate transitive closure on non-DAG.\n";
    return;
  }

  std::cout << "Number of leaf nodes: " << nodes.size() << std::endl;

  // process nodes, starting with leafs
  while (nodes.size()) {
    term_id currentNode = nodes.front();
//    std::cout << "currentNode: " << dict_.Find(currentNode) << std::endl;
    nodes.pop();

    pendingNodes[currentNode] = false;

    auto parents = adjacentNodesInverse.find(currentNode);
    auto parent_it(std::begin(parents->second));
    for (; parent_it != std::end(parents->second); ++parent_it) {
      closure[*parent_it].insert(currentNode);
      // add the children of the current node as children of the parent node
      auto children_it(std::begin(closure[currentNode]));
      for (; children_it != std::end(closure[currentNode]); ++children_it) {
        closure[*parent_it].insert(*children_it);
      }
      // push updated parent node to the queue, if it is not there already
      // and if it does have children
      auto parentsChildren(adjacentNodesInverse.find(*parent_it));
      if ((parentsChildren != std::end(adjacentNodesInverse))
          && (parentsChildren->second.size())
          && !pendingNodes[*parent_it]) {
        nodes.push(*parent_it);
        pendingNodes[*parent_it] = true;
      }
    }
  }

//  printClosure(closure, true);
}

void NativeReasoner::computeClosure_InverseTopological(TermMap& successorMap,
    const TermMap& predecessorMap)
{
  TermQueue nodes;
  std::unordered_map<term_id, bool> finishedNodes;

  // initialize the queue with the leaf nodes
  auto it(std::begin(predecessorMap));
  for (; it != std::end(predecessorMap); ++it) {
    if (successorMap.find(it->first) == std::end(successorMap)) {
      nodes.push(it->first);
    }
  }

  // no leafs means, the graph contains cycles
  if (!nodes.size()) {
    std::cout << "Cannot calculate transitive closure on non-DAG.\n";
    return;
  }

  while (nodes.size()) {
    term_id currentNode = nodes.front();
//    std::cout << "currentNode: " << dict_.Find(currentNode) << std::endl;
    nodes.pop();

    auto pit = predecessorMap.find(currentNode);
    if (pit != std::end(predecessorMap)) {
      auto parent_it = std::begin(pit->second);
      for (; parent_it != std::end(pit->second); ++parent_it) {
        if (!finishedNodes[*parent_it]) {
          nodes.push(*parent_it);
          finishedNodes[*parent_it] = true;
        }
      }
    }

    // if the current node has children
    auto cit = successorMap.find(currentNode);
    if (cit != std::end(successorMap)) {
      // go through all children of the current node
      auto children_it(std::begin(cit->second));
      for (; children_it != std::end(cit->second); ++children_it) {
        auto gcit(successorMap.find(*children_it));
        if (gcit != std::end(successorMap)) {
          // add all of the children's children as the current node's children
          auto grandchildren_it(std::begin(gcit->second));
          for (; grandchildren_it != std::end(gcit->second); ++grandchildren_it) {
            successorMap[currentNode].insert(*grandchildren_it);
          }
        }
      }
    }
  }

//  printClosure(successorMap, true);
}

void NativeReasoner::computeClosure()
{
//  Reasoner::computeClosure();
  if (scTerms_.size()) {
//    computeClosure_InverseAdjacency(scPairs_, scPairsInverse_);
    computeClosure_InverseTopological(scSuccessors_, scPredecessors_);
  }

//  if (spTerms_.size()) {
//    computeClosure_InverseAdjacency(spPairs_, spPairsInverse_);
//  }
}

void NativeReasoner::printClosure(const TermMap& closure, bool translate)
{
  auto closure_it(std::begin(closure));
  for (; closure_it != std::end(closure); ++closure_it) {
    if (translate) {
      std::cout << dict_.Find(closure_it->first) << ": ";
    } else {
      std::cout << closure_it->first << ": ";
    }
    auto children = closure_it->second;
    auto children_it(std::begin(children));
    for (; children_it != std::end(children); ++children_it) {
      if (translate) {
        std::cout << dict_.Find(*children_it) << " ";
      } else {
        std::cout << *children_it << " ";
      }
    }
    std::cout << std::endl;
  }
}
