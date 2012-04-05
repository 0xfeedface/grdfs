//
//  Store.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include "Store.h"

#include <iostream>

using std::cout;

bool Store::addTriple(triple t) {
  PredicateMap& predicates = storage_[t.subject];
  TermSet& objects = predicates[t.predicate];
  auto result = objects.insert(t.object).second;
  
  if (result) {
    ++tripleCount_;
  }
  
  return result;
}

void Store::printStatistics() {
  cout << "Distinct subjects: " << storage_.size() << "\n";
}

void Store::copySubjects(TermVector& subjects) {
  for (auto value : storage_) {
    subjects.push_back(value.first);
  }
}

void Store::copyPredicates(TermVector& predicates) {
  auto subjectIter(std::begin(storage_));
  for (; subjectIter != std::end(storage_); ++subjectIter) {
    PredicateMap preds(subjectIter->second);
    
    auto predicateIter(std::begin(preds));
    for (; predicateIter != std::end(preds); ++predicateIter) {
      term_id p(predicateIter->first);
      TermSet objects(predicateIter->second);
      // Insert p as many times as there are objects for it
      predicates.insert(predicates.cend(), objects.size(), p);
    }
  }
}

void Store::copyObjects(TermVector& objectsTarget) {
  auto subjectIter(std::begin(storage_));
  for (; subjectIter != std::end(storage_); ++subjectIter) {
    PredicateMap predicates(subjectIter->second);
    
    auto predicateIter(std::begin(predicates));
    for (; predicateIter != std::end(predicates); ++predicateIter) {
      TermSet objects(predicateIter->second);
      
      auto objectIter(std::begin(objects));
      for (; objectIter != std::end(objects); ++objectIter) {
        term_id o(*objectIter);
        
        objectsTarget.push_back(o);
      }
    }
  }
}

void Store::copyTriples(TripleVector& triples) {
  auto subjectIter(std::begin(storage_));
  for (; subjectIter != std::end(storage_); ++subjectIter) {
    term_id s(subjectIter->first);
    PredicateMap predicates(subjectIter->second);
    
    auto predicateIter(std::begin(predicates));
    for (; predicateIter != std::end(predicates); ++predicateIter) {
      term_id p(predicateIter->first);
      TermSet objects(predicateIter->second);
      
      auto objectIter(std::begin(objects));
      for (; objectIter != std::end(objects); ++objectIter) {
        term_id o(*objectIter);
        
        triples.push_back(triple(s, p, o));
      }
    }
  }
}
