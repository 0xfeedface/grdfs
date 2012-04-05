//
//  OpenCLReasoner.h
//  grdfs
//
//  Created by Norman Heino on 11-12-11.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef OpenCLReasoner_h
#define OpenCLReasoner_h

#include "Reasoner.h"
#include "Store.h"

#define __CL_ENABLE_EXCEPTIONS
#if defined(__APPLE__) || defined(__MACOSX)

// disable some warnings during include
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wignored-qualifiers"
#include "cl.hpp"
#pragma clang diagnostic pop

#else
#include <CL/cl.hpp>
#endif

#include <map>
#include <string>

class OpenCLReasoner : public Reasoner {
public:
  OpenCLReasoner(Dictionary& dict);
  ~OpenCLReasoner();
  void computeClosure();
private:
  cl::Context* context_ = nullptr;
  cl::CommandQueue* queue_ = nullptr;
  cl::Device* device_ = nullptr;
  cl::Program* program_ = nullptr;
  
  cl::Context* context(cl_device_type type = CL_DEVICE_TYPE_GPU);
  cl::CommandQueue* commandQueue(bool enableProfiling = false);
  cl::Program* program(const std::string& source);
  std::string loadSource(const std::string& filename);
  
  template <typename T>
  void createBuffer(cl::Buffer& buffer, cl_mem_flags, std::vector<T>&);

  void computeTransitiveClosure(Store::TermMap& successorMap, const Store::TermMap& predecessorMap);
  
  // Join source against match and store the result in target.
  void computeJoin(Store::TermVector& target, Store::TermVector& source, Store::TermVector& match);
  
  void spanTriplesByPredicate(Store::TripleVector& triples, Store::TermVector& predicateMapIndexes, Store::TermMap& predicateMap);
  void spanTriplesByObject(Store::TripleVector& triples, Store::TermVector& objectMapIndexes, Store::TermMap& objectMap, term_id predicate);
};

#endif
