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
#include <set>
#include <map>

#include "types.h"
#include "Store.hh"
#include "Dictionary.hh"
#include "Timer.hh"

// compare pairs by components
bool operator<(const so_pair& p1, const so_pair& p2);

class Reasoner
{
public:
  enum RuleSet {
    kRhoDFRuleSet = 0,
    kRDFSRuleSet  = 1 << 0
  };
  class Error
  {
  public:
    Error(const std::string& message) : message_(message) {}
    explicit Error(const char* message) : message_(message) {}
    std::string& message() {
      return message_;
    }
  private:
    std::string message_;
  };

  Reasoner(Dictionary& dict, RuleSet ruleSet);
  virtual ~Reasoner() {}
  void addAxiomaticTriples();
  virtual bool addTriple(const Store::Triple&, const Store::TripleFlags f = Store::kFlagsNone);
  virtual void computeClosure() = 0;
  virtual void printStatistics();
  std::size_t inferredTriples() {
    return inferredTriplesCount_;
  }
  std::size_t inferredDuplicates() {
    return inferredDuplicatesCount_;
  }
  static const Dictionary::KeyType literalMask = (1UL << (sizeof(Dictionary::KeyType) * 8 - 1));
  typedef std::map<std::string, double> TimingMap;
  virtual TimingMap timings() = 0;

  // TODO: should be private
  Store triples_;       // instance triples
  Store typeTriples_;   // rdf:type triples
  Store schemaTriples_; // schema triples
protected:
  typedef std::set<term_id> TermSet;
  typedef std::map<term_id, TermSet> TermMap;
  typedef Dictionary::KeyType KeyType;
  typedef std::vector<std::pair<Dictionary::KeyType, Dictionary::KeyType>> PairVector;
  std::size_t inferredTriplesCount_ = 0;
  std::size_t inferredDuplicatesCount_ = 0;

  term_id subClassOf_, subPropertyOf_, domain_, range_, type_;  // term identifiers for schema vocabulary
  Dictionary& dict_;  // URI -> term identifier dictionary

  TermSet membershipProperties_;  // container membership propertes used

  RuleSet ruleSet_;

  TermMap scSuccessors_;      // rdfs:subClassOf successor sets
  TermMap scPredecessors_;    // rdfs:subClassOf predecessor sets
  TermMap spSuccessors_;      // rdfs:subPropertyOf successor sets
  TermMap spPredecessors_;    // rdfs:subPropertyOf predecessor sets

  TermMap domTriples_;        // rdfs:domain
  TermMap rngTriples_;        // rdfs:range

  TermSet scTerms_; // unique rdfs:subClassOf terms
  TermSet spTerms_; // unique rdfs:subPropertyOf terms

  Timer storeTimer_;
  Timer uniqueingTimer_;
  Timer tempTimer_;

  bool isSchemaProperty(term_id property) const {
    return (property <= range_);
  }
};

#endif
