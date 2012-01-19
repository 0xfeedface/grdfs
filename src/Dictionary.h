//
//  Dictionary.h
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef Dictionary_h
#define Dictionary_h

#include "types.h"

#include <string>
#include <unordered_map>
#include <vector>

typedef std::unordered_map<std::string, term_id> literal_map_t;
//typedef std::unordered_map<term_id, std::string> key_map_t;
typedef std::vector<const std::string*> key_map_t;

class Dictionary {
public:
  Dictionary() : nextKey_(1) {}
  term_id NextKey();
  term_id Lookup(const std::string& lit);
  term_id Lookup(const std::string& lit, const bool lit_hint);
  void Add(const std::string& lit);
  void Add(const std::string& lit, const bool lit_hint);
  int Size() const;
  void Print() const;
  void PrintStatistics() const;
  std::string Find(const term_id key) const;
private:
  term_id nextKey_;
  literal_map_t map_;
  key_map_t keys_;
};

#endif
