#include "Store.h"
#include <iostream>
#include <cassert>

#define ROT(val, shift) (((val) >> shift) | ((val) << (64 - shift)))

bool Store::Triple::operator==(const Triple& rhs) const
{
  return subject == rhs.subject && predicate == rhs.predicate && object == rhs.object;
}

bool Store::Triple::operator!=(const Triple& rhs) const
{
  return subject != rhs.subject || predicate != rhs.predicate || object != rhs.object;
}

// Hashes a triple.
// Subject, predicate and object are interpreted as one
// byte sequence which is hashed using Google's CityHash
// specialized for a length of 24 bytes.
// See: http://code.google.com/p/cityhash
std::size_t Store::Triple::hash() const
{
  assert(sizeof(std::size_t) == 8);

  uint64_t a = this->subject * k1;
  uint64_t b = this->predicate;
  uint64_t c = this->object * k2;
  uint64_t d = this->predicate * k0;

  d = ROT(a - b, 43) + ROT(c, 30) + d;
  b = a + ROT(b ^ k3, 20) - c + 24 /* len */;

  c = (d ^ b) * kMul;
  c ^= (c >> 47);
  b = (b ^ c) * kMul;
  b ^= (b >> 47);
  b *= kMul;

  return b;
}

Store::Iterator& Store::Iterator::operator++()
{
  if (entailedOnly_) {
    currentIndex_ = indexOfNextEntailedTriple(currentIndex_);
  } else {
    ++currentIndex_;
  }

  return *this;
}

Store::Iterator Store::Iterator::operator++(int)
{
  Iterator old(*this);
  ++(*this);
  return old;
}

Store::Triple Store::Iterator::operator*() const
{
  return Triple(
           store_.subjects()[currentIndex_],
           store_.predicates()[currentIndex_],
           store_.objects()[currentIndex_]);
}

bool Store::addTriple(const Triple& t, TripleFlags flags)
{
  std::size_t hash(t.hash());
  auto it(indices_.find(hash));
  if (it != std::end(indices_)) {
    std::size_t index = it->second;
    if (t != Triple(subjects_[index], predicates_[index], objects_[index])) {
      std::cout << "crap, we have a collision!\n";
    }
    return false;
  }

  indices_.insert(std::make_pair(hash, subjects_.size()));
  subjects_.push_back(t.subject);
  predicates_.push_back(t.predicate);
  objects_.push_back(t.object);
  flags_.push_back(flags);
  ++size_;

  return true;
}

