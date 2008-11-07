#ifndef BUFFEROVERFLOW_R_H
#define BUFFEROVERFLOW_R_H
#include "rose.h"

#include <iostream>
#include <list>
#include "BinAnalyses.h"
#include "RoseBin_DataFlowAbstract.h"
#include "RoseBin_DefUseAnalysis.h"
//#include "RoseBin_VariableAnalysis.h"

class BufferOverflow : public BinAnalyses,  RoseBin_DataFlowAbstract {
 public:
  BufferOverflow(){};
  virtual ~BufferOverflow(){};
  void run();
  std::string name();
  std::string getDescription();

  bool run(std::string& name, SgDirectedGraphNode* node,
	   SgDirectedGraphNode* previous);


 private:
  
  void init(RoseBin_Graph* vg) {
    vizzGraph = vg;
  }
  bool runEdge(SgDirectedGraphNode* node, SgDirectedGraphNode* next) {
    return false;
  }


};



#endif
