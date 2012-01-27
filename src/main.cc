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
#include <raptor2/raptor.h>

#include "types.h"
#include "Dictionary.h"
#include "OpenCLReasoner.h"
#include "NativeReasoner.h"

//#undef GRDFS_PROFILING

void printUsage();
std::shared_ptr<std::string> raptorTermToString(raptor_term* term);
void statementHandler(void* userData, raptor_statement* statement);

int main (int argc, const char * argv[]) {
  if (argc < 2) {
    printUsage();
    return EXIT_FAILURE;
  }
  
  const char* fileName = argv[1];
  FILE* file = fopen(fileName, "rb");
  if (!file) {
    std::cout << "File " << fileName << " not readable." << std::endl;
    return EXIT_FAILURE;
  }
  
  Dictionary dictionary;
//  OpenCLReasoner reasoner(dictionary);
  NativeReasoner reasoner(&dictionary);
  
  // Parse the turtle file
  raptor_world* world = raptor_new_world();
  raptor_parser* turtleParser = raptor_new_parser(world, "turtle");
  
  std::pair<Dictionary*, Reasoner*> dar(&dictionary, &reasoner);
  raptor_parser_set_statement_handler(turtleParser, &dar, statementHandler);
  raptor_uri* baseURI = raptor_new_uri(world, reinterpret_cast<const unsigned char*>(fileName));
  raptor_parser_parse_file_stream(turtleParser, file, fileName, baseURI);
  raptor_free_uri(baseURI);
  fclose(file);
  raptor_free_parser(turtleParser);
  raptor_free_world(world);
  
//  dictionary.PrintStatistics();
  
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

std::shared_ptr<std::string> raptorTermToString(raptor_term* term) {
  assert(term != NULL);
  std::shared_ptr<std::string> termString;
  switch (term->type) {
    case RAPTOR_TERM_TYPE_URI:
      termString.reset(new std::string(reinterpret_cast<char*>(raptor_uri_as_string(term->value.uri))));
      break;
    case RAPTOR_TERM_TYPE_LITERAL:
      termString.reset(new std::string(reinterpret_cast<char*>(term->value.literal.string)));
      break;
    case RAPTOR_TERM_TYPE_BLANK:
      termString.reset(new std::string(reinterpret_cast<char*>(term->value.blank.string)));
      break;
    case RAPTOR_TERM_TYPE_UNKNOWN:
    default:
      break;
  }
  return termString;
}

void statementHandler(void* userData, raptor_statement* statement) {
  std::pair<Dictionary*, Reasoner*>* dar = static_cast<std::pair<Dictionary*, Reasoner*>*>(userData);
  Dictionary* dictionary = dar->first;
  Reasoner* reasoner = dar->second;
  
  term_id subject   = dictionary->Lookup(*raptorTermToString(statement->subject));
  term_id predicate = dictionary->Lookup(*raptorTermToString(statement->predicate));
  term_id object    = dictionary->Lookup(*raptorTermToString(statement->object), 
                                         statement->object->type == RAPTOR_TERM_TYPE_LITERAL);
  
//  std::cout << *raptorTermToString(statement->predicate) << std::endl;
  
  reasoner->addTriple(triple(subject, predicate, object));
}
