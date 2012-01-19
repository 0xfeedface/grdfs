//
//  Store.cc
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#include "Store.h"

void Store::addTriple(triple t) {
  storage_.push_back(t);
}
