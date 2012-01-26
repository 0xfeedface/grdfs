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

#define __CL_ENABLE_EXCEPTIONS
#if defined(__APPLE__) || defined(__MACOSX)
#include "cl.hpp"
#else
#include <CL/cl.hpp>
#endif

#include <map>
#include <string>

class OpenCLReasoner : public Reasoner {
public:
  OpenCLReasoner(Dictionary* dict);
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
};

#endif
