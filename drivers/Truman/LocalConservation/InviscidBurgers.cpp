#include "OptimalInnerProduct.h"
#include "Mesh.h"
#include "Solution.h"
#include "ZoltanMeshPartitionPolicy.h"

#include "RefinementStrategy.h"
#include "NonlinearStepSize.h"
#include "NonlinearSolveStrategy.h"

// Trilinos includes
#include "Epetra_Time.h"
#include "Intrepid_FieldContainer.hpp"
#ifdef HAVE_MPI
#include "Epetra_MpiComm.h"
#else
#include "Epetra_SerialComm.h"
#endif

#ifdef HAVE_MPI
#include <Teuchos_GlobalMPISession.hpp>
#else
#endif

#include "InnerProductScratchPad.h"
#include "PreviousSolutionFunction.h"
#include "TestSuite.h"
#include "RefinementPattern.h"
#include "PenaltyConstraints.h"
#include "LagrangeConstraints.h"

int numRefs = 3;
bool enforceLocalConservation = false;
double newtonStepSize = 1.0;
int maxNewtonIterations = 100;

class U0 : public SimpleFunction
{
public:
  double value(double x, double y)
  {
    return 1 - 2 * x;
    // double pi = 2.0*acos(0.0);
    // return sin(pi*x);
  }
};

class BottomBoundary : public SpatialFilter
{
public:
  bool matchesPoint(double x, double y)
  {
    double tol = 1e-14;
    bool match = (abs(y) < tol);
    return match;
  }
};

class LeftBoundary : public SpatialFilter
{
public:
  bool matchesPoint(double x, double y)
  {
    double tol = 1e-14;
    bool match = (abs(x) < tol);
    return match;
  }
};

class RightBoundary : public SpatialFilter
{
public:
  bool matchesPoint(double x, double y)
  {
    double tol = 1e-14;
    bool match = (abs(x-1.0) < tol);
    return match;
  }
};


