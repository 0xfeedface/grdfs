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

bool Reasoner::addTriple(const Store::Triple& t, Store::TripleFlags flags) {
  bool result(false);
  if (isSchemaProperty(t.predicate)) {
    if (schemaTriples_.addTriple(t, flags)) {
      // triple is new, put it in the schema indexes
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
      result = true;
    }
  }

  // store separate non-schema triples
  if (t.predicate == type_) {
    // index for rdf:type triples
    result = typeTriples_.addTriple(t, flags);
  } else {
    // everything else
    result = triples_.addTriple(t, flags);
  }

  if (flags & Store::kFlagsEntailed) {
    if (result) {
      ++inferredTriplesCount_;
    } else {
      ++inferredDuplicatesCount_;
    }
  }

  return result;
}

void Reasoner::printStatistics() {
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
