#ifndef MESH_TEST_SUITE
#define MESH_TEST_SUITE

/*
 *  MeshTestSuite.h
 *
 */

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

#include "DofOrderingFactory.h"
#include "Mesh.h"

#include "TestSuite.h"

using namespace Camellia;

class MeshTestSuite : public TestSuite
{
private:
//  static bool checkMeshDofConnectivities(Teuchos::RCP<Mesh> mesh);
  static bool checkDofOrderingHasNoOverlap(Teuchos::RCP<DofOrdering> dofOrdering);
  static bool vectorPairsEqual( vector< pair<unsigned,unsigned> > &first, vector< pair<unsigned,unsigned> > &second);
  static void printParities(Teuchos::RCP<Mesh> mesh);
  // checkDofOrderingHasNoOverlap returns true if no two (varID,basisOrdinal,sideIndex) tuples map to same dofIndex
public:
//  static bool checkMeshConsistency(Teuchos::RCP<Mesh> mesh);
  static bool neighborBasesAgreeOnSides(Teuchos::RCP<Mesh> mesh, const FieldContainer<double> &testPointsRefCoords,
                                        bool reportErrors = false);

  void runTests(int &numTestsRun, int &numTestsPassed);
  string testSuiteName()
  {
    return "MeshTestSuite";
  }
  static bool testBuildMesh();
  static bool testMeshSolvePointwise();
  static bool testExactSolution(bool checkL2Error);
  static bool testSacadoExactSolution();
  static bool testPoissonConvergence();
  static bool testBasisRefinement();
  static bool testFluxNorm();
  static bool testFluxIntegration();
  static bool testDofOrderingFactory();
  static bool testPRefinement();
  static bool testSinglePointBC();
  static bool testSolutionForMultipleElementTypes();
  static bool testSolutionForSingleElementUpgradedSide();
  static bool testHRefinement();
  static bool testHRefinementForConfusion();
  static bool testHUnrefinementForConfusion();
  static bool testRefinementPattern();
  static bool testPointContainment();

  // added by Jesse
  static bool testJesseMultiBasisRefinement();
  static bool testJesseAnisotropicRefinement();
  static bool testPRefinementAdjacentCells();
};

#endif
