//
//  Dictionary.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include <iostream>

#include "Dictionary.h"

static term_id kMSBMask = (1UL << (sizeof(term_id) * 8 - 1)); // get the most significant bit

term_id Dictionary::NextKey() {
  return nextKey_++;
}

term_id Dictionary::Lookup(const std::string& lit) {
  return Lookup(lit, false);
}

term_id Dictionary::Lookup(const std::string& lit, const bool lit_hint) {
  term_id key = map_[lit];
  if (!key) {
    key = NextKey();
    
    // For literal strings, the most significant bit will be 1
    if (lit_hint) {
      key |= kMSBMask;
    }

    auto res = map_.insert(std::make_pair(lit, key));
    keys_.insert(std::make_pair(key, res.first->first));
  } else {
    key = map_[lit];
  }
  
  return key;
}

void Dictionary::Add(const std::string& lit) {
  Lookup(lit);
}

void Dictionary::Add(const std::string& lit, const bool lit_hint) {
  Lookup(lit, lit_hint);
}

std::string Dictionary::Find(const term_id key) const {
  if (keys_.find(key) != keys_.end()) {
    return keys_.find(key)->second;
  }
  
  return nullptr;
}

int Dictionary::Size() const {
  return static_cast<int>(map_.size());
}

void Dictionary::Print() const {
  for (auto iter = map_.begin(); iter != map_.end(); iter++) {
    std::cout << iter->first << " => " << iter->second << std::endl;
  }
}

void Dictionary::PrintStatistics() const {
  std::cout << "Dictionary with " << map_.size() << " entries.\n";
}
