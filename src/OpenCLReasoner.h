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
  Timer uniqueingTimer_;

  cl::Context* context(cl_device_type type);
  cl::CommandQueue* commandQueue(bool enableProfiling = false);
  cl::Program* program();
  std::string loadSource(const std::string& filename);

  std::size_t batchSize(std::size_t globalSize);

  void computeClosureInternal();

  template <typename T>
  void createBuffer(cl::Buffer& buffer, cl_mem_flags, const std::vector<T>& data);

  template <typename T>
  void createBuffer(cl::Buffer&, cl_mem_flags,
                    typename std::vector<T>::const_iterator,
                    typename std::vector<T>::const_iterator);

  void computeTransitiveClosure(TermMap& successorMap,
                                const TermMap& predecessorMap,
                                const Dictionary::KeyType predicate);

  // Join source against match and store the result in target.
  void computeJoin(Store::KeyVector& target, const Store::KeyVector& source, Store::KeyVector& match);

  void computeJoinRule(Store::KeyVector& objectTarget,
                       Store::KeyVector& subjectTarget,
                       Store::KeyVector::const_iterator subjectsBegin,
                       Store::KeyVector::const_iterator subjectsEnd,
                       Store::KeyVector::const_iterator predicatesBegin,
                       Store::KeyVector::const_iterator predicatesEnd,
                       Store::KeyVector::const_iterator objectsBegin,
                       Store::KeyVector::const_iterator objectsEnd,
                       Store::KeyVector::const_iterator joinBegin,
                       Store::KeyVector::const_iterator joinEnd,
                       const TermMap&,
                       bool useIndex = true);

  std::size_t hashTerm(uint64_t first);
  std::size_t hashPair(uint64_t first, uint64_t second);

  void buildHash(BucketInfoVector& bucketInfos,
                 BucketVector& buckets,
                 Store::KeyVector::const_iterator subjectsBegin,
                 Store::KeyVector::const_iterator subjectsEnd,
                 Store::KeyVector::const_iterator predicatesBegin,
                 Store::KeyVector::const_iterator predicatesEnd,
                 Store::KeyVector::const_iterator objectsBegin,
                 Store::KeyVector::const_iterator objectsEnd,
                 cl_uint& size);

  void buildSchemaHash(BucketInfoVector& bucketInfos,
                       Store::KeyVector& values,
                       const TermMap& successorMap,
                       cl_uint& size);

  void materializeWithProperty(const Store::KeyVector& subjects,
                               const Store::KeyVector& objects,
                               const KeyType property);

  void spanTriplesByPredicate(const Store::KeyVector& subjects,
                              const Store::KeyVector& predicates,
                              const Store::KeyVector& objects,
                              const Store::KeyVector& predicateMapIndexes,
                              const TermMap& predicateMap);
};

#endif
