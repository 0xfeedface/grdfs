//
//  Store.h
//  grdfs
//
//  Created by Norman Heino on 11-12-10.
//  Copyright (c) 2011 Norman Heino. All rights reserved.
//

#ifndef Store_h
#define Store_h

#include <vector>

#include "types.h"

class Store {
public:
  void addTriple(triple);
private:
  std::vector<triple> storage_;
};

#endif
