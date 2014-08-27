//
//  CondensedDofInterpreter.cpp
//  Camellia-debug
//
//  Created by Nate Roberts on 2/6/14.
//
//


#include "CondensedDofInterpreter.h"

#include "Epetra_SerialDenseSolver.h"
#include "Epetra_SerialSymDenseMatrix.h"
#include "Epetra_SerialSpdDenseSolver.h"
#include "Epetra_DataAccess.h"

#include <Teuchos_GlobalMPISession.hpp>
#include "MPIWrapper.h"

#include "Epetra_SerialComm.h"

#include "GlobalDofAssignment.h"

#include "CamelliaDebugUtility.h"

CondensedDofInterpreter::CondensedDofInterpreter(Mesh* mesh, LagrangeConstraints* lagrangeConstraints, const set<int> &fieldIDsToExclude, bool storeLocalStiffnessMatrices) {
  _mesh = mesh;
  _lagrangeConstraints = lagrangeConstraints;
  _storeLocalStiffnessMatrices = storeLocalStiffnessMatrices;
  _uncondensibleVarIDs.insert(fieldIDsToExclude.begin(),fieldIDsToExclude.end());
  
  int numGlobalConstraints = lagrangeConstraints->numGlobalConstraints();
  for (int i=0; i<numGlobalConstraints; i++) {
    set<int> constrainedVars = lagrangeConstraints->getGlobalConstraint(i).linearTerm()->varIDs();
    _uncondensibleVarIDs.insert(constrainedVars.begin(), constrainedVars.end());
  }
  
  int numElementConstraints = lagrangeConstraints->numElementConstraints();
  for (int i=0; i<numElementConstraints; i++) {
    set<int> constrainedVars = lagrangeConstraints->getElementConstraint(i).linearTerm()->varIDs();
    _uncondensibleVarIDs.insert(constrainedVars.begin(), constrainedVars.end());
  }
  
  initializeGlobalDofIndices();
}

void CondensedDofInterpreter::getSubmatrices(set<int> fieldIndices, set<int> fluxIndices,
                                             const FieldContainer<double> &K, Epetra_SerialDenseMatrix &K_field,
                                             Epetra_SerialDenseMatrix &K_coupl, Epetra_SerialDenseMatrix &K_flux) {
  int numFieldDofs = fieldIndices.size();
  int numFluxDofs = fluxIndices.size();
  K_field.Reshape(numFieldDofs,numFieldDofs);
  K_flux.Reshape(numFluxDofs,numFluxDofs);
  K_coupl.Reshape(numFieldDofs,numFluxDofs); // upper right hand corner matrix - symmetry gets the other
  
  set<int>::iterator dofIt1;
  set<int>::iterator dofIt2;
  
  int i,j,j_flux,j_field;
  i = 0;
  for (dofIt1 = fieldIndices.begin();dofIt1!=fieldIndices.end();dofIt1++){
    int rowInd = *dofIt1;
    j_flux = 0;
    j_field = 0;
    
    // get block field matrices
    for (dofIt2 = fieldIndices.begin();dofIt2!=fieldIndices.end();dofIt2++){
      int colInd = *dofIt2;
      //      cout << "rowInd, colInd = " << rowInd << ", " << colInd << endl;
      K_field(i,j_field) = K(rowInd,colInd);
      j_field++;
    }
    
    // get field/flux couplings
    for (dofIt2 = fluxIndices.begin();dofIt2!=fluxIndices.end();dofIt2++){
      int colInd = *dofIt2;
      K_coupl(i,j_flux) = K(rowInd,colInd);
      j_flux++;
    }
    i++;
  }
  
  // get flux coupling terms
  i = 0;
  for (dofIt1 = fluxIndices.begin();dofIt1!=fluxIndices.end();dofIt1++){
    int rowInd = *dofIt1;
    j = 0;
    for (dofIt2 = fluxIndices.begin();dofIt2!=fluxIndices.end();dofIt2++){
      int colInd = *dofIt2;
      K_flux(i,j) = K(rowInd,colInd);
      j++;
    }
    i++;
  }
}

