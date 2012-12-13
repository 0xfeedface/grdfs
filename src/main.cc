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
#include <cstring>
#include <boost/program_options.hpp>
#include "infra/util/Type.hpp"
#include "cts/parser/TurtleParser.hpp"

#include "types.h"
#include "Dictionary.h"
#include "Reasoner.h"
#include "OpenCLReasoner.h"
// #include "NativeReasoner.h"
#include "Timer.h"

void PrintUsage();
void PrintTriple(const Store::Triple& t, Dictionary& d);

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
  std::string device, fileName;
  bool printTriples, timeExecution, useAxioms, noLocalDeduplication, noGlobalDeduplication;

  namespace po = boost::program_options;
  po::options_description desc("options");
  desc.add_options()
    ("help,h", "produce this help message")
    ("input-file,i", po::value<std::string>(&fileName), "source turtle file")
    ("device", po::value<std::string>(&device)->default_value("gpu"), "OpenCL device to run on (gpu, cpu)")
    ("no-local-dedup,l", po::bool_switch(&noLocalDeduplication), "disable local deduplication (default if using CPU)")
    ("no-global-dedup,g", po::bool_switch(&noGlobalDeduplication), "disable global deduplication")
    ("axioms,a", po::bool_switch(&useAxioms), "include finite RDFS axiomatic triples")
    ("time,t", po::bool_switch(&timeExecution), "time reasoner execution")
    ("print-triples,p", po::bool_switch(&printTriples), "write triples in ntriples format to stdout")
  ;

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
  } catch (...) {
    PrintUsage();
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }
  po::notify(vm);

  if (!fileName.size()) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  std::ifstream file(fileName, std::ifstream::in);
  if (file.fail()) {
    std::cerr << "could not read file '" << fileName << "'.\n";
    return EXIT_FAILURE;
  }

  if (device.size() != 3) {
    std::cout << "Unknown device type: " << device << std::endl;
    PrintUsage();
    return EXIT_FAILURE;
  }

  cl_device_type deviceType;
  if (std::strncmp(device.c_str(), "cpu", 3) == 0) {
    deviceType = CL_DEVICE_TYPE_CPU;
    std::cout << "Using CPU device." << std::endl;
  } else if (std::strncmp(device.c_str(), "gpu", 3) == 0) {
    deviceType = CL_DEVICE_TYPE_GPU;
    std::cout << "Using GPU device." << std::endl;
  } else {
    std::cout << "Unknown device type: " << device << std::endl;
    PrintUsage();
    return EXIT_FAILURE;
  }

  Dictionary dictionary;
  OpenCLReasoner reasoner(dictionary, deviceType, !noLocalDeduplication, !noGlobalDeduplication);

  if (useAxioms) {
    reasoner.addAxiomaticTriples();
  }

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

      // ignore rest of the line and continue with the next one
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

    storage.start();
    reasoner.addTriple(Store::Triple(subjectID, predicateID, objectID));
    storage.stop();
  }

  closure.start();
  try {
    reasoner.computeClosure();

    if (printTriples) {
      for (auto it(reasoner.triples_.ebegin()); it != reasoner.triples_.eend(); it++) {
        PrintTriple(*it, dictionary);;
      }
      for (auto it(reasoner.typeTriples_.ebegin()); it != reasoner.typeTriples_.eend(); it++) {
        PrintTriple(*it, dictionary);
      }
      for (auto it(reasoner.schemaTriples_.ebegin()); it != reasoner.schemaTriples_.eend(); it++) {
        PrintTriple(*it, dictionary);
      }
      std::cout << std::endl;
    }
  } catch (Reasoner::Error& err) {
    std::cerr << err.message() << std::endl;
    exit(EXIT_FAILURE);
  }

  closure.stop();
  std::clog << "Parsed triples: " << triplesParsed << std::endl;
  std::clog << "Inferred triples: " << reasoner.inferredTriples() << std::endl;
  std::clog << "Inferred duplicates: " << reasoner.inferredDuplicates() << std::endl;

  if (timeExecution) {
    std::clog.setf(std::ios::fixed, std::ios::floatfield);
    std::clog.precision(2);
    std::clog << "Parsing: " << parsing.elapsed() << " ms" << std::endl;;
    std::clog << "Dictionary lookup: " << lookup.elapsed() << " ms" << std::endl;;
    std::clog << "Storage: " << storage.elapsed() << " ms" << std::endl;;
    std::clog << "Closure calculation: " << closure.elapsed() << " ms" << std::endl;
    std::clog << "Detailed reasoner timings" << std::endl;;
    for (auto value : reasoner.timings()) {
      std::clog << "    " << value.first << ": " << value.second << " ms" << std::endl;;
    }
  }

  return EXIT_SUCCESS;
}

void PrintUsage() {
  std::cout << "usage: grdfs --input-file <file.ttl> [options]" << std::endl;
}

void PrintTriple(const Store::Triple& triple, Dictionary& dictionary)
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
