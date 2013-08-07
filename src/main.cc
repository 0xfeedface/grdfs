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
#include "Dictionary.hh"
#include "Reasoner.hh"
#include "OpenCLReasoner.hh"
// #include "NativeReasoner.h"
#include "Timer.hh"

void PrintUsage();
void PrintTriple(const Store::Triple& t, Dictionary& d);

int main(int argc, const char* argv[])
{
  std::string device, ruleSet, fileName;
  bool printTriples, timeExecution, useAxioms, noLocalDeduplication, noGlobalDeduplication;

  namespace po = boost::program_options;
  po::options_description desc("options");
  desc.add_options()
    ("help,h", "produce this help message")
    ("input-file,i", po::value<std::string>(&fileName), "source turtle file")
    ("device", po::value<std::string>(&device)->default_value("gpu"), "OpenCL device to run on (gpu, cpu)")
    ("rules", po::value<std::string>(&ruleSet)->default_value("rhodf"), "rule set to use (rhodf, rdfs")
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
    std::clog << desc << std::endl;
    return EXIT_FAILURE;
  }
  po::notify(vm);

  if (!fileName.size()) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  std::ifstream file(fileName, std::ifstream::in);
  if (file.fail()) {
    std::clog << "could not read file '" << fileName << "'.\n";
    return EXIT_FAILURE;
  }

  if (device.size() != 3) {
    std::clog << "Unknown device type: " << device << std::endl;
    PrintUsage();
    return EXIT_FAILURE;
  }

  cl_device_type deviceType;
  if (std::strncmp(device.c_str(), "cpu", 3) == 0) {
    deviceType = CL_DEVICE_TYPE_CPU;
    std::clog << "Using CPU device." << std::endl;
  } else if (std::strncmp(device.c_str(), "gpu", 3) == 0) {
    deviceType = CL_DEVICE_TYPE_GPU;
    std::clog << "Using GPU device." << std::endl;
  } else {
    std::clog << "Unknown device type: " << device << std::endl;
    PrintUsage();
    return EXIT_FAILURE;
  }

  Dictionary dictionary;
  std::shared_ptr<OpenCLReasoner> reasoner;

  try {
    reasoner = std::make_shared<OpenCLReasoner>(
        dictionary,
        ruleSet == "rdfs" ? Reasoner::kRDFSRuleSet : Reasoner::kRhoDFRuleSet,
        deviceType,
        !noLocalDeduplication,
        !noGlobalDeduplication);
  } catch (std::exception& e) {
    std::clog << "Could not instantiate reasoner: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  Timer parsing, lookup, storage, closure;
  TurtleParser parser(file);
  std::size_t triplesParsed(0);
  std::string subject, predicate, object, subType;
  Type::ID subjectType, objectType;
  while (true) {
    try {
      parsing.start();
      if (!parser.parse(subject, predicate, object, subjectType, objectType, subType)) {
        break;
      }
      parsing.stop();
    } catch (TurtleParser::Exception& e) {
      std::cerr << e.message << std::endl;

      // ignore rest of the line and continue with the next one
      while (file.get() != '\n') {
        continue;
      }
    }

    lookup.start();
    // Dictionary::KeyType subjectID = dictionary.Lookup(subject);
    Dictionary::KeyType subjectID = dictionary.Lookup(subject, [subjectType](Dictionary::KeyType& key) {
      if (subjectType == Type::ID::Blank) {
        key |= Reasoner::blankMask;
      }
    });
    Dictionary::KeyType predicateID = dictionary.Lookup(predicate);

    Dictionary::KeyType objectID = dictionary.Lookup(object, [objectType](Dictionary::KeyType& key) {
      if (objectType == Type::ID::Blank) {
        key |= Reasoner::blankMask;
      } else if (objectType != Type::ID::URI) {
        key |= Reasoner::literalMask;
      }
    });
    lookup.stop();

    storage.start();
    bool stored = reasoner->addTriple(Store::Triple(subjectID, predicateID, objectID));
    storage.stop();

    if (stored) {
      ++triplesParsed;
      /*
       * std::cout << subjectID << " " << predicateID << " " << objectID << std::endl;
       */
    }
  }

  if (useAxioms) {
    reasoner->addAxiomaticTriples();
  }

  closure.start();
  try {
    reasoner->computeClosure();

    if (printTriples) {
      for (auto it(reasoner->triples_.ebegin()); it != reasoner->triples_.eend(); it++) {
        PrintTriple(*it, dictionary);
      }
      for (auto it(reasoner->typeTriples_.ebegin()); it != reasoner->typeTriples_.eend(); it++) {
        PrintTriple(*it, dictionary);
      }
      for (auto it(reasoner->schemaTriples_.ebegin()); it != reasoner->schemaTriples_.eend(); it++) {
        PrintTriple(*it, dictionary);
      }
    }
  } catch (Reasoner::Error& err) {
    std::cerr << "Reasoner error: " << err.message() << std::endl;
    exit(EXIT_FAILURE);
  } catch (std::exception& e) {
    std::cerr << "Unknownd error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  closure.stop();

  std::clog << "Data statistics" << std::endl;
  std::clog << "    input triples: " << triplesParsed << std::endl;
  std::clog << "    inferred unique triples: " << reasoner->inferredTriples() << std::endl;
  std::clog << "    inferred duplicate triples: " << reasoner->inferredDuplicates() << std::endl;

  if (timeExecution) {
    std::clog.setf(std::ios::fixed, std::ios::floatfield);
    std::clog.precision(2);
    std::clog << "Import timings" << std::endl;
    std::clog << "    parsing: " << parsing.elapsed() << " ms" << std::endl;
    std::clog << "    dictionary lookup: " << lookup.elapsed() << " ms" << std::endl;
    std::clog << "    storage: " << storage.elapsed() << " ms" << std::endl;
    std::clog << "Reasoner timings" << std::endl;
    std::clog << "    total closure calculation: " << closure.elapsed() << " ms" << std::endl;
    std::clog << "    detailed timings" << std::endl;
    for (auto value : reasoner->timings()) {
      std::clog << "        " << value.first << ": " << value.second << " ms" << std::endl;
    }
  }

  return EXIT_SUCCESS;
}

void PrintUsage() {
  std::clog << "usage: grdfs --input-file <file.ttl> [options]" << std::endl;
}

void PrintTriple(const Store::Triple& triple, Dictionary& dictionary)
{
  std::string subject(dictionary.Find(triple.subject));
  std::string predicate(dictionary.Find(triple.predicate));
  std::string object(dictionary.Find(triple.object));

  if (triple.predicate & Reasoner::blankMask) {
  // if (predicate[0] == '_') {
    // non-RDF triple (blank node property)
    std::clog << "non-standard RDF triple (not written)" << std::endl;
    return;
  } else {
    predicate = "<" + predicate + ">";
  }

  if (!(triple.subject & Reasoner::blankMask)) {
  // if (subject[0] != '_') {
    subject = "<" + subject + ">";
  }

  if (triple.object & Reasoner::literalMask) {
    object = "\"" + object + "\"";
  } else if (triple.object & Reasoner::blankMask) {
  // } else if (object[0] == '_') {
    // do nothing (blank node)
  } else {
    object = "<" + object + ">";
  }

  std::cout << subject << " " << predicate << " " << object << " ." << std::endl;
}
