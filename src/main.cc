//
//  main.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#if defined(__APPLE__) || defined(__MACOSX)
#define GRDFS_PROFILING 1
#include <mach/mach_time.h>
#endif

#include <iostream>
#include <cassert>
#include <memory>
#include <fstream>
#include "infra/util/Type.hpp"
#include "cts/parser/TurtleParser.hpp"

#include "types.h"
#include "Dictionary.h"
#include "OpenCLReasoner.h"
#include "NativeReasoner.h"

// #undef GRDFS_PROFILING

void printUsage();

int main (int argc, const char* argv[]) {
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
  OpenCLReasoner reasoner(dictionary);
  // NativeReasoner reasoner(dictionary);

#ifdef GRDFS_PROFILING
  uint64_t preParsing, parsing(0), preLookup, lookup(0), preStorage, storage(0);
#endif
  TurtleParser parser(file);
  std::string subject, predicate, object, subType;
  Type::ID type;
  while (true) {
#ifdef GRDFS_PROFILING
    preParsing = mach_absolute_time();
#endif
    try {
      if (!parser.parse(subject, predicate, object, type, subType)) {
        break;
      }
    } catch (TurtleParser::Exception& e) {
      std::cerr << e.message << std::endl;

      // ignore rest of the line an conintue with the next one
      while (file.get() != '\n') {
        continue;
      }
    }
#ifdef GRDFS_PROFILING
    parsing += mach_absolute_time() - preParsing;
#endif

#ifdef GRDFS_PROFILING
    preLookup = mach_absolute_time();
#endif
    term_id subjectID   = dictionary.Lookup(subject);
    term_id predicateID = dictionary.Lookup(predicate);
    term_id objectID    = dictionary.Lookup(object, type != Type::ID::URI);
#ifdef GRDFS_PROFILING
    lookup += mach_absolute_time() - preLookup;
#endif

    /*
     * if (type != Type::ID::URI) {
     *   std::cout << object << " (" << objectID << ")\n";
     * }
     */

#ifdef GRDFS_PROFILING
    preStorage = mach_absolute_time();
#endif
    reasoner.addTriple(Store::Triple(subjectID, predicateID, objectID));
#ifdef GRDFS_PROFILING
    storage += mach_absolute_time() - preStorage;
#endif
  }

//  dictionary.Print();
//  std::cout << "----------------\n";

#ifdef GRDFS_PROFILING
  uint64_t beforeClosure = mach_absolute_time();
#endif
  try {
    reasoner.computeClosure();

/*
 *     for (auto it(reasoner.triples_.ebegin()); it != reasoner.triples_.eend(); it++) {
 *       std::string subject(dictionary.Find(it->subject));
 *       std::string predicate(dictionary.Find(it->predicate));
 *       std::string object(dictionary.Find(it->object));
 * 
 *       if (it->object & Dictionary::literalMask) {
 *         std::cout << "<" << subject << "> <" << predicate << "> \"" << object << "\" .\n";
 *       } else {
 *         std::cout << "<" << subject << "> <" << predicate << "> <" << object << "> .\n";
 *       }
 *     }
 *     std::cout << std::endl;
 */

  } catch (Reasoner::Error& err) {
    std::cerr << err.message() << std::endl;
    exit(EXIT_FAILURE);
  }
#ifdef GRDFS_PROFILING
  uint64_t afterClosure = mach_absolute_time();
  struct mach_timebase_info info;
  mach_timebase_info(&info);
  std::clog.setf(std::ios::fixed, std::ios::floatfield);
  std::clog.precision(2);
  std::clog << "Inferred triples: " << reasoner.inferredTriples() << std::endl;
  std::clog << "Closure calculation took " << 1e-6 * ((afterClosure - beforeClosure) * info.numer / info.denom) << " ms" << std::endl;
  std::clog << "Parsing: " << 1e-6 * parsing * info.numer / info.denom << " ms\n";
  std::clog << "Dictionary lookup: " << 1e-6 * lookup * info.numer / info.denom << " ms\n";
  std::clog << "Storage: " << 1e-6 * storage * info.numer / info.denom << " ms\n";
#endif

  return EXIT_SUCCESS;
}

void printUsage() {
  std::cerr <<  "usage: grdfs <rdf_file.ttl>" << std::endl;
//  std::cout <<  "usage: grdfs <rdf_file.ttl> [-p]" << std::endl;
//  std::cout <<  "       -p: print profiling information" << std::endl;
}

