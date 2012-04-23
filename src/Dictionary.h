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
  // The type we return as keys (i.e. literal IDs).
  typedef std::size_t KeyType;
  // Functor to give clients a chance to modify the key.
  // Only the most significant byte is allowed to be modified.
  typedef std::function<void (KeyType&)> KeyModifier;
  // default constructor, uses anonymous backing file
  Dictionary();
  // fileName constructor, tries to create file with requested name
  Dictionary(const std::string& fileName);
  // destructir
  ~Dictionary();
  // lookup lit and return its ID
  KeyType Lookup(const std::string& lit);
  // Lookup lit and return its ID with modifier.
  // Modifier is only applied to newly generated keys.
  KeyType Lookup(const std::string& lit, const KeyModifier& keyModifier);
  // Find a literal for key and return its value as an std::string
  std::string Find(KeyType key) const;
  // returns the current size (number of entries)
  std::size_t Size() const { return nextKey_; }
private:
  // std::vector of keys
  typedef std::vector<KeyType> KeyVector;
  // C++11 specialization of std::hash for std::string
  typedef std::hash<std::string> StringHasher;
  // hash-based map for mapping keys to hash values
  typedef std::unordered_map<KeyType, std::size_t> KeyMap;

  // Header of a dictionary entry,
  // The actual entry is made of the header
  // plus character data.
  struct EntryHeader;

  // maps string hashes to ids
  KeyMap ids_;
  // stores entry offsets at index (key - 1)
  KeyVector literals_;
  // current size of the mapping in bytes
  std::size_t size_;
  // size of the mapping in pages
  std::size_t numPages_;
  // first key issued
  KeyType nextKey_ = 1;
  // file name for non-anonymous backing file
  std::string fileName_;
  // hash function used (std::hash<std::string>)
  StringHasher hasher_;
  // file descriptor of backing file
  int fd_;
  // root pointer of the mapping
  char* map_ = nullptr;
  // pointer to the beginning of the next entry
  char* pos_ = nullptr;

  // creates a mapping of size pages
  void map(std::size_t size);
  // destroys the current mapping
  void unmap();

  // template method to write a value of type T
  template <typename T>
  void writeValue(const KeyType offset, const T value) { ::memcpy(map_ + offset, &value, sizeof(T)); }

  // template method to read a value of type T
  template <typename T>
  T readValue(const KeyType offset) { T value; ::memcpy(&value, map_ + offset, sizeof(T)); return value; }

  // writes the entryHeader at offset
  std::size_t writeHeader(const EntryHeader& entryHeader, std::size_t offset);
  // writes the bytes in lit at offset
  std::size_t writeLiteral(const std::string& lit, std::size_t offset);

  // reads an entry from offset
  inline EntryHeader readEntry(const KeyType offset);
  // reads character data from offset and returns a new std::string
  inline std::string readLiteral(const KeyType offset, std::size_t length);

  // writes an entry ad offset, creating a new key if necessary
  KeyType writeEntry(const std::string& lit, std::ptrdiff_t offset, const KeyModifier& keyModifier);

  // not used
  std::size_t hash(const std::string& str);
};

#endif
