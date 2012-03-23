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

#include "BilinearForm.h"
#include "BasisCache.h"
#include "ElementType.h"
#include "BilinearFormUtility.h"

#include "Intrepid_FunctionSpaceTools.hpp"

typedef Teuchos::RCP<DofOrdering> DofOrderingPtr;
typedef Teuchos::RCP<ElementType> ElementTypePtr;

static const string & S_OPERATOR_VALUE = "";
static const string & S_OPERATOR_GRAD = "\\nabla ";
static const string & S_OPERATOR_CURL = "\\nabla \\times ";
static const string & S_OPERATOR_DIV = "\\nabla \\cdot ";
static const string & S_OPERATOR_D1 = "D1 ";
static const string & S_OPERATOR_D2 = "D2 ";
static const string & S_OPERATOR_D3 = "D3 ";
static const string & S_OPERATOR_D4 = "D4 ";
static const string & S_OPERATOR_D5 = "D5 ";
static const string & S_OPERATOR_D6 = "D6 ";
static const string & S_OPERATOR_D7 = "D7 ";
static const string & S_OPERATOR_D8 = "D8 ";
static const string & S_OPERATOR_D9 = "D9 ";
static const string & S_OPERATOR_D10 = "D10 ";
static const string & S_OPERATOR_X = "{1 \\choose 0} \\cdot ";
static const string & S_OPERATOR_Y = "{0 \\choose 1} \\cdot ";
static const string & S_OPERATOR_Z = "\\bf{k} \\cdot ";
static const string & S_OPERATOR_DX = "\\frac{\\partial}{\\partial x} ";
static const string & S_OPERATOR_DY = "\\frac{\\partial}{\\partial y} ";
static const string & S_OPERATOR_DZ = "\\frac{\\partial}{\\partial z} ";
static const string & S_OPERATOR_CROSS_NORMAL = "\\times \\widehat{n} ";
static const string & S_OPERATOR_DOT_NORMAL = "\\cdot \\widehat{n} ";
static const string & S_OPERATOR_TIMES_NORMAL = " \\widehat{n} \\cdot ";
static const string & S_OPERATOR_TIMES_NORMAL_X = " \\widehat{n}_x ";
static const string & S_OPERATOR_TIMES_NORMAL_Y = " \\widehat{n}_y ";
static const string & S_OPERATOR_TIMES_NORMAL_Z = " \\widehat{n}_z ";
static const string & S_OPERATOR_VECTORIZE_VALUE = ""; // handle this one separately...
static const string & S_OPERATOR_UNKNOWN = "[UNKNOWN OPERATOR] ";

set<int> BilinearForm::_normalOperators;

const vector< int > & BilinearForm::trialIDs() {
  return _trialIDs;
}

const vector< int > & BilinearForm::testIDs() {
  return _testIDs;
}

void BilinearForm::applyBilinearFormData(FieldContainer<double> &trialValues, FieldContainer<double> &testValues, 
                                         int trialID, int testID, int operatorIndex,
                                         const FieldContainer<double> &points) {
  applyBilinearFormData(trialID,testID,trialValues,testValues,points);
}

void BilinearForm::applyBilinearFormData(FieldContainer<double> &trialValues, FieldContainer<double> &testValues, 
                                         int trialID, int testID, int operatorIndex,
                                         Teuchos::RCP<BasisCache> basisCache) {
  applyBilinearFormData(trialValues, testValues, trialID, testID, operatorIndex, basisCache->getPhysicalCubaturePoints());
}

void BilinearForm::trialTestOperators(int testID1, int testID2, 
                                      vector<EOperatorExtended> &testOps1,
                                      vector<EOperatorExtended> &testOps2) {
  EOperatorExtended testOp1, testOp2;
  testOps1.clear();
  testOps2.clear();
  if (trialTestOperator(testID1,testID2,testOp1,testOp2)) {
    testOps1.push_back(testOp1);
    testOps2.push_back(testOp2);
  }
}

void BilinearForm::multiplyFCByWeight(FieldContainer<double> & fc, double weight) {
  int size = fc.size();
  double *valuePtr = &fc[0]; // to make this as fast as possible, do some pointer arithmetic...
  for (int i=0; i<size; i++) {
    *valuePtr *= weight;
    valuePtr++;
  }
}

