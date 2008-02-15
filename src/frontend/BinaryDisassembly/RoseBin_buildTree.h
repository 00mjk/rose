/****************************************************
 * RoseBin :: Binary Analysis for ROSE
 * Author : tps
 * Date : 3Apr07
 * Decription : Code that actually builds the Rose Tree
 ****************************************************/

#ifndef __RoseBin_buildTree__
#define __RoseBin_buildTree__

#include <stdio.h>
#include <iostream>
#include <map>
#include <string>

// #include "rose.h"
#include "RoseBin_IDAPRO_exprTree.h"
// #include "RoseBin_support.h"

class RoseBin_buildTree  {
 protected:
  std::map <SgAsmNode*, exprTreeType> debugHelpMap;


  /****************************************************
   * return information about the register
   ****************************************************/
  void resolveRegister(std::string symbol, 
		       SgAsmRegisterReferenceExpression::x86_register_enum *registerSg,
		       SgAsmRegisterReferenceExpression::x86_position_in_register_enum *regSize);

  void resolveRegister(std::string symbol, 
		       SgAsmRegisterReferenceExpression::arm_register_enum *registerSg,
		       SgAsmRegisterReferenceExpression::arm_position_in_register_enum *regSize);

 public:
  RoseBin_buildTree() {};


  /****************************************************
   * return debug information from the helpMap
   ****************************************************/
  exprTreeType getDebugHelp (SgAsmNode* sgBinNode);

};

#endif



