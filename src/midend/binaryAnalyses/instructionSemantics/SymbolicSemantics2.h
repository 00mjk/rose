#ifndef Rose_SymbolicSemantics2_H
#define Rose_SymbolicSemantics2_H

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "BaseSemantics2.h"
#include "SMTSolver.h"
#include "InsnSemanticsExpr.h"

#include <map>
#include <vector>

namespace BinaryAnalysis {              // documented elsewhere
namespace InstructionSemantics2 {       // documented elsewhere

/** A fully symbolic semantic domain.
*
*  This semantic domain can be used to emulate the execution of a single basic block of instructions.  It is similar in nature
*  to PartialSymbolicSemantics, but with a different type of semantics value (SValue): instead of values being a constant or
*  variable with offset, values here are expression trees.
*
*  <ul>
*    <li>SValue: the values stored in registers and memory and used for memory addresses.</li>
*    <li>MemoryCell: an address-expression/value-expression pair for memory.</li>
*    <li>MemoryState: the collection of MemoryCells that form a complete memory state.</li>
*    <li>RegisterState: the collection of registers that form a complete register state.</li>
*    <li>State: represents the state of the virtual machine&mdash;its registers and memory.</li>
*    <li>RiscOperators: the low-level operators called by instruction dispatchers (e.g., DispatcherX86).</li>
*  </ul>
*
*  If an SMT solver is supplied a to the RiscOperators then that SMT solver will be used to answer various questions such as
*  when two memory addresses can alias one another.  When an SMT solver is lacking, the questions will be answered by very
*  naive comparison of the expression trees. */
namespace SymbolicSemantics {

typedef InsnSemanticsExpr::LeafNode LeafNode;
typedef InsnSemanticsExpr::LeafNodePtr LeafNodePtr;
typedef InsnSemanticsExpr::InternalNode InternalNode;
typedef InsnSemanticsExpr::InternalNodePtr InternalNodePtr;
typedef InsnSemanticsExpr::TreeNodePtr TreeNodePtr;
typedef std::set<SgAsmInstruction*> InsnSet;



/******************************************************************************************************************
 *                                      Value type
 ******************************************************************************************************************/

/** Smart pointer to an SValue object.  SValue objects are reference counted and should not be explicitly deleted. */
typedef BaseSemantics::Pointer<class SValue> SValuePtr;

/** Type of values manipulated by the SymbolicSemantics domain.
 *
 *  Values of type type are used whenever a value needs to be stored, such as memory addresses, the values stored at those
 *  addresses, the values stored in registers, the operands for RISC operations, and the results of those operations.
 *
 *  An SValue points to an expression composed of the TreeNode types defined in InsnSemanticsExpr.h, also stores the set of
 *  instructions that were used in defining the value.  This provides a framework for some simple forms of def-use
 *  analysis. See get_defining_instructions() for details.
 * 
 *  @section Unk_Uinit Unknown versus Uninitialized Values
 *
 *  One sometimes needs to distinguish between registers (or other named storage locations) that contain an
 *  "unknown" value versus registers that have not been initialized. By "unknown" we mean a value that has no
 *  constraints except for its size (e.g., a register that contains "any 32-bit value").  By "uninitialized" we
 *  mean a register that still contains the value that was placed there before we started symbolically evaluating
 *  instructions (i.e., a register to which symbolic evaluation hasn't written a new value).
 *
 *  An "unknown" value might be produced by a RISC operation that is unable/unwilling to express its result
 *  symbolically.  For instance, the RISC "add(A,B)" operation could return an unknown/unconstrained result if
 *  either A or B are unknown/unconstrained (in other words, add(A,B) returns C). An unconstrained value is
 *  represented by a free variable. ROSE's SymbolicSemantics RISC operations never return unconstrained values, but
 *  rather always return a new expression (add(A,B) returns the symbolic expression A+B). However, user-defined
 *  subclasses of ROSE's SymbolicSemantics classes might return unconstrained values, and in fact it is quite
 *  common for a programmer to first stub out all the RISC operations to return unconstrained values and then
 *  gradually implement them as they're required.  When a RISC operation returns an unconstrained value, it should
 *  set the returned value's defining instructions to the CPU instruction that caused the RISC operation to be
 *  called (and possibly the union of the sets of instructions that defined the RISC operation's operands).
 *
 *  An "uninitialized" register (or other storage location) is a register that hasn't ever had a value written to
 *  it as a side effect of a machine instruction, and thus still contains the value that was initialially stored
 *  there before analysis started (perhaps by a default constructor).  Such values will generally be unconstrained
 *  (i.e., "unknown" as defined above) but will have an empty defining instruction set.  The defining instruction
 *  set is empty because the register contains a value that was not generated as the result of simulating some
 *  machine instruction.
 *
 *  Therefore, it is possible to destinguish between an uninitialized register and an unconstrained register by
 *  looking at its value.  If the value is a variable with an empty set of defining instructions, then it must be
 *  an initial value.  If the value is a variable but has a non-empty set of defining instructions, then it must be
 *  a value that came from some RISC operation invoked on behalf of a machine instruction.
 *
 *  One should note that a register that contains a variable is not necessarily unconstrained: another register
 *  might contain the same variable, in which case the two registers are constrained to have the same value,
 *  although that value could be anything.  Consider the following example:
 *
 *  Step 1: Initialize registers. At this point EAX contains v1[32]{}, EBX contains v2[32]{}, and ECX contains
 *  v3[32]{}. The notation "vN" is a variable, "[32]" means the variable is 32-bits wide, and "{}" indicates that
 *  the set of defining instructions is empty. Since the defining sets are empty, the registers can be considered
 *  to be "uninitialized" (more specifically, they contain initial values that were created by the symbolic machine
 *  state constructor, or by the user explicitly initializing the registers; they don't contain values that were
 *  put there as a side effect of executing some machine instruction).
 *
 *  Step 2: Execute an x86 "I1: MOV EAX, EBX" instruction that moves the value stored in EBX into the EAX register.
 *  After this instruction executes, EAX contains v2[32]{I1}, EBX contains v2[32]{}, and ECX contains
 *  v3[32]{}. Registers EBX and ECX continue to have empty sets of defining instructions and thus contain their
 *  initial values.  Reigister EAX refers to the same variable (v2) as EBX and therefore must have the same value
 *  as EBX, although that value can be any 32-bit value.  We can also tell that EAX no longer contains its initial
 *  value because the set of defining instructions is non-empty ({I1}).
 *
 *  Step 3: Execute the imaginary "I2: FOO ECX, EAX" instruction and presume that it performs an operation using
 *  ECX and EAX and stores the result in ECX.  The operation is implemented by a new user-defined RISC operation
 *  called "doFoo(A,B)". Furthermore, presume that the operation encoded by doFoo(A,B) cannot be represented by
 *  ROSE's expression trees either directly or indirectly via other expression tree operations. Therefore, the
 *  implementation of doFoo(A,B) is such that it always returns an unconstrained value (i.e., a new variable):
 *  doFoo(A,B) returns C.  After this instruction executes, EAX and EBX continue to contain the results they had
 *  after step 2, and ECX now contains v4[32]{I2}.  We can tell that ECX contains an unknown value (because its
 *  value is a variable)  that is 32-bits wide.  We can also tell that ECX no longer contains its initial value
 *  because its set of defining instructions is non-empty ({I2}).
 */
class SValue: public BaseSemantics::SValue {
protected:
    /** The symbolic expression for this value.  Symbolic expressions are reference counted. */
    TreeNodePtr expr;

