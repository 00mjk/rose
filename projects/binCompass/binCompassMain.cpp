#include <rose.h>
#include <string>

#include "binCompassAnalysisInterface.h"
#include "GraphAnalysisInterface.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <string>
#include <iostream>
#include "ltdl.h"

#include <sys/stat.h>
#include <sys/types.h>
using namespace std;

bool containsArgument(int argc, char** argv, string pattern) {
  for (int i = 2; i < argc ; i++) {
    if (argv[i]== pattern) {
      return true;
    }
  }
  return false;
}

int getdir (string dir, vector<string> &files)
{
  DIR *dp;
  struct dirent *dirp;
  if((dp  = opendir(dir.c_str())) == NULL) {
    cout << "Error(" << errno << ") opening " << dir << endl;
    return errno;
  }

  while ((dirp = readdir(dp)) != NULL) {
    string name = string(dirp->d_name);
    int find = name.find(".lo");
    if (find>=0) {
      name = name.substr(0,find);
      files.push_back(name);
    }
  }
  closedir(dp);
  return 0;
}

void loadAnalysisFiles(vector <BC_AnalysisInterface*>& checkers) {  
  string dir = string("analyses");
  vector<string> files = vector<string>();
  getdir(dir,files);
  for (unsigned int i = 0;i < files.size();i++) {
    string name = files[i];
    cout << "Loading Binary Checker --- " << name << endl;

    string filename = "analyses/.libs/lib" + name + ".so";
    lt_dlhandle dl = lt_dlopenext((filename).c_str());
    if (!dl) {cerr << "lt_dlopenext (" << filename << "): " << lt_dlerror() << endl;}
    ROSE_ASSERT (dl);
    void* symRaw = lt_dlsym(dl, "create");
    if (!symRaw) {cerr << "lt_dlsym (create): " << lt_dlerror() << endl;}
    ROSE_ASSERT (symRaw);
    BC_AnalysisInterface*(*sym)() = (BC_AnalysisInterface*(*)())symRaw;
    BC_AnalysisInterface* intf = sym();
    intf->set_name(name);
    checkers.push_back(intf);
  }
}

void loadGraphAnalysisFiles(vector <BC_GraphAnalysisInterface*>& checkers) {  
  string dir = string("graphanalyses");
  vector<string> files = vector<string>();
  getdir(dir,files);
  for (unsigned int i = 0;i < files.size();i++) {
    string name = files[i];
    cout << "Loading Binary Graph Checker --- " << name << endl;

    string filename = "graphanalyses/.libs/lib" + name + ".so";
    lt_dlhandle dl = lt_dlopenext((filename).c_str());
    if (!dl) {cerr << "lt_dlopenext (" << filename << "): " << lt_dlerror() << endl;}
    ROSE_ASSERT (dl);
    void* symRaw = lt_dlsym(dl, "create");
    if (!symRaw) {cerr << "lt_dlsym (create): " << lt_dlerror() << endl;}
    ROSE_ASSERT (symRaw);
    BC_GraphAnalysisInterface*(*sym)() = (BC_GraphAnalysisInterface*(*)())symRaw;
    BC_GraphAnalysisInterface* intf = sym();
    intf->set_name(name);
    checkers.push_back(intf);
  }
}

bool test;

