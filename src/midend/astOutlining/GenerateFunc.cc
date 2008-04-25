/**
 *  \file Transform/GenerateFunc.cc
 *
 *  \brief Generates an outlined (independent) C-callable function
 *  from an SgBasicBlock.
 *
 *  This outlining implementation specifically generates C-callable
 *  routines for use in an empirical tuning application. Such routines
 *  can be isolated into their own, dynamically shareable modules.
 */

#include <iostream>
#include <string>
#include <sstream>

#include <rose.h>
#include "Transform.hh"
#include "ASTtools.hh"
#include "VarSym.hh"
#include "Copy.hh"
#include "StmtRewrite.hh"

//! Stores a variable symbol remapping.
typedef std::map<const SgVariableSymbol *, SgVariableSymbol *> VarSymRemap_t;

// =====================================================================

using namespace std;

/* ===========================================================
 */

//! Creates a non-member function.
static
SgFunctionDeclaration *
createFuncSkeleton (const string& name, SgType* ret_type,
                    SgFunctionParameterList* params)
{
  SgFunctionType *func_type = new SgFunctionType (ret_type, false);
  ROSE_ASSERT (func_type);

  SgFunctionDeclaration* func;
  SgProcedureHeaderStatement* fortranRoutine;
// Liao 12/13/2007, generate SgProcedureHeaderStatement for Fortran code
  if (SageInterface::is_Fortran_language()) 
  {
    fortranRoutine = new SgProcedureHeaderStatement(ASTtools::newFileInfo (), name, func_type);
    fortranRoutine->set_subprogram_kind(SgProcedureHeaderStatement::e_subroutine_subprogram_kind);

    func = isSgFunctionDeclaration(fortranRoutine);   
  }
  else
  {
    func = new SgFunctionDeclaration (ASTtools::newFileInfo (), name, func_type);
  }

  ROSE_ASSERT (func);
// DQ (9/7/2007): Fixup the defining and non-defining declarations
  ROSE_ASSERT(func->get_definingDeclaration() == NULL);
  func->set_definingDeclaration(func);
  ROSE_ASSERT(func->get_firstNondefiningDeclaration() != func);

  SgFunctionDefinition *func_def =
    new SgFunctionDefinition (ASTtools::newFileInfo (), func);
  func_def->set_parent (func);	//necessary or not?

  SgBasicBlock *func_body = new SgBasicBlock (ASTtools::newFileInfo ());
  func_def->set_body (func_body);
  func_body->set_parent (func_def);

  func->set_parameterList (params);
  params->set_parent (func);

  return func;
}

// ===========================================================

//! Creates an SgInitializedName.
static
SgInitializedName *
createInitName (const string& name, SgType* type,
                SgDeclarationStatement* decl,
                SgScopeStatement* scope,
                SgInitializer* init = 0)
{
  SgName sg_name (name.c_str ());
  SgInitializedName* new_name =
    new SgInitializedName (ASTtools::newFileInfo (), sg_name, type, init,
                           decl, scope, 0);
  ROSE_ASSERT (new_name);

  // Insert symbol
  if (scope)
    {
      SgVariableSymbol* new_sym = new SgVariableSymbol (new_name);
      scope->insert_symbol (sg_name, new_sym);
    }

  return new_name;
}

//! Stores a new outlined-function parameter.
typedef std::pair<string, SgType *> OutlinedFuncParam_t;

//! Returns 'true' if the base type is a primitive type.
static
bool
isBaseTypePrimitive (const SgType* type)
{
  if (!type) return false;
  const SgType* base_type = type->findBaseType ();
  if (base_type)
    switch (base_type->variantT ())
      {
      case V_SgTypeBool:
      case V_SgTypeChar:
      case V_SgTypeDouble:
      case V_SgTypeFloat:
      case V_SgTypeInt:
      case V_SgTypeLong:
      case V_SgTypeLongDouble:
      case V_SgTypeLongLong:
      case V_SgTypeShort:
      case V_SgTypeSignedChar:
      case V_SgTypeSignedInt:
      case V_SgTypeSignedLong:
      case V_SgTypeSignedShort:
      case V_SgTypeUnsignedChar:
      case V_SgTypeUnsignedInt:
      case V_SgTypeUnsignedLong:
      case V_SgTypeUnsignedShort:
      case V_SgTypeVoid:
      case V_SgTypeWchar:
        return true;
      default:
        break;
      }
  return false;
}

