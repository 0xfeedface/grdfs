#ifndef STORE_H
#define STORE_H

#include "Dictionary.h"
#include <cstddef> // size_t
#include <vector>
#include <unordered_map>
#include <utility>
#include <string>
#include <memory>
#include <boost/functional/hash.hpp>

class Store {
public:
  enum TripleFlags {
    kFlagsNone     = 0,
    kFlagsEntailed = 1 << 0
  };

  // typedef Dictionary::KeyType KeyType;

  struct Triple {
    Dictionary::KeyType subject;
    Dictionary::KeyType predicate;
    Dictionary::KeyType object;
    Triple() : subject(0), predicate(0), object(0) {};
    Triple(Dictionary::KeyType s, Dictionary::KeyType p, Dictionary::KeyType o)
      : subject(s), predicate(p), object(o) {};
    std::size_t hash() const;
    bool operator==(const Triple& rhs) const;
    bool operator!=(const Triple& rhs) const;
  private:
    static const uint64_t k0   = 0xc3a5c85c97cb3127ULL;
    static const uint64_t k1   = 0xb492b66fbe98f273ULL;
    static const uint64_t k2   = 0x9ae16a3b2f90404fULL;
    static const uint64_t k3   = 0xc949d7c7509e6557ULL;
    static const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  }; // Store::Triple

  struct Iterator {
    Iterator(Store& s, std::size_t beginIndex) : store_(s), currentIndex_(beginIndex) {};
    Iterator(Store& s, std::size_t beginIndex, bool entailedOnly) :
        store_(s), currentIndex_(beginIndex), entailedOnly_(entailedOnly) {};
    Iterator& operator++();
    Iterator operator++(int);
    bool operator==(const Iterator& rhs) const { return (currentIndex_ == rhs.currentIndex_); };
    bool operator!=(const Iterator& rhs) const { return (currentIndex_ != rhs.currentIndex_); };
    Triple operator*() const;
    std::shared_ptr<Triple> operator->() const { return std::make_shared<Triple>(**this); };

  private:
    Store& store_;
    std::size_t currentIndex_ = 0;
    bool entailedOnly_ = false;
  }; // Store::Iterator

  friend struct Iterator;

  bool addTriple(const Triple& t, TripleFlags flags = kFlagsNone);

  Iterator begin() { return Iterator(*this, 0); };
  Iterator end() { return Iterator(*this, size_); };

  Iterator ebegin() { return Iterator(*this, 0, true); };
  Iterator eend() { return Iterator(*this, size_, true); };
  
  std::size_t size() { return size_; }

  typedef std::vector<Dictionary::KeyType> KeyVector;

  // Return the underlying vectors as const references
  const KeyVector& subjects() const { return subjects_; };
  const KeyVector& predicates() const { return predicates_; };
  const KeyVector& objects() const { return objects_; };
  const std::vector<TripleFlags>& flags() const { return flags_; };

private:
  KeyVector subjects_;
  KeyVector predicates_;
  KeyVector objects_;
  std::vector<TripleFlags> flags_;
  std::unordered_map<std::size_t, std::size_t> indices_;
  std::size_t size_ = 0;
};

#endif
