//
//  Reasoner.h
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef Reasoner_h
#define Reasoner_h

#include <vector>
#include <unordered_set>

#include "types.h"
#include "Store.h"
#include "Dictionary.h"

typedef std::vector<term_id> TermVector;
typedef std::vector<so_pair> PairVector;
typedef std::vector<triple> TripleVector;
typedef std::unordered_set<term_id> TermSet;

class Reasoner {
public:
  Reasoner(Dictionary& dict);
  virtual ~Reasoner() {}
  void addTriple(triple);
  virtual void computeClosure() = 0;
protected:
  term_id subClassOf_, subPropertyOf_, domain_, range_, type_;  // term identifiers for schema vocabulary
  Dictionary& dict_;  // URI -> term identifier dictionary
  
  PairVector scTriples_;      // rdfs:subClassOf
  PairVector spTriples_;      // rdfs:subPropertyOf
  PairVector domTriples_;     // rdfs:domain
  PairVector rngTriples_;     // rdfs:range
  TripleVector typeTriples_;  // rdf:type
  TripleVector triples_;      // instance + rdf:type triples
  
  TermSet scTerms_; // unique rdfs:subClassOf terms
  TermSet spTerms_; // unique rdfs:subPropertyOf terms
  
  inline bool isSchemaProperty(term_id property) const { return (property <= range_); }
};

#endif
