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
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif
#include "cl.hpp"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else
#include <CL/cl.hpp>
#endif

#include <map>
#include <string>

class OpenCLReasoner : public Reasoner {
public:
  OpenCLReasoner(Dictionary& dict, cl_device_type deviceType = CL_DEVICE_TYPE_GPU);
  ~OpenCLReasoner();
  void computeClosure();
  Reasoner::TimingMap timings();
private:
  cl::Context* context_ = nullptr;
  cl::CommandQueue* queue_ = nullptr;
  cl::Device* device_ = nullptr;
  cl::Program* program_ = nullptr;
  const std::string programName_ = "src/grdfs_kernels.cl";
  Timer deviceTime_;
  Timer hostTime_;
  Timer storeTimer_;
  
  cl::Context* context(cl_device_type type);
  cl::CommandQueue* commandQueue(bool enableProfiling = false);
  cl::Program* program();
  std::string loadSource(const std::string& filename);
  
  void createBuffer(cl::Buffer& buffer, cl_mem_flags, const Store::KeyVector& data);

  void computeTransitiveClosure(TermMap& successorMap,
                                const TermMap& predecessorMap,
                                const Dictionary::KeyType predicate);
  
  // Join source against match and store the result in target.
  void computeJoin(Store::KeyVector& target, const Store::KeyVector& source, Store::KeyVector& match);
  
  void spanTriplesByPredicate(const Store::KeyVector& subjects,
                              const Store::KeyVector& predicates,
                              const Store::KeyVector& objects,
                              const Store::KeyVector& predicateMapIndexes,
                              const TermMap& predicateMap);

  void spanTriplesByObject(const Store::KeyVector& subjects,
                           const Store::KeyVector& predicates,
                           const Store::KeyVector& objects,
                           const Store::KeyVector& objectMapIndexes,
                           const TermMap& objectMap,
                           const KeyType predicate,
                           const bool iseObject);

  void spanTriplesByObject(const Store::KeyVector& subjects,
                           const Store::KeyVector& predicates,
                           const Store::KeyVector& objects,
                           const Store::KeyVector& objectMapIndexes,
                           const TermMap& objectMap,
                           const KeyType predicate);
};

#endif
