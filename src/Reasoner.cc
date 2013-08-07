//
//  Reasoner.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include <iostream>
#include <cstring> // memcmp

#include "Reasoner.hh"

Reasoner::Reasoner(Dictionary& dict, RuleSet ruleSet) : dict_(dict)
{
  subClassOf_    = dict.Lookup(kSubClassOfURI);
  subPropertyOf_ = dict.Lookup(kSubPropertyOfURI);
  domain_        = dict.Lookup(kDomainURI);
  range_         = dict.Lookup(kRangeURI);
  type_          = dict.Lookup(kTypeURI);

  ruleSet_       = ruleSet;
}

bool Reasoner::addTriple(const Store::Triple& t, Store::TripleFlags flags)
{
  bool result(false);
  tempTimer_.start();
  if (isSchemaProperty(t.predicate)) {
    if (schemaTriples_.addTriple(t, flags)) {
      // triple is new, put it in the schema indexes
      if (t.predicate == subClassOf_) {
        scSuccessors_[t.subject].insert(t.object);
        scPredecessors_[t.object].insert(t.subject);
        scTerms_.insert(t.subject);
        scTerms_.insert(t.object);
      } else if (t.predicate == subPropertyOf_) {
        spSuccessors_[t.subject].insert(t.object);
        spPredecessors_[t.object].insert(t.subject);
        spTerms_.insert(t.subject);
        spTerms_.insert(t.object);
      } else if (t.predicate == domain_) {
        domTriples_[t.subject].insert(t.object);
      } else if (t.predicate == range_) {
        rngTriples_[t.subject].insert(t.object);
      }
      result = true;
    }
  } else {
    // store separate non-schema triples
    if (t.predicate == type_) {
      // index for rdf:type triples
      result = typeTriples_.addTriple(t, flags);
    } else {
      // everything else
      result = triples_.addTriple(t, flags);
    }
  }

  std::string predicateURI = dict_.Find(t.predicate);
  static const std::string rdfMemberPrefix = "http://www.w3.org/1999/02/22-rdf-syntax-ns#_";

  if (predicateURI.size() > rdfMemberPrefix.size()
      && predicateURI.compare(0, 44, rdfMemberPrefix) == 0) {

    membershipProperties_.insert(t.predicate);
  }

  tempTimer_.stop();

  // count entailed triples and duplicates
  if (flags & Store::kFlagsEntailed) {
    if (result) {
      ++inferredTriplesCount_;
      storeTimer_.addTimer(tempTimer_);
    } else {
      ++inferredDuplicatesCount_;
      uniqueingTimer_.addTimer(tempTimer_);
    }
    tempTimer_.reset();
  }

  return result;
}

