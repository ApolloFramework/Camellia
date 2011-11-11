
// @HEADER
//
// Copyright © 2011 Sandia Corporation. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are 
// permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice, this list of 
// conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of 
// conditions and the following disclaimer in the documentation and/or other materials 
// provided with the distribution.
// 3. The name of the author may not be used to endorse or promote products derived from 
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY 
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR 
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Nate Roberts (nate@nateroberts.com).
//
// @HEADER

/*
 *  PoissonBCQuartic.cpp
 *
 *  Created by Nathan Roberts on 6/27/11.
 *
 */

#include "PoissonBCQuartic.h"
#include "PoissonBilinearForm.h"

bool PoissonBCQuartic::bcsImposed(int varID){
  // returns true if there are any BCs anywhere imposed on varID
  if ((varID == PoissonBilinearForm::PHI_HAT) || (varID == PoissonBilinearForm::PSI_HAT_N)) {
    return true;
  } else {
    return false;
  }

}
void PoissonBCQuartic::imposeBC(int varID, FieldContainer<double> &physicalPoints,
                           FieldContainer<double> &unitNormals,
                           FieldContainer<double> &dirichletValues,
                           FieldContainer<bool> &imposeHere) {
  int numCells = physicalPoints.dimension(0);
  int numPoints = physicalPoints.dimension(1);
  int spaceDim = physicalPoints.dimension(2);
  
  TEST_FOR_EXCEPTION( ( spaceDim != 2  ),
                     std::invalid_argument,
                     "PoissonBCQuartic expects spaceDim==2.");  
  
  TEST_FOR_EXCEPTION( ( dirichletValues.dimension(0) != numCells ) 
                     || ( dirichletValues.dimension(1) != numPoints ) 
                     || ( dirichletValues.rank() != 2  ),
                     std::invalid_argument,
                     "dirichletValues dimensions should be (numCells,numPoints).");
  TEST_FOR_EXCEPTION( ( imposeHere.dimension(0) != numCells ) 
                     || ( imposeHere.dimension(1) != numPoints ) 
                     || ( imposeHere.rank() != 2  ),
                     std::invalid_argument,
                     "imposeHere dimensions should be (numCells,numPoints).");
  if (varID == PoissonBilinearForm::PHI_HAT) {
    for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
      for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
        double x = physicalPoints(cellIndex,ptIndex,0);
        double y = physicalPoints(cellIndex,ptIndex,1);
        dirichletValues(cellIndex,ptIndex) = x*x*x*x + x*x*x*y;
        imposeHere(cellIndex,ptIndex) = true; // for now, just impose everywhere...
      }
    }
  } else if (varID == PoissonBilinearForm::PSI_HAT_N) {
    // value will be (1 1) \cdot n, the outward normal
    // TODO: change this to use the passed-in normals, once we pass those in
    for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
      for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
        // value = n1 * (4x^3 + 3x^2 y) + x^3 n2, where (n1, n2) is the outward unit normal
        double x = physicalPoints(cellIndex,ptIndex,0);
        double y = physicalPoints(cellIndex,ptIndex,1);
        double n1 = unitNormals(cellIndex,ptIndex,0);
        double n2 = unitNormals(cellIndex,ptIndex,1);
        double value = (4.0*x*x*x + 3.0*x*x*y)*n1 + x*x*x*n2;
        dirichletValues(cellIndex,ptIndex) = value;
        imposeHere(cellIndex,ptIndex) = true; // for now, just impose everywhere...
      }
    }
  }
  
}