void CondensedDofInterpreter::getSubvectors(set<int> fieldIndices, set<int> fluxIndices, const FieldContainer<double> &b, Epetra_SerialDenseVector &b_field, Epetra_SerialDenseVector &b_flux){
  
  int numFieldDofs = fieldIndices.size();
  int numFluxDofs = fluxIndices.size();
  
  b_field.Resize(numFieldDofs);
  b_flux.Resize(numFluxDofs);
  set<int>::iterator dofIt;
  int i;
  i = 0;
  for (dofIt=fieldIndices.begin();dofIt!=fieldIndices.end();dofIt++){
    int ind = *dofIt;
    b_field(i) = b(ind);
    i++;
  }
  i = 0;
  for (dofIt=fluxIndices.begin();dofIt!=fluxIndices.end();dofIt++){
    int ind = *dofIt;
    b_flux(i) = b(ind);
    i++;
  }
}

set<GlobalIndexType> CondensedDofInterpreter::globalDofIndicesForCell(GlobalIndexType cellID)  {
  set<GlobalIndexType> interpretedDofIndicesForCell = _mesh->globalDofIndicesForCell(cellID);
  set<GlobalIndexType> globalDofIndicesForCell;

  for (set<GlobalIndexType>::iterator interpretedDofIndexIt = interpretedDofIndicesForCell.begin();
       interpretedDofIndexIt != interpretedDofIndicesForCell.end(); interpretedDofIndexIt++) {
    GlobalIndexType interpretedDofIndex = *interpretedDofIndexIt;
    if (_interpretedToGlobalDofIndexMap.find(interpretedDofIndex) == _interpretedToGlobalDofIndexMap.end()) {
     // that's OK; we skip the fields...
    } else {
      GlobalIndexType globalDofIndex = _interpretedToGlobalDofIndexMap[interpretedDofIndex];
      globalDofIndicesForCell.insert(globalDofIndex);
    }
  }
  
  return globalDofIndicesForCell;
}

bool CondensedDofInterpreter::varDofsAreCondensible(int varID, int sideOrdinal, DofOrderingPtr dofOrdering) {
  int sideCount = dofOrdering->getNumSidesForVarID(varID);
  BasisPtr basis = dofOrdering->getBasis(varID, sideOrdinal);
  
  IntrepidExtendedTypes::EFunctionSpaceExtended fs = basis->functionSpace();
  
  bool isL2 =  (fs==IntrepidExtendedTypes::FUNCTION_SPACE_HVOL)
  || (fs==IntrepidExtendedTypes::FUNCTION_SPACE_VECTOR_HVOL)
  || (fs==IntrepidExtendedTypes::FUNCTION_SPACE_TENSOR_HVOL);
  
  return (isL2) && (sideCount==1) && (_uncondensibleVarIDs.find(varID) == _uncondensibleVarIDs.end());
}

map<GlobalIndexType, IndexType> CondensedDofInterpreter::interpretedFluxMapForPartition(PartitionIndexType partition, bool storeFluxDofIndices) { // add the partitionDofOffset to get the globalDofIndices
  
  map<GlobalIndexType, IndexType> interpretedFluxMap; // from the interpreted dofs (the global dof indices as seen by mesh) to the partition-local condensed IDs
  
  set< GlobalIndexType > localCellIDs = _mesh->globalDofAssignment()->cellsInPartition(partition);
  
  set<GlobalIndexType> interpretedFluxDofs;
  
  set<GlobalIndexType> interpretedDofIndicesForPartition = _mesh->globalDofIndicesForPartition(partition);
  
  vector<int> trialIDs = _mesh->bilinearForm()->trialIDs();
  set< GlobalIndexType >::iterator cellIDIt;
  
  IndexType partitionLocalDofIndex = 0;
  
  for (cellIDIt=localCellIDs.begin(); cellIDIt!=localCellIDs.end(); cellIDIt++){
    GlobalIndexType cellID = *cellIDIt;
    
    DofOrderingPtr trialOrder = _mesh->getElementType(cellID)->trialOrderPtr;
    
    for (vector<int>::iterator idIt = trialIDs.begin();idIt!=trialIDs.end();idIt++){
      int trialID = *idIt;
      int numSides = trialOrder->getNumSidesForVarID(trialID);
      
      for (int sideOrdinal=0; sideOrdinal<numSides; sideOrdinal++) {
        BasisPtr basis = trialOrder->getBasis(trialID, sideOrdinal);
        
        FieldContainer<double> dummyLocalBasisData(basis->getCardinality());
        FieldContainer<double> dummyGlobalData;
        FieldContainer<GlobalIndexType> interpretedDofIndicesForBasis;
        vector< int > localDofIndicesForBasis = trialOrder->getDofIndices(trialID,sideOrdinal);
        
        _mesh->interpretLocalBasisCoefficients(cellID, trialID, sideOrdinal, dummyLocalBasisData, dummyGlobalData, interpretedDofIndicesForBasis);
        
        if (storeFluxDofIndices) {
          pair< int, int > basisIdentifier = make_pair(trialID,sideOrdinal);
          _interpretedDofIndicesForBasis[cellID][basisIdentifier] = interpretedDofIndicesForBasis;
        }
        
        bool isCondensible = varDofsAreCondensible(trialID, sideOrdinal, trialOrder);
        
        for (int dofOrdinal=0; dofOrdinal < interpretedDofIndicesForBasis.size(); dofOrdinal++) {
          GlobalIndexType interpretedDofIndex = interpretedDofIndicesForBasis(dofOrdinal);
          
          bool isOwnedByThisPartition = (interpretedDofIndicesForPartition.find(interpretedDofIndex) != interpretedDofIndicesForPartition.end());
          
          if (!isCondensible) {
            if (storeFluxDofIndices) {
              _interpretedFluxDofIndices.insert(interpretedDofIndex);
            }
          }
          
          if (isOwnedByThisPartition && !isCondensible) {
            if (interpretedFluxDofs.find(interpretedDofIndex) == interpretedFluxDofs.end()) {
              interpretedFluxMap[interpretedDofIndex] = partitionLocalDofIndex++;
              interpretedFluxDofs.insert(interpretedDofIndex);
              
//              cout << interpretedDofIndex << " --> " << interpretedFluxMap[interpretedDofIndex] << endl;
            }
          }
        }
      }
    }
  }
  
  return interpretedFluxMap;
}

