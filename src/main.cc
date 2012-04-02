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
#include "cts/parser/TurtleParser.hpp"

#include "types.h"
#include "Dictionary.h"
#include "OpenCLReasoner.h"
#include "NativeReasoner.h"

//#undef GRDFS_PROFILING

void printUsage();

int main (int argc, const char * argv[]) {
  if (argc < 2) {
    printUsage();
    return EXIT_FAILURE;
  }
  
  const char* fileName = argv[1];
  std::ifstream file(fileName, std::ifstream::in);
  if (file.fail()) {
    std::cout << "Could not read file '" << fileName << "'.\n";
    return EXIT_FAILURE;
  }
  
  Dictionary dictionary;
//  OpenCLReasoner reasoner(dictionary);
  NativeReasoner reasoner(dictionary);

  TurtleParser parser(file);
  std::string subject, predicate, object, subType;
  Type::ID type;
  while (true) {
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

    term_id subjectID   = dictionary.Lookup(subject);
    term_id predicateID = dictionary.Lookup(predicate);
    term_id objectID    = dictionary.Lookup(object, type == Type::ID::Literal);

    reasoner.addTriple(triple(subjectID, predicateID, objectID));
  }


  dictionary.PrintStatistics();

#ifdef GRDFS_PROFILING
  uint64_t beforeClosure = mach_absolute_time();
#endif
  reasoner.computeClosure();
#ifdef GRDFS_PROFILING
  uint64_t afterClosure = mach_absolute_time();
  struct mach_timebase_info info;
  mach_timebase_info(&info);
  std::cout << "Closure calculation took " << 1e-6 * ((afterClosure - beforeClosure) * info.numer / info.denom) << " ms" << std::endl;
#endif
  
  return EXIT_SUCCESS;
}

void printUsage() {
  std::cout <<  "usage: grdfs <rdf_file.ttl>" << std::endl;
//  std::cout <<  "usage: grdfs <rdf_file.ttl> [-p]" << std::endl;
//  std::cout <<  "       -p: print profiling information" << std::endl;
}

