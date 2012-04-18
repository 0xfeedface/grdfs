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
#include "Timer.h"

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
  virtual void addTriple(const Store::Triple&);
  virtual void computeClosure() = 0;
  virtual void printStatistics();
  std::size_t inferredTriples() { return inferredTriplesCount_; }
  Store triples_;             // instance + rdf:type triples
  static const Dictionary::KeyType literalMask = (1UL << (sizeof(Dictionary::KeyType) * 8 - 1));
  typedef std::map<std::string, double> TimingMap;
  virtual TimingMap timings() = 0;
protected:
  typedef std::vector<so_pair> PairVector;
  typedef std::unordered_set<term_id> TermSet;
  typedef std::unordered_map<term_id, TermSet> TermMap;
  typedef Dictionary::KeyType KeyType;
  std::size_t inferredTriplesCount_ = 0;

  term_id subClassOf_, subPropertyOf_, domain_, range_, type_;  // term identifiers for schema vocabulary
  Dictionary& dict_;  // URI -> term identifier dictionary

  TermMap scSuccessors_;      // rdfs:subClassOf successor sets
  TermMap scPredecessors_;    // rdfs:subClassOf predecessor sets
  TermMap spSuccessors_;      // rdfs:subPropertyOf successor sets
  TermMap spPredecessors_;    // rdfs:subPropertyOf predecessor sets

  TermMap domTriples_;        // rdfs:domain
  TermMap rngTriples_;        // rdfs:range

  TermSet scTerms_; // unique rdfs:subClassOf terms
  TermSet spTerms_; // unique rdfs:subPropertyOf terms

  bool isSchemaProperty(term_id property) const { return (property <= range_); }
};

#endif
