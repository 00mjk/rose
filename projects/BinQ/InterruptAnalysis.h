#ifndef INTERRUPTANALYSIS_R_H
#define INTERRUPTANALYSIS_R_H
#include "rose.h"

#include <iostream>
#include <list>
#include "BinAnalyses.h"
#include "RoseBin_DataFlowAbstract.h"
#include "RoseBin_DefUseAnalysis.h"


class InterruptAnalysis : public BinAnalyses,  RoseBin_DataFlowAbstract {
 public:
  InterruptAnalysis(){};
  virtual ~InterruptAnalysis(){};
  void run();
  std::string name();
  std::string getDescription();

  bool run(std::string& name, SgDirectedGraphNode* node,
	   SgDirectedGraphNode* previous);

  bool twoFiles() {return false;}
 private:
  
  void init(RoseBin_Graph* vg) {
    vizzGraph = vg;
  }
  bool runEdge(SgDirectedGraphNode* node, SgDirectedGraphNode* next) {
    return false;
  }


  void getValueForDefinition(std::vector<uint64_t>& vec,
			     std::vector<uint64_t>& positions,
			     uint64_t& fpos,
			     SgDirectedGraphNode* node,
                             std::pair<X86RegisterClass, int> reg );

  std::string getIntCallName_Linux32bit(uint64_t rax,RoseBin_DataTypes::DataTypes& data_ebx,
					RoseBin_DataTypes::DataTypes& data_ecx,
					RoseBin_DataTypes::DataTypes& data_edx,
					std::vector<uint64_t>& val_rbx, 
					std::vector<uint64_t>& val_rcx, 
					std::vector<uint64_t>& val_rdx,
					std::vector<uint64_t>& pos_ebx,
					std::vector<uint64_t>& pos_ecx,
					std::vector<uint64_t>& pos_edx,
					uint64_t fpos_rbx, uint64_t fpos_rcx, uint64_t fpos_rdx);
  std::string getIntCallName_Linux64bit(uint64_t rax,RoseBin_DataTypes::DataTypes& data_ebx,
					RoseBin_DataTypes::DataTypes& data_ecx,
					RoseBin_DataTypes::DataTypes& data_edx,
					std::vector<uint64_t>& val_rbx, 
					std::vector<uint64_t>& val_rcx, 
					std::vector<uint64_t>& val_rdx,
					std::vector<uint64_t>& pos_ebx,
					std::vector<uint64_t>& pos_ecx,
					std::vector<uint64_t>& pos_edx,
					uint64_t fpos_rbx, uint64_t fpos_rcx, uint64_t fpos_rdx);
  std::string getIntCallName(uint64_t rax,RoseBin_DataTypes::DataTypes& data_ebx,
			     RoseBin_DataTypes::DataTypes& data_ecx,
			     RoseBin_DataTypes::DataTypes& data_edx,
			    std::vector<uint64_t>& val_rbx, 
			    std::vector<uint64_t>& val_rcx, 
			    std::vector<uint64_t>& val_rdx,
			     std::vector<uint64_t>& pos_ebx,
			     std::vector<uint64_t>& pos_ecx,
			     std::vector<uint64_t>& pos_edx,
			     uint64_t fpos_rbx, uint64_t fpos_rcx, uint64_t fpos_rdx);



};



#endif
