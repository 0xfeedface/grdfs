//
//  OpenCLReasoner.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-11.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include "OpenCLReasoner.h"

#include <vector>
#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <cassert>
#include <queue>
#include <sstream>

#include <iostream>

typedef std::queue<term_id> TermQueue;

void PrintVector(Store::TermVector const& terms) {
  auto iter(terms.cbegin());
  std::cout << *iter++;
  for (; iter != terms.end(); ++iter) {
    std::cout << " " << *iter;
  }
  std::cout << "\n----------------\n";
}


////////////////////////////////////////////////////////////////////////////////

OpenCLReasoner::OpenCLReasoner(Dictionary& dict) : Reasoner(dict) {
  context_ = context(CL_DEVICE_TYPE_CPU);
  // query devices
  std::vector<cl::Device> devices = context_->getInfo<CL_CONTEXT_DEVICES>();
  device_ = &devices[0];
  queue_ = commandQueue();
  program_ = program(loadSource("src/grdfs_kernels.cl"));
}

////////////////////////////////////////////////////////////////////////////////

OpenCLReasoner::~OpenCLReasoner() {
  delete context_;
  delete queue_;

  // program_ is created lazily, so at this point we don't know
  // whether it has been created or not
  if (program_ != nullptr) {
    delete program_;
  }
}

////////////////////////////////////////////////////////////////////////////////

