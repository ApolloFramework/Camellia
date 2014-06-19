#ifndef XDMFEXPORTER_H
#define XDMFEXPORTER_H

/*
 *  XDMFExporter.h
 *
 *  Created by Truman Ellis on 6/19/2014.
 *
 */

#include "Solution.h"
#include "Mesh.h"
#include "MeshTopology.h"
#include "VarFactory.h"

class XDMFExporter {
protected:
  // SolutionPtr _solution;
  MeshTopologyPtr _mesh;
  // VarFactory& _varFactory;
public:
  XDMFExporter(MeshTopologyPtr mesh, bool deleteOldFiles=false) : _mesh(mesh)
    {
      if (deleteOldFiles)
      {
        system("rm -rf *.xmf");
        system("rm -rf HDF5/*");
      }
      system("mkdir -p HDF5");
    }
  void exportFunction(FunctionPtr function, string functionName="function", string filename="output", set<GlobalIndexType> cellIndices=set<GlobalIndexType>(), unsigned int num1DPts=0);
  void exportFunction(vector<FunctionPtr> functions, vector<string> functionNames, string filename, set<GlobalIndexType> cellIndices=set<GlobalIndexType>(), unsigned int num1DPts=0);
};

#endif /* end of include guard: XDMFEXPORTER_H */