/*!
 *  \brief Creates a new outlined-function parameter for a given
 *  variable.
 *
 *  Given a variable (i.e., its type and name) whose references are to
 *  be outlined, create a suitable outlined-function parameter. The
 *  parameter is created as a pointer, to support parameter passing of
 *  aggregate types in C programs. Moreover, the type is made 'void'
 *  if the base type is not a primitive type.
 */
static
OutlinedFuncParam_t
createParam (const string& init_name, const SgType* init_type)
{
  ROSE_ASSERT (init_type);

  SgType* param_base_type = 0;
  if (isBaseTypePrimitive (init_type))
    {
      // Duplicate the initial type.
      param_base_type = const_cast<SgType *> (init_type); //!< \todo Is shallow copy here OK?
      ROSE_ASSERT (param_base_type);
    }
  else
    {
      param_base_type = SgTypeVoid::createType ();
      ROSE_ASSERT (param_base_type);
      if (ASTtools::isConstObj (init_type))
        {
          SgModifierType* mod = new SgModifierType (param_base_type);
          ROSE_ASSERT (mod);
          mod->get_typeModifier ().get_constVolatileModifier ().setConst ();
          param_base_type = mod;
        }
    }

  // Stores the new parameter.
  string new_param_name = init_name + "p__";
  SgType* new_param_type = SgPointerType::createType (param_base_type);

  ROSE_ASSERT (new_param_type);
  if (SageInterface::is_Fortran_language())
    return OutlinedFuncParam_t (new_param_name,param_base_type);
  else 
  return OutlinedFuncParam_t (new_param_name, new_param_type);
}

/*!
 *  \brief Creates a local variable declaration to "unpack" an
 *  outlined-function parameter that has been passed as a pointer
 *  value.
 */
static
SgVariableDeclaration *
createUnpackDecl (SgInitializedName* param,
                  const string& local_var_name,
                  SgType* local_var_type)
{
  // Create an expression that "unpacks" (dereferences) the parameter.
  SgVariableSymbol* param_sym = new SgVariableSymbol (param);
  ROSE_ASSERT (param_sym);

  SgVarRefExp* param_ref = new SgVarRefExp (ASTtools::newFileInfo (),
                                            param_sym);
  ROSE_ASSERT (param_ref);

  SgType* param_deref_type = const_cast<SgType *> (local_var_type);
  ROSE_ASSERT (param_deref_type);

  // Cast from 'void *' to 'LOCAL_VAR_TYPE *'
  SgReferenceType* ref = isSgReferenceType (param_deref_type);
  SgType* local_var_type_ptr =
    SgPointerType::createType (ref ? ref->get_base_type (): param_deref_type);
  ROSE_ASSERT (local_var_type_ptr);
  SgCastExp* cast_expr = new SgCastExp (ASTtools::newFileInfo (),
                                        param_ref,
                                        local_var_type_ptr);
  ROSE_ASSERT (cast_expr);

#if SET_TYPE_EXPLICITLY
  SgPointerDerefExp* param_deref_expr =
    new SgPointerDerefExp (ASTtools::newFileInfo (),
                           cast_expr,
                           param_deref_type);
#else
  SgPointerDerefExp* param_deref_expr =
    new SgPointerDerefExp (ASTtools::newFileInfo (),
                           cast_expr);
#endif

  ROSE_ASSERT (param_deref_expr);

  // Declare a local variable to store the dereferenced argument.
  SgName local_name (local_var_name.c_str ());
  SgAssignInitializer* local_val =
    new SgAssignInitializer (ASTtools::newFileInfo (),
                             param_deref_expr, param_deref_type);
  ROSE_ASSERT (local_val);
  SgType* local_type = isSgReferenceType (param_deref_type)
    ? param_deref_type
    : SgReferenceType::createType (param_deref_type);
  ROSE_ASSERT (local_type);
  SgVariableDeclaration* decl =
    new SgVariableDeclaration (ASTtools::newFileInfo (),
                               local_name, local_type, local_val);
  ROSE_ASSERT (decl);
  decl->set_firstNondefiningDeclaration (decl); //!< \todo Do in constructor?

  //! \todo Needed to overcome a bug in Cxx_tests/test2005_155.C---why?
  decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration (false);

  return decl;
}

