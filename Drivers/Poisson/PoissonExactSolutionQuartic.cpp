
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
 *  PoissonExactSolutionQuartic.cpp
 *
 *  Created by Nathan Roberts on 7/5/11.
 *
 */

#include "PoissonBCQuartic.h"
#include "PoissonRHSQuartic.h"
#include "PoissonBilinearForm.h"

#include "PoissonExactSolutionQuartic.h"

PoissonExactSolutionQuartic::PoissonExactSolutionQuartic() {
  _bc = Teuchos::rcp(new PoissonBCQuartic());
  _bilinearForm = Teuchos::rcp(new PoissonBilinearForm());
  _rhs = Teuchos::rcp(new PoissonRHSQuartic());
}

double PoissonExactSolutionQuartic::solutionValue(int trialID,
                                                FieldContainer<double> &physicalPoint) {
  double x = physicalPoint(0);
  double y = physicalPoint(1);
  switch(trialID) {
      // (psi, grad v1)_K + (psi_hat_n, v1)_dK
    case PoissonBilinearForm::PHI:
    case PoissonBilinearForm::PHI_HAT:
      return x*x*x*x + x*x*x*y;
      break;
    case PoissonBilinearForm::PSI_1:
      return 4.0*x*x*x + 3.0*x*x*y;
      break;
    case PoissonBilinearForm::PSI_2:
      return x*x*x;
      break;
    case PoissonBilinearForm::PSI_HAT_N:
      // TODO: throw exception: other solutionValue (with normal) should be called here
      break;
  }
  // TODO: throw exception
  return 0.0;
}

double PoissonExactSolutionQuartic::solutionValue(int trialID,
                                                 FieldContainer<double> &physicalPoint,
                                                 FieldContainer<double> &unitNormal) {
  // TODO: implement this
  return 0.0;
}

int PoissonExactSolutionQuartic::H1Order() {
  return 5; // order is the H1 order--since x^4 + x^3 y is quartic in HVOL, we're quintic.
}