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

typedef std::unordered_map<term_id, TermSet> TermMap;

class NativeReasoner : public Reasoner {
public:
  NativeReasoner(Dictionary* dict) : Reasoner(dict) {}
  void computeClosure();
  void addTriple(triple);
  void computeClosure_Boost();
  void computeClosure_InverseAdjacency(const TermMap&, const TermMap&);
protected:
  TermMap scPairs_;
  TermMap scPairsInverse_;
  TermMap spPairs_;
  TermMap spPairsInverse_;
};

#endif
