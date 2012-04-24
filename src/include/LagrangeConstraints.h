//
//  LagrangeConstraints.h
//  Camellia
//
//  Created by Nathan Roberts on 4/23/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef Camellia_LagrangeConstraints_h
#define Camellia_LagrangeConstraints_h

#include "SpatialFilter.h"
#include "Constraint.h"

class LagrangeConstraints {
private:
  vector< Constraint > _constraints;
  vector< Constraint > _globalConstraints;
public: 
  void addConstraint(const Constraint &c);
  void addGlobalConstraint(const Constraint &c);
  int numElementConstraints();
  int numGlobalConstraints();
//  void addConstraint(const Constraint &c, SpatialFilterPtr sf);
};

#endif
