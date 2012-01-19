//
//  types.h
//  grdfs
//
//  Created by Norman Heino on 11-10-26.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef types_h
#define types_h

#include <string>

// Type for RDF term IDs
typedef uint64_t term_id;

// Schema properties
const std::string kSubClassOfURI    = "http://www.w3.org/2000/01/rdf-schema#subClassOf";
const std::string kSubPropertyOfURI = "http://www.w3.org/2000/01/rdf-schema#subPropertyOf";
const std::string kDomainURI        = "http://www.w3.org/2000/01/rdf-schema#domain";
const std::string kRangeURI         = "http://www.w3.org/2000/01/rdf-schema#range";
const std::string kTypeURI          = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";

// POD triple structure for a fixed property
struct so_pair {
  term_id subject;
  term_id object;
  
  so_pair() : subject(0), object(0) {}
  so_pair(term_id s, term_id o) : subject(s), object(o) {}
};

std::ostream& operator<<(std::ostream& outStream, so_pair const& pair);

// POD structure for RDF triples
struct triple {
  term_id subject;
  term_id predicate;
  term_id object;
  
  triple() : subject(0), predicate(0), object(0) {}
  triple(term_id s, term_id p, term_id o) : subject(s), predicate(p), object(o) {}
};

std::ostream& operator<<(std::ostream& outStream, triple const& t);

#endif