void CondensedDofInterpreter::initializeGlobalDofIndices() {
  PartitionIndexType rank = Teuchos::GlobalMPISession::getRank();
  map<GlobalIndexType, IndexType> partitionLocalFluxMap = interpretedFluxMapForPartition(rank, true);
  
  int numRanks = Teuchos::GlobalMPISession::getNProc();
  FieldContainer<GlobalIndexType> fluxDofCountForRank(numRanks);
  
  _myGlobalDofIndexCount = partitionLocalFluxMap.size();
  fluxDofCountForRank(rank) = _myGlobalDofIndexCount;
  
  MPIWrapper::entryWiseSum(fluxDofCountForRank);
  
  _myGlobalDofIndexOffset = 0;
  for (int i=0; i<rank; i++){
    _myGlobalDofIndexOffset += fluxDofCountForRank(i);
  }

  // initialize _interpretedToGlobalDofIndexMap for the guys we own
  for (map<GlobalIndexType, IndexType>::iterator entryIt = partitionLocalFluxMap.begin(); entryIt != partitionLocalFluxMap.end(); entryIt++) {
    _interpretedToGlobalDofIndexMap[entryIt->first] = entryIt->second + _myGlobalDofIndexOffset;
//    cout << "Rank " << rank << ": " << entryIt->first << " --> " << entryIt->second + _myGlobalDofIndexOffset << endl;
  }
  
  map< PartitionIndexType, map<GlobalIndexType, GlobalIndexType> > partitionInterpretedFluxMap;

  // fill in the guys we don't own but do see
  for (set<GlobalIndexType>::iterator interpretedFluxIt=_interpretedFluxDofIndices.begin(); interpretedFluxIt != _interpretedFluxDofIndices.end(); interpretedFluxIt++) {
    GlobalIndexType interpretedFlux = *interpretedFluxIt;
    if (_interpretedToGlobalDofIndexMap.find(interpretedFlux) == _interpretedToGlobalDofIndexMap.end()) {
      // not a local guy, then
      PartitionIndexType owningPartition = _mesh->partitionForGlobalDofIndex(interpretedFlux);
      if (partitionInterpretedFluxMap.find(owningPartition) == partitionInterpretedFluxMap.end()) {
        partitionLocalFluxMap = interpretedFluxMapForPartition(owningPartition, false);
        GlobalIndexType owningPartitionDofOffset = 0;
        for (int i=0; i<owningPartition; i++){
          owningPartitionDofOffset += fluxDofCountForRank(i);
        }
        map<GlobalIndexType, GlobalIndexType> owningPartitionInterpretedToGlobalDofIndexMap;
        for (map<GlobalIndexType, IndexType>::iterator entryIt = partitionLocalFluxMap.begin(); entryIt != partitionLocalFluxMap.end(); entryIt++) {
          owningPartitionInterpretedToGlobalDofIndexMap[entryIt->first] = entryIt->second + owningPartitionDofOffset;
        }
        partitionInterpretedFluxMap[owningPartition] = owningPartitionInterpretedToGlobalDofIndexMap;
      }
      _interpretedToGlobalDofIndexMap[interpretedFlux] = partitionInterpretedFluxMap[owningPartition][interpretedFlux];
//      cout << "Rank " << rank << ": " << interpretedFlux << " --> " << partitionInterpretedFluxMap[owningPartition][interpretedFlux] << endl;
    }
  }
}

