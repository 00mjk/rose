// ROSE is a tool for building preprocessors, this file is an example preprocessor built with ROSE.
#include "rose.h"

// DQ (2/9/2010): Testing use of ROSE to compile ROSE.
#ifndef USE_ROSE

#include <algorithm>
#include <functional>
#include <numeric>

#include <string>
#include "RtedSymbols.h"
#include "DataStructures.h"
#include "RtedTransformation.h"

#include "RtedVisit.h"

using namespace std;
using namespace SageInterface;
using namespace SageBuilder;

VariableTraversal::VariableTraversal(RtedTransformation* t)  {
  transf = t;
  rightOfbinaryOp = new std::vector<SgExpression*>();
  ROSE_ASSERT(rightOfbinaryOp);
};

bool VariableTraversal::isInterestingAssignNode (SgNode* exp) {
  if (!isSgDotExp(exp) && !isSgPointerDerefExp(exp))
    return true;
  else
    cerr << " ----------------------- NODE is not interesting : " << exp->class_name() << endl;
  return false;
}

bool VariableTraversal::isRightOfBinaryOp(SgNode* astNode ) {
  bool isRightBranchOfBinary = false;
  SgNode* temp = astNode;
  while (!isSgProject(temp) ) {
    if (temp->get_parent() && isSgBinaryOp(temp->get_parent())) {
      if ( isSgDotExp(temp->get_parent())  || isSgPointerDerefExp(temp->get_parent())) {
	temp=temp->get_parent();
	continue;
      }
      if (isSgBinaryOp(temp->get_parent())->get_rhs_operand()==temp	      ) {
	isRightBranchOfBinary=true;
      } else {
	break;
      }
    }
    temp=temp->get_parent();
  }
  return isRightBranchOfBinary;
}

