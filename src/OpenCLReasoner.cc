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
    : Reasoner(dict) {
  context_ = context(deviceType);
  // query devices
  std::vector<cl::Device> devices = context_->getInfo<CL_CONTEXT_DEVICES>();
  // we use the first device
  device_ = new cl::Device(devices[0]);
  queue_ = commandQueue();
}

////////////////////////////////////////////////////////////////////////////////

OpenCLReasoner::~OpenCLReasoner() {
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
                                  const std::vector<T>& data) {
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
    str << err.what()
        << " (" << err.err() << ")";
    throw Error(str.str());
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeClosure() {
  if (false && spTerms_.size()) {
    // 1) compute rule 5 (subPropertyOf transitivity)
    computeTransitiveClosure(spSuccessors_, spPredecessors_, subPropertyOf_);

    // 2) compute rule 7 (subPropertyOf inheritance)
    if (triples_.size()) {
      hostTime_.start();
      // We use plain non-type, non-schema triples only.
      // Otherwise it would be non-authorative.
      const Store::KeyVector& predicates(triples_.predicates());
      Store::KeyVector results(predicates.size(), 0);
      Store::KeyVector schemaSubjects;
      for (auto spSubject : spSuccessors_) {
        term_id subject(spSubject.first);
        schemaSubjects.push_back(subject);
      }
      hostTime_.stop();
      try {
        computeJoin(results, predicates, schemaSubjects);
      } catch (cl::Error& err) {
        std::stringstream str;
        str << err.what() << " (" << err.err() << ")";
        throw Error(str.str());
      }
      
      spanTriplesByPredicate(triples_.subjects(), triples_.predicates(),
                             triples_.objects(), results, spSuccessors_);
    }
  }

  if (false && domTriples_.size() && triples_.size()) {
    // 3) compute rules 2, 3 (domain, range expansion)
    hostTime_.start();
    // We use plain non-type, non-schema triples only.
    // Otherwise it would be non-authorative.
    const Store::KeyVector& predicates(triples_.predicates());
    Store::KeyVector results(predicates.size(), 0);
    Store::KeyVector schemaSubjects;
    for (auto domSubject : domTriples_) {
      term_id subject(domSubject.first);
      schemaSubjects.push_back(subject);
    }
    hostTime_.stop();
    try {
      computeJoin(results, predicates, schemaSubjects);
    } catch (cl::Error& err) {
      std::stringstream str;
      str << err.what() << " (" << err.err() << ")";
      throw Error(str.str());
    }

    spanTriplesByObject(triples_.subjects(), triples_.predicates(),
                        triples_.objects(), results, domTriples_, type_);
  }

  if (false && rngTriples_.size() && triples_.size()) {
    // 3) compute rules 2, 3 (domain, range expansion)
    hostTime_.start();
    // We use plain non-type, non-schema triples only.
    // Otherwise it would be non-authorative.
    const Store::KeyVector& predicates(triples_.predicates());
    Store::KeyVector results(predicates.size(), 0);
    Store::KeyVector schemaSubjects;
    for (auto rangeValue : rngTriples_) {
      term_id subject(rangeValue.first);
      schemaSubjects.push_back(subject);
    }
    hostTime_.stop();
    try {
      computeJoin(results, predicates, schemaSubjects);
    } catch (cl::Error& err) {
      std::stringstream str;
      str << err.what() << " (" << err.err() << ")";
      throw Error(str.str());
    }

    spanTriplesByObject(triples_.subjects(), triples_.predicates(),
                        triples_.objects(), results, rngTriples_, type_, true);
  }

  if (scTerms_.size()) {
    // compute rule 11 (subClassOf transitivity)
    computeTransitiveClosure(scSuccessors_, scPredecessors_, subClassOf_);

    // compute rule 9 (subClassOf inheritance)
    if (typeTriples_.size()) {
      hostTime_.start();
      // According to rule 9, we use rdf:type triples only
      const Store::KeyVector& objects(typeTriples_.objects());
      const Store::KeyVector& subjects(typeTriples_.subjects());
      Store::KeyVector objectResults;
      Store::KeyVector subjectResults;
      Store::KeyVector schemaSubjects;
      std::vector<std::pair<cl_uint, cl_uint>> schemaSuccessorInfo;
      Store::KeyVector schemaSuccessors;
      cl_uint successorSum(0);

      for (auto& svalue : scSuccessors_) {
        // the schema subjects
        schemaSubjects.push_back(svalue.first);
        // the actual successors
        for (auto& successor : svalue.second) {
          schemaSuccessors.push_back(successor);
        }
        // number of successors for each schema subject
        schemaSuccessorInfo.push_back(std::make_pair(svalue.second.size(), successorSum));
        // update successor sum
        successorSum += svalue.second.size();
      }

      hostTime_.stop();
      try {
        computeJoin2(objectResults, subjectResults, objects, subjects, schemaSubjects, schemaSuccessorInfo, schemaSuccessors);
      } catch (cl::Error& err) {
        std::stringstream str;
        str << err.what() << " (" << err.err() << ")";
        throw Error(str.str());
      }

      for (std::size_t i(0), max(objectResults.size()); i != max; ++i) {
        auto subject(subjectResults[i]);
        auto object(objectResults[i]);
        if (subject) {
          // std::cout << dict_.Find(subject) << " " << dict_.Find(object) << std::endl;
          Timer t;
          t.start();
          Store::Triple triple(subject, type_, object);
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
  }
}

////////////////////////////////////////////////////////////////////////////////

Reasoner::TimingMap OpenCLReasoner::timings() {
  TimingMap result;
  result.insert(std::make_pair("host", hostTime_.elapsed()));
  result.insert(std::make_pair("device", deviceTime_.elapsed()));
  result.insert(std::make_pair("storage", storeTimer_.elapsed()));
  result.insert(std::make_pair("uniqueing", uniqueingTimer_.elapsed()));
  return result;
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
                                            const TermMap& predicateMap) {
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

/*!
 * Given a vector of triples (s, p, o), a map (o : o1, o2, o3 ...), 
 * and a vector of indexes into the map for each triple, constructs
 * the triples (s, p, o1), (s, p, o2), (s, p, o3), ...
 */
void OpenCLReasoner::spanTriplesByObject(const Store::KeyVector& subjects,
                                         const Store::KeyVector& predicates,
                                         const Store::KeyVector& objects,
                                         const Store::KeyVector& objectMapIndexes,
                                         const TermMap& objectMap,
                                         const KeyType predicate,
                                         const bool useObject) {
  for (std::size_t i(0), size(subjects.size()); i != size; ++i) {
    KeyType subject(useObject ? objects[i] : subjects[i]);
    if (!(subject & literalMask)) {
      KeyType objectMapIndex(objectMapIndexes[i]);
      if (objectMapIndex) {
        Timer t;
        bool stored(false);
        try {
          for (auto object : objectMap.at(objectMapIndex)) {
            t.start();
            stored = addTriple(Store::Triple(subject, predicate, object), Store::kFlagsEntailed);
            t.stop();
          }
        } catch (std::out_of_range& oor) {
          t.stop();
          std::stringstream str(oor.what());
          str << " (" << objectMapIndex << " not found).";
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
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::spanTriplesByObject(const Store::KeyVector& subjects,
                                         const Store::KeyVector& predicates,
                                         const Store::KeyVector& objects,
                                         const Store::KeyVector& objectMapIndexes,
                                         const TermMap& objectMap,
                                         const KeyType predicate) {
  spanTriplesByObject(subjects, predicates, objects, objectMapIndexes, objectMap, predicate, false);
}

////////////////////////////////////////////////////////////////////////////////

std::size_t OpenCLReasoner::hashPair(uint64_t first, uint64_t second)
{
  static uint64_t kMul = 0x9ddfea08eb382d69ULL;
  uint64_t b = ROT(second + 16 /* len */, 16 /* len */);
  uint64_t c = (first ^ b) * kMul;

  c ^= (c >> 47);
  b = (b ^ c) * kMul;
  b ^= (b >> 47);
  b *= kMul;

  return b ^ second;
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::buildHash(BucketInfoVector& bucketInfos,
                               BucketVector& buckets,
                               const Store::KeyVector& subjects,
                               const Store::KeyVector& objects,
                               cl_uint& size)
{
  std::size_t logSize(ceil(log2(subjects.size())));
  std::size_t globalSize((1 << logSize));
  std::size_t entries(0);

  size = globalSize;

  // count number of entries for each bucket
  std::vector<cl_uint> bucketSizes(globalSize, 0);
  for (std::size_t i(0), end(subjects.size()); i != end; ++i) {
    std::size_t index(hashPair(subjects[i], objects[i]) & (globalSize - 1));
    ++bucketSizes[index];
    ++entries;
  }

  // determine index for each bucket
  std::size_t accumBucketSize(0);
  for (std::size_t i(0); i != globalSize; ++i) {
    if (bucketSizes[i]) {
      bucketInfos.push_back(BucketInfo(accumBucketSize, bucketSizes[i]));
      accumBucketSize += bucketSizes[i];
    } else {
      bucketInfos.push_back(BucketInfo(CL_UINT_MAX, 0));
    }
  }

  // store bucket data
  buckets.resize(entries);
  for (std::size_t i(0), end(subjects.size()); i != end; ++i) {
    std::size_t hash(hashPair(subjects[i], objects[i]) & (globalSize - 1));
    BucketInfo& info(bucketInfos[hash]);
    cl_uint bucketIndex = info.start + info.free++;
    buckets[bucketIndex] = BucketEntry(subjects[i], objects[i]);
  }
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeJoin2(Store::KeyVector& entailedObjects,
                                  Store::KeyVector& entailedSubjects,
                                  const Store::KeyVector& objectSource,
                                  const Store::KeyVector& subjectSource,
                                  const Store::KeyVector& schemaSubjects,
                                  const std::vector<std::pair<cl_uint, cl_uint>>& schemaSuccessorInfo,
                                  const Store::KeyVector& schemaSuccessors) {
  deviceTime_.start();

  cl::Kernel inheritanceKernel(*program(), "count_results");
  std::size_t globalSize = subjectSource.size();
  // std::size_t globalSize = ((((objectSource.size() - 1) >> 8) + 1) << 8);

  /* input elements to match */
  cl::Buffer inputBuffer;
  createBuffer(inputBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, objectSource);
  inheritanceKernel.setArg(0, inputBuffer);

  /* output with matching elements or 0 otherwise */
  std::vector<std::pair<cl_uint, cl_uint>> resultInfo(globalSize);
  cl::Buffer outputBuffer;
  createBuffer(outputBuffer, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, resultInfo);
  inheritanceKernel.setArg(1, outputBuffer);

  /* schema elements to be matched against */
  cl::Buffer schemaSubjectBuffer;
  createBuffer(schemaSubjectBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, schemaSubjects);
  inheritanceKernel.setArg(2, schemaSubjectBuffer);

  cl::Buffer schemaSuccessorInfoBuffer;
  createBuffer(schemaSuccessorInfoBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, schemaSuccessorInfo);
  inheritanceKernel.setArg(3, schemaSuccessorInfoBuffer);

  inheritanceKernel.setArg<cl_uint>(4, static_cast<cl_uint>(schemaSubjects.size()));

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
                            resultInfo.size() * sizeof(term_id),
                            resultInfo.data());

  deviceTime_.stop();

  // TODO: perform exclusive scan on device
  std::size_t accumResultSize(0);
  for (auto& value : resultInfo) {
    cl_uint tmp = value.second;
    value.second = accumResultSize;
    accumResultSize += tmp;
  }

  BucketVector buckets;
  BucketInfoVector bucketInfos;
  cl_uint size;
  hostTime_.start();
  buildHash(bucketInfos, buckets, subjectSource, objectSource, size);
  hostTime_.stop();

  deviceTime_.start();

  cl::Kernel matKernel(*program(), "materialize_results");

  /* output with entailed objects */
  cl::Buffer objectOutputBuffer;
  entailedObjects.resize(accumResultSize, 0);
  createBuffer(objectOutputBuffer, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, entailedObjects);
  matKernel.setArg(0, objectOutputBuffer);

  /* output with subjects for entailed triples */
  cl::Buffer subjectOutputBuffer;
  entailedSubjects.resize(accumResultSize, 0);
  createBuffer(subjectOutputBuffer, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, entailedSubjects);
  matKernel.setArg(1, subjectOutputBuffer);

  /* result from above from above */
  cl::Buffer previousResultsBuffer;
  createBuffer(previousResultsBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, resultInfo);
  matKernel.setArg(2, previousResultsBuffer);

  /* same as above */
  matKernel.setArg(3, schemaSuccessorInfoBuffer);

  /* schema successors */
  cl::Buffer schemaSuccessorBuffer;
  createBuffer(schemaSuccessorBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, schemaSuccessors);
  matKernel.setArg(4, schemaSuccessorBuffer);

  /* input subjects */
  cl::Buffer subjectBuffer;
  createBuffer(subjectBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, subjectSource);
  matKernel.setArg(5, subjectBuffer);

  /* bucket info */
  cl::Buffer bucketInfoBuffer;
  createBuffer(bucketInfoBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bucketInfos);
  matKernel.setArg(6, bucketInfoBuffer);

  /* buckets */
  cl::Buffer bucketBuffer;
  createBuffer(bucketBuffer, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, buckets);
  matKernel.setArg(7, bucketBuffer);

  /* hash table size */
  matKernel.setArg<cl_uint>(8, static_cast<cl_uint>(size));

  /* enqueue */
  queue_->enqueueNDRangeKernel(matKernel,
                               cl::NullRange,
                               cl::NDRange(globalSize),
                               cl::NullRange,
                               NULL,
                               NULL);

  /* read objects */
  queue_->enqueueReadBuffer(objectOutputBuffer,
                            CL_FALSE,
                            0,
                            entailedObjects.size() * sizeof(Dictionary::KeyType),
                            entailedObjects.data());
  /* read subjects */
  queue_->enqueueReadBuffer(subjectOutputBuffer,
                            CL_TRUE,
                            0,
                            entailedSubjects.size() * sizeof(Dictionary::KeyType),
                            entailedSubjects.data());

  deviceTime_.stop();

  /* block until done */
  queue_->finish();
}

////////////////////////////////////////////////////////////////////////////////

void OpenCLReasoner::computeJoin(Store::KeyVector& target,
                                 const Store::KeyVector& source,
                                 Store::KeyVector& match) {
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
                                              const Dictionary::KeyType property) {
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

cl::Program* OpenCLReasoner::program() {
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

std::string OpenCLReasoner::loadSource(const std::string& filename) {
  std::ifstream fin(filename, std::ios_base::in);
  if (!fin.is_open()) {
    throw Error("could not read kernel source file.");
  }

  std::istreambuf_iterator<char> fbegin(fin);
  std::istreambuf_iterator<char> eos;
  std::string source(fbegin, eos);
  return source;
}