GlobalIndexType CondensedDofInterpreter::globalDofCount() {
  return MPIWrapper::sum(_myGlobalDofIndexCount);
}

set<GlobalIndexType> CondensedDofInterpreter::globalDofIndicesForPartition(PartitionIndexType rank) {
  if (rank == Teuchos::GlobalMPISession::getRank()) {
    set<GlobalIndexType> myGlobalDofIndices;
    GlobalIndexType nextOffset = _myGlobalDofIndexOffset + _myGlobalDofIndexCount;
    for (GlobalIndexType dofIndex = _myGlobalDofIndexOffset; dofIndex < nextOffset; dofIndex++) {
      myGlobalDofIndices.insert(dofIndex);
    }
    return myGlobalDofIndices;
  } else {
    cout << "globalDofIndicesForPartition() requires that rank be the local MPI rank!\n";
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "globalDofIndicesForPartition() requires that rank be the local MPI rank!");
  }
}

void CondensedDofInterpreter::interpretLocalBasisCoefficients(GlobalIndexType cellID, int varID, int sideOrdinal, const FieldContainer<double> &basisCoefficients,
                                                              FieldContainer<double> &globalCoefficients, FieldContainer<GlobalIndexType> &globalDofIndices) {
  // NOTE: cellID *MUST* belong to this partition.
  int rank = Teuchos::GlobalMPISession::getRank();
  if (_mesh->partitionForCellID(cellID) != rank) {
    cout << "cellID " << cellID << " does not belong to partition " << rank << ".\n";
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "cellID does not belong to partition");
  }
  
  FieldContainer<double> interpretedCoefficients;
  FieldContainer<GlobalIndexType> interpretedDofIndices;
  _mesh->interpretLocalBasisCoefficients(cellID, varID, sideOrdinal, basisCoefficients,
                                         interpretedCoefficients, interpretedDofIndices);

  // all BC indices should map one to one from the mesh's "interpreted" view to our "global" view
  
  globalCoefficients = interpretedCoefficients;
  globalDofIndices.resize(interpretedDofIndices.size());
  
  for (int dofOrdinal=0; dofOrdinal<interpretedDofIndices.size(); dofOrdinal++) {
    GlobalIndexType interpretedDofIndex = interpretedDofIndices[dofOrdinal];
    if (_interpretedToGlobalDofIndexMap.find(interpretedDofIndex) != _interpretedToGlobalDofIndexMap.end()) {
      GlobalIndexType globalDofIndex = _interpretedToGlobalDofIndexMap[interpretedDofIndex];
      globalDofIndices[dofOrdinal] = globalDofIndex;
    } else {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "globalDofIndex not found for specified interpretedDofIndex (may not be a flux?)");
    }
  }
}