InheritedAttribute
VariableTraversal::evaluateInheritedAttribute (
					       SgNode* astNode,
					       InheritedAttribute inheritedAttribute ) {
  cerr << "  !!!! >>>> down node  : " << astNode->class_name() << endl;


  if (isSgFunctionDefinition(astNode))    {
    // The inherited attribute is true iff we are inside a function.
    InheritedAttribute ia(
			  true,
			  inheritedAttribute.isAssignInitializer,
			  inheritedAttribute.isArrowExp,
			  inheritedAttribute.isAddressOfOp,
			  inheritedAttribute.isLValue,
			  inheritedAttribute.isReferenceType,
			  inheritedAttribute.isInitializedName,
			  inheritedAttribute.isBinaryOp,
			  inheritedAttribute.isDotExp,
			  inheritedAttribute.isPointerDerefExp);
    printf ("  >>>>> evaluateInheritedAttr isFunctionDefinition yes...\n");
    return ia;
  }

  if (isSgAssignInitializer(astNode))    {
    // The inherited attribute is true iff we are inside a function.
    InheritedAttribute ia(
			  inheritedAttribute.function,
			  true,
			  inheritedAttribute.isArrowExp,
			  inheritedAttribute.isAddressOfOp,
			  inheritedAttribute.isLValue,
			  inheritedAttribute.isReferenceType,
			  inheritedAttribute.isInitializedName,
			  inheritedAttribute.isBinaryOp,
			  inheritedAttribute.isDotExp,
			  inheritedAttribute.isPointerDerefExp);
    cerr << "  >>>>> evaluateInheritedAttr isAssignInit yes..." << endl;
    return ia;
  }
   
  if (isSgArrowExp(astNode))
    return  InheritedAttribute (
				inheritedAttribute.function,
				inheritedAttribute.isAssignInitializer,
				true,
				inheritedAttribute.isAddressOfOp,
				inheritedAttribute.isLValue,
				inheritedAttribute.isReferenceType,
				inheritedAttribute.isInitializedName,
				inheritedAttribute.isBinaryOp,
				inheritedAttribute.isDotExp,
				inheritedAttribute.isPointerDerefExp);
   
  if (isSgAddressOfOp(astNode))
    return InheritedAttribute (
			       inheritedAttribute.function,
			       inheritedAttribute.isAssignInitializer,
			       inheritedAttribute.isArrowExp,
			       true,
			       inheritedAttribute.isLValue,
			       inheritedAttribute.isReferenceType,
			       inheritedAttribute.isInitializedName,
			       inheritedAttribute.isBinaryOp,
			       inheritedAttribute.isDotExp,
			       inheritedAttribute.isPointerDerefExp);
  /*

  if (isSgReferenceType(astNode))
    return InheritedAttribute (
			       inheritedAttribute.function,
			       inheritedAttribute.isAssignInitializer,
			       inheritedAttribute.isArrowExp,
			       inheritedAttribute.isAddressOfOp,
			       inheritedAttribute.isLValue,
			       true,
			       inheritedAttribute.isInitializedName,
			       inheritedAttribute.isBinaryOp,
			       inheritedAttribute.isDotExp,
			       inheritedAttribute.isPointerDerefExp);

  if (isSgInitializedName(astNode))
    return InheritedAttribute (			       
			       inheritedAttribute.function,
			       inheritedAttribute.isAssignInitializer,
			       inheritedAttribute.isArrowExp,
			       inheritedAttribute.isAddressOfOp,
			       inheritedAttribute.isLValue,
			       inheritedAttribute.isReferenceType,
			       true,
			       inheritedAttribute.isBinaryOp,
			       inheritedAttribute.isDotExp,
			       inheritedAttribute.isPointerDerefExp);
  */
  if (isSgBinaryOp(astNode)) {
    if (!inheritedAttribute.isArrowExp && !inheritedAttribute.isAddressOfOp)
      if (isInterestingAssignNode(astNode)) {
	ROSE_ASSERT(isSgBinaryOp(astNode) -> get_rhs_operand());
	rightOfbinaryOp->push_back(isSgBinaryOp(astNode) -> get_rhs_operand());
	cerr << "  --------- !!!! >>>>      push : " << isSgBinaryOp(astNode) -> class_name() << "    right == " << isSgBinaryOp(astNode)->get_rhs_operand()->class_name() << "  Elements on stack : " << rightOfbinaryOp->size()  << endl;
	return InheritedAttribute (
                                   inheritedAttribute.function,
                                   inheritedAttribute.isAssignInitializer,
                                   inheritedAttribute.isArrowExp,
                                   inheritedAttribute.isAddressOfOp,
                                   inheritedAttribute.isLValue,
                                   inheritedAttribute.isReferenceType,
                                   inheritedAttribute.isInitializedName,
                                   true,
                                   inheritedAttribute.isDotExp,
                                   inheritedAttribute.isPointerDerefExp);
      }
  }
  /*
  if (isSgDotExp(astNode))
    return InheritedAttribute (
			       inheritedAttribute.function,
			       inheritedAttribute.isAssignInitializer,
			       inheritedAttribute.isArrowExp,
			       inheritedAttribute.isAddressOfOp,
			       inheritedAttribute.isLValue,
			       inheritedAttribute.isReferenceType,
			       inheritedAttribute.isInitializedName,
			       inheritedAttribute.isBinaryOp,
			       true,
			       inheritedAttribute.isPointerDerefExp);

  if (isSgPointerDerefExp(astNode))
    return InheritedAttribute (
			       inheritedAttribute.function,
			       inheritedAttribute.isAssignInitializer,
			       inheritedAttribute.isArrowExp,
			       inheritedAttribute.isAddressOfOp,
			       inheritedAttribute.isLValue,
			       inheritedAttribute.isReferenceType,
			       inheritedAttribute.isInitializedName,
			       inheritedAttribute.isBinaryOp,
			       inheritedAttribute.isDotExp,
			       true);

  if (isSgExpression(astNode))
    return InheritedAttribute (
			       inheritedAttribute.function,
			       inheritedAttribute.isAssignInitializer,
			       inheritedAttribute.isArrowExp,
			       inheritedAttribute.isAddressOfOp,
			       isSgExpression(astNode)->isLValue(),
			       inheritedAttribute.isReferenceType,
			       inheritedAttribute.isInitializedName,
			       inheritedAttribute.isBinaryOp,
			       inheritedAttribute.isDotExp,
			       inheritedAttribute.isPointerDerefExp);

  */


  return inheritedAttribute;
}



