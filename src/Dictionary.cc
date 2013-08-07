#include "Dictionary.hh"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdexcept>
#include <utility>

#define PAGE_SIZE 4096
#define LOG2_PAGE_SIZE 12
#define INIT_NUM_PAGES 32
#define GOLDEN_RATIO 1.618f

#define MAX(x, y) (x > y ? x : y)

////////////////////////////////////////////////////////////////////////////////

struct Dictionary::EntryHeader {
  KeyType id;
  KeyType next;
  std::size_t lsize;
  EntryHeader() : id(0), next(0), lsize(0) {};
  EntryHeader(KeyType id_, KeyType next_, std::size_t lsize_)
    : id(id_), next(next_), lsize(lsize_) {};
};

////////////////////////////////////////////////////////////////////////////////

Dictionary::Dictionary()
{
  int openResult = ::fileno(::tmpfile());
  if (openResult < 0) {
    throw std::invalid_argument("Could not create backing file.");
  }
  fd_ = openResult;
  map(INIT_NUM_PAGES);
}

////////////////////////////////////////////////////////////////////////////////

Dictionary::Dictionary(const std::string& fileName)
{
  int openResult = ::open(fileName.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (openResult < 0) {
    throw std::invalid_argument("File could not be opened or created.");
  }
  struct stat st;
  fd_ = openResult;
  fstat(fd_, &st);
  // TODO: if the file has not been created by us, it does not
  // necessarily have the size of a PAGE_SIZE multiple
  map(MAX(st.st_size / PAGE_SIZE, INIT_NUM_PAGES));

}

////////////////////////////////////////////////////////////////////////////////

Dictionary::~Dictionary()
{
  unmap();
  close(fd_);
}

////////////////////////////////////////////////////////////////////////////////

std::size_t Dictionary::writeHeader(const EntryHeader& entry, std::size_t offset)
{
  // prevent string from crossing page boundary,
  // moving it entirely to the next page
  if ((offset >> LOG2_PAGE_SIZE) < (offset + sizeof(EntryHeader) + entry.lsize) >> LOG2_PAGE_SIZE) {
    offset = PAGE_SIZE * ((offset >> LOG2_PAGE_SIZE) + 1);
    pos_ = map_ + offset;
  }

  // remap if necessary
  if ((offset + sizeof(EntryHeader) + entry.lsize) > size_) {
    unmap();
    map(floorf(GOLDEN_RATIO * numPages_));
    offset = pos_ - map_;
  }

  // write entry
  writeValue(offset, entry);

  // advance pointer
  pos_ += sizeof(EntryHeader);

  return offset;
}

////////////////////////////////////////////////////////////////////////////////

std::size_t Dictionary::writeLiteral(const std::string& lit, std::size_t offset)
{
  // write literal
  lit.copy(map_ + offset, lit.size());

  // advance pointer
  // TODO: ofsetting to dword boundary seems to be slower!
  // pos_ += (((offset + lit.size() - 1) >> 3) + 1) * 8 - offset;
  pos_ += lit.size();

  return offset;
}

////////////////////////////////////////////////////////////////////////////////

Dictionary::EntryHeader Dictionary::readEntry(const KeyType offset)
{
  return readValue<EntryHeader>(offset);
}

////////////////////////////////////////////////////////////////////////////////

std::string Dictionary::readLiteral(const KeyType offset, std::size_t length)
{
  return std::string(map_ + offset, length);
}

////////////////////////////////////////////////////////////////////////////////

std::size_t Dictionary::hash(const std::string& str)
{
  std::size_t hash(0);
  for (std::size_t i(0); i != str.size(); ++i) {
    hash = hash * 101 + str[i];
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////

Dictionary::KeyType Dictionary::Lookup(const std::string& lit)
{
  KeyModifier emptyModifier;
  return Lookup(lit, emptyModifier);
}

////////////////////////////////////////////////////////////////////////////////

Dictionary::KeyType Dictionary::Lookup(const std::string& lit, const KeyModifier& keyModifier)
{
  // hash the string an use the hash as an index into the id map
  std::size_t hash = hasher_(lit);
  auto it(ids_.find(hash));
  KeyType literalID;

  if (it != std::end(ids_)) {
    // we found an id for hash but it could be a collision
    // check the catual string values
    literalID = it->second;
    std::ptrdiff_t parentOffset = literals_[literalID - 1];
    EntryHeader entry(0, parentOffset, 0);

    // check an entry and all overflow buckets
    // until there are no more entries or the strings match
    do {
      parentOffset = entry.next;
      entry = readEntry(parentOffset);
      if (entry.lsize == lit.size()) {
        if (::memcmp(map_ + parentOffset + sizeof(EntryHeader), lit.data(), entry.lsize) == 0) {
          // strings matched, return the actual id
          return entry.id;
        }
      }
    } while (entry.next);

    // no string did match, so we create a new overflow entry
    std::ptrdiff_t offset = pos_ - map_;
    writeValue(parentOffset + sizeof(KeyType), offset);
    return writeEntry(lit, offset, keyModifier);
  }

  // no id was found, so we just create a new entry
  std::ptrdiff_t offset = pos_ - map_;
  literalID = writeEntry(lit, offset, keyModifier);
  ids_[hash] = literalID;
  return literalID;
}

////////////////////////////////////////////////////////////////////////////////

Dictionary::KeyType Dictionary::writeEntry(const std::string& lit, std::ptrdiff_t offset,
    const KeyModifier& keyModifier)
{
  KeyType literalID = nextKey_++;
  if (static_cast<bool>(keyModifier)) {
    keyModifier(literalID);
  }
  EntryHeader newEntry(literalID, 0, lit.size());
  // write the header
  offset = writeHeader(newEntry, offset);
  // write the character data
  writeLiteral(lit, offset + sizeof(EntryHeader));
  // save the new offset
  literals_.push_back(offset);
  return literalID;
}

////////////////////////////////////////////////////////////////////////////////

std::string Dictionary::Find(KeyType key) const
{
  // we should read the complete entry header here,
  // but we only need the size and the string data
  std::ptrdiff_t offset = literals_[key - 1] + 2 * sizeof(KeyType);
  char* pos = map_ + offset;

  // read size
  std::size_t size;
  ::memcpy(&size, pos, sizeof(size));
  pos += sizeof(size);

  // instantiate string from byte pointer and size
  return std::string(pos, size);
}

////////////////////////////////////////////////////////////////////////////////

void Dictionary::unmap()
{
  ::munmap(map_, size_);
}

////////////////////////////////////////////////////////////////////////////////

void Dictionary::map(std::size_t numPages)
{
  // keep the current offset
  std::ptrdiff_t offset = pos_ - map_;

  // resize the backing file to the new size
  if (::ftruncate(fd_, numPages * PAGE_SIZE - 1) != 0) {
    throw std::runtime_error("Could not change file size.");
  }
  // map the file into memory
  void* map = ::mmap(0, numPages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (map == MAP_FAILED) {
    throw std::runtime_error("Could not map file.");
  }
  // the root pointer of the mapping
  map_ = static_cast<char*>(map);
  // restore psoition pointer
  pos_ = map_ + offset;
  // update size
  size_ = numPages * PAGE_SIZE;
  // update number of mapped pages
  numPages_ = numPages;
}