    /** Instructions defining this value.  Any instruction that saves the value to a register or memory location
     *  adds itself to the saved value. */
    InsnSet defs;

protected:
    // Protected constructors
    explicit SValue(size_t nbits): BaseSemantics::SValue(nbits) {
        expr = LeafNode::create_variable(nbits);
    }
    SValue(size_t nbits, uint64_t number): BaseSemantics::SValue(nbits) {
        expr = LeafNode::create_integer(nbits, number);
    }
    SValue(const TreeNodePtr &expr, const InsnSet &defs): BaseSemantics::SValue(expr->get_nbits()) {
        width = expr->get_nbits();
        this->expr = expr;
        this->defs = defs;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Static allocating constructors
public:
    /** Construct a prototypical value. Prototypical values are only used for their virtual constructors. */
    static SValuePtr instance() {
        return SValuePtr(new SValue(1));
    }

    /** Promote a base value to a SymbolicSemantics value.  The value @p v must have a SymbolicSemantics::SValue dynamic type. */
    static SValuePtr promote(const BaseSemantics::SValuePtr &v) { // hot
        SValuePtr retval = BaseSemantics::dynamic_pointer_cast<SValue>(v);
        assert(retval!=NULL);
        return retval;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Virtual allocating constructors
public:
    virtual BaseSemantics::SValuePtr undefined_(size_t nbits) const /*override*/ {
        return BaseSemantics::SValuePtr(new SValue(nbits));
    }
    virtual BaseSemantics::SValuePtr number_(size_t nbits, uint64_t value) const /*override*/ {
        return BaseSemantics::SValuePtr(new SValue(nbits, value));
    }
    virtual BaseSemantics::SValuePtr copy(size_t new_width=0) const /*override*/ {
        SValuePtr retval(new SValue(*this));
        if (new_width!=0 && new_width!=retval->get_width())
            retval->set_width(new_width);
        return retval;
    }

    /** Virtual allocating constructor. Constructs a new semantic value with full control over all aspects of the value. */
    virtual BaseSemantics::SValuePtr create(const TreeNodePtr &expr, const InsnSet &defs=InsnSet()) {
        return SValuePtr(new SValue(expr, defs));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Override virtual methods...
public:
    virtual bool may_equal(const BaseSemantics::SValuePtr &other, SMTSolver *solver=NULL) const /*override*/;
    virtual bool must_equal(const BaseSemantics::SValuePtr &other, SMTSolver *solver=NULL) const /*override*/;

    // It's not possible to change the size of a symbolic expression in place. That would require that we recursively change
    // the size of the InsnSemanticsExpr, which might be shared with many unrelated values whose size we don't want to affect.
    virtual void set_width(size_t nbits) /*override*/ {
        assert(nbits==get_width());
    }

    virtual bool is_number() const /*override*/ {
        return expr->is_known();
    }

    virtual uint64_t get_number() const /*override*/;

    virtual void print(std::ostream &output, BaseSemantics::PrintHelper *helper=NULL) const /*override*/;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Additional methods first declared in this class...
public:
    /** Adds instructions to the list of defining instructions.  Adds the specified instruction and defining sets into this
     *  value and returns a reference to this value. See also add_defining_instructions().
     *  @{ */
    virtual void defined_by(SgAsmInstruction *insn, const InsnSet &set1, const InsnSet &set2, const InsnSet &set3) {
        add_defining_instructions(set3);
        defined_by(insn, set1, set2);
    }
    virtual void defined_by(SgAsmInstruction *insn, const InsnSet &set1, const InsnSet &set2) {
        add_defining_instructions(set2);
        defined_by(insn, set1);
    }
    virtual void defined_by(SgAsmInstruction *insn, const InsnSet &set1) {
        add_defining_instructions(set1);
        defined_by(insn);
    }
    virtual void defined_by(SgAsmInstruction *insn) {
        add_defining_instructions(insn);
    }
    /** @} */

    /** Returns the expression stored in this value.  Expressions are reference counted; the reference count of the returned
     *  expression is not incremented. */
    virtual const TreeNodePtr& get_expression() const {
        return expr;
    }

    /** Changes the expression stored in the value.
     * @{ */
    virtual void set_expression(const TreeNodePtr &new_expr) {
        expr = new_expr;
    }
    virtual void set_expression(const SValuePtr &source) {
        set_expression(source->get_expression());
    }
    /** @} */

    /** Returns the set of instructions that defined this value.  The return value is a flattened lattice represented as a set.
     *  When analyzing this basic block starting with an initial default state:
     *
     *  @code
     *  1: mov eax, 2
     *  2: add eax, 1
     *  3: mov ebx, eax;
     *  4: mov ebx, 3
     *  @endcode
     *
     *  the defining set for EAX will be instructions {1, 2} and the defining set for EBX will be {4}.  Defining sets for other
     *  registers are the empty set. */
    virtual const InsnSet& get_defining_instructions() const {
        return defs;
    }

    /** Adds definitions to the list of defining instructions. Returns the number of items added that weren't already in the
     *  list of defining instructions.  @{ */
    virtual size_t add_defining_instructions(const InsnSet &to_add);
    virtual size_t add_defining_instructions(const SValuePtr &source) {
        return add_defining_instructions(source->get_defining_instructions());
    }
    virtual size_t add_defining_instructions(SgAsmInstruction *insn);
    /** @} */

    /** Set definint instructions.  This discards the old set of defining instructions and replaces it with the specified set.
     *  @{ */
    virtual void set_defining_instructions(const InsnSet &new_defs) {
        defs = new_defs;
    }
    virtual void set_defining_instructions(const SValuePtr &source) {
        set_defining_instructions(source->get_defining_instructions());
    }
    virtual void set_defining_instructions(SgAsmInstruction *insn);
    /** @} */
};



/******************************************************************************************************************
 *                                      Memory state
 ******************************************************************************************************************/

/** Smart pointer to a MemoryState object.  MemoryState objects are reference counted and should not be explicitly deleted. */
typedef boost::shared_ptr<class MemoryState> MemoryStatePtr;

/** Byte-addressable memory.
 *
 *  This class represents an entire state of memory via a list of memory cells.  The memory cell list is sorted in
 *  reverse chronological order and addresses that satisfy a "must-alias" predicate are pruned so that only the
 *  must recent such memory cell is in the table.
 *
 *  A memory write operation prunes away any existing memory cell that must-alias the newly written address, then
 *  adds a new memory cell to the front of the memory cell list.
 *
 *  A memory read operation scans the memory cell list and returns a McCarthy expression.  The read operates in two
 *  modes: a mode that returns a full McCarthy expression based on all memory cells in the cell list, or a mode
 *  that returns a pruned McCarthy expression consisting only of memory cells that may-alias the reading-from
 *  address.  The pruning mode is the default, but can be turned off by calling disable_read_pruning(). */
class MemoryState: public BaseSemantics::MemoryCellList {
protected:
    
    bool read_pruning;                      /**< Prune McCarthy expression for read operations. */

protected:
    // protected constructors, same as for the base class
    MemoryState(const BaseSemantics::MemoryCellPtr &protocell, const BaseSemantics::SValuePtr &protoval)
        : BaseSemantics::MemoryCellList(protocell, protoval), read_pruning(true) {}

public:
    /** Static allocating constructor.  This constructor uses BaseSemantics::MemoryCell as the cell type. */
    static  MemoryStatePtr instance(const BaseSemantics::SValuePtr &protoval) {
        BaseSemantics::MemoryCellPtr protocell = BaseSemantics::MemoryCell::instance(protoval, protoval);
        return MemoryStatePtr(new MemoryState(protocell, protoval));
    }

    /** Static allocating constructor. */
    static MemoryStatePtr instance(const BaseSemantics::MemoryCellPtr &protocell, const BaseSemantics::SValuePtr &protoval) {
        return MemoryStatePtr(new MemoryState(protocell, protoval));
    }

    virtual BaseSemantics::MemoryStatePtr create(const BaseSemantics::SValuePtr &protoval) const /*override*/ {
        return instance(protoval);
    }

    virtual BaseSemantics::MemoryStatePtr create(const BaseSemantics::MemoryCellPtr &protocell,
                                                 const BaseSemantics::SValuePtr &protoval) const /*override*/ {
        return instance(protocell, protoval);
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Methods that override the base class
public:
    /** Read a byte from memory.
     *
     *  In order to read a multi-byte value, use RiscOperators::readMemory(). */
    virtual BaseSemantics::SValuePtr readMemory(const BaseSemantics::SValuePtr &addr, const BaseSemantics::SValuePtr &dflt,
                                                size_t nbits, BaseSemantics::RiscOperators *ops) /*override*/;

    /** Write a byte to memory.
     *
     *  In order to write a multi-byte value, use RiscOperators::writeMemory(). */
    virtual void writeMemory(const BaseSemantics::SValuePtr &addr, const BaseSemantics::SValuePtr &value,
                             BaseSemantics::RiscOperators *ops) /*override*/;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Methods first declared in this class
public:
    /** Enables or disables pruning of the McCarthy expression for read operations.
     * @{ */
    virtual bool get_read_pruning() const { return read_pruning; }
    virtual void enable_read_pruning(bool b=true) { read_pruning = b; }
    virtual void disable_read_pruning() { read_pruning = false; }
    /** @} */

    /** Build a McCarthy expression to select the specified address from a list of memory cells. */
    virtual SValuePtr mccarthy(const CellList &cells, const SValuePtr &address);
};



/******************************************************************************************************************
 *                                      RISC Operators
 ******************************************************************************************************************/

/** Smart pointer to a RiscOperators object.  RiscOperators objects are reference counted and should not be explicitly
 *  deleted. */
typedef boost::shared_ptr<class RiscOperators> RiscOperatorsPtr;

/** Defines RISC operators for the SymbolicSemantics domain.
 *
 *  These RISC operators depend on functionality introduced into the SValue class hierarchy at the SymbolicSemantics::SValue
 *  level. Therfore, the prototypical value supplied to the constructor or present in the supplied state object must have a
 *  dynamic type which is a SymbolicSemantics::SValue.
 *
 *  Each RISC operator should return a newly allocated semantic value so that the caller can adjust definers for the result
 *  without affecting any of the inputs. For example, a no-op that returns its argument should be implemented like this:
 *
 * @code
 *  BaseSemantics::SValuePtr noop(const BaseSemantics::SValuePtr &arg) {
 *      return arg->copy();     //correct
 *      return arg;             //incorrect
 *  }
 * @endcode
 */
class RiscOperators: public BaseSemantics::RiscOperators {
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Protected constructors, same as those in the base class
protected:
    explicit RiscOperators(const BaseSemantics::SValuePtr &protoval, SMTSolver *solver=NULL)
        : BaseSemantics::RiscOperators(protoval, solver) {
        (void) SValue::promote(protoval); // make sure its dynamic type is a SymbolicSemantics::SValue
    }

    explicit RiscOperators(const BaseSemantics::StatePtr &state, SMTSolver *solver=NULL)
        : BaseSemantics::RiscOperators(state, solver) {
        (void) SValue::promote(state->get_protoval()); // values must have SymbolicSemantics::SValue dynamic type
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Public allocating constructors
public:
    /** Static allocating constructor.  Creates a new RiscOperators object and configures it to use semantic values and states
     * that are defaults for SymbolicSemantics. */
    static RiscOperatorsPtr instance(SMTSolver *solver=NULL) {
        BaseSemantics::SValuePtr protoval = SValue::instance();
        // FIXME: register state should probably be chosen based on an architecture [Robb Matzke 2013-03-01]
        BaseSemantics::RegisterStatePtr registers = BaseSemantics::RegisterStateX86::instance(protoval);
        BaseSemantics::MemoryStatePtr memory = MemoryState::instance(protoval);
        BaseSemantics::StatePtr state = BaseSemantics::State::instance(registers, memory);
        return RiscOperatorsPtr(new RiscOperators(state, solver));
    }

    /** Static allocating constructor.  An SMT solver may be specified as the second argument for convenience. See
     *  set_solver() for details. */
    static RiscOperatorsPtr instance(const BaseSemantics::SValuePtr &protoval, SMTSolver *solver=NULL) {
        return RiscOperatorsPtr(new RiscOperators(protoval, solver));
    }

    /** Static allocating constructor.  An SMT solver may be specified as the second argument for convenience. See set_solver()
     *  for details. */
    static RiscOperatorsPtr instance(const BaseSemantics::StatePtr &state, SMTSolver *solver=NULL) {
        return RiscOperatorsPtr(new RiscOperators(state, solver));
    }

    /** Virtual allocating constructor. */
    virtual BaseSemantics::RiscOperatorsPtr create(const BaseSemantics::SValuePtr &protoval,
                                                   SMTSolver *solver=NULL) const /*override*/ {
        return instance(protoval, solver);
    }

    /** Virtual allocating constructor. */
    virtual BaseSemantics::RiscOperatorsPtr create(const BaseSemantics::StatePtr &state,
                                                   SMTSolver *solver=NULL) const /*override*/ {
        return instance(state, solver);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Methods first introduced by this class
public:
    /** Convenience function to create a new symbolic semantic value having the specified expression and definers. This makes
     *  the RISC operator implementations less verbose.  We need to promote (dynamic cast) the prototypical value to a
     *  SymbolicSemantics::SValue in order to get to the methods that were introduced at that level of the class hierarchy. */
    virtual SValuePtr svalue(const TreeNodePtr &expr, const InsnSet &defs=InsnSet()) {
        BaseSemantics::SValuePtr newval = SValue::promote(protoval)->create(expr, defs);
        return SValue::promote(newval);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Override some non-virtual functions only to change their return type for the convenience of us not having to
    // constantly explicitly dynamic cast them to our own SValuePtr type.
public:
    SValuePtr number_(size_t nbits, uint64_t value) {
        return SValue::promote(protoval->number_(nbits, value));
    }
    SValuePtr true_() {
        return SValue::promote(protoval->true_());
    }
    SValuePtr false_() {
        return SValue::promote(protoval->false_());
    }
    SValuePtr undefined_(size_t nbits) {
        return SValue::promote(protoval->undefined_(nbits));
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Override methods from base class.  These are the RISC operators that are invoked by a Dispatcher.
public:
    virtual void interrupt(uint8_t inum) /*override*/;
    virtual void sysenter() /*override*/;
    virtual BaseSemantics::SValuePtr and_(const BaseSemantics::SValuePtr &a_,
                                          const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr or_(const BaseSemantics::SValuePtr &a_,
                                         const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr xor_(const BaseSemantics::SValuePtr &a_,
                                          const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr invert(const BaseSemantics::SValuePtr &a_) /*override*/;
    virtual BaseSemantics::SValuePtr extract(const BaseSemantics::SValuePtr &a_,
                                             size_t begin_bit, size_t end_bit) /*override*/;
    virtual BaseSemantics::SValuePtr concat(const BaseSemantics::SValuePtr &a_,
                                            const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr leastSignificantSetBit(const BaseSemantics::SValuePtr &a_) /*override*/;
    virtual BaseSemantics::SValuePtr mostSignificantSetBit(const BaseSemantics::SValuePtr &a_) /*override*/;
    virtual BaseSemantics::SValuePtr rotateLeft(const BaseSemantics::SValuePtr &a_,
                                                const BaseSemantics::SValuePtr &sa_) /*override*/;
    virtual BaseSemantics::SValuePtr rotateRight(const BaseSemantics::SValuePtr &a_,
                                                 const BaseSemantics::SValuePtr &sa_) /*override*/;
    virtual BaseSemantics::SValuePtr shiftLeft(const BaseSemantics::SValuePtr &a_,
                                               const BaseSemantics::SValuePtr &sa_) /*override*/;
    virtual BaseSemantics::SValuePtr shiftRight(const BaseSemantics::SValuePtr &a_,
                                                const BaseSemantics::SValuePtr &sa_) /*override*/;
    virtual BaseSemantics::SValuePtr shiftRightArithmetic(const BaseSemantics::SValuePtr &a_,
                                                          const BaseSemantics::SValuePtr &sa_) /*override*/;
    virtual BaseSemantics::SValuePtr equalToZero(const BaseSemantics::SValuePtr &a_) /*override*/;
    virtual BaseSemantics::SValuePtr ite(const BaseSemantics::SValuePtr &sel_,
                                         const BaseSemantics::SValuePtr &a_,
                                         const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr unsignedExtend(const BaseSemantics::SValuePtr &a_, size_t new_width) /*override*/;
    virtual BaseSemantics::SValuePtr signExtend(const BaseSemantics::SValuePtr &a_, size_t new_width) /*override*/;
    virtual BaseSemantics::SValuePtr add(const BaseSemantics::SValuePtr &a_,
                                         const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr addWithCarries(const BaseSemantics::SValuePtr &a_,
                                                    const BaseSemantics::SValuePtr &b_,
                                                    const BaseSemantics::SValuePtr &c_,
                                                    BaseSemantics::SValuePtr &carry_out/*out*/);
    virtual BaseSemantics::SValuePtr negate(const BaseSemantics::SValuePtr &a_) /*override*/;
    virtual BaseSemantics::SValuePtr signedDivide(const BaseSemantics::SValuePtr &a_,
                                                  const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr signedModulo(const BaseSemantics::SValuePtr &a_,
                                                  const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr signedMultiply(const BaseSemantics::SValuePtr &a_,
                                                    const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr unsignedDivide(const BaseSemantics::SValuePtr &a_,
                                                    const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr unsignedModulo(const BaseSemantics::SValuePtr &a_,
                                                    const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr unsignedMultiply(const BaseSemantics::SValuePtr &a_,
                                                      const BaseSemantics::SValuePtr &b_) /*override*/;
    virtual BaseSemantics::SValuePtr readMemory(X86SegmentRegister sg,
                                                const BaseSemantics::SValuePtr &addr,
                                                const BaseSemantics::SValuePtr &cond,
                                                size_t nbits) /*override*/;
    virtual void writeMemory(X86SegmentRegister sg,
                             const BaseSemantics::SValuePtr &addr,
                             const BaseSemantics::SValuePtr &data,
                             const BaseSemantics::SValuePtr &cond) /*override*/;
};

} /*namespace*/
} /*namespace*/
} /*namespace*/

#endif