//! Returns 'true' if the given type is 'const'.
static
bool
isReadOnlyType (const SgType* type)
{
  ROSE_ASSERT (type);

  const SgModifierType* mod = 0;
  switch (type->variantT ())
    {
    case V_SgModifierType:
      mod = isSgModifierType (type);
      break;
    case V_SgReferenceType:
      mod = isSgModifierType (isSgReferenceType (type)->get_base_type ());
      break;
    case V_SgPointerType:
      mod = isSgModifierType (isSgPointerType (type)->get_base_type ());
      break;
    default:
      mod = 0;
      break;
    }
  return mod
    && mod->get_typeModifier ().get_constVolatileModifier ().isConst ();
}

/*!
 *  \brief Creates an assignment to "pack" a local variable back into
 *  an outlined-function parameter that has been passed as a pointer
 *  value.
 *
 *  This routine takes the original "unpack" definition, of the form
 *
 *    TYPE local_unpack_var = *outlined_func_arg;
 *
 *  and creates the "re-pack" assignment expression,
 *
 *    *outlined_func_arg = local_unpack_var
 */
static
SgAssignOp *
createPackExpr (SgInitializedName* local_unpack_def)
{
  if (local_unpack_def
      && !isReadOnlyType (local_unpack_def->get_type ())
      && !isSgReferenceType (local_unpack_def->get_type ()))
    {
      SgName local_var_name (local_unpack_def->get_name ());

      SgAssignInitializer* local_var_init =
        isSgAssignInitializer (local_unpack_def->get_initializer ());
      ROSE_ASSERT (local_var_init);

      // Create the LHS, which derefs the function argument, by
      // copying the original dereference expression.
      SgPointerDerefExp* param_deref_unpack =
        isSgPointerDerefExp (local_var_init->get_operand_i ());
      ROSE_ASSERT (param_deref_unpack);
      SgPointerDerefExp* param_deref_pack =
        isSgPointerDerefExp (ASTtools::deepCopy (param_deref_unpack));
      ROSE_ASSERT (param_deref_pack);
              
      // Create the RHS, which references the local variable.
      SgScopeStatement* scope = local_unpack_def->get_scope ();
      ROSE_ASSERT (scope);
      SgVariableSymbol* local_var_sym =
        scope->lookup_var_symbol (local_var_name);
      ROSE_ASSERT (local_var_sym);
      SgVarRefExp* local_var_ref = new SgVarRefExp (ASTtools::newFileInfo (),
                                                    local_var_sym);
      ROSE_ASSERT (local_var_ref);

      // Assemble the final assignment expression.
      return new SgAssignOp (ASTtools::newFileInfo (),
                             param_deref_pack, local_var_ref);
    }
  return 0;
}

/*!
 *  \brief qCreates a pack statement.
 *
 *  This routine creates an SgExprStatement wrapper around the return
 *  of createPackExpr.
 */
static
SgExprStatement *
createPackStmt (SgInitializedName* local_unpack_def)
{
  SgAssignOp* pack_expr = createPackExpr (local_unpack_def);
  if (pack_expr)
    return new SgExprStatement (ASTtools::newFileInfo (), pack_expr);
  else
    return 0;
}


/*!
 *  \brief Records a mapping between two variable symbols, and record
 *  the new symbol.
 *
 *  This routine creates the target variable symbol from the specified
 *  SgInitializedName object. If the optional scope is specified
 *  (i.e., is non-NULL), then this routine also inserts the new
 *  variable symbol into the scope's symbol table.
 */
static
void
recordSymRemap (const SgVariableSymbol* orig_sym,
                SgInitializedName* name_new,
                SgScopeStatement* scope,
                VarSymRemap_t& sym_remap)
{
  if (orig_sym && name_new)
    {
      SgVariableSymbol* sym_new = new SgVariableSymbol (name_new);
      ROSE_ASSERT (sym_new);
      sym_remap.insert (VarSymRemap_t::value_type (orig_sym, sym_new));

      if (scope)
        {
          scope->insert_symbol (name_new->get_name (), sym_new);
          name_new->set_scope (scope);
        }
    }
}

/*!
 *  \brief Records a mapping between variable symbols.
 *
 *  \pre The variable declaration must contain only 1 initialized
 *  name.
 */
