#include "Dictionary.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
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

std::size_t Dictionary::writeHeader(Entry const& entry, std::size_t offset)
{
  // prevent string from crossing page boundary,
  // moving it entirely to the next page
  if ((offset >> LOG2_PAGE_SIZE) < (offset + sizeof(Entry) + entry.lsize) >> LOG2_PAGE_SIZE) {
    offset = PAGE_SIZE * ((offset >> LOG2_PAGE_SIZE) + 1);
    pos_ = map_ + offset;
  }

  // remap if necessary
  if ((offset + sizeof(Entry) + entry.lsize) > size_) {
    unmap();
    map(floorf(GOLDEN_RATIO * numPages_));
    offset = pos_ - map_;
  }

  // write entry
  writeValue(offset, entry);

  // advance pointer
  pos_ += sizeof(Entry);

  return offset;
}

////////////////////////////////////////////////////////////////////////////////

std::size_t Dictionary::writeLiteral(std::string const& lit, std::size_t offset)
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

Dictionary::Entry Dictionary::readEntry(KeyType const offset)
{
  return readValue<Entry>(offset);
}

////////////////////////////////////////////////////////////////////////////////

std::string Dictionary::readLiteral(KeyType const offset, std::size_t length)
{
  return std::string(map_ + offset, length);
}

////////////////////////////////////////////////////////////////////////////////

std::size_t Dictionary::hash(std::string const& str)
{
  std::size_t hash(0);
  for (int i(0); i != str.size(); ++i) {
    hash = hash * 101 + str[i];
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////

Dictionary::KeyType Dictionary::Lookup(std::string const& lit, bool literalHint)
{
  std::size_t key = hasher_(lit);
  auto it(ids_.find(key));
  KeyType literalID;

  if (it != std::end(ids_)) {
    literalID = it->second;
    std::ptrdiff_t parentOffset = literals_[literalID - 1];
    Entry entry(0, parentOffset, 0);

    do {
      parentOffset = entry.next;
      entry = readEntry(parentOffset);
      if (entry.lsize == lit.size()) {
        if (::memcmp(map_ + parentOffset + sizeof(Entry), lit.data(), entry.lsize) == 0) {
          return entry.id;
        }
      }
    } while (entry.next);

    // overflow entry
    std::ptrdiff_t offset = pos_ - map_;
    writeValue(parentOffset + sizeof(KeyType), offset);
    return writeEntry(lit, offset, literalHint);
  }

  // new entry
  std::ptrdiff_t offset = pos_ - map_;
  literalID = writeEntry(lit, offset, literalHint);
  ids_[key] = literalID;
  return literalID;
}

////////////////////////////////////////////////////////////////////////////////

Dictionary::KeyType Dictionary::writeEntry(std::string const& lit, std::ptrdiff_t offset, bool literal)
{
  // not found, create a new entry
  KeyType literalID = nextKey_++;
  if (literal) {
    literalID |= literalMask;
  }
  Entry newEntry(literalID, 0, lit.size());
  offset = writeHeader(newEntry, offset);
  writeLiteral(lit, offset + sizeof(Entry));
  literals_.push_back(offset);
  return literalID;
}

////////////////////////////////////////////////////////////////////////////////

std::string Dictionary::Find(KeyType key) const
{
  std::ptrdiff_t offset = literals_[key - 1] + 2 * sizeof(KeyType);
  char* pos = map_ + offset;

  // read size
  std::size_t size;
  ::memcpy(&size, pos, sizeof(size));
  pos += sizeof(size);

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
  std::ptrdiff_t offset = pos_ - map_;

  if (::ftruncate(fd_, numPages * PAGE_SIZE - 1) != 0) {
    throw std::runtime_error("Could not change file size.");
  }
  void* map = ::mmap(0, numPages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (map == MAP_FAILED) {
    throw std::runtime_error("Could not map file.");
  }
  map_ = static_cast<char*>(map);
  pos_ = map_ + offset;
  size_ = numPages * PAGE_SIZE;
  numPages_ = numPages;
}
