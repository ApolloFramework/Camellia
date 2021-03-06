#ifndef DPG_POISSON_EXACT_SOLUTION
#define DPG_POISSON_EXACT_SOLUTION

// @HEADER
//
// Copyright © 2014 Nathan V. Roberts. All Rights Reserved.
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
 *  PoissonExactSolution.h
 *
 *  Created by Nathan Roberts on 7/7/11.
 *
 */

#include "ExactSolution.h"
#include "BC.h"
#include "RHS.h"

#include "Function.h"

using namespace Camellia;

class PoissonExactSolution : public ExactSolution<double>
{
public:
  enum PoissonExactSolutionType
  {
    POLYNOMIAL = 0,
    EXPONENTIAL,
    TRIGONOMETRIC
  };
protected:
  int _polyOrder;
  PoissonExactSolutionType _type;

  BFPtr _bf;
  FunctionPtr phi();
public:
  PoissonExactSolution(PoissonExactSolutionType type, int polyOrder=-2, bool useConformingTraces=false); // poly order here means that of phi -- -2 for non-polynomial types
// ExactSolution
  virtual int H1Order(); // here it means the H1 order (i.e. polyOrder+1)
  virtual void setUseSinglePointBCForPHI(bool value, IndexType vertexIndexForZeroValue);

  std::vector<double> getPointForBCImposition();

  static Teuchos::RCP<ExactSolution<double>> poissonExactPolynomialSolution(int polyOrder, bool useConformingTraces = true);
};

#endif