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
  auto predicates = storage_[t.subject];
  auto objects = predicates[t.predicate];
  auto result = objects.insert(t.object);
  return result.second;
}

void Store::PrintStatistics() {
  cout << "Distinct subjects: " << storage_.size() << "\n";
}
