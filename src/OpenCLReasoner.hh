//
//  OpenCLReasoner.h
//  grdfs
//
//  Created by Norman Heino on 11-12-11.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef OpenCLReasoner_h
#define OpenCLReasoner_h

#include "Reasoner.hh"
#include "Store.hh"

#define __CL_ENABLE_EXCEPTIONS
#if defined(__APPLE__) || defined(__MACOSX)

// disable some warnings during include
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wignored-qualifiers"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
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

struct BucketInfo {
  cl_uint start;
  cl_ushort size;
  cl_ushort free;
  // default constuctor
  BucketInfo()
    : start(0), size(0), free(0) {};
  // value constructor
  BucketInfo(cl_uint start_, cl_ushort size_)
    : start(start_), size(size_), free(0) {};
  BucketInfo(cl_uint start_, cl_ushort size_, cl_ushort free_)
    : start(start_), size(size_), free(free_) {};
};

struct BucketEntry {
  cl_ulong subject;
  cl_ulong object;
  // default constructor
  BucketEntry() = default;
  // value constuctor
  BucketEntry(cl_ulong subject_, cl_ulong object_)
    : subject(subject_), object(object_) {};
};

typedef std::vector<BucketInfo> BucketInfoVector;
typedef std::vector<BucketEntry> BucketVector;

class OpenCLReasoner : public Reasoner
{
public:
  OpenCLReasoner(Dictionary& dict, RuleSet ruleSet, cl_device_type deviceType, bool localDedup, bool globalDedup);
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

  cl_device_type deviceType_;

  cl::Context* context(cl_device_type type);
  cl::CommandQueue* commandQueue(bool enableProfiling = false);
  cl::Program* program();
  std::string loadSource(const std::string& filename);

  bool localDeduplication_;
  bool globalDeduplication_;

  void computeClosureInternal();

  template <typename T>
  void createBuffer(cl::Buffer& buffer, cl_mem_flags, const std::vector<T>& data);

  void computeTransitiveClosure(TermMap& successorMap,
                                const TermMap& predecessorMap,
                                const Dictionary::KeyType predicate);

  // Join source against match and store the result in target.
  void computeJoin(Store::KeyVector& target, const Store::KeyVector& source, Store::KeyVector& match);

  void computeJoinRule(Store::KeyVector& objectTarget,
                       Store::KeyVector& subjectTarget,
                       const Store::KeyVector& objectSource,
                       const Store::KeyVector& subjectSource,
                       const TermMap& schemaSuccessorMap,
                       const Store::KeyVector& indexSubjects,
                       const Store::KeyVector& indexObjects);

  std::size_t hashTerm(uint64_t first);
  std::size_t hashPair(uint64_t first, uint64_t second);

  void buildHash(BucketInfoVector& bucketInfos,
                 BucketVector& buckets,
                 const Store::KeyVector& subjects,
                 const Store::KeyVector& objects,
                 cl_uint& size);

  void buildSchemaHash(BucketInfoVector& bucketInfos,
                       Store::KeyVector& values,
                       const TermMap& successorMap,
                       cl_uint& size);

  void materializeWithProperty(const Store::KeyVector& subjects,
                               const Store::KeyVector& objects,
                               const KeyType property);

  std::size_t spanTriplesByPredicate(const Store::KeyVector& subjects,
                                     const Store::KeyVector& predicates,
                                     const Store::KeyVector& objects,
                                     const Store::KeyVector& predicateMapIndexes,
                                     const TermMap& predicateMap);
};

#endif
