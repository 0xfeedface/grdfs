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
#include <unordered_map>

#include "types.h"
#include "Store.h"
#include "Dictionary.h"

// compare pairs by components
bool operator<(const so_pair& p1, const so_pair& p2);

class Reasoner {
public:
  class Error {
  public:
    Error(const std::string& message) : message_(message) {}
    explicit Error(const char* message) : message_(message) {}
    std::string& message() { return message_; }
  private:
    std::string message_;
  };

  Reasoner(Dictionary& dict);
  virtual ~Reasoner() {}
  virtual void addTriple(triple);
  virtual void computeClosure() = 0;
  virtual void printStatistics();
  virtual void copyPredicates(Store::TermVector&);
  virtual void copySubjects(Store::TermVector&);
  virtual void copyObjects(Store::TermVector&);
  virtual void copyTriples(Store::TripleVector&);
  std::size_t inferredTriples() { return inferredTriplesCount_; }
protected:
  typedef std::vector<so_pair> PairVector;
  std::size_t inferredTriplesCount_ = 0;

  term_id subClassOf_, subPropertyOf_, domain_, range_, type_;  // term identifiers for schema vocabulary
  Dictionary& dict_;  // URI -> term identifier dictionary

  Store::TermMap scSuccessors_;      // rdfs:subClassOf successor sets
  Store::TermMap scPredecessors_;    // rdfs:subClassOf predecessor sets
  Store::TermMap spSuccessors_;      // rdfs:subPropertyOf successor sets
  Store::TermMap spPredecessors_;    // rdfs:subPropertyOf predecessor sets

  PairVector domTriples_;     // rdfs:domain
  PairVector rngTriples_;     // rdfs:range
  Store::TripleVector typeTriples_;  // rdf:type
  Store triples_;             // instance + rdf:type triples

  Store::TermSet scTerms_; // unique rdfs:subClassOf terms
  Store::TermSet spTerms_; // unique rdfs:subPropertyOf terms

  bool isSchemaProperty(term_id property) const { return (property <= range_); }
};

#endif
