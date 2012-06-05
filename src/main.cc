//
//  main.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include <iostream>
#include <cassert>
#include <memory>
#include <fstream>
#include "infra/util/Type.hpp"
#include "cts/parser/TurtleParser.hpp"

#include "types.h"
#include "Dictionary.h"
#include "Reasoner.h"
#include "OpenCLReasoner.h"
#include "NativeReasoner.h"
#include "Timer.h"

#define GRDFS_PROFILING 1

void printUsage();
void printTriple(const Store::Triple& t, Dictionary& d);

// TODO: replace with lambda once clang 3.1 is out
struct LiteralModifier {
  bool isLiteral = false;
  void operator()(Dictionary::KeyType& key) {
    if (isLiteral) {
      key |= Reasoner::literalMask;
    }
  }
};

int main(int argc, const char* argv[])
{
  if (argc < 2) {
    printUsage();
    return EXIT_FAILURE;
  }

  const char* fileName = argv[1];
  std::ifstream file(fileName, std::ifstream::in);
  if (file.fail()) {
    std::cerr << "Could not read file '" << fileName << "'.\n";
    return EXIT_FAILURE;
  }

  Dictionary dictionary;
  OpenCLReasoner reasoner(dictionary, CL_DEVICE_TYPE_GPU);
  // NativeReasoner reasoner(dictionary);

  Timer parsing, lookup, storage, closure;
  TurtleParser parser(file);
  LiteralModifier modifier;
  std::size_t triplesParsed(0);
  std::string subject, predicate, object, subType;
  Type::ID type;
  while (true) {
    try {
      parsing.start();
      if (!parser.parse(subject, predicate, object, type, subType)) {
        break;
      }
      parsing.stop();
      ++triplesParsed;
    } catch (TurtleParser::Exception& e) {
      std::cerr << e.message << std::endl;

      // ignore rest of the line an conintue with the next one
      while (file.get() != '\n') {
        continue;
      }
    }

    lookup.start();
    Dictionary::KeyType subjectID   = dictionary.Lookup(subject);
    Dictionary::KeyType predicateID = dictionary.Lookup(predicate);

    modifier.isLiteral = (type != Type::ID::URI);
    Dictionary::KeyType objectID = dictionary.Lookup(object, modifier);
    lookup.stop();

    /*
     * if (type != Type::ID::URI) {
     *   std::cout << object << " (" << objectID << ")\n";
     * }
     */

    storage.start();
    reasoner.addTriple(Store::Triple(subjectID, predicateID, objectID));
    storage.stop();
  }

//  dictionary.Print();
//  std::cout << "----------------\n";

  closure.start();
  try {
    reasoner.computeClosure();

    /*
     * for (auto it(reasoner.triples_.ebegin()); it != reasoner.triples_.eend(); it++) {
     *   printTriple(*it, dictionary);
     * }
     * for (auto it(reasoner.typeTriples_.ebegin()); it != reasoner.typeTriples_.eend(); it++) {
     *   printTriple(*it, dictionary);
     * }
     * for (auto it(reasoner.schemaTriples_.ebegin()); it != reasoner.schemaTriples_.eend(); it++) {
     *   printTriple(*it, dictionary);
     * }
     * std::cout << std::endl;
     */

  } catch (Reasoner::Error& err) {
    std::cerr << err.message() << std::endl;
    exit(EXIT_FAILURE);
  }

  // reasoner.printStatistics();

  closure.stop();
  std::clog << "Parsed triples: " << triplesParsed << std::endl;
  std::clog << "Inferred triples: " << reasoner.inferredTriples() << std::endl;
  std::clog << "Inferred duplicates: " << reasoner.inferredDuplicates() << std::endl;
#ifdef GRDFS_PROFILING
  std::clog.setf(std::ios::fixed, std::ios::floatfield);
  std::clog.precision(2);
  std::clog << "Parsing: " << parsing.elapsed() << " ms\n";
  std::clog << "Dictionary lookup: " << lookup.elapsed() << " ms\n";
  std::clog << "Storage: " << storage.elapsed() << " ms\n";
  std::clog << "Closure calculation: " << closure.elapsed() << " ms" << std::endl;
  std::clog << "Detailed reasoner timings\n";
for (auto value : reasoner.timings()) {
    std::clog << "    " << value.first << ": " << value.second << " ms\n";
  }
#endif

  return EXIT_SUCCESS;
}

void printUsage()
{
  std::cerr <<  "usage: grdfs <rdf_file.ttl>" << std::endl;
//  std::cout <<  "usage: grdfs <rdf_file.ttl> [-p]" << std::endl;
//  std::cout <<  "       -p: print profiling information" << std::endl;
}

void printTriple(const Store::Triple& triple, Dictionary& dictionary)
{
  std::string subject(dictionary.Find(triple.subject));
  std::string predicate(dictionary.Find(triple.predicate));
  std::string object(dictionary.Find(triple.object));

  if (triple.object & Reasoner::literalMask) {
    std::cout << "<" << subject << "> <" << predicate << "> \"" << object << "\" .\n";
  } else {
    std::cout << "<" << subject << "> <" << predicate << "> <" << object << "> .\n";
  }
}
