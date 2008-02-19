

#ifndef __BinCompass_bufferoverflow__
#define __BinCompass_bufferoverflow__

#include "rose.h"
#include "../GraphAnalysisInterface.h"


class BufferOverflow: public BC_GraphAnalysisInterface {

 public:
  BufferOverflow() {}
  ~BufferOverflow() {}

  bool run(std::string& name, SgDirectedGraphNode* node,
	   SgDirectedGraphNode* previous);

  void getValueForDefinition(std::vector<uint64_t>& vec,
			     std::vector<uint64_t>& positions,
			     uint64_t& fpos,
			     SgDirectedGraphNode* node,
			     SgAsmRegisterReferenceExpression::x86_register_enum reg );

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

  bool runEdge(SgDirectedGraphNode* node, SgDirectedGraphNode* next) {
    return false;
  }

  void init(RoseBin_Graph* vg, RoseBin_unparse_visitor* unp) {
    unparser = unp;
    vizzGraph = vg;
  }

};

#endif

