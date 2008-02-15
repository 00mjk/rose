/*!
 *  \file ASTtools/StmtRewrite.hh
 *
 *  \brief Implements routines to support basic statement-level
 *  rewriting.
 *
 *  \author Richard Vuduc <richie@llnl.gov>
 *
 *  This module differs from the low-level rewrite mechanism in that
 *  it provides specific operations on particular kinds of statement
 *  nodes. However, it could be merged with the low-level rewrite at
 *  some point in the future.
 */

#include <rose.h>
#include "ASTtools.hh"
#include "Copy.hh"
#include "StmtRewrite.hh"

// ========================================================================

using namespace std;

// ========================================================================


void
ASTtools::appendCopy (const SgStatement* s, SgBasicBlock* b)
{
  if (b && s)
    {
      SgStatement* s_copy = isSgStatement (deepCopy (s));
      ROSE_ASSERT (s_copy);
      b->append_statement (s_copy); // TODO: a smarter append_statement should be
				// be able to set the symbol tables
      s_copy->set_parent(b);// needed ?
#if 1
      // liao, 11/5/1007
      // fixup symbol copy for variable declaration statement
      // reason: local symbols reside in higher scope of the copied stmt,
      // current copy cannot take care of them, neither can append_statement()
      if (isSgVariableDeclaration(s)) 
      {
        SgInitializedName *initName = isSgInitializedName\
		(*(isSgVariableDeclaration(s)->get_variables()).begin());
        ROSE_ASSERT(initName);
        SgVariableSymbol * symbol_1 = new SgVariableSymbol(initName);
        b->insert_symbol(initName->get_name(), symbol_1);
      }
      // similar work for funtion declaration
      // liao, 11/5/2007, not sure why the scope becomes global here in the AST graph
      // possible reason: c functions cannot be nested, so must be global scope here
      if (isSgFunctionDeclaration(s)) 
      {
        SgFunctionDeclaration *decl=const_cast<SgFunctionDeclaration*> (isSgFunctionDeclaration(s));
        //		((isSgFunctionDeclaration(s)->get_firstNondefiningDeclaration()));
        SgFunctionSymbol * symbol_1 = new SgFunctionSymbol(decl);
        SgGlobal* glob_scope =
	    const_cast<SgGlobal *> (TransformationSupport::getGlobalScope (s));
        ROSE_ASSERT(glob_scope);
        glob_scope->insert_symbol(decl->get_name(), symbol_1);
      }
#endif
    }
}

void
ASTtools::appendStmtsCopy (const SgBasicBlock* a, SgBasicBlock* b)
{
  if (a)
    {
      SgStatementPtrList src_stmts = a->get_statements ();
      for_each (src_stmts.begin (),
                src_stmts.end (),
                bind2nd (ptr_fun (appendCopy), b));
    }
}

/*!
 *  \brief
 *
 *  This routine sets the scope of the new statement to be the same as
 *  the old.
 */
void
ASTtools::replaceStatement (SgStatement* s_cur, SgStatement* s_new)
{
  SgBasicBlock* targetBB = isSgBasicBlock (s_cur->get_parent ());
  ROSE_ASSERT (targetBB != NULL);

  SgStatementPtrList& targetBB_list = targetBB->getStatementList ();
  SgStatementPtrList::iterator i =
    find (targetBB_list.begin (), targetBB_list.end (), s_cur);
  ROSE_ASSERT (i != targetBB_list.end ());
  SgStatement* oldblock = (*i);

  targetBB->insert_statement (i, s_new);
  LowLevelRewrite::remove (oldblock);
}

void
ASTtools::moveStatements (SgBasicBlock* src, SgBasicBlock* dest)
{
  if (!src) return; // no work to do
  ROSE_ASSERT (src && dest);

  // Move the statements.
  SgStatementPtrList& src_stmts = src->get_statements ();
  SgStatementPtrList& dest_stmts = dest->get_statements ();
  copy (src_stmts.begin (), src_stmts.end (),
        inserter (dest_stmts, dest_stmts.begin ()));
  src_stmts.clear ();

  // Copy the symbol table entries.
  SgSymbolTable* src_syms = src->get_symbol_table ();
  ROSE_ASSERT (src_syms);
  for (SgSymbol* i = src_syms->findfirstany (); i; i = src_syms->nextany ())
    dest->insert_symbol (i->get_name (), i);

  // Clear the source symbol table.
  delete src_syms;
  src_syms = new SgSymbolTable;
  ROSE_ASSERT (src_syms);
  src->set_symbol_table (src_syms);

  // Fix-up parent and scope pointers.
  for (SgStatementPtrList::iterator i = dest_stmts.begin ();
       i != dest_stmts.end (); ++i)
    {
      SgNode* par = (*i)->get_parent ();
      if (par == src)
        (*i)->set_parent (dest);

      SgDeclarationStatement* decl = isSgDeclarationStatement (*i);
      if (decl)
        {
          SgScopeStatement* scope = decl->get_scope ();
          if (scope == src)
            decl->set_scope (dest);
        }
    }
}

// =====================================================================

SgBasicBlock *
ASTtools::transformToBlockShell (SgBasicBlock* b_orig)
{
  // Create new block to store 'T', and move statements to it.
  SgBasicBlock* b_shell = new SgBasicBlock (ASTtools::newFileInfo ());
  ROSE_ASSERT (b_shell);
  ASTtools::moveStatements (b_orig, b_shell);
  b_orig->append_statement (b_shell);
  b_shell->set_parent (b_orig);
  return b_shell;
}

// eof
