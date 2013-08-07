//
//  NativeReasoner.h
//  grdfs
//
//  Created by Norman Heino on 11-12-11.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef NativeReasoner_h
#define NativeReasoner_h

#include "Dictionary.hh"
#include "Reasoner.hh"
#include "Store.hh"

class NativeReasoner : public Reasoner
{
public:
  NativeReasoner(Dictionary& dict, RuleSet ruleSet) : Reasoner(dict, ruleSet) {}
  void computeClosure();
  void computeClosure_InverseAdjacency(const TermMap&, const TermMap&);
  void computeClosure_InverseTopological(TermMap&, const TermMap&);
  void printClosure(const TermMap&, bool);
};

#endif