void CondensedDofInterpreter::interpretLocalData(GlobalIndexType cellID, const FieldContainer<double> &localStiffnessData, const FieldContainer<double> &localLoadData,
                                                 FieldContainer<double> &globalStiffnessData, FieldContainer<double> &globalLoadData,
                                                 FieldContainer<GlobalIndexType> &globalDofIndices) {
  // NOTE: cellID *MUST* belong to this partition.
  int rank = Teuchos::GlobalMPISession::getRank();
  if (_mesh->partitionForCellID(cellID) != rank) {
    cout << "cellID " << cellID << " does not belong to partition " << rank << ".\n";
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "cellID does not belong to partition");
  }
  
  FieldContainer<double> interpretedStiffnessData, interpretedLoadData;
  
  FieldContainer<GlobalIndexType> interpretedDofIndices;
  
  _mesh->DofInterpreter::interpretLocalData(cellID, localStiffnessData, localLoadData,
                                            interpretedStiffnessData, interpretedLoadData, interpretedDofIndices);
  
  if (_storeLocalStiffnessMatrices) {
    _localStiffnessMatrices[cellID] = localStiffnessData;
    _localLoadVectors[cellID] = localLoadData;
    _localInterpretedDofIndices[cellID] = interpretedDofIndices;
  }
  
  set<int> fieldIndices, fluxIndices; // which are fields and which are fluxes in the interpreted data containers
  for (int dofOrdinal=0; dofOrdinal < interpretedDofIndices.size(); dofOrdinal++) {
    GlobalIndexType interpretedDofIndex = interpretedDofIndices(dofOrdinal);
    if (_interpretedToGlobalDofIndexMap.find(interpretedDofIndex) == _interpretedToGlobalDofIndexMap.end()) {
      fieldIndices.insert(dofOrdinal);
    } else {
      fluxIndices.insert(dofOrdinal);
    }
  }
  
  int fieldCount = fieldIndices.size();
  int fluxCount = fluxIndices.size();
  
  Epetra_SerialDenseMatrix D, B, K_flux;
 
  getSubmatrices(fieldIndices, fluxIndices, interpretedStiffnessData, D, B, K_flux);
  
  // reduce matrix
  Epetra_SerialDenseMatrix Bcopy = B;
  Epetra_SerialDenseSolver solver;

  Epetra_SerialDenseMatrix DinvB(fieldCount,fluxCount);
  solver.SetMatrix(D);
  solver.SetVectors(DinvB, Bcopy);
  bool equilibrated = false;
  if ( solver.ShouldEquilibrate() ) {
    solver.EquilibrateMatrix();
    solver.EquilibrateRHS();
    equilibrated = true;
  }
  solver.Solve();
  if (equilibrated)
    solver.UnequilibrateLHS();
  
  K_flux.Multiply('T','N',-1.0,B,DinvB,1.0); // assemble condensed matrix - A - B^T*inv(D)*B
  
  // reduce vector
  Epetra_SerialDenseVector Dinvf(fieldCount);
  Epetra_SerialDenseVector BtDinvf(fluxCount);
  Epetra_SerialDenseVector b_field, b_flux;
  getSubvectors(fieldIndices, fluxIndices, interpretedLoadData, b_field, b_flux);
  //    solver.SetMatrix(D);
  solver.SetVectors(Dinvf, b_field);
  equilibrated = false;
  if ( solver.ShouldEquilibrate() ) {
    solver.EquilibrateMatrix();
    solver.EquilibrateRHS();
    equilibrated = true;
  }
  solver.Solve();
  if (equilibrated)
    solver.UnequilibrateLHS();
  
  b_flux.Multiply('T','N',-1.0,B,Dinvf,1.0); // condensed RHS - f - B^T*inv(D)*g
  
  // resize output FieldContainers
  globalDofIndices.resize(fluxCount);
  globalStiffnessData.resize( fluxCount, fluxCount );
  globalLoadData.resize( fluxCount );
  
  set<int>::iterator indexIt;
  int i = 0;
  for (indexIt = fluxIndices.begin();indexIt!=fluxIndices.end();indexIt++){
    int localFluxIndex = *indexIt;
    GlobalIndexType interpretedDofIndex = interpretedDofIndices(localFluxIndex);
    int condensedIndex = _interpretedToGlobalDofIndexMap[interpretedDofIndex];
    globalDofIndices(i) = condensedIndex;
    i++;
  }
  
  for (int i=0; i<fluxCount; i++) {
    globalLoadData(i) = b_flux(i);
    for (int j=0; j<fluxCount; j++) {
      globalStiffnessData(i,j) = K_flux(i,j);
    }
  }
}

void CondensedDofInterpreter::interpretGlobalCoefficients(GlobalIndexType cellID, FieldContainer<double> &localCoefficients,
                                                          const Epetra_MultiVector &globalCoefficients) {
  // here, globalCoefficients correspond to *flux* dofs
  
  // get elem data and submatrix data
  FieldContainer<double> K,rhs;
  FieldContainer<GlobalIndexType> interpretedDofIndices;
  if (! _storeLocalStiffnessMatrices ){
    // getElemData(elem,K,rhs);
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "CondensedDofInterpreter::interpretGlobalData() doesn't yet support _storeLocalStiffnessMatrices = false");
  } else {
    K = _localStiffnessMatrices[cellID];
    rhs = _localLoadVectors[cellID];
    interpretedDofIndices = _localInterpretedDofIndices[cellID];
  }
  