SynthesizedAttribute
VariableTraversal::evaluateSynthesizedAttribute (
						 SgNode* astNode,
						 InheritedAttribute inheritedAttribute,
						 SynthesizedAttributesList childAttributes ) {

  cerr << "  !!!!!!!! >>>> up node  : " << astNode->class_name() << endl;
  //  printf ("      evaluateSynthesizedAttribute Node...%s  isGlobal %d  isFunction %d  isAssign %d \n",astNode->class_name().c_str(),
  //        inheritedAttribute.global,inheritedAttribute.function,inheritedAttribute.isAssignInitializer);
  SynthesizedAttribute localResult =
    std::accumulate(childAttributes.begin(), childAttributes.end(),  false, std::logical_or<bool>());
  if (inheritedAttribute.function == true)
    {
      // Fold up the list of child attributes using logical or, i.e. the local
      // result will be true iff one of the child attributes is true.
      if (isSgFunctionDefinition(astNode) && localResult == true)
	{
	  printf ("  >>>>> evaluateSynthesizedAttribute Found a function containing a varRefExp  ...\n");
	}

      bool isRightBranchOfBinary =  isRightOfBinaryOp(astNode);
      if (isSgBinaryOp(astNode) ) {
	if (!inheritedAttribute.isArrowExp && !inheritedAttribute.isAddressOfOp)
	  if (isInterestingAssignNode(astNode)) {
            if (rightOfbinaryOp && !rightOfbinaryOp->empty()) rightOfbinaryOp->pop_back();
            cerr <<" ------ popp --- Elements on stack " << rightOfbinaryOp->size() <<endl;
	  }
      }

      cerr <<" ------ right of binary ? " << isRightBranchOfBinary << "  for current node : " << astNode->class_name() << "   exp: " << astNode->unparseToString() << endl;
      if (astNode->get_parent())
	cerr << "                  parent :: " << astNode->get_parent()->unparseToString()<<endl; 
      bool thinkItsStopSearch=false;
      if (isSgVarRefExp(astNode)) {
	if (!inheritedAttribute.isArrowExp && !inheritedAttribute.isAddressOfOp  )  {
	  bool stopSearch=false;
	  if (rightOfbinaryOp && !rightOfbinaryOp->empty()) {
#if 1
	      SgExpression* left = isSgBinaryOp(rightOfbinaryOp->back()->get_parent())->get_lhs_operand();
	      if (  isRightBranchOfBinary  && !isSgArrayType( rightOfbinaryOp->back()->get_type() )
		   && !isSgNewExp(rightOfbinaryOp->back())  && !isSgReferenceType( left->get_type() )) {
		stopSearch=false;
	      } else
		stopSearch=true;
#endif
	    }
	  // tps : this fails right now because of testcase : run/C/subprogram_call_errors/c_C_2_b  ... ROSE can not handle isUsedAsLValue right now
	  //              bool lval = isSgVarRefExp(astNode)->isUsedAsLValue();
	  //         string name = isSgVarRefExp(astNode)->get_symbol()->get_name().getString();
	  //        printf ("\n  $$$$$$$$$$ FOUND isSgVarRefExp : %s  LVALUE? %d...    :: %s \n",name.c_str(), lval,astNode->get_parent()->get_parent()->unparseToString().c_str() );

	  //            if (!isSgExpression(astNode)->isLValue())
#if 0
	  if (lval)
	    stopSearch=true;
#endif

#if 1
	  if (inheritedAttribute.isAssignInitializer)  {
	    cerr << " =======   inherited attribute :::::::: assignInitializer " << endl;
	    SgInitializedName* initName = isSgInitializedName( astNode -> get_parent() ->get_parent()-> get_parent());
	    if (initName==NULL)
	      initName = isSgInitializedName( astNode -> get_parent() ->get_parent());
	    if (initName   && isSgReferenceType( initName -> get_type()))
	      stopSearch=true;
	  }
#endif

#if 1
	  if (stopSearch==false) {
	    cout << " @@@@@@@@@ CALLING ADDING Variable access : " << astNode->unparseToString() << endl;
	    transf->visit_isSgVarRefExp(isSgVarRefExp(astNode),isRightBranchOfBinary,thinkItsStopSearch);
	  }
#else
	  if (stopSearch==false) {
	    // its a plain variable access
	    transf->variable_access_varref.push_back(isSgVarRefExp(astNode));
	    cout << " @@@@@@@@@ ADDING Variable access : " << astNode->unparseToString() << "  vec size: " << astNode->get_parent()->unparseToString() << endl;
	  }
#endif
	}
      }
    }
  return localResult;
}




