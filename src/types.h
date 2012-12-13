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
const std::string kSubClassOfURI     = "http://www.w3.org/2000/01/rdf-schema#subClassOf";
const std::string kSubPropertyOfURI  = "http://www.w3.org/2000/01/rdf-schema#subPropertyOf";
const std::string kDomainURI         = "http://www.w3.org/2000/01/rdf-schema#domain";
const std::string kRangeURI          = "http://www.w3.org/2000/01/rdf-schema#range";
const std::string kTypeURI           = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";

const std::string kResourceURI       = "http://www.w3.org/2000/01/rdf-schema#Resource";
const std::string kPropertyURI       = "http://www.w3.org/2000/01/rdf-schema#Property";
const std::string kClassURI          = "http://www.w3.org/2000/01/rdf-schema#Class";
const std::string kLiteralURI        = "http://www.w3.org/2000/01/rdf-schema#Literal";
const std::string kStatementURI      = "http://www.w3.org/2000/01/rdf-schema#Statement";
const std::string kContainerURI      = "http://www.w3.org/2000/01/rdf-schema#Container";
const std::string kConMemShipPropURI = "http://www.w3.org/2000/01/rdf-schema#ContainerMembershipProperty";

const std::string kMemberURI         = "http://www.w3.org/2000/01/rdf-schema#member";
const std::string kSeeAlsoURI        = "http://www.w3.org/2000/01/rdf-schema#seeAlso";
const std::string kIsDefinedByURI    = "http://www.w3.org/2000/01/rdf-schema#isDefinedBy";
const std::string kCommentURI        = "http://www.w3.org/2000/01/rdf-schema#comment";
const std::string kLabelURI          = "http://www.w3.org/2000/01/rdf-schema#label";

const std::string kSubjectURI        = "http://www.w3.org/1999/02/22-rdf-syntax-ns#subject";
const std::string kPredicateURI      = "http://www.w3.org/1999/02/22-rdf-syntax-ns#predicate";
const std::string kObjectURI         = "http://www.w3.org/1999/02/22-rdf-syntax-ns#object";
const std::string kFirstURI          = "http://www.w3.org/1999/02/22-rdf-syntax-ns#first";
const std::string kRestURI           = "http://www.w3.org/1999/02/22-rdf-syntax-ns#rest";
const std::string kValueURI          = "http://www.w3.org/1999/02/22-rdf-syntax-ns#value";

const std::string kListURI           = "http://www.w3.org/1999/02/22-rdf-syntax-ns#List";
const std::string kAltURI            = "http://www.w3.org/1999/02/22-rdf-syntax-ns#Alt";
const std::string kBagURI            = "http://www.w3.org/1999/02/22-rdf-syntax-ns#Bag";
const std::string kSeqURI            = "http://www.w3.org/1999/02/22-rdf-syntax-ns#Seq";
const std::string kXMLLiteralURI     = "http://www.w3.org/1999/02/22-rdf-syntax-ns#XMLLiteral";
const std::string kDatatypeURI       = "http://www.w3.org/1999/02/22-rdf-syntax-ns#Datatype";

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