int main(int argc, char** argv) {

  if (!containsArgument(argc, argv, "-checkAST") && 
      !containsArgument(argc, argv, "-checkGraph") &&
      !containsArgument(argc, argv, "-printTree") &&
      !containsArgument(argc, argv, "-callgraph") &&
      !containsArgument(argc, argv, "-cfa") &&
      !containsArgument(argc, argv, "-dfa") 
      ) {argc = 1;}

  if (argc < 2) {
    fprintf(stderr, "Usage: %s executableName [OPTIONS]\n", argv[0]);
    cout << "\nOPTIONS: " <<endl;
    cout << "-checkAST             - run all checkers on binary AST. " << endl; 
    cout << "-checkGraph           - run all checkers on dataflow graph. " << endl; 
    cout << "-printTree            - create dot file of AST. " << endl; 
    cout << "-callgraph            - perform callgraph analysis and print callgraph.dot file. " << endl; 
    cout << "-cfa                  - perform control flow analysis and print cfg.dot file. " << endl; 
    cout << "-dfa                  - perform dataflow flow analysis and print dfg.dot file. " << endl; 
    cout << "-inter                - perform dataflow analysis interprocedurally (default intraprocedural). " << endl; 
    cout << "-backward             - perform backward analysis (default forward). " << endl; 
    cout << "-gml                  - all graphs (except AST) are saved as gml files (default dot). " << endl; 
    cout << "-mergeedges           - aggregate edges between same nodes. " << endl; 
    cout << "-noedges              - do not print edges into dot or gml file (only nodes). " << endl; 
    return 1;
  }
  string execName = argv[1];

  lt_dlinit();
  
  // this is our test case input, we will assert on the data from this file
  test = false;
  if (execName=="buffer2.bin") {
    cerr << "running test case on buffer2.bin !! " << endl << endl; 
    test = true;
  }
  // create out folder
  string filenameDir="out";
  mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
  mkdir(filenameDir.c_str(), mode);

  std::ofstream myfile;

  bool interprocedural = false;
  if (containsArgument(argc, argv, "-inter")) {
    interprocedural = true;
  }
  bool forward = true;
  if (containsArgument(argc, argv, "-backward")) {
    forward = false;
  }
  bool dot = true;
  if (containsArgument(argc, argv, "-gml")) {
    dot = false;
  }
  bool mergedEdges = false;
  if (containsArgument(argc, argv, "-mergeedges")) {
    mergedEdges = true;
  }
  bool edges = true;
  if (containsArgument(argc, argv, "-noedges")) {
    edges = false;
  }

  RoseBin_Def::RoseAssemblyLanguage = RoseBin_Def::x86;
  fprintf(stderr, "Starting binCompass frontend...\n");
  SgProject* project = frontend(argc,argv);
  ROSE_ASSERT (project != NULL);
  SgAsmFile* file = project->get_file(0).get_binaryFile();

  if (containsArgument(argc, argv, "-printTree")) {
    fprintf(stderr, "Printing AST... _binary_tree.dot\n");
    string filename="_binary_tree.dot";
    AST_BIN_Traversal* trav = new AST_BIN_Traversal();
    trav->run(file->get_global_block(), filename);
    if (test) {
      int instrnr = trav->getNrOfInstructions();
      cerr << " Instructions written to file: " << instrnr << endl;
      ROSE_ASSERT(instrnr==861);
    }
  }

  RoseBin_Graph* graph;

  VirtualBinCFG::AuxiliaryInformation* info = new VirtualBinCFG::AuxiliaryInformation(file);

  // call graph analysis  *******************************************************
  if (containsArgument(argc, argv, "-callgraph")) {
    cerr << " creating call graph ... " << endl;
    graph= new RoseBin_DotGraph(info);
    string callFileName = "callgraph.dot";
    if (dot==false) {
      callFileName = "callgraph.gml";
      graph= new RoseBin_GMLGraph(info);
    }
    RoseBin_CallGraphAnalysis* callanalysis = new RoseBin_CallGraphAnalysis(file->get_global_block(), new RoseObj(), info);
    callanalysis->run(graph, callFileName, !mergedEdges);
    if (test) {
      cerr << " nr of nodes visited in callanalysis : " << callanalysis->nodesVisited() << endl;
      ROSE_ASSERT(callanalysis->nodesVisited()==10);
      cerr << " nr of edges visited in callanalysis : " << callanalysis->edgesVisited() << endl;
      ROSE_ASSERT(callanalysis->edgesVisited()==9);
    }
  }

  // control flow analysis  *******************************************************
  if (containsArgument(argc, argv, "-cfa")) {
    string cfgFileName = "cfg.dot";
    graph= new RoseBin_DotGraph(info);
    if (dot==false) {
      cfgFileName = "cfg.gml";
      graph= new RoseBin_GMLGraph(info);
    }
    RoseBin_ControlFlowAnalysis* cfganalysis = new RoseBin_ControlFlowAnalysis(file->get_global_block(), forward, new RoseObj(), edges, info);
    cfganalysis->run(graph, cfgFileName, mergedEdges);
    if (test) {
      cout << " cfa -- Number of nodes == " << cfganalysis->nodesVisited() << endl;
      cout << " cfa -- Number of edges == " << cfganalysis->edgesVisited() << endl;
      //ROSE_ASSERT(cfganalysis->nodesVisited()==210);
      //ROSE_ASSERT(cfganalysis->edgesVisited()==234);
      ROSE_ASSERT(cfganalysis->nodesVisited()==237);
      ROSE_ASSERT(cfganalysis->edgesVisited()==261);
    }
  }

  if (containsArgument(argc, argv, "-dfa")) {
    cerr << " creating dataflow graph ... " << endl;
    string dfgFileName = "dfg.dot";
    graph= new RoseBin_DotGraph(info);
    if (dot==false) {
      dfgFileName = "dfg.gml";
      graph= new RoseBin_GMLGraph(info);
    }
    RoseBin_DataFlowAnalysis* dfanalysis = new RoseBin_DataFlowAnalysis(file->get_global_block(), forward, new RoseObj(), info);
    dfanalysis->init(interprocedural, edges);
    dfanalysis->run(graph, dfgFileName, mergedEdges);
    if (test) {
      cout << " dfa -- Number of nodes == " << dfanalysis->nodesVisited() << endl;
      cout << " dfa -- Number of edges == " << dfanalysis->edgesVisited() << endl;
      cout << " dfa -- Number of memWrites == " << dfanalysis->nrOfMemoryWrites() << endl;
      cout << " dfa -- Number of regWrites == " << dfanalysis->nrOfRegisterWrites() << endl;
      cout << " dfa -- Number of definitions == " << dfanalysis->nrOfDefinitions() << endl;
      cout << " dfa -- Number of uses == " << dfanalysis->nrOfUses() << endl;
      if (interprocedural) {
	//ROSE_ASSERT(dfanalysis->nodesVisited()==210);
	//ROSE_ASSERT(dfanalysis->edgesVisited()==254);
	//ROSE_ASSERT(dfanalysis->nrOfMemoryWrites()==17);
	//ROSE_ASSERT(dfanalysis->nrOfRegisterWrites()==45);
	//ROSE_ASSERT(dfanalysis->nrOfDefinitions()==155);
	//ROSE_ASSERT(dfanalysis->nrOfUses()==23);
	ROSE_ASSERT(dfanalysis->nodesVisited()==237);
	ROSE_ASSERT(dfanalysis->edgesVisited()==284);
	ROSE_ASSERT(dfanalysis->nrOfMemoryWrites()==12);
	ROSE_ASSERT(dfanalysis->nrOfRegisterWrites()==36);
	ROSE_ASSERT(dfanalysis->nrOfDefinitions()==183);
	ROSE_ASSERT(dfanalysis->nrOfUses()==25);
      } else {
	//ROSE_ASSERT(dfanalysis->nodesVisited()==210);
	//ROSE_ASSERT(dfanalysis->edgesVisited()==248);
	//ROSE_ASSERT(dfanalysis->nrOfMemoryWrites()==12);
	//ROSE_ASSERT(dfanalysis->nrOfRegisterWrites()==33);
	//ROSE_ASSERT(dfanalysis->nrOfDefinitions()==104);
	//ROSE_ASSERT(dfanalysis->nrOfUses()==17);
	ROSE_ASSERT(dfanalysis->nodesVisited()==237);
	ROSE_ASSERT(dfanalysis->edgesVisited()==287);
	ROSE_ASSERT(dfanalysis->nrOfMemoryWrites()==18);
	ROSE_ASSERT(dfanalysis->nrOfRegisterWrites()==77);
	ROSE_ASSERT(dfanalysis->nrOfDefinitions()==216);
	ROSE_ASSERT(dfanalysis->nrOfUses()==31);

      }
    }
  }

  if (containsArgument(argc, argv, "-checkAST") || 
      containsArgument(argc, argv, "-checkGraph")) {
    // get a list of all checkers and traverse
    vector <BC_AnalysisInterface*> checkers;
    vector <BC_GraphAnalysisInterface*> graph_checkers;

    loadAnalysisFiles(checkers);

    vector <BC_AnalysisInterface*>::const_iterator it = checkers.begin();
    for (;it!=checkers.end();it++) {
      BC_AnalysisInterface* asmf = *it;
      cout << "\nRunning Binary Checker --- " << asmf->get_name() << endl;
      string filename = execName+"."+asmf->get_name();
      unsigned int pos = filename.find_last_of("/");
      if (filename.find_last_of("/")!=string::npos && (pos+1)<filename.length()) 
	filename = filename.substr(pos+1, filename.length());
      filename = "out/"+filename+".out";
      cerr << "Writing file : " << filename << endl;
      myfile.open(filename.c_str());
      asmf->init(file->get_global_block());
      asmf->traverse(file->get_global_block(), preorder);
      asmf->finish(file->get_global_block());
      string output = asmf->get_output();
      myfile << output << " \n";
      myfile.close();
    }  

    if (containsArgument(argc, argv, "-checkGraph")) {
      loadGraphAnalysisFiles(graph_checkers);

      cerr << "\n ---------------- preparing to run DataFlowAnalysis (-checkGraph)" << endl;
      string dfgFileName = "dfg.dot";
      graph= new RoseBin_DotGraph(info);
      if (dot==false) {
	dfgFileName = "dfg.gml";
	graph= new RoseBin_GMLGraph(info);
      }
      RoseBin_ControlFlowAnalysis* cfganalysis = new RoseBin_ControlFlowAnalysis(file->get_global_block(), forward, new RoseObj(), edges, info);
      cfganalysis->run(graph, dfgFileName, mergedEdges);
      if (test) {
	cerr << " cfa -- Number of nodes == " << cfganalysis->nodesVisited() << endl;
	cerr << " cfa -- Number of edges == " << cfganalysis->edgesVisited() << endl;
	//ROSE_ASSERT(cfganalysis->nodesVisited()==210);
	//ROSE_ASSERT(cfganalysis->edgesVisited()==234);
	ROSE_ASSERT(cfganalysis->nodesVisited()==237);
	ROSE_ASSERT(cfganalysis->edgesVisited()==261);
      }

      cerr << "CFG (-checkGraph) finished ----- Graph nr of nodes : " << graph->nodes.size() << endl;
      ROSE_ASSERT(graph->nodes.size()>0);

      RoseBin_DataFlowAnalysis* dfanalysis = new RoseBin_DataFlowAnalysis(file->get_global_block(), forward, new RoseObj(), info);
      //dfanalysis->init(interprocedural, edges,graph);
      dfanalysis->init(interprocedural, edges);
      dfanalysis->run(graph, dfgFileName, mergedEdges);

      cerr << "DFG (-checkGraph) finished ----- Graph nr of nodes : " << graph->nodes.size() << endl;
      vector<SgDirectedGraphNode*> rootNodes;
      dfanalysis->getRootNodes(rootNodes);

      //SgDirectedGraphNode* root1 = rootNodes[0];
      //rootNodes.clear();
      //rootNodes.push_back(root1);

      vector <BC_GraphAnalysisInterface*>::const_iterator it2 = graph_checkers.begin();
      cerr << "\n ---------------- running graph checkers : " << graph_checkers.size() << 
	"   rootNodes size : " << rootNodes.size() << "  interprocedural : " << 
	RoseBin_support::resBool(interprocedural) << endl;
      cout << "\n ---------------- running graph checkers : " << graph_checkers.size() << 
	"   rootNodes size : " << rootNodes.size() << "  interprocedural : " << 
	RoseBin_support::resBool(interprocedural) << endl;
      cerr << "Graph : " << graph->nodes.size() << endl;
      for (;it2!=graph_checkers.end();it2++) {
	BC_GraphAnalysisInterface* asmf = *it2;
	ROSE_ASSERT(asmf);
	cerr << "\nRunning Binary Graph Checker --- " << asmf->get_name() << "    " <<  "  roots : " <<
	  rootNodes.size() << endl;
	// tps 04/23/08 -- fixme: this code was broken when I added the testcase -- needs to be fixed
	dfanalysis->init();
	dfanalysis->traverseGraph(rootNodes, asmf, interprocedural);
      }  
    }
  }  

  unparseAsmStatementToFile("unparsed.s", file->get_global_block());

  lt_dlexit();

  return 0;
}


