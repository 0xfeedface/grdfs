//
//  Reasoner.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include <iostream>

#include "Reasoner.h"

Reasoner::Reasoner(Dictionary* dict) : dict_(dict) {
  subClassOf_    = dict->Lookup(kSubClassOfURI);
  subPropertyOf_ = dict->Lookup(kSubPropertyOfURI);
  domain_        = dict->Lookup(kDomainURI);
  range_         = dict->Lookup(kRangeURI);
  type_          = dict->Lookup(kTypeURI);
}

void Reasoner::addTriple(triple t) {
  if (isSchemaProperty(t.predicate)) {
    if (t.predicate == subClassOf_) {
      scTriples_.push_back(so_pair(t.subject, t.object));
      scTerms_.insert(t.subject);
      scTerms_.insert(t.object);
    } else if (t.predicate == subPropertyOf_) {
      spTriples_.push_back(so_pair(t.subject, t.object));
      spTerms_.insert(t.subject);
      spTerms_.insert(t.object);
    } else if (t.predicate == domain_) {
      domTriples_.push_back(so_pair(t.subject, t.object));
    } else if (t.predicate == range_) {
      rngTriples_.push_back(so_pair(t.subject, t.object));
    }
  } else {
    // additionally store separate rdf:type triples
    if (t.predicate == type_) {
      typeTriples_.push_back(t);
    }
    // instance triple
    triples_.push_back(t);
  }
}

void Reasoner::computeClosure() {
  std::cout << "triples: " << triples_.size() << std::endl;
  std::cout << "sc triples: " << scTriples_.size() << ", sc terms: " << scTerms_.size() << std::endl;
  std::cout << "sp triples: " << spTriples_.size() << std::endl;
  std::cout << "dom triples: " << domTriples_.size() << std::endl;
  std::cout << "range triples: " << rngTriples_.size() << std::endl;
  std::cout << "dictionary size: " << dict_->Size() << std::endl;
  std::cout << std::endl;
}