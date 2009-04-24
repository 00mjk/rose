/****************************************************
 * RoseBin :: Binary Analysis for ROSE
 * Author : tps
 * Date : Jul27 07
 * Decription : Control flow Analysis
 ****************************************************/

#ifndef __RoseBin_FlowAnalysis__
#define __RoseBin_FlowAnalysis__

//#include <mysql.h>
#include <stdio.h>
#include <iostream>

//#include "RoseBin_support.h"
#include "MyAstAttribute.h"
//#include "RoseBin_unparse_visitor.h"
//#include "../graph/RoseBin_DotGraph.h"
//#include "../graph/RoseBin_GmlGraph.h"

#include <cstdlib>



// **************** AS DEFINED BY ANDREAS *****************************************
class FindAsmFunctionsVisitor: public std::binary_function<SgNode*, std::vector<SgAsmFunctionDeclaration *>* , void* >
{
  public:
    void* operator()(first_argument_type node, std::vector<SgAsmFunctionDeclaration*>* insns ) const{
      if (isSgAsmFunctionDeclaration(node)) insns->push_back(isSgAsmFunctionDeclaration(node));
      return NULL;
    }
};

class FindSgFunctionsVisitor: public std::binary_function<SgNode*, std::vector<SgFunctionDeclaration *>* , void* >
{
  public:
    void* operator()(first_argument_type node, std::vector<SgFunctionDeclaration*>* insns ) const{
      if (isSgFunctionDeclaration(node)) insns->push_back(isSgFunctionDeclaration(node));
      return NULL;
    }
};


class FindInstructionsVisitor: public std::binary_function<SgNode*, std::vector<SgAsmInstruction *>* , void* >
{
 public:
  void* operator()(first_argument_type node, std::vector<SgAsmInstruction*>* insns ) const{
    if (isSgAsmInstruction(node)) insns->push_back(isSgAsmInstruction(node));
    return NULL;
  }
};


class FindInstructionsVisitorx86: public std::binary_function<SgNode*, std::vector<SgAsmx86Instruction *>* , void* >
{
 public:
  void* operator()(first_argument_type node, std::vector<SgAsmx86Instruction*>* insns ) const{
    if (isSgAsmx86Instruction(node)) insns->push_back(isSgAsmx86Instruction(node));
    return NULL;
  }
};

class FindAsmStatementsVisitor: public std::binary_function<SgNode*, std::vector<SgAsmStatement *>* , void* >
{
 public:
  void* operator()(first_argument_type node, std::vector<SgAsmStatement*>* insns ) const{
    if (isSgAsmStatement(node)) insns->push_back(isSgAsmStatement(node));
    return NULL;
  }
};

class FindAsmStatementsHeaderVisitor: public std::binary_function<SgNode*, std::vector<SgAsmNode *>* , void* >
{
 public:
  void* operator()(first_argument_type node, std::vector<SgAsmNode*>* insns ) const{
    if (isSgAsmStatement(node)) insns->push_back(isSgAsmStatement(node));
    if (isSgAsmExecutableFileFormat(node)) insns->push_back(isSgAsmExecutableFileFormat(node));
    return NULL;
  }
};

class FindStatementsVisitor: public std::binary_function<SgNode*, std::vector<SgStatement *>* , void* >
{
 public:
  void* operator()(first_argument_type node, std::vector<SgStatement*>* insns ) const{
    if (isSgStatement(node)) 
      //      if (!isSgStatement(node)->get_file_info()->isCompilerGenerated())
	insns->push_back(isSgStatement(node));
	//}
    return NULL;
  }
};

class FindNodeVisitor: public std::binary_function<SgNode*, std::vector<SgLocatedNode *>* , void* >
{
 public:
  void* operator()(first_argument_type node, std::vector<SgLocatedNode*>* insns ) const{
    if (isSgNode(node)) 
      insns->push_back(isSgLocatedNode(node));
    return NULL;
  }
};

// ************************************************************************************

class RoseBin_FlowAnalysis : public AstSimpleProcessing {
 protected:
   rose_hash::hash_map <uint64_t, SgAsmInstruction* > rememberInstructions; // Insn address -> ROSE insn

  typedef rose_hash::hash_map< uint64_t, SgDirectedGraphNode*> tabletype_inv;

  //tabletype_inv usetable_instr;
  tabletype_inv deftable_instr;

  int nrOfFunctions;

// '__gnu_cxx::hash_map<std::basic_string<char >, SgDirectedGraphNode*, __gnu_cxx::hash<std::basic_string<char > >, std::equal_to<std::basic_string<char > > >' 
// to non-scalar type 
// '__gnu_cxx::hash_map<std::basic_string<char >, SgDirectedGraphNode*, hash_string, eqstr_string, std::allocator<SgDirectedGraphNode*> >'

// typedef rose_hash::hash_map <std::string, SgDirectedGraphNode*> nodeType;
// typedef rose_hash::hash_map <std::string, SgDirectedGraphEdge*> edgeType;
// typedef rose_hash::hash_map <std::string, SgDirectedGraphNode*,hash_string,eqstr_string> nodeType;

