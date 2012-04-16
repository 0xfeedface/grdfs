#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <cstddef> // size_t
#include <cstring> // memcpy
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

class Dictionary {
public:
  typedef std::size_t KeyType;
  typedef std::function<void (KeyType&)> KeyModifier;
  // default constructor, uses anonymous backing file
  Dictionary();
  // fileName constructor, tries to create file with requested name
  Dictionary(const std::string& fileName);
  ~Dictionary();
  KeyType Lookup(const std::string& lit);
  KeyType Lookup(const std::string& lit, const KeyModifier& keyModifier);
  std::string Find(KeyType key) const;
  std::size_t Size() const { return 0; }
private:
  typedef std::vector<KeyType> KeyVector;
  typedef std::hash<std::string> StringHasher;
  typedef std::unordered_map<KeyType, std::size_t> KeyMap;

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
  void writeValue(const KeyType offset, const T value) { ::memcpy(map_ + offset, &value, sizeof(T)); }

  template <typename T>
  T readValue(const KeyType offset) { T value; ::memcpy(&value, map_ + offset, sizeof(T)); return value; }

  std::size_t writeHeader(const Entry& entry, std::size_t offset);
  std::size_t writeLiteral(const std::string& lit, std::size_t offset);
  inline Entry readEntry(const KeyType offset);
  inline std::string readLiteral(const KeyType offset, std::size_t length);

  KeyType writeEntry(const std::string& lit, std::ptrdiff_t offset, const KeyModifier& keyModifier);

  std::size_t hash(const std::string& str);
};

#endif
