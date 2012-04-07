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
#include <vector>
#include <unordered_map>

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
  static const term_id literalMask = (1UL << (sizeof(term_id) * 8 - 1));
private:
  typedef std::unordered_map<std::string, term_id> LiteralMap;
  typedef std::unordered_map<term_id, std::string> KeyMap;

  term_id nextKey_;

  LiteralMap map_;
  KeyMap keys_;
};

#endif
