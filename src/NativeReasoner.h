//
//  NativeReasoner.h
//  grdfs
//
//  Created by Norman Heino on 11-12-11.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef NativeReasoner_h
#define NativeReasoner_h

#include "Dictionary.h"

#include "Reasoner.h"

class NativeReasoner : public Reasoner {
public:
  NativeReasoner(Dictionary& dict) : Reasoner(dict) {}
  void computeClosure();
};

#endif