// Create an OpenCL buffer from STL vector
template <typename T>
void OpenCLReasoner::createBuffer(cl::Buffer& buffer, cl_mem_flags flags, std::vector<T>& data) {
  try {
    buffer = cl::Buffer(*context_,
                        flags,
                        data.size() * sizeof(T),
                        data.data());
  } catch (cl::Error& err) {
    std::stringstream str(err.what());
    str << " (" << err.err() << ")";
    throw Error(str.str());
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeClosure() {
  if (spTerms_.size()) {
    // 1) compute rule 5 (subPropertyOf transitivity)
    computeTransitiveClosure(spSuccessors_, spPredecessors_);

    // 2) compute rule 7 (subPropertyOf inheritance)
    if (triples_.size()) {
      Store::TermVector predicates;
      copyPredicates(predicates);
      Store::TermVector results(predicates.size(), 0);
      Store::TermVector schemaSubjects;
      for (auto spSubject : spSuccessors_) {
        term_id subject(spSubject.first);
        schemaSubjects.push_back(subject);
      }
      try {
        computeJoin(results, predicates, schemaSubjects);
      } catch (cl::Error& err) {
        std::stringstream str;
        str << err.what() << " (" << err.err() << ")";
        throw Error(str.str());
      }
      
      Store::TripleVector sourceTriples;
      triples_.copyTriples(sourceTriples);
      
      spanTriplesByPredicate(sourceTriples, results, spSuccessors_);
    }
  }

  if (domTriples_.size() || rngTriples_.size()) {
    // 3) compute rules 2, 3 (domain, range expansion)
    // TODO:
  }

  if (scTerms_.size()) {
    // compute rule 11 (subClassOf transitivity)
    computeTransitiveClosure(scSuccessors_, scPredecessors_);

    // compute rule 9 (subClassOf inheritance)
    if (triples_.size()) {
      Store::TermVector objects;
      copyObjects(objects);
      Store::TermVector results(objects.size(), 0);
      Store::TermVector schemaSubjects;
      for (auto scSubject : scSuccessors_) {
        term_id subject(scSubject.first);
        schemaSubjects.push_back(subject);
      }
      try {
        computeJoin(results, objects, schemaSubjects);
      } catch (cl::Error& err) {
        std::stringstream str;
        str << err.what() << " (" << err.err() << ")";
        throw Error(str.str());
      }

      Store::TripleVector sourceTriples;
      triples_.copyTriples(sourceTriples);
      
      spanTriplesByObject(sourceTriples, results, scSuccessors_, type_);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

/*!
 * Given a vector of triples (s, p, o) and a map (p : p1, p2, p3 ...), 
 * constructs the triples (s, p1, o), (s, p2, o), (s, p3, o), ...
 */
void OpenCLReasoner::spanTriplesByPredicate(Store::TripleVector& triples,
                                            Store::TermVector& predicates,
                                            Store::TermMap& predicateMap) {
  for (std::size_t i(0); i != triples.size(); ++i) {
    term_id subject(triples[i].subject);
    term_id object(triples[i].object);
    term_id predicateMapIndex(predicates[i]);
    if (predicateMapIndex) {
      try {
        for (auto predicate : predicateMap.at(predicateMapIndex)) {
          if (triples_.addTriple(triple(subject, predicate, object))) {
            ++inferredTriplesCount_;
          }
        }
      } catch (std::out_of_range& oor) {
        std::stringstream str(oor.what());
        str << " (" << predicateMapIndex << " not found).";
        throw Error(str.str());
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

/*!
 * Given a vector of triples (s, p, o) and a map (o : o1, o2, o3 ...), 
 * constructs the triples (s, p, o1), (s, p, o2), (s, p, o3), ...
 * The predicate is fixed.
 */
void OpenCLReasoner::spanTriplesByObject(Store::TripleVector& triples,
                                         Store::TermVector& objects,
                                         Store::TermMap& objectMap,
                                         term_id predicate) {
  for (std::size_t i(0); i != triples.size(); ++i) {
    term_id subject(triples[i].subject);
    term_id objectMapIndex(objects[i]);
    if (objectMapIndex) {
      try {
        for (auto object : objectMap.at(objectMapIndex)) {
          if (triples_.addTriple(triple(subject, predicate, object))) {
            ++inferredTriplesCount_;
          }
        }
      } catch (std::out_of_range& oor) {
        std::stringstream str(oor.what());
        str << " (" << objectMapIndex << " not found).";
        throw Error(str.str());
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeJoin(Store::TermVector& target,
                                 Store::TermVector& source,
                                 Store::TermVector& match) {
  cl::Kernel inheritanceKernel(*program_, "phase1");
  std::size_t globalSize = source.size();

  /* input elements to match */
  cl::Buffer inputBuffer;
  createBuffer(inputBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, source);
  inheritanceKernel.setArg(0, inputBuffer);

  /* output with matching elements or 0 otherwise */
  cl::Buffer outputBuffer;
  createBuffer(outputBuffer, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, target);
  inheritanceKernel.setArg(1, outputBuffer);

  /* schema elements to be matched against */
  std::sort(std::begin(match), std::end(match));
  cl::Buffer schemaBuffer;
  createBuffer(schemaBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, match);
  inheritanceKernel.setArg(2, schemaBuffer);
  inheritanceKernel.setArg(3, match.size());

  /* enqueue */
  queue_->enqueueNDRangeKernel(inheritanceKernel,
                               cl::NullRange,
                               cl::NDRange(globalSize),
                               cl::NullRange,
                               NULL,
                               NULL);

  /* read */
  queue_->enqueueReadBuffer(outputBuffer,
                            CL_TRUE,
                            0,
                            target.size() * sizeof(term_id),
                            target.data());

  /* block until done */
  queue_->finish();
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeTransitiveClosure(Store::TermMap& successorMap,
                                              const Store::TermMap& predecessorMap) {
  TermQueue nodes;
  std::unordered_map<term_id, bool> finishedNodes;

  // initialize the queue with the leaf nodes
  auto it(std::begin(predecessorMap));
  for (; it != std::end(predecessorMap); ++it) {
    if (successorMap.find(it->first) == std::end(successorMap)) {
      nodes.push(it->first);
    }
  }

  // no leafs means the graph contains cycles
  // TODO: detect strongly connected components and calculate closure on those
  if (!nodes.size()) {
    throw std::runtime_error("Cannot calculate transitive closure on non-DAG.");
  }

  while (nodes.size()) {
    term_id currentNode = nodes.front();
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
}

////////////////////////////////////////////////////////////////////////////////

cl::Context* OpenCLReasoner::context(cl_device_type type) {
  // query platforms
  std::vector<cl::Platform> platforms;
  cl::Platform::get(&platforms);
  // use first platform
  cl_context_properties contextProperties[] = {
    CL_CONTEXT_PLATFORM,
    (cl_context_properties)(platforms[0])(),
    0
  };
  cl::Context* context = new cl::Context(type, contextProperties);
  return context;
}

////////////////////////////////////////////////////////////////////////////////

cl::CommandQueue* OpenCLReasoner::commandQueue(bool enableProfiling) {
  // create command queue
  cl::CommandQueue* queue = new cl::CommandQueue(*context_,
                                                 *device_,
                                                 enableProfiling ? CL_QUEUE_PROFILING_ENABLE : 0);
  return queue;
}

////////////////////////////////////////////////////////////////////////////////

cl::Program* OpenCLReasoner::program(const std::string& source) {
  cl::Program::Sources sources(1, std::make_pair(source.c_str(), 0));
  cl::Program* program = new cl::Program(*context_, sources);

  try {
    program->build(context_->getInfo<CL_CONTEXT_DEVICES>());
  } catch (cl::Error err) {
    std::stringstream str;
    str << "build error: " << program_->getBuildInfo<CL_PROGRAM_BUILD_LOG>(*device_);
    throw std::runtime_error(str.str());
  }

  return program;
}

////////////////////////////////////////////////////////////////////////////////

std::string OpenCLReasoner::loadSource(const std::string& filename) {
  struct stat statbuf;
  FILE        *fh;
  char        *source;

  fh = fopen(filename.c_str(), "r");
  if (fh == 0) {
    throw std::runtime_error("source file not readable.");
  }

  stat(filename.c_str(), &statbuf);
  source = new char[statbuf.st_size + 1];
  fread(source, statbuf.st_size, 1, fh);
  source[statbuf.st_size] = '\0';
  std::string ret(source);
  delete [] source;
  return ret;
}