void RtedTransformation::visit_isSgVarRefExp(SgVarRefExp* n, bool isRightBranchOfBinary, bool thinkItsStopSearch) {
  cout <<"$$$$$ visit_isSgVarRefExp : " << n->unparseToString() <<
    "  in line : " << n->get_file_info()->get_line() << " -------------------------------------" <<endl;

  SgInitializedName *name = n -> get_symbol() -> get_declaration();
  if( name  && !isInInstrumentedFile( name -> get_declaration() )) {
    // we're not instrumenting the file that contains the declaration, so we'll always complain about accesses
    return;
  }

#if 1
  SgNode* parent = isSgVarRefExp(n);
  SgNode* last = parent;
  bool stopSearch=false;
  //cerr << "*********************************** DEBUGGING  " << n->unparseToString() << endl;
  while (!isSgProject(parent)) {
    last=parent;  parent=parent->get_parent();
    if( isSgProject(parent))  {
      stopSearch=true;  break;
    }
    else if (isSgAssignInitializer(parent))   break;

    else if (isSgBinaryOp(parent)) {
      if (isSgDotExp(parent) || isSgPointerDerefExp(parent)) continue;
      break;
    }
    else if (isSgExprListExp(parent) && isSgFunctionCallExp(parent->get_parent())) {
      cerr << "$$$$$ Found Function call - lets handle its parameters." << endl;
      SgType* arg_type = isSgExpression(last)->get_type();
      SgType* param_type = NULL;
#if 1
      // try to determine the parameter type
      SgFunctionDeclaration* fndecl = isSgFunctionCallExp( parent -> get_parent() )-> getAssociatedFunctionDeclaration();
      if( fndecl ) {
	int param_index = -1;
	SgExpressionPtrList& args = isSgExprListExp( parent ) -> get_expressions();
	for( unsigned int i = 0; i < args.size(); ++i ) {
	  if( args[ i ] == last ) {
	    param_index = i;
	    break;
	  }
	}
	ROSE_ASSERT( param_index > -1 );

	if( (int)fndecl -> get_parameterList() -> get_args().size() > param_index ) {
	  SgInitializedName* param  = fndecl -> get_parameterList()-> get_args()[ param_index ];
	  if( param )  param_type = param -> get_type();
	}
      }

      if ((   arg_type   && isUsableAsSgArrayType( arg_type ) != NULL )
	  || (param_type  && isUsableAsSgReferenceType( param_type ) != NULL ))
	stopSearch=true;
#endif
      break;
    }

    else if( isSgWhileStmt( parent )
	     || isSgDoWhileStmt( parent )
	     || isSgIfStmt( parent )) {

      break;
    }

    else if( isSgForStatement( parent )) {
      SgForStatement* for_stmt = isSgForStatement( parent );
#if 1
      // Capture for( int i = 0;
      vector< SgNode* > initialized_names = NodeQuery::querySubTree( for_stmt -> get_for_init_stmt(), V_SgInitializedName );

      // Capture int i; for( i = 0;
      vector< SgNode* > init_var_refs = NodeQuery::querySubTree( for_stmt -> get_for_init_stmt(), V_SgVarRefExp );
      for(vector< SgNode* >::iterator i = init_var_refs.begin(); i != init_var_refs.end();  ++i) {
	initialized_names.push_back(
				    // map the var refs to their initialized names
				    isSgVarRefExp( *i ) -> get_symbol() -> get_declaration()
				    );
      }

      if( find( initialized_names.begin(), initialized_names.end(), name )  != initialized_names.end() ) {
	// no need to check the ref
	stopSearch = true;
      }
      // either way, no need to keep going up the AST
#endif
      break;
    }
  } //while

  if (stopSearch==false) {
    // its a plain variable access
    variable_access_varref.push_back(n);
    cerr << " @@@@@@@@@ ADDING Variable access : " << n->unparseToString() << "  parent: " << n->get_parent()->unparseToString() << endl;
  }

#endif
}

#endif



