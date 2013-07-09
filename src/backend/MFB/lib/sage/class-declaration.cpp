
#include "MFB/Sage/class-declaration.hpp"
#include "MFB/Sage/namespace-declaration.hpp"

#include "sage3basic.h"

#ifndef PATCHING_SAGE_BUILDER_ISSUES
#  define PATCHING_SAGE_BUILDER_ISSUES 1
#endif

namespace MultiFileBuilder {

Sage<SgClassDeclaration>::object_desc_t::object_desc_t(
  std::string name_,
  unsigned long kind_,
  SgSymbol * parent_,
  unsigned long file_id_,
  bool create_definition_
) :
  name(name_),
  kind(kind_),
  parent(parent_),
  file_id(file_id_),
  create_definition(create_definition_)
{}

template <>
Sage<SgClassDeclaration>::build_result_t Driver<Sage>::build<SgClassDeclaration>(const Sage<SgClassDeclaration>::object_desc_t & desc) {
  Sage<SgClassDeclaration>::build_result_t result;

  assert(desc.file_id != 0);

  Sage<SgClassDeclaration>::build_scopes_t scopes = getBuildScopes<SgClassDeclaration>(desc);

  SgScopeStatement * decl_scope = scopes.decl_scope;

  // Decl
  SgClassDeclaration * class_decl = SageBuilder::buildNondefiningClassDeclaration_nfi(desc.name, (SgClassDeclaration::class_types)desc.kind, decl_scope, false, NULL);

  // Symbol handling
  {
    result.symbol = decl_scope->lookup_class_symbol(desc.name);
    assert(result.symbol != NULL);

    symbol_to_file_id_map.insert(std::pair<SgSymbol *, unsigned long>(result.symbol, desc.file_id));

    parent_map.insert(std::pair<SgSymbol *, SgSymbol *>(result.symbol, desc.parent));
  }

  // Defining decl
  SgClassDeclaration * class_defn = NULL;
  {
    class_defn = SageBuilder::buildNondefiningClassDeclaration_nfi(desc.name, (SgClassDeclaration::class_types)desc.kind, decl_scope, false, NULL);
    SageInterface::appendStatement(class_defn, decl_scope);

    result.definition = SageBuilder::buildClassDefinition_nfi(class_defn, false);
    class_defn->set_definition(result.definition);
    class_defn->unsetForward(); 
  
    assert(class_decl->get_definition() == NULL);
    assert(class_defn->get_definition() != NULL);
    assert(!class_defn->isForward());
  }

#if PATCHING_SAGE_BUILDER_ISSUES
  { // connection between decl/defn
    class_decl->set_definingDeclaration(class_defn);
    class_decl->set_firstNondefiningDeclaration(class_decl);

    class_defn->set_definingDeclaration(class_defn);
    class_defn->set_firstNondefiningDeclaration(class_decl);
  }
#endif

  return result;
}

template <>
Sage<SgClassDeclaration>::build_scopes_t Driver<Sage>::getBuildScopes<SgClassDeclaration>(const Sage<SgClassDeclaration>::object_desc_t & desc) {
  Sage<SgClassDeclaration>::build_scopes_t result;

  SgClassSymbol * class_parent = isSgClassSymbol(desc.parent);
  SgNamespaceSymbol * namespace_parent = isSgNamespaceSymbol(desc.parent);

  assert(desc.parent == NULL || ((class_parent != NULL) xor (namespace_parent != NULL))); // if parent, it needs to be either a class or a namespace

  if (class_parent != NULL) {
    assert(desc.file_id == 0);

    assert(false); // NIY
  }
  else {
    std::map<unsigned long, std::pair<SgSourceFile *, SgSourceFile *> >::iterator it_file_pair = file_pair_map.find(desc.file_id);
    std::map<unsigned long, SgSourceFile *>::iterator it_standalone_source_file = standalone_source_file_map.find(desc.file_id);

    assert((it_file_pair != file_pair_map.end()) xor (it_standalone_source_file != standalone_source_file_map.end()));

    SgSourceFile * decl_file = NULL;
    if (it_file_pair != file_pair_map.end())
      decl_file = it_file_pair->second.first; // the header file
    else if (it_standalone_source_file != standalone_source_file_map.end())
      decl_file = it_standalone_source_file->second; // decl local to the source file
    else assert(false);

    assert(decl_file != NULL);

    if (namespace_parent == NULL)
      result.decl_scope = decl_file->get_globalScope();
    else {
      result.decl_scope = Sage<SgNamespaceDeclarationStatement>::getDefinition(namespace_parent, decl_file);
    }
  }

  return result;
}

template <>
void Driver<Sage>::createForwardDeclaration<SgClassDeclaration>(Sage<SgClassDeclaration>::symbol_t symbol, SgSourceFile * target_file) {
  std::map<SgSymbol *, SgSymbol *>::iterator it_parent = parent_map.find(symbol);
  assert(it_parent != parent_map.end());

  SgClassDeclaration * orig_decl = symbol->get_declaration();
  SgScopeStatement * scope = NULL;
  SgScopeStatement * insert_scope = NULL;

  SgSymbol * parent = it_parent->second;
  if (parent == NULL) {
    scope = target_file->get_globalScope();
    insert_scope = scope;
  }
  else {
    assert(false); // TODO
  }

  SgClassDeclaration * class_decl = SageBuilder::buildNondefiningClassDeclaration_nfi(symbol->get_name(), orig_decl->get_class_type(), scope, false, NULL);
  SageInterface::prependStatement(class_decl, insert_scope);
}

}