void BilinearForm::stiffnessMatrix(FieldContainer<double> &stiffness, ElementTypePtr elemType,
                                   FieldContainer<double> &cellSideParities, Teuchos::RCP<BasisCache> basisCache) {

  DofOrderingPtr testOrdering  = elemType->testOrderPtr;
  DofOrderingPtr trialOrdering = elemType->trialOrderPtr;
  stiffnessMatrix(stiffness,trialOrdering,testOrdering,cellSideParities,basisCache);

}

void BilinearForm::stiffnessMatrix(FieldContainer<double> &stiffness, Teuchos::RCP<DofOrdering> trialOrdering, 
                                     Teuchos::RCP<DofOrdering> testOrdering,
                                     FieldContainer<double> &cellSideParities, Teuchos::RCP<BasisCache> basisCache) {
    
  // stiffness dimensions are: (numCells, # testOrdering Dofs, # trialOrdering Dofs)
  // (while (cell,trial,test) is more natural conceptually, I believe the above ordering makes
  //  more sense given the inversion that we must do to compute the optimal test functions...)
  
  // steps:
  // 0. Set up Cubature
  // 1. Determine Jacobians
  // 2. Determine quadrature points on interior and boundary
  // 3. For each (test, trial) combination:
  //   a. Apply the specified operators to the basis in the DofOrdering, at the cubature points
  //   b. Multiply the two bases together, weighted with Jacobian/Piola transform and cubature weights
  //   c. Pass the result to bilinearForm's applyBilinearFormData method
  //   d. Sum up (integrate) and place in stiffness matrix according to DofOrdering indices
  
  // check inputs
  int numTestDofs = testOrdering->totalDofs();
  int numTrialDofs = trialOrdering->totalDofs();
  
  shards::CellTopology cellTopo = basisCache->cellTopology();
  unsigned numCells = basisCache->getPhysicalCubaturePoints().dimension(0);
  unsigned spaceDim = cellTopo.getDimension();
  
  //cout << "trialOrdering: " << *trialOrdering;
  //cout << "testOrdering: " << *testOrdering;
  
  // check stiffness dimensions:
  TEST_FOR_EXCEPTION( ( numCells != stiffness.dimension(0) ),
                     std::invalid_argument,
                     "numCells and stiffness.dimension(0) do not match.");
  TEST_FOR_EXCEPTION( ( numTestDofs != stiffness.dimension(1) ),
                     std::invalid_argument,
                     "numTestDofs and stiffness.dimension(1) do not match.");
  TEST_FOR_EXCEPTION( ( numTrialDofs != stiffness.dimension(2) ),
                     std::invalid_argument,
                     "numTrialDofs and stiffness.dimension(2) do not match.");
  
  // 0. Set up BasisCache
  int cubDegreeTrial = trialOrdering->maxBasisDegree();
  int cubDegreeTest = testOrdering->maxBasisDegree();
  int cubDegree = cubDegreeTrial + cubDegreeTest;
  
  unsigned numSides = cellTopo.getSideCount();
  
  // 3. For each (test, trial) combination:
  vector<int> testIDs = this->testIDs();
  vector<int>::iterator testIterator;
  
  vector<int> trialIDs = this->trialIDs();
  vector<int>::iterator trialIterator;
  
  Teuchos::RCP < Intrepid::Basis<double,FieldContainer<double> > > trialBasis;
  Teuchos::RCP < Intrepid::Basis<double,FieldContainer<double> > > testBasis;
  
  stiffness.initialize(0.0);
  
  for (testIterator = testIDs.begin(); testIterator != testIDs.end(); testIterator++) {
    int testID = *testIterator;
    
    for (trialIterator = trialIDs.begin(); trialIterator != trialIDs.end(); trialIterator++) {
      int trialID = *trialIterator;
      
      vector<EOperatorExtended> trialOperators, testOperators;
      this->trialTestOperators(trialID, testID, trialOperators, testOperators);
      vector<EOperatorExtended>::iterator trialOpIt, testOpIt;
      testOpIt = testOperators.begin();
      TEST_FOR_EXCEPTION(trialOperators.size() != testOperators.size(), std::invalid_argument,
                         "trialOperators and testOperators must be the same length");
      int operatorIndex = -1;
      for (trialOpIt = trialOperators.begin(); trialOpIt != trialOperators.end(); trialOpIt++) {
        operatorIndex++;
        EOperatorExtended trialOperator = *trialOpIt;
        EOperatorExtended testOperator = *testOpIt;
        
        if (testOperator==OPERATOR_TIMES_NORMAL) {
          TEST_FOR_EXCEPTION(true,std::invalid_argument,"OPERATOR_TIMES_NORMAL not supported for tests.  Use for trial only");
        }
        
        Teuchos::RCP < const FieldContainer<double> > testValuesTransformed;
        Teuchos::RCP < const FieldContainer<double> > trialValuesTransformed;
        Teuchos::RCP < const FieldContainer<double> > testValuesTransformedWeighted;
        
        //cout << "trial is " <<  this->trialName(trialID) << "; test is " << this->testName(testID) << endl;
        
        if (! this->isFluxOrTrace(trialID)) {
          trialBasis = trialOrdering->getBasis(trialID);
          testBasis = testOrdering->getBasis(testID);
          
          FieldContainer<double> miniStiffness( numCells, testBasis->getCardinality(), trialBasis->getCardinality() );
          
          trialValuesTransformed = basisCache->getTransformedValues(trialBasis,trialOperator);
          testValuesTransformedWeighted = basisCache->getTransformedWeightedValues(testBasis,testOperator);
          
          FieldContainer<double> physicalCubaturePoints = basisCache->getPhysicalCubaturePoints();
          FieldContainer<double> materialDataAppliedToTrialValues = *trialValuesTransformed; // copy first
          FieldContainer<double> materialDataAppliedToTestValues = *testValuesTransformedWeighted; // copy first
          this->applyBilinearFormData(materialDataAppliedToTrialValues, materialDataAppliedToTestValues,
                                              trialID,testID,operatorIndex,basisCache);
          
          //integrate:
          FunctionSpaceTools::integrate<double>(miniStiffness,materialDataAppliedToTestValues,materialDataAppliedToTrialValues,COMP_CPP);
          // place in the appropriate spot in the element-stiffness matrix
          // copy goes from (cell,trial_basis_dof,test_basis_dof) to (cell,element_trial_dof,element_test_dof)
          
          //cout << "miniStiffness for volume:\n" << miniStiffness;
          
          //checkForZeroRowsAndColumns("miniStiffness for pre-stiffness", miniStiffness);
          
          //cout << "trialValuesTransformed for trial " << this->trialName(trialID) << endl << trialValuesTransformed
          //cout << "testValuesTransformed for test " << this->testName(testID) << ": \n" << testValuesTransformed;
          //cout << "weightedMeasure:\n" << weightedMeasure;
          
          // there may be a more efficient way to do this copying:
          // (one strategy would be to reimplement fst::integrate to support offsets, so that no copying needs to be done...)
          for (int i=0; i < testBasis->getCardinality(); i++) {
            int testDofIndex = testOrdering->getDofIndex(testID,i);
            for (int j=0; j < trialBasis->getCardinality(); j++) {
              int trialDofIndex = trialOrdering->getDofIndex(trialID,j);
              for (unsigned k=0; k < numCells; k++) {
                stiffness(k,testDofIndex,trialDofIndex) += miniStiffness(k,i,j);
              }
            }
          }          
        } else {  // boundary integral
          int trialBasisRank = trialOrdering->getBasisRank(trialID);
          int testBasisRank = testOrdering->getBasisRank(testID);
          
          TEST_FOR_EXCEPTION( ( trialBasisRank != 0 ),
                             std::invalid_argument,
                             "Boundary trial variable (flux or trace) given with non-scalar basis.  Unsupported.");
          
          for (unsigned sideOrdinal=0; sideOrdinal<numSides; sideOrdinal++) {
            trialBasis = trialOrdering->getBasis(trialID,sideOrdinal);
            testBasis = testOrdering->getBasis(testID);
            
            bool isFlux = false; // i.e. the normal is "folded into" the variable definition, so that we must take parity into account
            const set<int> normalOperators = BilinearForm::normalOperators();
            if (   (normalOperators.find(testOperator)  == normalOperators.end() ) 
                && (normalOperators.find(trialOperator) == normalOperators.end() ) ) {
              // normal not yet taken into account -- so it must be "hidden" in the trial variable
              isFlux = true;
            }
            
            FieldContainer<double> miniStiffness( numCells, testBasis->getCardinality(), trialBasis->getCardinality() );    
            
            // for trial: the value lives on the side, so we don't use the volume coords either:
            trialValuesTransformed = basisCache->getTransformedValues(trialBasis,trialOperator,sideOrdinal,false);
            // for test: do use the volume coords:
            testValuesTransformed = basisCache->getTransformedValues(testBasis,testOperator,sideOrdinal,true);
            // 
            testValuesTransformedWeighted = basisCache->getTransformedWeightedValues(testBasis,testOperator,sideOrdinal,true);
            
            // copy before manipulating trialValues--these are the ones stored in the cache, so we're not allowed to change them!!
            FieldContainer<double> materialDataAppliedToTrialValues = *trialValuesTransformed;
            
            if (isFlux) {
              // we need to multiply the trialValues by the parity of the normal, since
              // the trial implicitly contains an outward normal, and we need to adjust for the fact
              // that the neighboring cells have opposite normal
              // trialValues should have dimensions (numCells,numFields,numCubPointsSide)
              int numFields = trialValuesTransformed->dimension(1);
              int numPoints = trialValuesTransformed->dimension(2);
              for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
                double parity = cellSideParities(cellIndex,sideOrdinal);
                if (parity != 1.0) {  // otherwise, we can just leave things be...
                  for (int fieldIndex=0; fieldIndex<numFields; fieldIndex++) {
                    for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
                      materialDataAppliedToTrialValues(cellIndex,fieldIndex,ptIndex) *= parity;
                    }
                  }
                }
              }
            }
            
            FieldContainer<double> cubPointsSidePhysical = basisCache->getPhysicalCubaturePointsForSide(sideOrdinal);
            FieldContainer<double> materialDataAppliedToTestValues = *testValuesTransformedWeighted; // copy first
            this->applyBilinearFormData(materialDataAppliedToTrialValues,materialDataAppliedToTestValues,
                                                trialID,testID,operatorIndex,basisCache);
            
            
            //cout << "sideOrdinal: " << sideOrdinal << "; cubPointsSidePhysical" << endl << cubPointsSidePhysical;
            
            //   d. Sum up (integrate) and place in stiffness matrix according to DofOrdering indices
            FunctionSpaceTools::integrate<double>(miniStiffness,materialDataAppliedToTestValues,materialDataAppliedToTrialValues,COMP_CPP);
            
            //checkForZeroRowsAndColumns("side miniStiffness for pre-stiffness", miniStiffness);
            
            //cout << "miniStiffness for side " << sideOrdinal << "\n:" << miniStiffness;
            // place in the appropriate spot in the element-stiffness matrix
            // copy goes from (cell,trial_basis_dof,test_basis_dof) to (cell,element_trial_dof,element_test_dof)
            for (int i=0; i < testBasis->getCardinality(); i++) {
              int testDofIndex = testOrdering->getDofIndex(testID,i,0);
              for (int j=0; j < trialBasis->getCardinality(); j++) {
                int trialDofIndex = trialOrdering->getDofIndex(trialID,j,sideOrdinal);
                for (unsigned k=0; k < numCells; k++) {
                  stiffness(k,testDofIndex,trialDofIndex) += miniStiffness(k,i,j);
                }
              }
            }
          }
        }
        testOpIt++;
      }
    }
  }
  //cout << "trialOrdering: \n" << *trialOrdering;
  //cout << "testOrdering: \n" << *testOrdering;
  BilinearFormUtility::checkForZeroRowsAndColumns("pre-stiffness", stiffness);
}

