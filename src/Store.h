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
#include "types.h"

class Store {
public:
  bool addTriple(triple);
  void PrintStatistics();
private:
  typedef std::unordered_set<term_id> TermSet;
  typedef std::unordered_map<term_id, TermSet> PredicateMap;
  typedef std::unordered_map<term_id, PredicateMap> SubjectMap;
  SubjectMap storage_;
};

#endif
