#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <cstddef> // size_t
#include <string>
#include <unordered_map>
#include <vector>

class Dictionary {
public:
  typedef std::size_t KeyType;
  Dictionary(const std::string& fileName);
  ~Dictionary();
  KeyType Lookup(std::string const& key);
  std::string Find(KeyType key) const;
  std::size_t Size() const { return 0; }
  static const KeyType literalMask = (1UL << (sizeof(KeyType) * 8 - 1));
private:
  typedef std::vector<KeyType> KeyVector;
  typedef std::hash<std::string> StringHasher;
  typedef std::unordered_map<KeyType, std::size_t> KeyMap;
  // id, overflow, literal
  typedef std::tuple<KeyType, KeyType, std::string> ValueType;

  struct Entry {
    KeyType id;
    KeyType next;
    std::size_t lsize;
    Entry() : id(0), next(0), lsize(0) {};
    Entry(KeyType id_, KeyType next_, std::size_t lsize_) : id(id_), next(next_), lsize(lsize_) {};
  };

  KeyMap ids_;
  KeyVector literals_;
  std::size_t size_;
  std::size_t numPages_;
  KeyType nextKey_ = 1;
  std::string fileName_;
  StringHasher hasher_;
  int fd_;
  char* map_ = nullptr;
  char* pos_ = nullptr;

  void map(std::size_t size);
  void unmap();

  template <typename T>
  void writeValue(KeyType const offset, T const value) { ::memcpy(map_ + offset, &value, sizeof(T)); }

  template <typename T>
  T readValue(KeyType const offset) { T value; ::memcpy(&value, map_ + offset, sizeof(T)); return value; }

  std::size_t writeHeader(Entry const& entry, std::size_t offset);
  std::size_t writeLiteral(std::string const& lit, std::size_t offset);
  Entry readEntry(KeyType const offset);
  std::string readLiteral(KeyType const offset, std::size_t length);

  KeyType writeEntry(std::string const& lit, std::ptrdiff_t offset);

  std::size_t hash(std::string const& str);
};

#endif