vector<int> BilinearForm::trialVolumeIDs() {
  vector<int> ids;
  for (vector<int>::iterator trialIt = _trialIDs.begin(); trialIt != _trialIDs.end(); trialIt++) {
    int trialID = *(trialIt);
    if ( ! isFluxOrTrace(trialID) ) {
      ids.push_back(trialID);
    }
  }
  return ids;
}

vector<int> BilinearForm::trialBoundaryIDs() {
  vector<int> ids;
  for (vector<int>::iterator trialIt = _trialIDs.begin(); trialIt != _trialIDs.end(); trialIt++) {
    int trialID = *(trialIt);
    if ( isFluxOrTrace(trialID) ) {
      ids.push_back(trialID);
    }
  }
  return ids;  
}

int BilinearForm::operatorRank(EOperatorExtended op, EFunctionSpaceExtended fs) {
  // returns the rank of basis functions in the function space fs when op is applied
  // 0 scalar, 1 vector
  int SCALAR = 0, VECTOR = 1;
  switch (op) {
    case IntrepidExtendedTypes::OPERATOR_VALUE:
      if (   (fs == IntrepidExtendedTypes::FUNCTION_SPACE_HGRAD) 
          || (fs == IntrepidExtendedTypes::FUNCTION_SPACE_HVOL)
          || (fs == IntrepidExtendedTypes::FUNCTION_SPACE_ONE) )
        return SCALAR; 
      else
        return VECTOR;
    case IntrepidExtendedTypes::OPERATOR_GRAD:
    case IntrepidExtendedTypes::OPERATOR_CURL:
      return VECTOR;
    case IntrepidExtendedTypes::OPERATOR_DIV:
    case IntrepidExtendedTypes::OPERATOR_X:
    case IntrepidExtendedTypes::OPERATOR_Y:
    case IntrepidExtendedTypes::OPERATOR_Z:
    case IntrepidExtendedTypes::OPERATOR_DX:
    case IntrepidExtendedTypes::OPERATOR_DY:
    case IntrepidExtendedTypes::OPERATOR_DZ:
      return SCALAR; 
    case IntrepidExtendedTypes::OPERATOR_CROSS_NORMAL:
      return VECTOR; 
    case IntrepidExtendedTypes::OPERATOR_DOT_NORMAL:
      return SCALAR; 
    case IntrepidExtendedTypes::OPERATOR_TIMES_NORMAL:
      return VECTOR; 
    case IntrepidExtendedTypes::OPERATOR_TIMES_NORMAL_X:
      return SCALAR; 
    case IntrepidExtendedTypes::OPERATOR_TIMES_NORMAL_Y:
      return SCALAR; 
    case IntrepidExtendedTypes::OPERATOR_TIMES_NORMAL_Z:
      return SCALAR; 
    case IntrepidExtendedTypes::OPERATOR_VECTORIZE_VALUE:
      return VECTOR;
    default:
      return -1;
  }
}