int main(int argc, char *argv[])
{
#ifdef HAVE_MPI
  Teuchos::GlobalMPISession mpiSession(&argc, &argv,0);
  int rank=mpiSession.getRank();
  int numProcs=mpiSession.getNProc();
#else
  int rank = 0;
  int numProcs = 1;
#endif
  int polyOrder = 3;
  int pToAdd = 2; // for tests

  // define our manufactured solution or problem bilinear form:
  bool useTriangles = false;

  FieldContainer<double> meshPoints(4,2);

  meshPoints(0,0) = 0.0; // x1
  meshPoints(0,1) = 0.0; // y1
  meshPoints(1,0) = 1.0;
  meshPoints(1,1) = 0.0;
  meshPoints(2,0) = 1.0;
  meshPoints(2,1) = 1.0;
  meshPoints(3,0) = 0.0;
  meshPoints(3,1) = 1.0;

  int H1Order = polyOrder + 1;
  int horizontalCells = 4, verticalCells = 4;

  double energyThreshold = 0.2; // for mesh refinements
  double nonlinearStepSize = 0.5;
  double nonlinearRelativeEnergyTolerance = 1e-8; // used to determine convergence of the nonlinear solution

  ////////////////////////////////////////////////////////////////////
  // DEFINE VARIABLES
  ////////////////////////////////////////////////////////////////////

  // new-style bilinear form definition
  VarFactory varFactory;
  VarPtr fhat = varFactory.fluxVar("\\widehat{f}");
  VarPtr u = varFactory.fieldVar("u");

  VarPtr v = varFactory.testVar("v",HGRAD);
  BFPtr bf = Teuchos::rcp( new BF(varFactory) ); // initialize bilinear form

  ////////////////////////////////////////////////////////////////////
  // CREATE MESH
  ////////////////////////////////////////////////////////////////////

  // create a pointer to a new mesh:
  Teuchos::RCP<Mesh> mesh = Mesh::buildQuadMesh(meshPoints, horizontalCells,
                            verticalCells, bf, H1Order,
                            H1Order+pToAdd, useTriangles);
  mesh->setPartitionPolicy(Teuchos::rcp(new ZoltanMeshPartitionPolicy("HSFC")));

  ////////////////////////////////////////////////////////////////////
  // INITIALIZE BACKGROUND FLOW FUNCTIONS
  ////////////////////////////////////////////////////////////////////
  BCPtr nullBC = Teuchos::rcp((BC*)NULL);
  RHSPtr nullRHS = Teuchos::rcp((RHS*)NULL);
  IPPtr nullIP = Teuchos::rcp((IP*)NULL);
  SolutionPtr backgroundFlow = Teuchos::rcp(new Solution(mesh, nullBC,
                               nullRHS, nullIP) );

  vector<double> e1(2); // (1,0)
  e1[0] = 1;
  vector<double> e2(2); // (0,1)
  e2[1] = 1;

  FunctionPtr u_prev = Teuchos::rcp( new PreviousSolutionFunction(backgroundFlow, u) );
  FunctionPtr beta = e1 * u_prev + Teuchos::rcp( new ConstantVectorFunction( e2 ) );

  ////////////////////////////////////////////////////////////////////
  // DEFINE BILINEAR FORM
  ////////////////////////////////////////////////////////////////////

  // v:
  // (sigma, grad v)_K - (sigma_hat_n, v)_dK - (u, beta dot grad v) + (u_hat * n dot beta, v)_dK
  bf->addTerm( -u, beta * v->grad());
  bf->addTerm( fhat, v);

  // ==================== SET INITIAL GUESS ==========================
  mesh->registerSolution(backgroundFlow);
  FunctionPtr zero = Teuchos::rcp( new ConstantScalarFunction(0.0) );
  FunctionPtr u0 = Teuchos::rcp( new U0 );

  map<int, Teuchos::RCP<Function> > functionMap;
  functionMap[u->ID()] = u0;

  backgroundFlow->projectOntoMesh(functionMap);
  // ==================== END SET INITIAL GUESS ==========================

  ////////////////////////////////////////////////////////////////////
  // DEFINE INNER PRODUCT
  ////////////////////////////////////////////////////////////////////
  IPPtr ip = Teuchos::rcp( new IP );
  ip->addTerm( v );
  ip->addTerm( beta * v->grad() );

  ////////////////////////////////////////////////////////////////////
  // DEFINE RHS
  ////////////////////////////////////////////////////////////////////
  Teuchos::RCP<RHSEasy> rhs = Teuchos::rcp( new RHSEasy );
  FunctionPtr u_prev_squared_div2 = 0.5 * u_prev * u_prev;
  rhs->addTerm( (e1 * u_prev_squared_div2 + e2 * u_prev) * v->grad());

  ////////////////////////////////////////////////////////////////////
  // DEFINE DIRICHLET BC
  ////////////////////////////////////////////////////////////////////
  Teuchos::RCP<BCEasy> inflowBC = Teuchos::rcp( new BCEasy );

  // Create spatial filters
  SpatialFilterPtr bottomBoundary = Teuchos::rcp( new BottomBoundary );
  SpatialFilterPtr leftBoundary = Teuchos::rcp( new LeftBoundary );
  SpatialFilterPtr rightBoundary = Teuchos::rcp( new LeftBoundary );

  // Create BCs
  FunctionPtr n = Teuchos::rcp( new UnitNormalFunction );
  FunctionPtr u0_squared_div_2 = 0.5 * u0 * u0;
  SimpleFunction* u0Ptr = static_cast<SimpleFunction *>(u0.get());
  double u0Left = u0Ptr->value(0,0);
  double u0Right = u0Ptr->value(1.0,0);
  FunctionPtr leftVal = Teuchos::rcp( new ConstantScalarFunction( -0.5*u0Left*u0Left ) );
  FunctionPtr rightVal = Teuchos::rcp( new ConstantScalarFunction( 0.5*u0Right*u0Right ) );
  inflowBC->addDirichlet(fhat, bottomBoundary, -u0 );
  inflowBC->addDirichlet(fhat, leftBoundary, leftVal );
  inflowBC->addDirichlet(fhat, rightBoundary, rightVal );

  ////////////////////////////////////////////////////////////////////
  // CREATE SOLUTION OBJECT
  ////////////////////////////////////////////////////////////////////
  Teuchos::RCP<Solution> solution = Teuchos::rcp(new Solution(mesh, inflowBC, rhs, ip));
  mesh->registerSolution(solution);

  if (enforceLocalConservation)
  {
    FunctionPtr zero = Teuchos::rcp( new ConstantScalarFunction(0.0) );
    solution->lagrangeConstraints()->addConstraint(fhat == zero);
  }

  ////////////////////////////////////////////////////////////////////
  // DEFINE REFINEMENT STRATEGY
  ////////////////////////////////////////////////////////////////////
  Teuchos::RCP<RefinementStrategy> refinementStrategy;
  refinementStrategy = Teuchos::rcp(new RefinementStrategy(solution,energyThreshold));

  ////////////////////////////////////////////////////////////////////
  // SOLVE
  ////////////////////////////////////////////////////////////////////

  for (int refIndex=0; refIndex<=numRefs; refIndex++)
  {
    double L2Update = 1e7;
    int iterCount = 0;
    while (L2Update > nonlinearRelativeEnergyTolerance && iterCount < maxNewtonIterations)
    {
      solution->solve();
      L2Update = solution->L2NormOfSolutionGlobal(u->ID());
      cout << "L2 Norm of Update = " << L2Update << endl;
      // backgroundFlow->clear();
      backgroundFlow->addSolution(solution, newtonStepSize);
      iterCount++;
    }
    cout << endl;

    // check conservation
    VarPtr testOne = varFactory.testVar("1", CONSTANT_SCALAR);
    // Create a fake bilinear form for the testing
    BFPtr fakeBF = Teuchos::rcp( new BF(varFactory) );
    // Define our mass flux
    FunctionPtr massFlux = Teuchos::rcp( new PreviousSolutionFunction(solution, fhat) );
    LinearTermPtr massFluxTerm = massFlux * testOne;

    Teuchos::RCP<shards::CellTopology> quadTopoPtr = Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Quadrilateral<4> >() ));
    DofOrderingFactory dofOrderingFactory(fakeBF);
    int fakeTestOrder = H1Order;
    DofOrderingPtr testOrdering = dofOrderingFactory.testOrdering(fakeTestOrder, *quadTopoPtr);

    int testOneIndex = testOrdering->getDofIndex(testOne->ID(),0);
    vector< ElementTypePtr > elemTypes = mesh->elementTypes(); // global element types
    map<int, double> massFluxIntegral; // cellID -> integral
    double maxMassFluxIntegral = 0.0;
    double totalMassFlux = 0.0;
    double totalAbsMassFlux = 0.0;
    for (vector< ElementTypePtr >::iterator elemTypeIt = elemTypes.begin(); elemTypeIt != elemTypes.end(); elemTypeIt++)
    {
      ElementTypePtr elemType = *elemTypeIt;
      vector< ElementPtr > elems = mesh->elementsOfTypeGlobal(elemType);
      vector<int> cellIDs;
      for (int i=0; i<elems.size(); i++)
      {
        cellIDs.push_back(elems[i]->cellID());
      }
      FieldContainer<double> physicalCellNodes = mesh->physicalCellNodesGlobal(elemType);
      BasisCachePtr basisCache = Teuchos::rcp( new BasisCache(elemType,mesh) );
      basisCache->setPhysicalCellNodes(physicalCellNodes,cellIDs,true); // true: create side caches
      FieldContainer<double> cellMeasures = basisCache->getCellMeasures();
      FieldContainer<double> fakeRHSIntegrals(elems.size(),testOrdering->totalDofs());
      massFluxTerm->integrate(fakeRHSIntegrals,testOrdering,basisCache,true); // true: force side evaluation
      for (int i=0; i<elems.size(); i++)
      {
        int cellID = cellIDs[i];
        // pick out the ones for testOne:
        massFluxIntegral[cellID] = fakeRHSIntegrals(i,testOneIndex);
      }
      // find the largest:
      for (int i=0; i<elems.size(); i++)
      {
        int cellID = cellIDs[i];
        maxMassFluxIntegral = max(abs(massFluxIntegral[cellID]), maxMassFluxIntegral);
      }
      for (int i=0; i<elems.size(); i++)
      {
        int cellID = cellIDs[i];
        maxMassFluxIntegral = max(abs(massFluxIntegral[cellID]), maxMassFluxIntegral);
        totalMassFlux += massFluxIntegral[cellID];
        totalAbsMassFlux += abs( massFluxIntegral[cellID] );
      }
    }
    if (rank==0)
    {
      cout << endl;
      cout << "largest mass flux: " << maxMassFluxIntegral << endl;
      cout << "total mass flux: " << totalMassFlux << endl;
      cout << "sum of mass flux absolute value: " << totalAbsMassFlux << endl;
      cout << endl;

      stringstream outfile;
      outfile << "burgers_" << refIndex;
      backgroundFlow->writeToVTK(outfile.str(), 5);
    }

    if (refIndex < numRefs)
      refinementStrategy->refine(rank==0); // print to console on rank 0
  }

  return 0;
}
