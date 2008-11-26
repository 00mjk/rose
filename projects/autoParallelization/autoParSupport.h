#ifndef auto_par_support_INCLUDED
#define auto_par_support_INCLUDED
#include <rose.h>

#include "CommandOptions.h"
// OpenMP attribute for OpenMP 3.0
#include "OmpAttribute.h"

//Array Annotation headers
#include <CPPAstInterface.h>
#include <ArrayAnnot.h>
#include <ArrayRewrite.h>

//dependence graph headers
#include <AstInterface_ROSE.h>
#include <LoopTransformInterface.h>
#include <AnnotCollect.h>
#include <OperatorAnnotation.h>
#include <LoopTreeDepComp.h>

//Variable classification support
#include "DefUseAnalysis.h"
#include "LivenessAnalysis.h"

// Other standard C++ headers
#include <vector>
#include <string>
#include <iostream>
namespace AutoParallelization
{
  //Handle annotation, debugging flags
  void autopar_command_processing(std::vector<std::string>&argvList);
  // Required analysis and their initialization
  extern DFAnalysis * defuse;
  extern LivenessAnalysis* liv;
  // Conduct necessary analyses on the project, can be called multiple times during program transformations. 
  bool initialize_analysis(SgProject* project=NULL,bool debug=false);
  //Release the resources for analyses
  void release_analysis();
  // Return the loop invariant of a canonical loop
  SgInitializedName* getLoopInvariant(SgNode* loop);
  
  //Compute dependence graph for a loop, using ArrayInterface and ArrayAnnoation
  LoopTreeDepGraph* ComputeDependenceGraph(SgNode* loop, ArrayInterface*, ArrayAnnotation* annot);
  
  // Get the live-in and live-out variable sets for a for loop, recomputing liveness analysis if requested (useful after program transformation)
  void GetLiveVariables(SgNode* loop, std::vector<SgInitializedName*> &liveIns,
                       std::vector<SgInitializedName*> &liveOuts,bool reCompute=false);

  // Collect visible referenced variables within a scope. 
  // ignoring local variables declared within the scope. They are less interesting in many analyses
  void CollectVisibleVaribles(SgNode* loop, std::vector<SgInitializedName*>& resultVars, bool scalarOnly=false);

  // Variable classification for a loop node 
  // Collect private, firstprivate, lastprivate, reduction and save into attribute
  void AutoScoping(SgNode *sg_node, OmpSupport::OmpAttribute* attribute);

  // Recognize reduction variables for a loop
  void RecognizeReduction(SgNode *sg_node, OmpSupport::OmpAttribute* attribute, std::vector<SgInitializedName*>& candidateVars); 

  // Collect all classified variables from an OmpAttribute attached to a loop node,regardless their omp type
  void CollectScopedVariables(OmpSupport::OmpAttribute* attribute, std::vector<SgInitializedName*>& result);

  // Eliminate irrelevant dependencies for a loop node 'sg_node'
  // Save the remaining dependencies which prevent parallelization
  void DependenceElimination(SgNode* sg_node, LoopTreeDepGraph* depgraph, std::vector<DepInfo>&remain, OmpSupport::OmpAttribute* attribute, ArrayInterface* array_interface=0, ArrayAnnotation* annot=0);

  //Generate and insert OpenMP pragmas according to OmpAttribute
  void generatedOpenMPPragmas(SgNode* node);

} //end namespace

#endif //auto_par_support_INCLUDED
