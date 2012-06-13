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
#include <cmath>
#include <queue>
#include <sstream>
#include <fstream>
#include <iostream>

#define MIN(x, y) (x < y ? x : y)
#define ROT(val, shift) (((val) >> shift) | ((val) << (64 - shift)))

typedef std::queue<term_id> TermQueue;

////////////////////////////////////////////////////////////////////////////////

OpenCLReasoner::OpenCLReasoner(Dictionary& dict, cl_device_type deviceType)
  : Reasoner(dict), deviceType_(deviceType)
{
  context_ = context(deviceType);
  // query devices
  std::vector<cl::Device> devices = context_->getInfo<CL_CONTEXT_DEVICES>();
  // we use the first device
  device_ = new cl::Device(devices[0]);
  queue_ = commandQueue();
}

////////////////////////////////////////////////////////////////////////////////

OpenCLReasoner::~OpenCLReasoner()
{
  delete context_;
  delete device_;
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
void OpenCLReasoner::createBuffer(cl::Buffer& buffer, cl_mem_flags flags,
                                  const std::vector<T>& data)
{
  try {
    buffer = cl::Buffer(*context_,
                        flags,
                        data.size() * sizeof(T),
                        // FIXME:
                        // OpenCL C++ bindings do not provide a Buffer constructor
                        // with a const argument, even if the memory is CL_MEM_READ_ONLY,
                        // so we have to cast away the const. :(
                        const_cast<T*>(data.data()));
  } catch (cl::Error& err) {
    std::stringstream str;
    switch (err.err()) {
    case CL_INVALID_BUFFER_SIZE:
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
      str << "Insufficient device memory (tried to allocate "
          << data.size() * sizeof(T)
          << " bytes).";
      break;
    case CL_OUT_OF_HOST_MEMORY:
      str << "Insufficient host memory.";
      break;
    default:
      throw;
    }
    throw Error(str.str());
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeClosure()
{
  try {
    computeClosureInternal();
  } catch (cl::Error& clerr) {
    std::stringstream str;
    switch (clerr.err()) {
    case CL_INVALID_BUFFER_SIZE:
      str << "Insufficient device memory.";
      break;
    default:
      str << "Unhandled OpenCL error: "
          << clerr.what()
          << " (" << clerr.err() << ").";
      break;
    }
    throw Error(str.str());
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeClosureInternal()
{
  if (spTerms_.size()) {
    // 1) compute rule 5 (subPropertyOf transitivity)
    computeTransitiveClosure(spSuccessors_, spPredecessors_, subPropertyOf_);

    // 2) compute rule 7 (subPropertyOf inheritance)
    if (triples_.size()) {
      // We use plain non-type, non-schema triples only.
      // Otherwise it would be non-authorative.
      const Store::KeyVector& predicates(triples_.predicates());
      Store::KeyVector results(predicates.size(), 0);
      Store::KeyVector schemaSubjects;
      for (auto spSubject : spSuccessors_) {
        term_id subject(spSubject.first);
        schemaSubjects.push_back(subject);
      }
      computeJoin(results, predicates, schemaSubjects);

      spanTriplesByPredicate(triples_.subjects(), triples_.predicates(),
                             triples_.objects(), results, spSuccessors_);
    }
  }

  if (domTriples_.size() && triples_.size()) {
    // 3) compute rule 2 (domain expansion)
    // We use plain non-type, non-schema triples only.
    // Otherwise it would be non-authorative.
    const Store::KeyVector& predicates(triples_.predicates());
    const Store::KeyVector& subjects(triples_.subjects());
    Store::KeyVector objectResults;
    Store::KeyVector subjectResults;
    computeJoinRule(objectResults, subjectResults, predicates, subjects, domTriples_,
                    typeTriples_.subjects(), typeTriples_.objects());
    materializeWithProperty(subjectResults, objectResults, type_);
  }

  if (rngTriples_.size() && triples_.size()) {
    // 4) compute rule 3 (range expansion)
    // We use plain non-type, non-schema triples only.
    // Otherwise it would be non-authorative.
    const Store::KeyVector& predicates(triples_.predicates());
    const Store::KeyVector& objects(triples_.objects());
    Store::KeyVector objectResults;
    Store::KeyVector subjectResults;
    computeJoinRule(objectResults, subjectResults, predicates, objects, rngTriples_,
                    typeTriples_.subjects(), typeTriples_.objects());
    materializeWithProperty(subjectResults, objectResults, type_);
  }

  if (scTerms_.size()) {
    // compute rule 11 (subClassOf transitivity)
    computeTransitiveClosure(scSuccessors_, scPredecessors_, subClassOf_);

    // compute rule 9 (subClassOf inheritance)
    if (typeTriples_.size()) {
      // According to rule 9, we use rdf:type triples only
      const Store::KeyVector& objects(typeTriples_.objects());
      const Store::KeyVector& subjects(typeTriples_.subjects());
      Store::KeyVector objectResults;
      Store::KeyVector subjectResults;
      computeJoinRule(objectResults, subjectResults, objects, subjects, scSuccessors_,
                      subjects, objects);
      materializeWithProperty(subjectResults, objectResults, type_);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

Reasoner::TimingMap OpenCLReasoner::timings()
{
  TimingMap result;
  result.insert(std::make_pair("host", hostTime_.elapsed()));
  result.insert(std::make_pair("device", deviceTime_.elapsed()));
  result.insert(std::make_pair("storage", storeTimer_.elapsed()));
  result.insert(std::make_pair("uniqueing", uniqueingTimer_.elapsed()));
  return result;
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::materializeWithProperty(const Store::KeyVector& subjects,
    const Store::KeyVector& objects,
    const KeyType property)
{
  for (std::size_t i(0), end(subjects.size()); i != end; ++i) {
    auto subject(subjects[i]);
    auto object(objects[i]);
    if (subject) {
      // std::cout << subject << " " << object << std::endl;
      // std::cout << dict_.Find(subject) << " " << dict_.Find(object) << std::endl;
      Timer t;
      t.start();
      Store::Triple triple(subject, property, object);
      bool stored = addTriple(triple, Store::kFlagsEntailed);
      t.stop();
      if (stored) {
        storeTimer_.addTimer(t);
      } else {
        uniqueingTimer_.addTimer(t);
        // print rejected s, p
        // std::cout << triple.subject << " " << triple.object << std::endl;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

/*!
 * Given a vector of triples (s, p, o), a map (p : p1, p2, p3 ...),
 * and a vector of indexes into the map for each triple, comstructs
 * the triples (s, p1, o), (s, p2, o), (s, p3, o), ...
 */
void OpenCLReasoner::spanTriplesByPredicate(const Store::KeyVector& subjects,
    const Store::KeyVector& predicates,
    const Store::KeyVector& objects,
    const Store::KeyVector& predicateMapIndexes,
    const TermMap& predicateMap)
{
  // We iterate over subjects but all vectors should have the same size
  for (std::size_t i(0), size(subjects.size()); i != size; ++i) {
    KeyType subject(subjects[i]);
    KeyType object(objects[i]);
    KeyType predicateMapIndex(predicateMapIndexes[i]);
    if (predicateMapIndex) {
      Timer t;
      bool stored(false);
      try {
        for (auto predicate : predicateMap.at(predicateMapIndex)) {
          t.start();
          stored = addTriple(Store::Triple(subject, predicate, object), Store::kFlagsEntailed);
          t.stop();
        }
      } catch (std::out_of_range& oor) {
        t.stop();
        std::stringstream str(oor.what());
        str << " (" << predicateMapIndex << " not found).";
        throw Error(str.str());
      }
      if (stored) {
        storeTimer_.addTimer(t);
      } else {
        uniqueingTimer_.addTimer(t);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

std::size_t OpenCLReasoner::hashTerm(uint64_t term)
{
  // TODO: real 8 byte hash
  return hashPair(term, 0UL);
}

////////////////////////////////////////////////////////////////////////////////

std::size_t OpenCLReasoner::hashPair(uint64_t first, uint64_t second)
{
  static uint64_t kMul = 0x9ddfea08eb382d69UL;
  uint64_t b = ROT(second + 16 /* len */, 16 /* len */);
  uint64_t c = (first ^ b) * kMul;

  c ^= (c >> 47);
  b = (b ^ c) * kMul;
  b ^= (b >> 47);
  b *= kMul;

  return b ^ second;
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::buildSchemaHash(BucketInfoVector& bucketInfos,
                                     Store::KeyVector& values,
                                     const TermMap& successorMap,
                                     cl_uint& size)
{
  // round to next power of two for sizeo
  std::size_t logSize(ceil(log2(successorMap.size())) + 1);
  std::size_t entries(0);
  size = (1 << logSize);

  // count number of entries per bucket index
  std::vector<cl_uint> bucketSizes(size, 0);
  for (auto & v : successorMap) {
    if (v.second.size()) {
      std::size_t hash(hashTerm(v.first) % size);
      // one new bucket entry
      bucketSizes[hash] += 1 + v.second.size();
      entries += 1 + v.second.size();
    }
  }

  // determine index for each bucket
  std::size_t accumBucketSize(0);
  for (std::size_t i(0); i != size; ++i) {
    cl_uint bucketSize(bucketSizes[i]);
    if (bucketSize) {
      bucketInfos.emplace_back(BucketInfo(accumBucketSize, bucketSize));
      accumBucketSize += bucketSize;
    } else {
      bucketInfos.emplace_back(BucketInfo(CL_UINT_MAX, 0));
    }
  }

  // store bucket data
  values.resize(entries);
  for (auto & v : successorMap) {
    if (v.second.size()) {
      std::size_t hash(hashTerm(v.first) % size);
      BucketInfo& info(bucketInfos[hash]);
      cl_uint bucketIndex = info.start + info.free;
      values[bucketIndex] = v.first | (v.second.size() << 48);
      unsigned i(0);
      for (auto & w : v.second) {
        values[bucketIndex + ++i] = w;
      }
      info.free += 1 + v.second.size();
    }
  }

  /*
   * // debug check
   * for (auto& v : successorMap) {
   *   BucketInfo& i(bucketInfos[hashTerm(v.first)%size]);
   *   bool found(false);
   *   for (std::size_t j(0); j != i.size;) {
   *     if ((values[i.start + j] & 0x0000ffffffffffff) == v.first) {
   *       found = true;
   *       break;
   *     }
   *     cl_uint size((values[i.start + j] & 0xffff000000000000) >> 48);
   *     j += size + 1;
   *   }
   *   assert(found == true);
   * }
   */
}

void OpenCLReasoner::buildHash(BucketInfoVector& bucketInfos,
                               BucketVector& buckets,
                               const Store::KeyVector& subjects,
                               const Store::KeyVector& objects,
                               cl_uint& size)
{
  std::size_t logSize(ceil(log2(subjects.size())));
  size = (1 << logSize);

  // count number of entries for each bucket
  std::vector<cl_ushort> bucketSizes(size, 0);
  for (std::size_t i(0), end(subjects.size()); i != end; ++i) {
    KeyType s(subjects[i]), o(objects[i]);
    std::size_t index(hashPair(s, o) & (size - 1));
    ++bucketSizes[index];
  }

  // Timer t1;

  // determine index for each bucket
  bucketInfos.resize(size, BucketInfo());
  // t1.start();
  for (std::size_t i(0), end(subjects.size()); i != end; ++i) {
    KeyType s(subjects[i]), o(objects[i]);
    std::size_t hash(hashPair(s, o) & (size - 1));
    cl_ushort bucketSize(bucketSizes[hash]);
    if (bucketSize == 1) {
      // overflow-free bucket
      BucketInfo info(buckets.size(), 1, 1);
      bucketInfos[hash] = info;
      buckets.emplace_back(s, o);
    } else if (bucketSize > 1) {
      // bucket with overflows entry
      BucketInfo& info(bucketInfos[hash]);
      if (!info.size) {
        info.start = buckets.size();
        info.size = bucketSize;
        buckets.resize(buckets.size() + bucketSize);
        buckets[info.start] = BucketEntry(s, o);
      } else {
        buckets[info.start + info.free] = BucketEntry(s, o);
      }
      ++info.free;
    }
  }
  // t1.stop();

  /*
   * std::cout << "t1: " << t1.elapsed() << " ms\n\n";
   */

  /*
   * // debug check
   * for (std::size_t i(0), end(subjects.size()); i != end; ++i) {
   *   KeyType s(subjects[i]), o(objects[i]);
   *   std::size_t hash(hashPair(s, o) & (size - 1));
   *   BucketInfo info(bucketInfos[hash]);
   *   // make sure buckets are full
   *   assert(info.size == info.free);
   *   bool found(false);
   *   for (cl_uint i(info.start), end(info.start + info.size); i != end; ++i) {
   *     if (buckets[i].subject == s && buckets[i].object == o) {
   *       found = true;
   *     }
   *   }
   *   assert(found == true);
   * }
   */
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeJoinRule(Store::KeyVector& entailedObjects,
                                     Store::KeyVector& entailedSubjects,
                                     const Store::KeyVector& objectSource,
                                     const Store::KeyVector& subjectSource,
                                     const TermMap& schemaSuccessorMap,
                                     const Store::KeyVector& indexSubjects,
                                     const Store::KeyVector& indexObjects)
{
  deviceTime_.start();

  cl::Kernel inheritanceKernel(*program(), "count_results_hashed");
  std::size_t globalSize = subjectSource.size();

  // output with pair of matched element or CL_UINT_MAX, successor size
  std::vector<std::pair<cl_uint, cl_uint>> resultInfo(globalSize);
  cl::Buffer outputBuffer;
  createBuffer(outputBuffer, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, resultInfo);
  inheritanceKernel.setArg(0, outputBuffer);

  // input elements to join
  cl::Buffer inputBuffer;
  createBuffer(inputBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, objectSource);
  inheritanceKernel.setArg(1, inputBuffer);

  // actual number of elements
  inheritanceKernel.setArg<cl_uint>(2, globalSize);

  // build hash table for join
  BucketInfoVector schemaBucketInfos;
  Store::KeyVector schemaBuckets;
  cl_uint schemaBucketHashTableSize;
  buildSchemaHash(schemaBucketInfos, schemaBuckets, schemaSuccessorMap, schemaBucketHashTableSize);

  // schema bucket infos
  cl::Buffer schemaBucketInfoBuffer;
  createBuffer(schemaBucketInfoBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, schemaBucketInfos);
  inheritanceKernel.setArg(3, schemaBucketInfoBuffer);

  // schema buckets
  cl::Buffer schemaBucketBuffer;
  createBuffer(schemaBucketBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, schemaBuckets);
  inheritanceKernel.setArg(4, schemaBucketBuffer);

  // size of the hash table
  inheritanceKernel.setArg<cl_uint>(5, schemaBucketHashTableSize);

  // determine optimal work group size for the kernel
  // and the global enqueued size as an integer multiple of work group size
  std::size_t workGoupSize(inheritanceKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(*device_));
  std::size_t shiftWidth(log2(workGoupSize));
  std::size_t enqueueSize((((globalSize - 1) >> shiftWidth) + 1) << shiftWidth);

  // enqueue
  queue_->enqueueNDRangeKernel(inheritanceKernel,
                               cl::NullRange,
                               cl::NDRange(enqueueSize),
                               cl::NDRange(workGoupSize),
                               NULL,
                               NULL);
  // read
  queue_->enqueueReadBuffer(outputBuffer,
                            CL_TRUE,
                            0,
                            resultInfo.size() * sizeof(std::pair<cl_uint, cl_uint>),
                            resultInfo.data());

  deviceTime_.stop();

  // TODO: perform exclusive scan on device
  std::size_t accumResultSize(0);
  std::vector<std::pair<cl_uint, cl_uint>> localResultInfo;
  for (std::size_t i(0), end(resultInfo.size()); i != end; ++i) {
    auto& value(resultInfo[i]);
    cl_uint tmp = value.second;
    value.second = accumResultSize;
    // create local mapping
    // each thread can determine which successor of a given
    // subject it operates on
    for (std::size_t localSize(0); localSize != tmp; ++localSize) {
      localResultInfo.push_back(std::make_pair(i, localSize));
    }
    accumResultSize += tmp;
  }

  if (accumResultSize) {
    BucketVector buckets;
    BucketInfoVector bucketInfos;
    cl_uint size;
    hostTime_.start();
    buildHash(bucketInfos, buckets, indexSubjects, indexObjects, size);
    hostTime_.stop();

    deviceTime_.start();

    cl::Kernel matKernel(*program(), "materialize_results");

    // output with entailed objects
    cl::Buffer objectOutputBuffer;
    entailedObjects.resize(accumResultSize, 0);
    createBuffer(objectOutputBuffer, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, entailedObjects);
    matKernel.setArg(0, objectOutputBuffer);

    // output with subjects for entailed triples
    cl::Buffer subjectOutputBuffer;
    entailedSubjects.resize(accumResultSize, 0);
    createBuffer(subjectOutputBuffer, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, entailedSubjects);
    matKernel.setArg(1, subjectOutputBuffer);

    // result from above
    cl::Buffer previousResultsBuffer;
    createBuffer(previousResultsBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, resultInfo);
    matKernel.setArg(2, previousResultsBuffer);

    // local result info
    cl::Buffer localResultInfoBuffer;
    createBuffer(localResultInfoBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, localResultInfo);
    matKernel.setArg(3, localResultInfoBuffer);

    // schema buckets, same as above
    matKernel.setArg(4, schemaBucketBuffer);

    // input subjects
    cl::Buffer subjectBuffer;
    createBuffer(subjectBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, subjectSource);
    matKernel.setArg(5, subjectBuffer);

    // actual global size
    matKernel.setArg<cl_uint>(6, static_cast<cl_uint>(accumResultSize));

    // bucket info
    cl::Buffer bucketInfoBuffer;
    createBuffer(bucketInfoBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bucketInfos);
    matKernel.setArg(7, bucketInfoBuffer);

    // buckets
    cl::Buffer bucketBuffer;
    createBuffer(bucketBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, buckets);
    matKernel.setArg(8, bucketBuffer);

    // hash table size
    matKernel.setArg<cl_uint>(9, static_cast<cl_uint>(size));

    // determine optimal work group size for the kernel
    // and the global enqueued size as an integer multiple of work group size
    workGoupSize = matKernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(*device_);
    shiftWidth = log2(workGoupSize);
    enqueueSize = (((accumResultSize - 1) >> shiftWidth) + 1) << shiftWidth;

    // enqueue
    queue_->enqueueNDRangeKernel(matKernel,
                                 cl::NullRange,
                                 cl::NDRange(enqueueSize),
                                 cl::NDRange(workGoupSize),
                                 NULL,
                                 NULL);

    // local deduplication
    cl::Kernel dedupKernel(*program(), "deduplication");

    workGoupSize = 256;
    shiftWidth = log2(workGoupSize);
    enqueueSize = (accumResultSize >> shiftWidth) << shiftWidth;

    if (enqueueSize && !(deviceType_ & CL_DEVICE_TYPE_CPU)) {
      dedupKernel.setArg(0, objectOutputBuffer);
      dedupKernel.setArg(1, subjectOutputBuffer);

      queue_->enqueueNDRangeKernel(dedupKernel,
                                   cl::NullRange,
                                   cl::NDRange(enqueueSize),
                                   cl::NDRange(workGoupSize),
                                   NULL,
                                   NULL);
    }

    // read objects
    queue_->enqueueReadBuffer(objectOutputBuffer,
                              CL_FALSE,
                              0,
                              accumResultSize * sizeof(Dictionary::KeyType),
                              entailedObjects.data());
    // read subjects
    queue_->enqueueReadBuffer(subjectOutputBuffer,
                              CL_TRUE,
                              0,
                              accumResultSize * sizeof(Dictionary::KeyType),
                              entailedSubjects.data());

    // block until done
    queue_->finish();

    deviceTime_.stop();
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeJoin(Store::KeyVector& target,
                                 const Store::KeyVector& source,
                                 Store::KeyVector& match)
{
  deviceTime_.start();

  cl::Kernel inheritanceKernel(*program(), "phase1");
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
  inheritanceKernel.setArg<cl_uint>(3, static_cast<cl_uint>(match.size()));

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

  deviceTime_.stop();
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeTransitiveClosure(TermMap& successorMap,
                                              const TermMap& predecessorMap,
                                              const Dictionary::KeyType property)
{
  hostTime_.start();

  TermQueue nodes;
  std::unordered_map<term_id, bool> finishedNodes;

  // initialize the queue with the leaf nodes
  auto it(std::begin(predecessorMap));
  auto end(std::end(predecessorMap));
  auto succEnd(std::end(successorMap));
  for (; it != end; ++it) {
    if (successorMap.find(it->first) == succEnd) {
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

    auto pit(predecessorMap.find(currentNode));
    if (pit != std::end(predecessorMap)) {
      auto parent_it(std::begin(pit->second));
      auto parent_end(std::end(pit->second));
      for (; parent_it != parent_end; ++parent_it) {
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
      auto children_end(std::end(cit->second));
      for (; children_it != children_end; ++children_it) {
        auto gcit(successorMap.find(*children_it));
        if (gcit != std::end(successorMap)) {
          // add all of the children's children as the current node's children
          auto grandchildren_it(std::begin(gcit->second));
          auto grandchildren_end(std::end(gcit->second));
          for (; grandchildren_it != grandchildren_end; ++grandchildren_it) {
            hostTime_.stop();
            Timer t;
            t.start();
            bool stored = addTriple(Store::Triple(currentNode, property, *grandchildren_it),
                                    Store::kFlagsEntailed);
            t.stop();
            if (stored) {
              storeTimer_.addTimer(t);
            } else {
              uniqueingTimer_.addTimer(t);
              // std::cout << currentNode << " " << *grandchildren_it << std::endl;
            }
            hostTime_.start();
          }
        }
      }
    }
  }

  hostTime_.stop();
}

////////////////////////////////////////////////////////////////////////////////

cl::Context* OpenCLReasoner::context(cl_device_type type)
{
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

cl::CommandQueue* OpenCLReasoner::commandQueue(bool enableProfiling)
{
  // create command queue
  cl::CommandQueue* queue = new cl::CommandQueue(*context_,
      *device_,
      enableProfiling ? CL_QUEUE_PROFILING_ENABLE : 0);
  return queue;
}

////////////////////////////////////////////////////////////////////////////////

cl::Program* OpenCLReasoner::program()
{
  if (nullptr != program_) {
    return program_;
  }

  std::string source(loadSource(programName_));
  cl::Program::Sources sources(1, std::make_pair(source.c_str(), 0));
  program_ = new cl::Program(*context_, sources);

  try {
    program_->build(context_->getInfo<CL_CONTEXT_DEVICES>());
  } catch (cl::Error err) {
    std::stringstream str;
    str << "build error: " << program_->getBuildInfo<CL_PROGRAM_BUILD_LOG>(*device_);
    throw Error(str.str());
  }

  return program_;
}

////////////////////////////////////////////////////////////////////////////////

std::string OpenCLReasoner::loadSource(const std::string& filename)
{
  std::ifstream fin(filename, std::ios_base::in);
  if (!fin.is_open()) {
    throw Error("could not read kernel source file.");
  }

  std::istreambuf_iterator<char> fbegin(fin);
  std::istreambuf_iterator<char> eos;
  std::string source(fbegin, eos);
  return source;
}