void Reasoner::addAxiomaticTriples() {
  term_id Resource = dict_.Lookup(kResourceURI);
  term_id Property = dict_.Lookup(kPropertyURI);
  term_id Class = dict_.Lookup(kClassURI);
  term_id Literal = dict_.Lookup(kLiteralURI);
  term_id Statement = dict_.Lookup(kStatementURI); 
  term_id Container = dict_.Lookup(kContainerURI);
  term_id ContainerMembershipProperty = dict_.Lookup(kConMemShipPropURI);

  term_id member = dict_.Lookup(kMemberURI);
  term_id seeAlso = dict_.Lookup(kSeeAlsoURI);
  term_id isDefinedBy = dict_.Lookup(kIsDefinedByURI);
  term_id comment = dict_.Lookup(kCommentURI);
  term_id label = dict_.Lookup(kLabelURI);

  term_id subject = dict_.Lookup(kSubjectURI);
  term_id predicate = dict_.Lookup(kPredicateURI);
  term_id object = dict_.Lookup(kObjectURI);
  term_id first = dict_.Lookup(kFirstURI);
  term_id rest = dict_.Lookup(kRestURI);
  term_id value = dict_.Lookup(kValueURI);

  term_id List = dict_.Lookup(kListURI);
  term_id Alt = dict_.Lookup(kAltURI);
  term_id Bag = dict_.Lookup(kBagURI);
  term_id Seq = dict_.Lookup(kSeqURI);
  term_id XMLLiteral = dict_.Lookup(kXMLLiteralURI);
  term_id Datatype = dict_.Lookup(kDatatypeURI);

  addTriple(Store::Triple(type_, domain_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(domain_, domain_, Property), Store::kFlagsEntailed);
  addTriple(Store::Triple(range_, domain_, Property), Store::kFlagsEntailed);
  addTriple(Store::Triple(subPropertyOf_, domain_, Property), Store::kFlagsEntailed);
  addTriple(Store::Triple(subClassOf_, domain_, Property), Store::kFlagsEntailed);
  addTriple(Store::Triple(subject, domain_, Statement), Store::kFlagsEntailed);
  addTriple(Store::Triple(predicate, domain_, Statement), Store::kFlagsEntailed);
  addTriple(Store::Triple(object, domain_, Statement), Store::kFlagsEntailed);
  addTriple(Store::Triple(member, domain_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(first, domain_, List), Store::kFlagsEntailed);
  addTriple(Store::Triple(rest, domain_, List), Store::kFlagsEntailed);
  addTriple(Store::Triple(seeAlso, domain_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(isDefinedBy, domain_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(comment, domain_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(label, domain_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(value, domain_, Resource), Store::kFlagsEntailed);

  addTriple(Store::Triple(type_, range_, Class), Store::kFlagsEntailed);
  addTriple(Store::Triple(domain_, range_, Class), Store::kFlagsEntailed);
  addTriple(Store::Triple(range_, range_, Class), Store::kFlagsEntailed);
  addTriple(Store::Triple(subPropertyOf_, range_, Property), Store::kFlagsEntailed);
  addTriple(Store::Triple(subClassOf_, range_, Class), Store::kFlagsEntailed);
  addTriple(Store::Triple(subject, range_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(predicate, range_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(object, range_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(member, range_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(rest, range_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(seeAlso, range_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(isDefinedBy, range_, Resource), Store::kFlagsEntailed);
  addTriple(Store::Triple(comment, range_, Literal), Store::kFlagsEntailed);
  addTriple(Store::Triple(label, range_, Literal), Store::kFlagsEntailed);
  addTriple(Store::Triple(value, range_, Resource), Store::kFlagsEntailed);

  addTriple(Store::Triple(Alt, subClassOf_, Container), Store::kFlagsEntailed);
  addTriple(Store::Triple(Bag, subClassOf_, Container), Store::kFlagsEntailed);
  addTriple(Store::Triple(Seq, subClassOf_, Container), Store::kFlagsEntailed);
  addTriple(Store::Triple(ContainerMembershipProperty, subClassOf_, Property), Store::kFlagsEntailed);

  addTriple(Store::Triple(XMLLiteral, type_, Datatype), Store::kFlagsEntailed);
  addTriple(Store::Triple(XMLLiteral, subClassOf_, Literal), Store::kFlagsEntailed);
  addTriple(Store::Triple(Datatype, subClassOf_, Class), Store::kFlagsEntailed);

  // add finite axiomatic triples for container membership properties
  for (term_id membershipProperty : membershipProperties_) {
    addTriple(Store::Triple(membershipProperty, type_, ContainerMembershipProperty), Store::kFlagsEntailed);
    addTriple(Store::Triple(membershipProperty, domain_, Resource), Store::kFlagsEntailed);
    addTriple(Store::Triple(membershipProperty, range_, Resource), Store::kFlagsEntailed);
  }
}

void Reasoner::printStatistics()
{
  std::cout << "sc terms: " << scSuccessors_.size() << std::endl;
  std::cout << "sp terms: " << spSuccessors_.size() << std::endl;
  std::cout << "dom triples: " << domTriples_.size() << std::endl;
  std::cout << "range triples: " << rngTriples_.size() << std::endl;
  std::cout << "dictionary size: " << dict_.Size() << std::endl;
  std::cout << std::endl;
}

Reasoner::TimingMap Reasoner::timings()
{
  TimingMap result;
  result.insert(std::make_pair("storage", storeTimer_.elapsed()));
  result.insert(std::make_pair("uniqueing", uniqueingTimer_.elapsed()));
  return result;
}

bool operator<(const so_pair& p1, const so_pair& p2)
{
  if (p1.subject > p2.subject || p1.object > p2.object) {
    return false;
  }

  return true;
}
