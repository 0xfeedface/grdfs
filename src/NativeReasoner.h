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
#include "Store.h"

class NativeReasoner : public Reasoner
{
public:
  NativeReasoner(Dictionary& dict) : Reasoner(dict) {}
  void computeClosure();
  void computeClosure_Boost();
  void computeClosure_InverseAdjacency(const TermMap&, const TermMap&);
  void computeClosure_InverseTopological(TermMap&, const TermMap&);
  void printClosure(const TermMap&, bool);
};

#endif