static
void
recordSymRemap (const SgVariableSymbol* orig_sym,
                SgVariableDeclaration* new_decl,
                SgScopeStatement* scope,
                VarSymRemap_t& sym_remap)
{
  if (orig_sym && new_decl)
    {
      SgInitializedNamePtrList& vars = new_decl->get_variables ();
      ROSE_ASSERT (vars.size () == 1);
      for (SgInitializedNamePtrList::iterator i = vars.begin ();
           i != vars.end (); ++i)
        recordSymRemap (orig_sym, *i, scope, sym_remap);
    }
}

/*!
 *  \brief Creates new function parameters for a set of variable
 *  symbols.
 *
 *  In addition to creating the function parameters, this routine
 *  records the mapping between the given variable symbols and the new
 *  symbols corresponding to the new parameters.
 *
 *  To support C programs, this routine assumes parameters passed
 *  using pointers (rather than references).  Moreover, it inserts
 *  "packing" and "unpacking" statements at the beginning and end of
 *  the function declaration, respectively.
 */
static
void
appendParams (const ASTtools::VarSymSet_t& syms,
              SgFunctionDeclaration* func,
              VarSymRemap_t& sym_remap)
{
  ROSE_ASSERT (func);

  SgFunctionParameterList* params = func->get_parameterList ();
  ROSE_ASSERT (params);

  SgFunctionDefinition* def = func->get_definition ();
  ROSE_ASSERT (def);

  SgBasicBlock* body = def->get_body ();
  ROSE_ASSERT (body);

  // Place in which to put new outlined variable symbols.
  SgScopeStatement* args_scope = isSgScopeStatement (body);
  ROSE_ASSERT (args_scope);

  // For each variable symbol, create an equivalent function
  // parameter. Also create unpacking and repacking statements.
  for (ASTtools::VarSymSet_t::const_reverse_iterator i = syms.rbegin ();
       i != syms.rend (); ++i)
    {
      // Variable symbol name
      const SgInitializedName* i_name = (*i)->get_declaration ();
      ROSE_ASSERT (i_name);

      // Function parameter (initialized) name
      string name_str = i_name->get_name ().str ();
      SgType* i_type = i_name->get_type ();

      // Create parameter and insert it into the parameter list.
      OutlinedFuncParam_t param = createParam (name_str, i_type);
      SgName p_sg_name (param.first.c_str ());
      SgInitializedName* p_init_name = createInitName (param.first,
                                                       param.second,
						      def->get_declaration(),
                                                       def);
      ROSE_ASSERT (p_init_name);
      params->prepend_arg (p_init_name);
      p_init_name->set_parent (params); //!< \todo Set in 'createInitName()'?
      p_init_name->set_scope (def); //!< \todo Set in 'createInitName()'?

	// pack/unpack is not necessary for Frotran, Liao 12/14/2007
     if (!SageInterface::is_Fortran_language())
      {
      // Create and insert unpack statement.
      SgVariableDeclaration* local_var_decl =
        createUnpackDecl (p_init_name, name_str, i_type);
      ROSE_ASSERT (local_var_decl);
      body->prepend_statement (local_var_decl);
      recordSymRemap (*i, local_var_decl, args_scope, sym_remap);
          
      // Create and insert companion re-pack statement.
      SgInitializedName* local_var_init =
        local_var_decl->get_decl_item (SgName (name_str.c_str ()));
      ROSE_ASSERT (local_var_init);
      SgExprStatement* pack_stmt = createPackStmt (local_var_init);
      if (pack_stmt)
        body->append_statement (pack_stmt);
    }
      else
      {
     //TODO Not sure the right scope for Fortran parameter, assuming function definition now
   // recordSymRemap() has two instances: the 2nd parameter could be initialized name in one case
     // p_init_name should already have a symbol associated with it after call createInitName()
     // pass NULL scope to avoid duplicated insertation
     //recordSymRemap (*i, i_name, def, sym_remap);
        recordSymRemap (*i, p_init_name, NULL, sym_remap);

	// Liao, 12/26/2007, addtional work for Fortran
	// for translator-generated arguments, prepend explicit type specifications to the body
	SgVariableDeclaration * varDecl= new SgVariableDeclaration(ASTtools::newFileInfo (),\
		p_sg_name,i_type,NULL);
        ROSE_ASSERT(varDecl);
        varDecl->set_firstNondefiningDeclaration(varDecl);
        SgInitializedName *initName = varDecl->get_decl_item (p_sg_name);
        ROSE_ASSERT(initName);
        initName->set_prev_decl_item(p_init_name);

        body->prepend_statement(varDecl);
      } // end if     
    } //end for
}

