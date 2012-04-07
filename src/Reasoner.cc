//
//  Reasoner.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include <iostream>

#include "Reasoner.h"

Reasoner::Reasoner(Dictionary& dict) : dict_(dict) {
  subClassOf_    = dict.Lookup(kSubClassOfURI);
  subPropertyOf_ = dict.Lookup(kSubPropertyOfURI);
  domain_        = dict.Lookup(kDomainURI);
  range_         = dict.Lookup(kRangeURI);
  type_          = dict.Lookup(kTypeURI);
}

void Reasoner::addTriple(triple t) {
  if (isSchemaProperty(t.predicate)) {
    if (t.predicate == subClassOf_) {
      scSuccessors_[t.subject].insert(t.object);
      scPredecessors_[t.object].insert(t.subject);
      scTerms_.insert(t.subject);
      scTerms_.insert(t.object);
    } else if (t.predicate == subPropertyOf_) {
      spSuccessors_[t.subject].insert(t.object);
      spPredecessors_[t.object].insert(t.subject);
      spTerms_.insert(t.subject);
      spTerms_.insert(t.object);
    } else if (t.predicate == domain_) {
      domTriples_[t.subject].insert(t.object);
    } else if (t.predicate == range_) {
      rngTriples_[t.subject].insert(t.object);
    }
  } else {
    // additionally store separate rdf:type triples
    if (triples_.addTriple(t) && t.predicate == type_) {
      typeTriples_.push_back(t);
    }
  }
}

void Reasoner::copySubjects(Store::TermVector& subjects) {
  return triples_.copySubjects(subjects);
}

void Reasoner::copyPredicates(Store::TermVector& predicates) {
  return triples_.copyPredicates(predicates);
}

void Reasoner::copyObjects(Store::TermVector& objects) {
  return triples_.copyObjects(objects);
}

void Reasoner::copyTriples(Store::TripleVector& triples) {
  return triples_.copyTriples(triples);
}

void Reasoner::printStatistics() {
  triples_.printStatistics();
  std::cout << "sc terms: " << scSuccessors_.size() << std::endl;
  std::cout << "sp terms: " << spSuccessors_.size() << std::endl;
  std::cout << "dom triples: " << domTriples_.size() << std::endl;
  std::cout << "range triples: " << rngTriples_.size() << std::endl;
  std::cout << "dictionary size: " << dict_.Size() << std::endl;
  std::cout << std::endl;
}

bool operator<(const so_pair& p1, const so_pair& p2) {
  if (p1.subject > p2.subject || p1.object > p2.object) {
    return false;
  }
  
  return true;
}
