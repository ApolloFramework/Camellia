#ifndef DPGTrilinos_Vectorized_BasisDef
#define DPGTrilinos_Vectorized_BasisDef


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

using namespace std;

namespace Intrepid {
  
  template<class Scalar, class ArrayScalar>
  Vectorized_Basis<Scalar, ArrayScalar>::Vectorized_Basis(Teuchos::RCP< Basis<Scalar, ArrayScalar> > basis, int numComponents)
  {
    this -> _componentBasis = basis;
    this -> _numComponents = numComponents;
    int componentCardinality = basis->getCardinality();
    this -> basisCardinality_  = numComponents * componentCardinality;
    this -> basisDegree_       = basis->getDegree();
    this -> basisCellTopology_ = basis->getBaseCellTopology();
    this -> basisType_         = basis->getBasisType();
    this -> basisCoordinates_  = basis->getCoordinateSystem();
  }
  
  template<class Scalar, class ArrayScalar>
  void Vectorized_Basis<Scalar, ArrayScalar>::initializeTags() {
    //_componentBasis->initializeTags();
    // TODO: find out whether we actually should do anything here!!
  }
  
  template<class Scalar, class ArrayScalar>
  void Vectorized_Basis<Scalar, ArrayScalar>::getValues(ArrayScalar &        outputValues,
                                                        const ArrayScalar &  inputPoints,
                                                        const EOperator      operatorType) const {
    Teuchos::Array<int> dimensions;
    outputValues.dimensions(dimensions);
    int numComponents = dimensions[dimensions.size() - 1];
    if (_numComponents != numComponents) {
      TEST_FOR_EXCEPTION ( _numComponents != numComponents, std::invalid_argument,
                          "final dimension of outputValues must match the number of vector components.");
    }
    // get rid of last dimension:
    dimensions.pop_back();
    // this is now the size of the array of vector-values.
    
    Teuchos::Array<int> componentDimensions = dimensions;
    int componentCardinality = _componentBasis->getCardinality();
    componentDimensions[0] = componentCardinality;
    
    ArrayScalar componentOutputValues(componentDimensions);
    _componentBasis->getValues(componentOutputValues,inputPoints,operatorType);
    
    int fieldIndex = 0;
    this->getVectorizedValues(outputValues,componentOutputValues,fieldIndex);
  }
  
  template<class Scalar, class ArrayScalar>
  void Vectorized_Basis<Scalar, ArrayScalar>::getVectorizedValues(ArrayScalar& outputValues,
                                                                  const ArrayScalar & componentOutputValues,
                                                                  int fieldIndex) const {
    //cout << "componentOutputValues: \n" << componentOutputValues;
    TEST_FOR_EXCEPTION( outputValues.dimension(fieldIndex) != this->basisCardinality_,
                       std::invalid_argument, "outputValues.dimension(fieldIndex) != this->basisCardinality_");
    TEST_FOR_EXCEPTION( componentOutputValues.dimension(fieldIndex) != _componentBasis->getCardinality(),
                       std::invalid_argument, "componentOutputValues.dimension(fieldIndex) != _componentBasis->getCardinality()");
    int pointIndex = fieldIndex+1;
    TEST_FOR_EXCEPTION( outputValues.dimension(pointIndex) != componentOutputValues.dimension(pointIndex),
                       std::invalid_argument, "outputValues.dimension(pointIndex) != componentOutputValues.dimension(pointIndex)");
    Teuchos::Array<int> dimensions;
    outputValues.dimensions(dimensions);
    outputValues.initialize(0.0);
    int numComponents = dimensions[dimensions.size() - 1];
    if (_numComponents != numComponents) {
      TEST_FOR_EXCEPTION ( _numComponents != numComponents, std::invalid_argument,
                          "final dimension of outputValues must match the number of vector components.");
    }
    int componentCardinality = _componentBasis->getCardinality();
    
    int numComponentOutputValues = componentOutputValues.size();
    int numVectorValues = outputValues.size() / _numComponents;
    
    Teuchos::Array< Teuchos::Array<int> > basisAddresses(this->basisCardinality_);
    
    for (int i=0; i<this->basisCardinality_; i++) {
      int compositeAddress = i;
      Teuchos::Array<int> compAddress(_numComponents,-1); // a vector each of whose entries corresponds to a field in the component basis, initialized to -1
      int liveComponentIndex = compositeAddress / componentCardinality;
      int componentField = compositeAddress % componentCardinality;
      compAddress[liveComponentIndex] = componentField;
      basisAddresses[i] = compAddress;
    }
    for (int i=0; i<numVectorValues; i++) {
      // the enumeration indices in outputValues will be i*_numComponents + componentIndex
      Teuchos::Array<int> multiIndex(outputValues.rank());
      outputValues.getMultiIndex(multiIndex, i*_numComponents);
      int basisIndex = multiIndex[fieldIndex];
      Teuchos::Array<int> componentMultiIndex = multiIndex;
      componentMultiIndex.pop_back(); // get rid of last, component dimension
      for (int compIndex=0; compIndex<_numComponents; compIndex++) {
        if (basisAddresses[basisIndex][compIndex] >= 0) {
          componentMultiIndex[fieldIndex] = basisAddresses[basisIndex][compIndex];
          //cout <<  "basisAddresses[basisIndex][compIndex]: " <<  basisAddresses[basisIndex][compIndex] << endl;
          outputValues[i*_numComponents+compIndex] = componentOutputValues.getValue(componentMultiIndex);
        } else {
          outputValues[i*_numComponents+compIndex] = 0.0;
        }
      }
    }
    //cout << "getVectorizedValues: componentOutputValues:\n" << componentOutputValues;
    //cout << "getVectorizedValues: outputValues:\n" << outputValues;
  }
  
  template<class Scalar, class ArrayScalar>
  void Vectorized_Basis<Scalar, ArrayScalar>::getValues(ArrayScalar&           outputValues,
                                                        const ArrayScalar &    inputPoints,
                                                        const ArrayScalar &    cellVertices,
                                                        const EOperator        operatorType) const {
    // TODO: implement this
  }
  
  template<class Scalar, class ArrayScalar>
  const Teuchos::RCP< Basis<Scalar, ArrayScalar> > Vectorized_Basis<Scalar, ArrayScalar>::getComponentBasis() const {
    return _componentBasis;
  }
  
}// namespace Intrepid

#endif