// ===========================================================

//! Fixes up references in a block to point to alternative symbols.
static
void
remapVarSyms (const VarSymRemap_t& vsym_remap, SgBasicBlock* b)
{
  if (!vsym_remap.empty ()) // Check if any remapping is even needed.
    {
      typedef Rose_STL_Container<SgNode *> NodeList_t;
      NodeList_t refs = NodeQuery::querySubTree (b, V_SgVarRefExp);
      for (NodeList_t::iterator i = refs.begin (); i != refs.end (); ++i)
        {
          // Reference possibly in need of fix-up.
          SgVarRefExp* ref_orig = isSgVarRefExp (*i);
          ROSE_ASSERT (ref_orig);

          // Search for a replacement symbol.
          VarSymRemap_t::const_iterator ref_new =
            vsym_remap.find (ref_orig->get_symbol ());
          if (ref_new != vsym_remap.end ()) // Needs replacement
            {
              SgVariableSymbol* sym_new = ref_new->second;
              ref_orig->set_symbol (sym_new);
            }
        }
    }
}

// =====================================================================

SgFunctionDeclaration *
Outliner::Transform::generateFunction (const SgBasicBlock* s,
                                          const string& func_name_str,
                                          const ASTtools::VarSymSet_t& syms)
{
  ROSE_ASSERT (s);

  // Create function skeleton, 'func'.
  SgName func_name (func_name_str);
  SgFunctionParameterList *parameterList =
    new SgFunctionParameterList (ASTtools::newFileInfo ());
  ROSE_ASSERT (parameterList);

  //! \todo Why doesn't constructor do this by default?
  parameterList->set_definingDeclaration (parameterList);
  //! \todo Why doesn't constructor do this by default?
  parameterList->set_firstNondefiningDeclaration (parameterList);

  SgFunctionDeclaration* func = createFuncSkeleton (func_name,
                                                    SgTypeVoid::createType (),
                                                    parameterList);
  ROSE_ASSERT (func);
// liao 10/30/207 maintain the symbol table
   SgFunctionSymbol * func_symbol = new SgFunctionSymbol(func);
   const_cast<SgBasicBlock *>(s)->insert_symbol(func->get_name(), func_symbol);

// not apply to Fortran
  if (!SageInterface::is_Fortran_language())
  {
  // Make function 'extern "C"'
  func->get_declarationModifier ().get_storageModifier ().setExtern ();
  func->set_linkage ("C");
  }

  // Generate the function body by deep-copying 's'.
  SgBasicBlock* func_body = func->get_definition ()->get_body ();
  ROSE_ASSERT (func_body);

  ASTtools::appendStmtsCopy (s, func_body);
#if 0
  // Liao, 12/27/2007, for DO .. CONTINUE loop, bug 171
  // copy a labeled CONTINUE at the end when it is missing
  // SgFortranDo --> SgLabelSymbol --> SgLabelStatement (CONTINUE)
  // end_numeric_label  fortran_statement    numeric_label
  if (SageInterface::is_Fortran_language())
  {
    SgStatementPtrList stmtList = func_body->get_statements();
    ROSE_ASSERT(stmtList.size()>0);
    SgStatementPtrList::reverse_iterator stmtIter;
    stmtIter = stmtList.rbegin();
    SgFortranDo * doStmt = isSgFortranDo(*stmtIter);
    if (doStmt) {
      SgLabelSymbol* label1= doStmt->get_end_numeric_label();
      if (label1)
      {
        SgLabelSymbol* label2=isSgLabelSymbol(ASTtools::deepCopy(label1));
        ROSE_ASSERT(label2);
        SgLabelStatement * contStmt = isSgLabelStatement(ASTtools::deepCopy(label1->\
              get_fortran_statement()));
        ROSE_ASSERT(contStmt);

        func_body->insert_symbol(label2->get_name(),isSgSymbol(label2));
        doStmt->set_end_numeric_label(label2);
        contStmt->set_numeric_label(label2);
        func_body->append_statement(contStmt);
      }
    } // end doStmt
  }
#endif
  //----------------------------------

  // Store parameter list information.
  VarSymRemap_t vsym_remap;

  // Create parameters for outlined vars, and fix-up symbol refs in
  // the body.
  appendParams (syms, func, vsym_remap);
  remapVarSyms (vsym_remap, func_body);

  ROSE_ASSERT (func);
  return func;
}

// eof