const string & BilinearForm::operatorName(EOperatorExtended op) {
  switch (op) {
    case IntrepidExtendedTypes::OPERATOR_VALUE:
      return S_OPERATOR_VALUE; 
      break;
    case IntrepidExtendedTypes::OPERATOR_GRAD:
      return S_OPERATOR_GRAD; 
      break;
    case IntrepidExtendedTypes::OPERATOR_CURL:
      return S_OPERATOR_CURL; 
      break;
    case IntrepidExtendedTypes::OPERATOR_DIV:
      return S_OPERATOR_DIV; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D1:
      return S_OPERATOR_D1; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D2:
      return S_OPERATOR_D2; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D3:
      return S_OPERATOR_D3; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D4:
      return S_OPERATOR_D4; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D5:
      return S_OPERATOR_D5; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D6:
      return S_OPERATOR_D6; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D7:
      return S_OPERATOR_D7; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D8:
      return S_OPERATOR_D8; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D9:
      return S_OPERATOR_D9; 
      break;
    case IntrepidExtendedTypes::OPERATOR_D10:
      return S_OPERATOR_D10; 
      break;
    case IntrepidExtendedTypes::OPERATOR_X:
      return S_OPERATOR_X; 
      break;
    case IntrepidExtendedTypes::OPERATOR_Y:
      return S_OPERATOR_Y; 
      break;
    case IntrepidExtendedTypes::OPERATOR_Z:
      return S_OPERATOR_Z; 
      break;
    case IntrepidExtendedTypes::OPERATOR_DX:
      return S_OPERATOR_DX; 
      break;
    case IntrepidExtendedTypes::OPERATOR_DY:
      return S_OPERATOR_DY; 
      break;
    case IntrepidExtendedTypes::OPERATOR_DZ:
      return S_OPERATOR_DZ; 
      break;
    case IntrepidExtendedTypes::OPERATOR_CROSS_NORMAL:
      return S_OPERATOR_CROSS_NORMAL; 
      break;
    case IntrepidExtendedTypes::OPERATOR_DOT_NORMAL:
      return S_OPERATOR_DOT_NORMAL; 
      break;
    case IntrepidExtendedTypes::OPERATOR_TIMES_NORMAL:
      return S_OPERATOR_TIMES_NORMAL; 
      break;
    case IntrepidExtendedTypes::OPERATOR_TIMES_NORMAL_X:
      return S_OPERATOR_TIMES_NORMAL_X; 
      break;
    case IntrepidExtendedTypes::OPERATOR_TIMES_NORMAL_Y:
      return S_OPERATOR_TIMES_NORMAL_Y; 
      break;
    case IntrepidExtendedTypes::OPERATOR_TIMES_NORMAL_Z:
      return S_OPERATOR_TIMES_NORMAL_Z; 
      break;
    case IntrepidExtendedTypes::OPERATOR_VECTORIZE_VALUE:
      return S_OPERATOR_VECTORIZE_VALUE; 
      break;
    default:
      return S_OPERATOR_UNKNOWN;
      break;
  }
}

