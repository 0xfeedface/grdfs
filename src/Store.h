//
//  Store.h
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef Store_h
#define Store_h

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "types.h"

class Store {
public:
  typedef std::vector<term_id> TermVector;
  typedef std::unordered_set<term_id> TermSet;
  typedef std::unordered_map<term_id, TermSet> TermMap;
  typedef std::vector<triple> TripleVector;

  bool addTriple(triple);
  void printStatistics();
  void copySubjects(TermVector&);
  void copyPredicates(TermVector&);
  void copyObjects(TermVector&);
  void copyTriples(TripleVector& triples);
  
  std::size_t size() { return tripleCount_; }

private:
  typedef std::unordered_map<term_id, TermSet> PredicateMap;
  typedef std::unordered_map<term_id, PredicateMap> SubjectMap;
  
  std::size_t tripleCount_ = 0;
  SubjectMap storage_;
};

#endif
