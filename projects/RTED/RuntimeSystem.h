#ifndef RTEDRUNTIME_H
#define RTEDRUNTIME_H

#include <stddef.h>

#include "CppRuntimeSystem/rted_iface_structs.h"
#include "CppRuntimeSystem/ptrops.h"
#include "CppRuntimeSystem/rtedsync.h"

#include "ParallelRTS.h"

#define EXITCODE_OK 0

#ifdef __cplusplus
extern "C" {
#endif

/***************************** HELPER FUNCTIONS *************************************/
// \pp \todo using convertIntToString currently leakes memory as the generated
//           string is never freed. This could (likely) be done in the function numval...
const char*
rted_ConvertIntToString(size_t num);


#ifdef ROSE_WITH_ROSEQT

// USE GUI for debugging
void Rted_debugDialog(rted_SourceInfo si);

#endif /* ROSE_WITH_ROSEQT */

void rted_Checkpoint(rted_SourceInfo si);
/***************************** HELPER FUNCTIONS *************************************/



/***************************** ARRAY FUNCTIONS *************************************/

/// \brief   notifies the runtime system that memory for an array is allocated
///          without involving dynamic memory utilities. For example,
///          a global array (could be UPC shared), an array declared within
///          function scope (could be static).
/// \param   td          describes the type, indirections (incl. shareing)
/// \param   address     array start
/// \param   totalsize   sizeof(array)
/// \param   distributed true, iff array is distributed across (UPC) threads
/// \param   dimDescr    points to an array describing the dimensions; first entry is element count
/// \details Since this function is called for static allocations, we do not
///          add a UPC runtime version. (1) arrays in function scope cannot be
///          shared; (2) global shared arrays are initialized by all UPC threads.
void rted_CreateArray( rted_TypeDesc   td,
                       rted_Address    address,
                       size_t          totalsize,
                       rted_AllocKind  allocKind,
                       long            blocksize,
                       int             initialized,
                       const size_t*   dimDescr,
                       const char*     name,
                       const char*     mangl_name,
                       const char*     class_name,
                       rted_SourceInfo si
                     );

/// \brief   notifies the runtime system that dynamic memory has been
///          allocated.
/// \param   td          describes the type, indirections (incl. shareing)
/// \param   address     start address
/// \param   size        element size (unused)
/// \param   blocksize   upc blocksize (0 if the memory is not distributed)
/// \param   mallocSize  size of allocated memory
/// \param   allocKind   describes which function was used to allocate memory
/// \param   classname   string containing the name of a user defined type
/// \param   si          describes the source locations
/// \details Since this function is called for static allocations, we do not
///          add a UPC runtime version. (1) arrays in function scope cannot be
///          shared; (2) global shared arrays are initialized by all UPC threads.
void rted_AllocMem( rted_TypeDesc   td,
                    rted_Address    address,
                    size_t          size,
                    rted_AllocKind  allocKind,
                    long            blocksize,
                    size_t          mallocSize,
                    const char*     class_name,
                    rted_SourceInfo si
                  );

/// \brief   internal variant
/// \details adds parameters heap_address and heap_desc explicitly,
///          as those can only be computed locally (from the thread
///          that initially invokes rted_AllocMem
void _rted_AllocMem( rted_TypeDesc    td,
                     rted_Address     address,
                     rted_Address     heap_address,
                     long             blocksize,
                     size_t           mallocSize,
                     rted_AllocKind   allocKind,
                     const char*      class_name,
                     rted_SourceInfo  si,
                     int              originloc
                   );

void rted_AccessArray( rted_Address     base_address, // &( array[ 0 ])
                       rted_Address     address,
                       size_t           size,
                       int              read_write_mask,  // 1 = read, 2 = write
                       rted_SourceInfo  si
                     );

/***************************** ARRAY FUNCTIONS *************************************/



/***************************** FUNCTION CALLS *************************************/

void rted_AssertFunctionSignature( const char*     name,
                                   size_t          type_count,
                                   rted_TypeDesc*  typedescs,
                                   rted_SourceInfo si
                                 );

void rted_ConfirmFunctionSignature(const char* name, size_t type_count, rted_TypeDesc* types);

void rted_IOFunctionCall( const char*     fname,
                          const char*     stmtStr,
                          const char*     leftVar,
                          void*           file,
                          const char*     arg1,
                          const char*     arg2,
                          rted_SourceInfo si
                        );

void rted_FunctionCall( const char*     name,
                        const char*     unused_stmtStr,
                        const char*     unused_leftVar,
                        rted_SourceInfo si,
                        size_t          argc,
                        const char**    args
                      );

/***************************** FUNCTION CALLS *************************************/



/***************************** MEMORY FUNCTIONS *************************************/

/// \param addr     the address that is about to be freed
/// \param freeKind describes the kind of allocation that this free performs.
///                 Also indicates when ptr needs to be interpreted as shared ptr.
/// \param si       source location
void rted_FreeMemory(rted_Address addr, rted_AllocKind freeKind, rted_SourceInfo si);

/// \brief internal version
void _rted_FreeMemory(rted_Address addr, rted_AllocKind freeKind, rted_SourceInfo si, int originloc);

void rted_ReallocateMemory( void* ptr, size_t size, rted_SourceInfo si );
/***************************** MEMORY FUNCTIONS *************************************/



/***************************** SCOPE *************************************/
// handle scopes (so we can detect when locals go out of scope, free up the
// memory and possibly complain if the local was the last var pointing to some
// memory)
void rted_EnterScope(const char* scope_name);
void rted_ExitScope(const char*, rted_SourceInfo si);

/***************************** SCOPE *************************************/


void rted_Close(const char* from);

// function used to indicate error
// \pp is this function used / defined?
// void RuntimeSystem_callExit(const char* filename, const char* line,
//    const char* reason, const char* stmtStr);

extern int RuntimeSystem_original_main(int argc, char**argv, char**envp);
/***************************** INIT AND EXIT *************************************/



/***************************** VARIABLES *************************************/

int rted_CreateVariable( rted_TypeDesc   td,
                         rted_Address    address,
                         size_t          size,
                         int             init,
                         rted_AllocKind  ak,
                         const char*     name,
                         const char*     mangled_name,
                         const char*     class_name,
                         rted_SourceInfo si
                       );

/**
 * Register the creation of a C++ object.  This function should only be called
 * at the start of an object's constructor.  It is safe to call this function
 * multiple times for the same address: e.g. if called in a base class
 * constructor and a sub class constructor.
 */
int rted_CreateObject( rted_TypeDesc td, rted_Address address, size_t sz, rted_SourceInfo si );


int rted_InitVariable( rted_TypeDesc   td,
                       rted_Address    address,
                       size_t          size,
                       int             pointer_changed,
                       const char*     class_name,
                       rted_SourceInfo si
                     );

/// \brief internal version
int _rted_InitVariable( rted_TypeDesc    td,
                        rted_Address     address,
                        rted_Address     heap_address,
                        size_t           size,
                        int              pointer_move,
                        const char*      class_name,
                        rted_SourceInfo  si,
                        int              originloc
                      );


/**
 * This function is called when pointers are incremented.  For example, it will
 * be called for the following:
 @code
       int* p;
       // ...
       ++p;
 @endcode
 * but not for simple assignment, as in the following:
 @code
       int* p;
       // ...
       p = ...
 @endcode
 * It verifies that the pointer stays within “memory bounds”.  In particular, if
 * the pointer points to an array, rted_MovePointer checks that it
 * isn't incremented beyond the bounds of the array, even if doing so results in
 * a pointer to allocated memory.
 */
void rted_MovePointer( rted_TypeDesc    td,
                       rted_Address     address,
                       const char*      class_name,
                       rted_SourceInfo  si
                     );

/// \brief internal version
void _rted_MovePointer( rted_TypeDesc    td,
                        rted_Address     address,
                        rted_Address     heap_address,
                        const char*      class_name,
                        rted_SourceInfo  si,
                        int              originloc
                      );

void rted_AccessVariable( rted_Address    read_address,
                          size_t          read_size,
                          rted_Address    write_address,
                          size_t          write_size,
                          int             read_write_mask,
                          rted_SourceInfo si
                        );

void rted_CheckIfThisNULL( void* thisExp, rted_SourceInfo si );
/***************************** VARIABLES *************************************/



/***************************** TYPES *************************************/
// handle structs and classes
void rted_RegisterTypeCall( const char*     nameC,
                            const char*     unused_typeC,
                            int             isUnion,
                            size_t          sizeC,
                            rted_SourceInfo si,
                            size_t          argc,
                            ...
                          );

/***************************** TYPES *************************************/


#ifdef __cplusplus
} // extern "C"
#endif

#endif
