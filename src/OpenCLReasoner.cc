//
//  OpenCLReasoner.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-11.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include "OpenCLReasoner.h"

#include <vector>
#include <map>
#include <iostream>
#include <sys/stat.h>
#include <cassert>

OpenCLReasoner::OpenCLReasoner(Dictionary& dict) : Reasoner(dict) {
  context_ = context();
  // query devices
  std::vector<cl::Device> devices = context_->getInfo<CL_CONTEXT_DEVICES>();
  device_ = &devices[0];
  queue_ = commandQueue();
}

OpenCLReasoner::~OpenCLReasoner() {
  delete context_;
  delete queue_;
  
  // program_ is created lazily, so at this point we don't know 
  // whether it has been created or not
  if (program_ != nullptr) {
    delete program_;
  }
}

void OpenCLReasoner::computeClosure() {
  TermVector::size_type scNodeNumber(scTerms_.size());
  if (scNodeNumber) {
    unsigned long long scClosureSize((unsigned long long)scNodeNumber * (unsigned long long)scNodeNumber);
    
    term_id* indexedTerms = new (std::nothrow) term_id[scNodeNumber];
    assert(nullptr != indexedTerms);
    std::map<term_id, size_t> termsIndexes;
    // copy from the set to the vector so we get indices
    std::copy(std::begin(scTerms_), std::end(scTerms_), indexedTerms);
    
    
    for (unsigned i(0); i != scNodeNumber; ++i) {
      term_id val(indexedTerms[i]);
      termsIndexes[val] = i;
    }
    
    cl_uint* inputClosure = new (std::nothrow) cl_uint[scClosureSize];
    assert(nullptr != inputClosure);
    // create boost graph
    auto it(std::begin(scSuccessors_));
    for (; it != std::end(scSuccessors_); ++it) {
      auto sit(std::begin(it->second));
      if (sit != std::end(it->second)) {
        size_t sidx = termsIndexes[it->first];
        for (; sit != std::end(it->second); ++sit) {
          size_t oidx = termsIndexes[*sit];
          // add to boost graph
          inputClosure[sidx * scNodeNumber + oidx] = 1;
        }
      }
    }
    
    // load program
    program_ = program("src/grdfs_kernels.cl");
    cl::Kernel scKernel(*program_, "transitivity");
    try {
      cl::Buffer inputOutputBuffer(*context_, 
                                   CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, 
                                   scClosureSize * sizeof(cl_uint), 
                                   inputClosure);
      
      scKernel.setArg(0, inputOutputBuffer);
      scKernel.setArg(1, scNodeNumber);
      queue_->flush();
      
      for (unsigned i(0); i < scNodeNumber; ++i) {
        // set the pass kernel argument
        scKernel.setArg(2, i);
        queue_->enqueueNDRangeKernel(scKernel, cl::NullRange, cl::NDRange(scClosureSize), cl::NDRange(scNodeNumber), NULL, NULL);
      }
      
      queue_->flush();
      
      queue_->enqueueReadBuffer(inputOutputBuffer, CL_TRUE, 0, scClosureSize * sizeof(cl_uint), &inputClosure[0]);
    } catch (cl::Error e) {
      std::cout << "Error in " << e.what() << " (" << e.err() << ")" << std::endl;
    }
    
    scSuccessors_.clear();
    for (size_t row(0); row < scNodeNumber; ++row) {
      for (size_t col(0); col < scNodeNumber; ++col) {
        if (inputClosure[row * scNodeNumber + col] == 1) {
          term_id subj = indexedTerms[row];
          term_id obj  = indexedTerms[col];
          scSuccessors_[subj].insert(obj);
        }
      }
    }
    
    delete [] inputClosure;
    delete [] indexedTerms;
  }
}

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

cl::CommandQueue* OpenCLReasoner::commandQueue(bool enableProfiling) {
  // create command queue
  cl::CommandQueue* queue = new cl::CommandQueue(*context_, *device_, enableProfiling ? CL_QUEUE_PROFILING_ENABLE : 0);
  return queue;
}

cl::Program* OpenCLReasoner::program(const std::string& source) {
  cl::Program::Sources sources(1, std::make_pair(source.c_str(), 0));
  cl::Program* program = new cl::Program(*context_, sources);
  
  try {
    program->build(context_->getInfo<CL_CONTEXT_DEVICES>());
  } catch (cl::Error err) {
    std::cout << program_->getBuildInfo<CL_PROGRAM_BUILD_LOG>(*device_) << std::endl;
    throw err;
  }
  
  return program;
}

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