void BilinearForm::printTrialTestInteractions() {
  for (vector<int>::iterator testIt = _testIDs.begin(); testIt != _testIDs.end(); testIt++) {
    int testID = *testIt;
    cout << endl << "b(U," << testName(testID) << ") &= " << endl;
    bool first = true;
    int spaceDim = 2;
    FieldContainer<double> point(1,2); // (0,0)
    FieldContainer<double> testValueScalar(1,1,1); // 1 cell, 1 basis function, 1 point...
    FieldContainer<double> testValueVector(1,1,1,spaceDim); // 1 cell, 1 basis function, 1 point, spaceDim dimensions...
    FieldContainer<double> trialValueScalar(1,1,1); // 1 cell, 1 basis function, 1 point...
    FieldContainer<double> trialValueVector(1,1,1,spaceDim); // 1 cell, 1 basis function, 1 point, spaceDim dimensions...
    FieldContainer<double> testValue, trialValue;
    for (vector<int>::iterator trialIt = _trialIDs.begin(); trialIt != _trialIDs.end(); trialIt++) {
      int trialID = *trialIt;
      vector<EOperatorExtended> trialOperators, testOperators;
      trialTestOperators(trialID, testID, trialOperators, testOperators);
      vector<EOperatorExtended>::iterator trialOpIt, testOpIt;
      testOpIt = testOperators.begin();
      int operatorIndex = 0;
      for (trialOpIt = trialOperators.begin(); trialOpIt != trialOperators.end(); trialOpIt++) {
        EOperatorExtended opTrial = *trialOpIt;
        EOperatorExtended opTest = *testOpIt;
        int trialRank = operatorRank(opTrial, functionSpaceForTrial(trialID));
        int testRank = operatorRank(opTest, functionSpaceForTest(testID));
        trialValue = ( trialRank == 0 ) ? trialValueScalar : trialValueVector;
        testValue = (testRank == 0) ? testValueScalar : testValueVector;
        
        trialValue[0] = 1.0; testValue[0] = 1.0;
        FieldContainer<double> testWeight(1), trialWeight(1); // for storing values that come back from applyBilinearForm
        applyBilinearFormData(trialValue,testValue,trialID,testID,operatorIndex,point);
        if ((trialRank==1) && (trialValue.rank() == 3)) { // vector that became a scalar (a dot product)
          trialWeight.resize(spaceDim);
          trialWeight[0] = trialValue[0];
          for (int dim=1; dim<spaceDim; dim++) {
            trialValue = trialValueVector;
            trialValue.initialize(0.0);
            testValue = (testRank == 0) ? testValueScalar : testValueVector;
            trialValue[dim] = 1.0;
            applyBilinearFormData(trialValue,testValue,trialID,testID,operatorIndex,point);
            trialWeight[dim] = trialValue[0];
          }
        } else {
          trialWeight[0] = trialValue[0];
        }
        // same thing, but now for testWeight
        if ((testRank==1) && (testValue.rank() == 3)) { // vector that became a scalar (a dot product)
          testWeight.resize(spaceDim);
          testWeight[0] = trialValue[0];
          for (int dim=1; dim<spaceDim; dim++) {
            testValue = testValueVector;
            testValue.initialize(0.0);
            trialValue = (trialRank == 0) ? trialValueScalar : trialValueVector;
            testValue[dim] = 1.0;
            applyBilinearFormData(trialValue,testValue,trialID,testID,operatorIndex,point);
            testWeight[dim] = testValue[0];
          }
        } else {
          testWeight[0] = testValue[0];
        }
        if ((testWeight.size() == 2) && (trialWeight.size() == 2)) { // both vector values (unsupported)
          TEST_FOR_EXCEPTION( true, std::invalid_argument, "unsupported form." );
        } else {
          // scalar & vector: combine into one, in testWeight
          if ( (trialWeight.size() + testWeight.size()) == 3) {
            FieldContainer<double> smaller = (trialWeight.size()==1) ? trialWeight : testWeight;
            FieldContainer<double> bigger =  (trialWeight.size()==2) ? trialWeight : testWeight;
            testWeight.resize(spaceDim);
            for (int dim=0; dim<spaceDim; dim++) {
              testWeight[dim] = smaller[0] * bigger[dim];
            }
          } else { // both scalars: combine into one, in testWeight
            testWeight[0] *= trialWeight[0];
          }
        }
        if (testWeight.size() == 1) { // scalar weight
          if ( testWeight[0] == -1.0 ) {
            cout << " - ";
          } else {
            if (testWeight[0] == 1.0) {
              if (! first) cout << " + ";
            } else {
              if (testWeight[0] < 0.0) {
                cout << testWeight[0] << " ";
              } else {
                cout << " + " << testWeight[0] << " ";
              }
            }
          }
          if (! isFluxOrTrace(trialID) ) {
            cout << "\\int_{K} " ;
          } else {
            cout << "\\int_{\\partial K} " ;            
          }
          cout << operatorName(opTrial) << trialName(trialID) << " ";
        } else { // 
          if (! first) cout << " + ";
          if (! isFluxOrTrace(trialID) ) {
            cout << "\\int_{K} " ;
          } else {
            cout << "\\int_{\\partial K} " ;
          }
          if (opTrial != OPERATOR_TIMES_NORMAL) {
            cout << " \\begin{bmatrix}";
            for (int dim=0; dim<spaceDim; dim++) {
              if (testWeight[dim] != 1.0) {
                cout << testWeight[0];
              }
              if (dim != spaceDim-1) {
                cout << " \\\\ ";
              }
            }
            cout << "\\end{bmatrix} ";
            cout << trialName(trialID);
            cout << " \\cdot ";
          } else if (opTrial == OPERATOR_TIMES_NORMAL) {
            if (testWeight.size() == 2) {
              cout << " {";
              if (testWeight[0] != 1.0) {
                cout << testWeight[0];
              }
              cout << " n_1 " << " \\choose ";
              if (testWeight[1] != 1.0) {
                cout << testWeight[1];
              }
              cout << " n_2 " << "} " << trialName(trialID) << " \\cdot ";
            } else {
              if (testWeight[0] != 1.0) {
                cout << testWeight[0] << " " << trialName(trialID) << operatorName(opTrial);
              } else {
                cout << trialName(trialID) << operatorName(opTrial);
              }
            }
          }
        }
        if ((opTest == OPERATOR_CROSS_NORMAL) || (opTest == OPERATOR_DOT_NORMAL)) {
          // reverse the order:
          cout << testName(testID) << operatorName(opTest);
        } else {
          cout << operatorName(opTest) << testName(testID);
        }
        first = false;
        testOpIt++;
        operatorIndex++;
      }
    }
    cout << endl << "\\\\";
  }
}

const set<int> & BilinearForm::normalOperators() {
  if (_normalOperators.size() == 0) {
    _normalOperators.insert(OPERATOR_CROSS_NORMAL);
    _normalOperators.insert(OPERATOR_DOT_NORMAL);
    _normalOperators.insert(OPERATOR_TIMES_NORMAL);
    _normalOperators.insert(OPERATOR_TIMES_NORMAL_X);
    _normalOperators.insert(OPERATOR_TIMES_NORMAL_Y);
    _normalOperators.insert(OPERATOR_TIMES_NORMAL_Z);
  }
  return _normalOperators;
}