//  cout << "interpretedDofIndices for cell " << cellID << ":\n" << interpretedDofIndices;
  
  FieldContainer<GlobalIndexTypeToCast> interpretedDofIndicesCast(interpretedDofIndices.size());
  for (int i=0; i<interpretedDofIndices.size(); i++) {
    interpretedDofIndicesCast[i] = (GlobalIndexTypeToCast) interpretedDofIndices[i];
  }
  
  // construct map for interpretedCoefficients:
  Epetra_SerialComm SerialComm; // rank-local map
  Epetra_Map    interpretedFluxIndicesMap((GlobalIndexTypeToCast)-1, (GlobalIndexTypeToCast)interpretedDofIndices.size(), &interpretedDofIndicesCast[0], 0, SerialComm);
  Epetra_MultiVector interpretedCoefficients(interpretedFluxIndicesMap, 1);

  for (int i=0; i<interpretedDofIndices.size(); i++) {
    GlobalIndexTypeToCast interpretedDofIndex = interpretedDofIndicesCast[i];
    int lID_interpreted = interpretedFluxIndicesMap.LID(interpretedDofIndex);
    if (_interpretedToGlobalDofIndexMap.find(interpretedDofIndex) != _interpretedToGlobalDofIndexMap.end()) {
      GlobalIndexTypeToCast globalDofIndex = _interpretedToGlobalDofIndexMap[interpretedDofIndex];
      int lID_global = globalCoefficients.Map().LID(globalDofIndex);
      interpretedCoefficients[0][lID_interpreted] = globalCoefficients[0][lID_global];
//      cout << "globalCoefficient for globalDofIndex " << globalDofIndex << ": " << globalCoefficients[0][lID_global] << endl;
    } else {
      interpretedCoefficients[0][lID_interpreted] = 0; // zeros for fields, for now
    }
  }
  
  DofOrderingPtr trialOrder = _mesh->getElementType(cellID)->trialOrderPtr;
  
  _mesh->interpretGlobalCoefficients(cellID, localCoefficients, interpretedCoefficients); // *only* fills in fluxes in localCoefficients (fields are zeros).  We still need to back out the fields
  
  set<int> fieldIndices, fluxIndices; // which are fields and which are fluxes in the local cell coefficients
  set<int> trialIDs = trialOrder->getVarIDs();
  for (set<int>::iterator trialIDIt = trialIDs.begin(); trialIDIt != trialIDs.end(); trialIDIt++) {
    int trialID = *trialIDIt;
    int sideCount = trialOrder->getNumSidesForVarID(trialID);
    for (int sideOrdinal=0; sideOrdinal<sideCount; sideOrdinal++) {
      vector<int> varIndices = trialOrder->getDofIndices(trialID, sideOrdinal);
      if (varDofsAreCondensible(trialID, sideOrdinal, trialOrder)) {
        fieldIndices.insert(varIndices.begin(), varIndices.end());
      } else {
        fluxIndices.insert(varIndices.begin(),varIndices.end());
      }
    }
  }
  
  int fieldCount = fieldIndices.size();
  int fluxCount = fluxIndices.size();
  
  Epetra_SerialDenseVector flux_dofs(fluxCount);

  int fluxOrdinal=0;
  for (set<int>::iterator fluxIt = fluxIndices.begin(); fluxIt != fluxIndices.end(); fluxIt++, fluxOrdinal++) {
    flux_dofs[fluxOrdinal] = localCoefficients[*fluxIt];
  }

  Epetra_SerialDenseMatrix D, B, fluxMat;
  Epetra_SerialDenseVector b_field, b_flux, field_dofs(fieldCount);
  getSubmatrices(fieldIndices, fluxIndices, K, D, B, fluxMat);
  getSubvectors(fieldIndices, fluxIndices, rhs, b_field, b_flux);
  
  b_field.Multiply('N','N',-1.0,B,flux_dofs,1.0);
  
  // solve for field dofs
  Epetra_SerialDenseSolver solver;
  solver.SetMatrix(D);
  solver.SetVectors(field_dofs,b_field);
  bool equilibrated = false;
  if ( solver.ShouldEquilibrate() ) {
    solver.EquilibrateMatrix();
    solver.EquilibrateRHS();
    equilibrated = true;
  }
  solver.Solve();
  if (equilibrated)
    solver.UnequilibrateLHS();
  
  int fieldOrdinal = 0; // index into field_dofs
  for (set<int>::iterator fieldIt = fieldIndices.begin(); fieldIt != fieldIndices.end(); fieldIt++, fieldOrdinal++) {
    localCoefficients[*fieldIt] = field_dofs[fieldOrdinal];
  }
}