   typedef RoseBin_Graph::nodeType nodeType;
   typedef rose_hash::hash_map < std::string, SgDirectedGraphEdge*,rose_hash::hash_string,rose_hash::eqstr_string> edgeType;

  SgAsmNode* globalBin;
  int func_nr;
  int nr_target_missed;
  RoseBin_Graph* vizzGraph;
  std::string fileName;
  bool printEdges;
  // the name of the analysis
  std::string analysisName;

  // the string types of nodes and edges
  std::string typeNode;
  std::string typeEdge;

  // needed for CallGraphAnalysis
  SgAsmFunctionDeclaration* funcDecl; 
  SgDirectedGraphNode* funcDeclNode;


  
  // worklist to build the CFG graph
  std::stack <SgAsmInstruction*> worklist_forthisfunction;

  // visited map for the CFG graph
// DQ (4/23/2009): We need to specify the default template parameters explicitly.
// rose_hash::hash_map <std::string, SgAsmInstruction*> local_visited;
  rose_hash::hash_map <std::string, SgAsmInstruction*,rose_hash::hash_string,rose_hash::eqstr_string> local_visited;

  typedef std::map<std::string, SgAsmFunctionDeclaration*> bin_funcs_type;
  bin_funcs_type bin_funcs;

  // vector of graphs
  rose_hash::hash_map <std::string, SgDirectedGraph*> graphs;

  static bool initialized;

  VirtualBinCFG::AuxiliaryInformation* info;

  void initFunctionList(SgAsmNode* global);
  void process_jumps();
  SgAsmInstruction* process_jumps_get_target(SgAsmx86Instruction* inst);
  void resolveFunctions(SgAsmNode* global);
  SgAsmInstruction* resolveFunction(SgAsmInstruction* inst, bool hasStopCondition);
  void convertBlocksToFunctions(SgAsmNode* globalNode);
  void flattenBlocks(SgAsmNode* globalNode);

  bool db;

  int nrNodes;
  int nrEdges;


 public:
  //  RoseBin* roseBin;

  RoseBin_FlowAnalysis(SgAsmNode* global, VirtualBinCFG::AuxiliaryInformation* cfgInfo) {
    info = cfgInfo;
    nrNodes=0;
    nrEdges=0;
    db = RoseBin_support::getDataBaseSupport();
    RoseBin_support::setDebugMode(false);    
    RoseBin_support::setDebugModeMin(false);    
    func_nr=0;
    globalBin = global;
    // todo: optimize later
    if (initialized==false) {
#if 0
      // (tps 2Jun08) : Jeremiah implemented functions in his disassembler, 
      // so we do not need to perform a conversion from block to function anymore.
      // However, for now we want to pertain the flat hierarchy of function-instruction
      // instead of function-block-instruction and hence have to convert this.
      if (!db)
	flattenBlocks(globalBin);
#endif 
#if 0
      if (!db)
	convertBlocksToFunctions(globalBin);
#endif
      initFunctionList(globalBin);
#if 0
      if (!db) {
      	resolveFunctions(globalBin);
      }
#endif
      //      printAST(globalBin);
      process_jumps();
      initialized = true;
    }
    //    deftable_instr.clear();
    //usetable_instr.clear();
  }
  virtual ~RoseBin_FlowAnalysis() {}

  void setInitializedFalse() {
    initialized=false;
  }

  bool forward_analysis;
  void printAST(SgAsmNode* globalNode);
  // run this analysis
  virtual void run(RoseBin_Graph* vg, std::string fileN, bool multiedge) =0;

  std::string getName() { return analysisName;}

  void visit(SgNode* node);
  void checkControlFlow( SgAsmInstruction* binInst,
			 int functionSize, int countDown,
			 std::string& currentFunctionName, int func_nr);

  SgDirectedGraphNode* 
    getNodeFor(uint64_t inst) { return deftable_instr[inst];}


  void createInstToNodeTable();
  uint64_t getAddressForNode(SgDirectedGraphNode* node);

  // converts string to hex
  template <class T>
    bool from_string(T& t, 
		     const std::string& s, 
		     std::ios_base& (*f)(std::ios_base&))
    {
      std::istringstream iss(s);
      return !(iss >> f >> t).fail();
    }

  int nodesVisited() {
    return nrNodes;
  }

  int edgesVisited() {
    return nrEdges;
  }

  bool sameParents(SgDirectedGraphNode* node, SgDirectedGraphNode* next);
  void getRootNodes(std::vector <SgDirectedGraphNode*>& rootNodes);


};

#endif

