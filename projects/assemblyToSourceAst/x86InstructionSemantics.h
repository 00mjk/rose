#ifndef ROSE_X86INSTRUCTIONSEMANTICS_H
#define ROSE_X86INSTRUCTIONSEMANTICS_H

#include "rose.h"
#include "semanticsModule.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include "integerOps.h"

static inline X86SegmentRegister getSegregFromMemoryReference(SgAsmMemoryReferenceExpression* mr) {
  X86SegmentRegister segreg = x86_segreg_none;
  SgAsmx86RegisterReferenceExpression* seg = isSgAsmx86RegisterReferenceExpression(mr->get_segment());
  if (seg) {
    ROSE_ASSERT (seg->get_register_class() == x86_regclass_segment);
    segreg = (X86SegmentRegister)(seg->get_register_number());
  } else {
    ROSE_ASSERT (!"Bad segment expr");
  }
  if (segreg == x86_segreg_none) segreg = x86_segreg_ds;
  return segreg;
}

#ifdef Word
#error "Having a macro called \"Word\" conflicts with x86InstructionSemantics.h"
#endif

template <typename Policy, template <size_t> class WordType>
struct X86InstructionSemantics {
#define Word(Len) WordType<(Len)>
  Policy& policy;

  X86InstructionSemantics(Policy& policy): policy(policy) {}

  template <size_t Len>
  Word(Len) invertMaybe(const Word(Len)& w, bool inv) {
    if (inv) {
      return policy.invert(w);
    } else {
      return w;
    }
  }

  template <size_t Len>
  Word(Len) number(uintmax_t v) {
    return policy.template number<Len>(v);
  }

  template <size_t From, size_t To, size_t Len>
  Word(To - From) extract(Word(Len) w) {
    return policy.template extract<From, To>(w);
  }

  template <size_t From, size_t To>
  Word(To) signExtend(Word(From) w) {
    return policy.template signExtend<From, To>(w);
  }

  template <size_t Len>
  Word(1) greaterOrEqualToTen(Word(Len) w) {
    Word(Len) carries = number<Len>(0);
    policy.addWithCarries(w, number<Len>(6), policy.false_(), carries);
    return extract<Len - 1, Len>(carries);
  }

  template <size_t Len> // In bits
  Word(Len) readMemory(X86SegmentRegister segreg, const Word(32)& addr, Word(1) cond) {
    return policy.template readMemory<Len>(segreg, addr, cond);
  }

  Word(32) readEffectiveAddress(SgAsmExpression* expr) {
    assert (isSgAsmMemoryReferenceExpression(expr));
    return read32(isSgAsmMemoryReferenceExpression(expr)->get_address());
  }

  Word(8) read8(SgAsmExpression* e) {
    switch (e->variantT()) {
      case V_SgAsmx86RegisterReferenceExpression: {
        SgAsmx86RegisterReferenceExpression* rre = isSgAsmx86RegisterReferenceExpression(e);
        switch (rre->get_register_class()) {
          case x86_regclass_gpr: {
            X86GeneralPurposeRegister reg = (X86GeneralPurposeRegister)(rre->get_register_number());
            Word(32) rawValue = policy.readGPR(reg);
            switch (rre->get_position_in_register()) {
              case x86_regpos_low_byte: return extract<0, 8>(rawValue);
              case x86_regpos_high_byte: return extract<8, 16>(rawValue);
              default: ROSE_ASSERT (!"Bad position in register");
            }
          }
          default: fprintf(stderr, "Bad register class %s\n", regclassToString(rre->get_register_class())); abort();
        }
        break;
      }
      case V_SgAsmBinaryAdd: {
        return policy.add(read8(isSgAsmBinaryAdd(e)->get_lhs()), read8(isSgAsmBinaryAdd(e)->get_rhs()));
      }
      case V_SgAsmBinaryMultiply: {
        SgAsmByteValueExpression* rhs = isSgAsmByteValueExpression(isSgAsmBinaryMultiply(e)->get_rhs());
        ROSE_ASSERT (rhs);
        SgAsmExpression* lhs = isSgAsmBinaryMultiply(e)->get_lhs();
        return extract<0, 8>(policy.unsignedMultiply(read8(lhs), read8(rhs)));
      }
      case V_SgAsmMemoryReferenceExpression: {
        return readMemory<8>(getSegregFromMemoryReference(isSgAsmMemoryReferenceExpression(e)), readEffectiveAddress(e), policy.true_());
      }
      case V_SgAsmByteValueExpression:
      case V_SgAsmWordValueExpression:
      case V_SgAsmDoubleWordValueExpression:
      case V_SgAsmQuadWordValueExpression: {
        uint64_t val = SageInterface::getAsmConstant(isSgAsmValueExpression(e));
        return number<8>(val & 0xFFU);
      }
      default: fprintf(stderr, "Bad variant %s in read8\n", e->class_name().c_str()); abort();
    }
  }

  Word(16) read16(SgAsmExpression* e) {
    switch (e->variantT()) {
      case V_SgAsmx86RegisterReferenceExpression: {
        SgAsmx86RegisterReferenceExpression* rre = isSgAsmx86RegisterReferenceExpression(e);
        ROSE_ASSERT (rre->get_position_in_register() == x86_regpos_word);
        switch (rre->get_register_class()) {
          case x86_regclass_gpr: {
            X86GeneralPurposeRegister reg = (X86GeneralPurposeRegister)(rre->get_register_number());
            Word(32) rawValue = policy.readGPR(reg);
            return extract<0, 16>(rawValue);
          }
          case x86_regclass_segment: {
            X86SegmentRegister sr = (X86SegmentRegister)(rre->get_register_number());
            Word(16) value = policy.readSegreg(sr);
            return value;
          }
          default: fprintf(stderr, "Bad register class %s\n", regclassToString(rre->get_register_class())); abort();
        }
        break;
      }
      case V_SgAsmBinaryAdd: {
        return policy.add(read16(isSgAsmBinaryAdd(e)->get_lhs()), read16(isSgAsmBinaryAdd(e)->get_rhs()));
      }
      case V_SgAsmBinaryMultiply: {
        SgAsmByteValueExpression* rhs = isSgAsmByteValueExpression(isSgAsmBinaryMultiply(e)->get_rhs());
        ROSE_ASSERT (rhs);
        SgAsmExpression* lhs = isSgAsmBinaryMultiply(e)->get_lhs();
        return extract<0, 16>(policy.unsignedMultiply(read16(lhs), read8(rhs)));
      }
      case V_SgAsmMemoryReferenceExpression: {
        return readMemory<16>(getSegregFromMemoryReference(isSgAsmMemoryReferenceExpression(e)), readEffectiveAddress(e), policy.true_());
      }
      case V_SgAsmByteValueExpression:
      case V_SgAsmWordValueExpression:
      case V_SgAsmDoubleWordValueExpression:
      case V_SgAsmQuadWordValueExpression: {
        uint64_t val = SageInterface::getAsmConstant(isSgAsmValueExpression(e));
        return number<16>(val & 0xFFFFU);
      }
      default: fprintf(stderr, "Bad variant %s in read16\n", e->class_name().c_str()); abort();
    }
  }

  Word(32) read32(SgAsmExpression* e) {
    switch (e->variantT()) {
      case V_SgAsmx86RegisterReferenceExpression: {
        SgAsmx86RegisterReferenceExpression* rre = isSgAsmx86RegisterReferenceExpression(e);
        ROSE_ASSERT (rre->get_position_in_register() == x86_regpos_dword || rre->get_position_in_register() == x86_regpos_all);
        switch (rre->get_register_class()) {
          case x86_regclass_gpr: {
            X86GeneralPurposeRegister reg = (X86GeneralPurposeRegister)(rre->get_register_number());
            Word(32) rawValue = policy.readGPR(reg);
            return rawValue;
          }
          case x86_regclass_segment: {
            X86SegmentRegister sr = (X86SegmentRegister)(rre->get_register_number());
            Word(16) value = policy.readSegreg(sr);
            return policy.concat(value, number<16>(0));
          }
          default: fprintf(stderr, "Bad register class %s\n", regclassToString(rre->get_register_class())); abort();
        }
        break;
      }
      case V_SgAsmBinaryAdd: {
        return policy.add(read32(isSgAsmBinaryAdd(e)->get_lhs()), read32(isSgAsmBinaryAdd(e)->get_rhs()));
      }
      case V_SgAsmBinaryMultiply: {
        SgAsmByteValueExpression* rhs = isSgAsmByteValueExpression(isSgAsmBinaryMultiply(e)->get_rhs());
        ROSE_ASSERT (rhs);
        SgAsmExpression* lhs = isSgAsmBinaryMultiply(e)->get_lhs();
        return extract<0, 32>(policy.unsignedMultiply(read32(lhs), read8(rhs)));
      }
      case V_SgAsmMemoryReferenceExpression: {
        return readMemory<32>(getSegregFromMemoryReference(isSgAsmMemoryReferenceExpression(e)), readEffectiveAddress(e), policy.true_());
      }
      case V_SgAsmByteValueExpression:
      case V_SgAsmWordValueExpression:
      case V_SgAsmDoubleWordValueExpression:
      case V_SgAsmQuadWordValueExpression: {
        uint64_t val = SageInterface::getAsmConstant(isSgAsmValueExpression(e));
        return number<32>(val & 0xFFFFFFFFU);
      }
      default: fprintf(stderr, "Bad variant %s in read32\n", e->class_name().c_str()); abort();
    }
  }

  void updateGPRLowByte(X86GeneralPurposeRegister reg, const Word(8)& value) {
    Word(32) oldValue = policy.readGPR(reg);
    policy.writeGPR(reg, policy.concat(value, extract<8, 32>(oldValue)));
  }
  
  void updateGPRHighByte(X86GeneralPurposeRegister reg, const Word(8)& value) {
    Word(32) oldValue = policy.readGPR(reg);
    policy.writeGPR(reg, policy.concat(extract<0, 8>(oldValue), policy.concat(value, extract<16, 32>(oldValue))));
  }

  void updateGPRLowWord(X86GeneralPurposeRegister reg, const Word(16)& value) {
    Word(32) oldValue = policy.readGPR(reg);
    policy.writeGPR(reg, policy.concat(value, extract<16, 32>(oldValue)));
  }
  
  void write8(SgAsmExpression* e, const Word(8)& value) {
    switch (e->variantT()) {
      case V_SgAsmx86RegisterReferenceExpression: {
        SgAsmx86RegisterReferenceExpression* rre = isSgAsmx86RegisterReferenceExpression(e);
        switch (rre->get_register_class()) {
          case x86_regclass_gpr: {
            X86GeneralPurposeRegister reg = (X86GeneralPurposeRegister)(rre->get_register_number());
            switch (rre->get_position_in_register()) {
              case x86_regpos_low_byte: updateGPRLowByte(reg, value); break;
              case x86_regpos_high_byte: updateGPRHighByte(reg, value); break;
              default: ROSE_ASSERT (!"Bad position in register");
            }
            break;
          }
          default: fprintf(stderr, "Bad register class %s\n", regclassToString(rre->get_register_class())); abort();
        }
        break;
      }
      case V_SgAsmMemoryReferenceExpression: {
        policy.writeMemory(getSegregFromMemoryReference(isSgAsmMemoryReferenceExpression(e)), readEffectiveAddress(e), value, policy.true_());
        break;
      }
      default: fprintf(stderr, "Bad variant %s in write8\n", e->class_name().c_str()); abort();
    }
  }

  void write16(SgAsmExpression* e, const Word(16)& value) {
    switch (e->variantT()) {
      case V_SgAsmx86RegisterReferenceExpression: {
        SgAsmx86RegisterReferenceExpression* rre = isSgAsmx86RegisterReferenceExpression(e);
        switch (rre->get_register_class()) {
          case x86_regclass_gpr: {
            X86GeneralPurposeRegister reg = (X86GeneralPurposeRegister)(rre->get_register_number());
            switch (rre->get_position_in_register()) {
              case x86_regpos_word: updateGPRLowWord(reg, value); break;
              default: ROSE_ASSERT (!"Bad position in register");
            }
            break;
          }
          case x86_regclass_segment: {
            X86SegmentRegister sr = (X86SegmentRegister)(rre->get_register_number());
            policy.writeSegreg(sr, value);
            break;
          }
          default: fprintf(stderr, "Bad register class %s\n", regclassToString(rre->get_register_class())); abort();
        }
        break;
      }
      case V_SgAsmMemoryReferenceExpression: {
        policy.writeMemory(getSegregFromMemoryReference(isSgAsmMemoryReferenceExpression(e)), readEffectiveAddress(e), value, policy.true_());
        break;
      }
      default: fprintf(stderr, "Bad variant %s in write16\n", e->class_name().c_str()); abort();
    }
  }

  void write32(SgAsmExpression* e, const Word(32)& value) {
    switch (e->variantT()) {
      case V_SgAsmx86RegisterReferenceExpression: {
        SgAsmx86RegisterReferenceExpression* rre = isSgAsmx86RegisterReferenceExpression(e);
        switch (rre->get_register_class()) {
          case x86_regclass_gpr: {
            X86GeneralPurposeRegister reg = (X86GeneralPurposeRegister)(rre->get_register_number());
            switch (rre->get_position_in_register()) {
              case x86_regpos_dword:
              case x86_regpos_all: {
                break;
              }
              default: ROSE_ASSERT (!"Bad position in register");
            }
            policy.writeGPR(reg, value);
            break;
          }
          case x86_regclass_segment: { // Used for pop of segment registers
            X86SegmentRegister sr = (X86SegmentRegister)(rre->get_register_number());
            policy.writeSegreg(sr, extract<0, 16>(value));
            break;
          }
          default: fprintf(stderr, "Bad register class %s\n", regclassToString(rre->get_register_class())); abort();
        }
        break;
      }
      case V_SgAsmMemoryReferenceExpression: {
        policy.writeMemory(getSegregFromMemoryReference(isSgAsmMemoryReferenceExpression(e)), readEffectiveAddress(e), value, policy.true_());
        break;
      }
      default: fprintf(stderr, "Bad variant %s in write32\n", e->class_name().c_str()); abort();
    }
  }

  Word(1) parity(Word(8) w) {
    Word(1) p01 = policy.xor_(extract<0, 1>(w), extract<1, 2>(w));
    Word(1) p23 = policy.xor_(extract<2, 3>(w), extract<3, 4>(w));
    Word(1) p45 = policy.xor_(extract<4, 5>(w), extract<5, 6>(w));
    Word(1) p67 = policy.xor_(extract<6, 7>(w), extract<7, 8>(w));
    Word(1) p0123 = policy.xor_(p01, p23);
    Word(1) p4567 = policy.xor_(p45, p67);
    return policy.invert(policy.xor_(p0123, p4567));
  }

  template <size_t Len>
  void setFlagsForResult(const Word(Len)& result) {
    policy.writeFlag(x86_flag_pf, parity(extract<0, 8>(result)));
    policy.writeFlag(x86_flag_sf, extract<Len - 1, Len>(result));
    policy.writeFlag(x86_flag_zf, policy.equalToZero(result));
  }

  template <size_t Len>
  void setFlagsForResult(const Word(Len)& result, Word(1) cond) {
    policy.writeFlag(x86_flag_pf, policy.ite(cond, parity(extract<0, 8>(result)), policy.readFlag(x86_flag_pf)));
    policy.writeFlag(x86_flag_sf, policy.ite(cond, extract<Len - 1, Len>(result), policy.readFlag(x86_flag_sf)));
    policy.writeFlag(x86_flag_zf, policy.ite(cond, policy.equalToZero(result), policy.readFlag(x86_flag_zf)));
  }

  template <size_t Len>
  Word(Len) doAddOperation(const Word(Len)& a, const Word(Len)& b, bool invertCarries, Word(1) carryIn) { // Does add (subtract with two's complement input and invertCarries set), and sets correct flags; only does this if cond is true
    Word(Len) carries = number<Len>(0);
    Word(Len) result = policy.addWithCarries(a, b, invertMaybe(carryIn, invertCarries), carries);
    setFlagsForResult<Len>(result);
    policy.writeFlag(x86_flag_af, invertMaybe(extract<3, 4>(carries), invertCarries));
    policy.writeFlag(x86_flag_cf, invertMaybe(extract<Len - 1, Len>(carries), invertCarries));
    policy.writeFlag(x86_flag_of, policy.xor_(extract<Len - 1, Len>(carries), extract<Len - 2, Len - 1>(carries)));
    return result;
  }

  template <size_t Len>
  Word(Len) doAddOperation(const Word(Len)& a, const Word(Len)& b, bool invertCarries, Word(1) carryIn, Word(1) cond) { // Does add (subtract with two's complement input and invertCarries set), and sets correct flags; only does this if cond is true
    Word(Len) carries = number<Len>(0);
    Word(Len) result = policy.addWithCarries(a, b, invertMaybe(carryIn, invertCarries), carries);
    setFlagsForResult<Len>(result, cond);
    policy.writeFlag(x86_flag_af, policy.ite(cond, invertMaybe(extract<3, 4>(carries), invertCarries), policy.readFlag(x86_flag_af)));
    policy.writeFlag(x86_flag_cf, policy.ite(cond, invertMaybe(extract<Len - 1, Len>(carries), invertCarries), policy.readFlag(x86_flag_cf)));
    policy.writeFlag(x86_flag_of, policy.ite(cond, policy.xor_(extract<Len - 1, Len>(carries), extract<Len - 2, Len - 1>(carries)), policy.readFlag(x86_flag_of)));
    return result;
  }

  template <size_t Len>
  Word(Len) doIncOperation(const Word(Len)& a, bool dec, bool setCarry) { // Does inc (dec with dec set), and sets correct flags
    Word(Len) carries = number<Len>(0);
    Word(Len) result = policy.addWithCarries(a, number<Len>(dec ? -1 : 1), policy.false_(), carries);
    setFlagsForResult<Len>(result);
    policy.writeFlag(x86_flag_af, invertMaybe(extract<3, 4>(carries), dec));
    policy.writeFlag(x86_flag_of, policy.xor_(extract<Len - 1, Len>(carries), extract<Len - 2, Len - 1>(carries)));
    if (setCarry) {
      policy.writeFlag(x86_flag_cf, invertMaybe(extract<Len - 1, Len>(carries), dec));
    }
    return result;
  }

  void translate(SgAsmx86Instruction* insn) {
    // fprintf(stderr, "%s\n", unparseInstructionWithAddress(insn).c_str());
    policy.writeIP(number<32>((unsigned int)(insn->get_address() + insn->get_raw_bytes().size())));
    X86InstructionKind kind = insn->get_kind();
    const SgAsmExpressionPtrList& operands = insn->get_operandList()->get_operands();
    switch (kind) {
      case x86_mov: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: write8(operands[0], read8(operands[1])); break;
          case 2: write16(operands[0], read16(operands[1])); break;
          case 4: write32(operands[0], read32(operands[1])); break;
          default: ROSE_ASSERT ("Bad size"); break;
        }
        break;
      }
      case x86_xchg: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {Word(8) temp = read8(operands[1]); write8(operands[1], read8(operands[0])); write8(operands[0], temp); break;}
          case 2: {Word(16) temp = read16(operands[1]); write16(operands[1], read16(operands[0])); write16(operands[0], temp); break;}
          case 4: {Word(32) temp = read32(operands[1]); write32(operands[1], read32(operands[0])); write32(operands[0], temp); break;}
          default: ROSE_ASSERT ("Bad size"); break;
        }
        break;
      }
      case x86_movzx: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 2: write16(operands[0], policy.concat(read8(operands[1]), number<8>(0))); break;
          case 4: {
            switch (numBytesInAsmType(operands[1]->get_type())) {
              case 1: write32(operands[0], policy.concat(read8(operands[1]), number<24>(0))); break;
              case 2: write32(operands[0], policy.concat(read16(operands[1]), number<16>(0))); break;
              default: ROSE_ASSERT ("Bad size");
            }
            break;
          }
          default: ROSE_ASSERT ("Bad size"); break;
        }
        break;
      }
      case x86_movsx: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 2: {
            Word(8) op1 = read8(operands[1]);
            Word(16) result = signExtend<8, 16>(op1);
            write16(operands[0], result);
            break;
          }
          case 4: {
            switch (numBytesInAsmType(operands[1]->get_type())) {
              case 1: {
                Word(8) op1 = read8(operands[1]);
                Word(32) result = signExtend<8, 32>(op1);
                write32(operands[0], result);
                break;
              }
              case 2: {
                Word(16) op1 = read16(operands[1]);
                Word(32) result = signExtend<16, 32>(op1);
                write32(operands[0], result);
                break;
              }
              default: ROSE_ASSERT ("Bad size");
            }
            break;
          }
          default: ROSE_ASSERT ("Bad size"); break;
        }
        break;
      }
      case x86_cbw: {
        ROSE_ASSERT (operands.size() == 0);
        updateGPRLowWord(x86_gpr_ax, signExtend<8, 16>(extract<0, 8>(policy.readGPR(x86_gpr_ax))));
        break;
      }
      case x86_cwde: {
        ROSE_ASSERT (operands.size() == 0);
        policy.writeGPR(x86_gpr_ax, signExtend<16, 32>(extract<0, 16>(policy.readGPR(x86_gpr_ax))));
        break;
      }
      case x86_cwd: {
        ROSE_ASSERT (operands.size() == 0);
        updateGPRLowWord(x86_gpr_dx, extract<16, 32>(signExtend<16, 32>(extract<0, 16>(policy.readGPR(x86_gpr_ax)))));
        break;
      }
      case x86_cdq: {
        ROSE_ASSERT (operands.size() == 0);
        policy.writeGPR(x86_gpr_dx, extract<32, 64>(signExtend<32, 64>(policy.readGPR(x86_gpr_ax))));
        break;
      }
      case x86_lea: {
        ROSE_ASSERT (operands.size() == 2);
        write32(operands[0], readEffectiveAddress(operands[1]));
        break;
      }
      case x86_and: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = policy.and_(read8(operands[0]), read8(operands[1]));
            setFlagsForResult<8>(result);
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = policy.and_(read16(operands[0]), read16(operands[1]));
            setFlagsForResult<16>(result);
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = policy.and_(read32(operands[0]), read32(operands[1]));
            setFlagsForResult<32>(result);
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        policy.writeFlag(x86_flag_of, policy.false_());
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.false_());
        break;
      }
      case x86_or: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = policy.or_(read8(operands[0]), read8(operands[1]));
            setFlagsForResult<8>(result);
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = policy.or_(read16(operands[0]), read16(operands[1]));
            setFlagsForResult<16>(result);
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = policy.or_(read32(operands[0]), read32(operands[1]));
            setFlagsForResult<32>(result);
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        policy.writeFlag(x86_flag_of, policy.false_());
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.false_());
        break;
      }
      case x86_test: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = policy.and_(read8(operands[0]), read8(operands[1]));
            setFlagsForResult<8>(result);
            break;
          }
          case 2: {
            Word(16) result = policy.and_(read16(operands[0]), read16(operands[1]));
            setFlagsForResult<16>(result);
            break;
          }
          case 4: {
            Word(32) result = policy.and_(read32(operands[0]), read32(operands[1]));
            setFlagsForResult<32>(result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        policy.writeFlag(x86_flag_of, policy.false_());
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.false_());
        break;
      }
      case x86_xor: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = policy.xor_(read8(operands[0]), read8(operands[1]));
            setFlagsForResult<8>(result);
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = policy.xor_(read16(operands[0]), read16(operands[1]));
            setFlagsForResult<16>(result);
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = policy.xor_(read32(operands[0]), read32(operands[1]));
            setFlagsForResult<32>(result);
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        policy.writeFlag(x86_flag_of, policy.false_());
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.false_());
        break;
      }
      case x86_not: {
        ROSE_ASSERT (operands.size() == 1);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = policy.invert(read8(operands[0]));
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = policy.invert(read16(operands[0]));
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = policy.invert(read32(operands[0]));
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_add: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = doAddOperation<8>(read8(operands[0]), read8(operands[1]), false, policy.false_());
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = doAddOperation<16>(read16(operands[0]), read16(operands[1]), false, policy.false_());
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = doAddOperation<32>(read32(operands[0]), read32(operands[1]), false, policy.false_());
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_adc: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = doAddOperation<8>(read8(operands[0]), read8(operands[1]), false, policy.readFlag(x86_flag_cf));
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = doAddOperation<16>(read16(operands[0]), read16(operands[1]), false, policy.readFlag(x86_flag_cf));
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = doAddOperation<32>(read32(operands[0]), read32(operands[1]), false, policy.readFlag(x86_flag_cf));
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_sub: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = doAddOperation<8>(read8(operands[0]), policy.invert(read8(operands[1])), true, policy.false_());
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = doAddOperation<16>(read16(operands[0]), policy.invert(read16(operands[1])), true, policy.false_());
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = doAddOperation<32>(read32(operands[0]), policy.invert(read32(operands[1])), true, policy.false_());
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_sbb: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = doAddOperation<8>(read8(operands[0]), policy.invert(read8(operands[1])), true, policy.readFlag(x86_flag_cf));
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = doAddOperation<16>(read16(operands[0]), policy.invert(read16(operands[1])), true, policy.readFlag(x86_flag_cf));
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = doAddOperation<32>(read32(operands[0]), policy.invert(read32(operands[1])), true, policy.readFlag(x86_flag_cf));
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_cmp: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            doAddOperation<8>(read8(operands[0]), policy.invert(read8(operands[1])), true, policy.false_());
            break;
          }
          case 2: {
            doAddOperation<16>(read16(operands[0]), policy.invert(read16(operands[1])), true, policy.false_());
            break;
          }
          case 4: {
            doAddOperation<32>(read32(operands[0]), policy.invert(read32(operands[1])), true, policy.false_());
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_neg: {
        ROSE_ASSERT (operands.size() == 1);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = doAddOperation<8>(number<8>(0), policy.invert(read8(operands[0])), true, policy.false_());
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = doAddOperation<16>(number<16>(0), policy.invert(read16(operands[0])), true, policy.false_());
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = doAddOperation<32>(number<32>(0), policy.invert(read32(operands[0])), true, policy.false_());
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_inc: {
        ROSE_ASSERT (operands.size() == 1);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = doIncOperation<8>(read8(operands[0]), false, false);
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = doIncOperation<16>(read16(operands[0]), false, false);
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = doIncOperation<32>(read32(operands[0]), false, false);
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_dec: {
        ROSE_ASSERT (operands.size() == 1);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) result = doIncOperation<8>(read8(operands[0]), true, false);
            write8(operands[0], result);
            break;
          }
          case 2: {
            Word(16) result = doIncOperation<16>(read16(operands[0]), true, false);
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) result = doIncOperation<32>(read32(operands[0]), true, false);
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_cmpxchg: {
        ROSE_ASSERT (operands.size() == 2);
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) op0 = read8(operands[0]);
            Word(8) oldAx = extract<0, 8>(policy.readGPR(x86_gpr_ax));
            doAddOperation<8>(oldAx, policy.invert(op0), true, policy.false_());
            write8(operands[0], policy.ite(policy.readFlag(x86_flag_zf), read8(operands[1]), op0));
            updateGPRLowByte(x86_gpr_ax, policy.ite(policy.readFlag(x86_flag_zf), oldAx, op0));
            break;
          }
          case 2: {
            Word(16) op0 = read16(operands[0]);
            Word(16) oldAx = extract<0, 16>(policy.readGPR(x86_gpr_ax));
            doAddOperation<16>(oldAx, policy.invert(op0), true, policy.false_());
            write16(operands[0], policy.ite(policy.readFlag(x86_flag_zf), read16(operands[1]), op0));
            updateGPRLowWord(x86_gpr_ax, policy.ite(policy.readFlag(x86_flag_zf), oldAx, op0));
            break;
          }
          case 4: {
            Word(32) op0 = read32(operands[0]);
            Word(32) oldAx = policy.readGPR(x86_gpr_ax);
            doAddOperation<32>(oldAx, policy.invert(op0), true, policy.false_());
            write32(operands[0], policy.ite(policy.readFlag(x86_flag_zf), read32(operands[1]), op0));
            policy.writeGPR(x86_gpr_ax, policy.ite(policy.readFlag(x86_flag_zf), oldAx, op0));
            break;
          }
          default: ROSE_ASSERT (!"Bad size"); break;
        }
        break;
      }
      case x86_shl: {
        Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
        Word(1) shiftCountZero = policy.equalToZero(shiftCount);
        policy.writeFlag(x86_flag_af, policy.ite(shiftCountZero, policy.readFlag(x86_flag_af), policy.undefined_())); // Undefined
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) op = read8(operands[0]);
            Word(8) output = policy.shiftLeft(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<7, 8>(policy.shiftLeft(op, policy.add(shiftCount, number<5>(7)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), policy.xor_(extract<7, 8>(output), newCf)));
            write8(operands[0], output);
            setFlagsForResult<8>(output, policy.invert(shiftCountZero));
            break;
          }
          case 2: {
            Word(16) op = read16(operands[0]);
            Word(16) output = policy.shiftLeft(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<15, 16>(policy.shiftLeft(op, policy.add(shiftCount, number<5>(15)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), policy.xor_(extract<15, 16>(output), newCf)));
            write16(operands[0], output);
            setFlagsForResult<16>(output, policy.invert(shiftCountZero));
            break;
          }
          case 4: {
            Word(32) op = read32(operands[0]);
            Word(32) output = policy.shiftLeft(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<31, 32>(policy.shiftLeft(op, policy.add(shiftCount, number<5>(31)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), policy.xor_(extract<31, 32>(output), newCf)));
            write32(operands[0], output);
            setFlagsForResult<32>(output, policy.invert(shiftCountZero));
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }
      case x86_shr: {
        Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
        Word(1) shiftCountZero = policy.equalToZero(shiftCount);
        policy.writeFlag(x86_flag_af, policy.ite(shiftCountZero, policy.readFlag(x86_flag_af), policy.undefined_())); // Undefined
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) op = read8(operands[0]);
            Word(8) output = policy.shiftRight(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<0, 1>(policy.shiftRight(op, policy.add(shiftCount, number<5>(7)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), extract<7, 8>(op)));
            write8(operands[0], output);
            setFlagsForResult<8>(output, policy.invert(shiftCountZero));
            break;
          }
          case 2: {
            Word(16) op = read16(operands[0]);
            Word(16) output = policy.shiftRight(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<0, 1>(policy.shiftRight(op, policy.add(shiftCount, number<5>(15)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), extract<15, 16>(op)));
            write16(operands[0], output);
            setFlagsForResult<16>(output, policy.invert(shiftCountZero));
            break;
          }
          case 4: {
            Word(32) op = read32(operands[0]);
            Word(32) output = policy.shiftRight(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<0, 1>(policy.shiftRight(op, policy.add(shiftCount, number<5>(31)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), extract<31, 32>(op)));
            write32(operands[0], output);
            setFlagsForResult<32>(output, policy.invert(shiftCountZero));
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }
      case x86_sar: {
        Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
        Word(1) shiftCountZero = policy.equalToZero(shiftCount);
        Word(1) shiftCountNotZero = policy.invert(shiftCountZero);
        policy.writeFlag(x86_flag_af, policy.ite(shiftCountZero, policy.readFlag(x86_flag_af), policy.undefined_())); // Undefined
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) op = read8(operands[0]);
            Word(8) output = policy.shiftRightArithmetic(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<0, 1>(policy.shiftRight(op, policy.add(shiftCount, number<5>(7)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), policy.false_())); // No change with sc = 0, clear when sc = 1, undefined otherwise
            write8(operands[0], output);
            setFlagsForResult<8>(output, shiftCountNotZero);
            break;
          }
          case 2: {
            Word(16) op = read16(operands[0]);
            Word(16) output = policy.shiftRightArithmetic(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<0, 1>(policy.shiftRight(op, policy.add(shiftCount, number<5>(15)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), policy.false_())); // No change with sc = 0, clear when sc = 1, undefined otherwise
            write16(operands[0], output);
            setFlagsForResult<16>(output, shiftCountNotZero);
            break;
          }
          case 4: {
            Word(32) op = read32(operands[0]);
            Word(32) output = policy.shiftRightArithmetic(op, shiftCount);
            Word(1) newCf = policy.ite(shiftCountZero, policy.readFlag(x86_flag_cf), extract<0, 1>(policy.shiftRight(op, policy.add(shiftCount, number<5>(31)))));
            policy.writeFlag(x86_flag_cf, newCf);
            policy.writeFlag(x86_flag_of, policy.ite(shiftCountZero, policy.readFlag(x86_flag_of), policy.false_())); // No change with sc = 0, clear when sc = 1, undefined otherwise
            write32(operands[0], output);
            setFlagsForResult<32>(output, shiftCountNotZero);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }
      case x86_rol: {
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) op = read8(operands[0]);
            Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
            Word(8) output = policy.rotateLeft(op, shiftCount);
            policy.writeFlag(x86_flag_cf, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_cf), extract<0, 1>(output)));
            policy.writeFlag(x86_flag_of, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_of), policy.xor_(extract<0, 1>(output), extract<7, 8>(output))));
            write8(operands[0], output);
            break;
          }
          case 2: {
            Word(16) op = read16(operands[0]);
            Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
            Word(16) output = policy.rotateLeft(op, shiftCount);
            policy.writeFlag(x86_flag_cf, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_cf), extract<0, 1>(output)));
            policy.writeFlag(x86_flag_of, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_of), policy.xor_(extract<0, 1>(output), extract<15, 16>(output))));
            write16(operands[0], output);
            break;
          }
          case 4: {
            Word(32) op = read32(operands[0]);
            Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
            Word(32) output = policy.rotateLeft(op, shiftCount);
            policy.writeFlag(x86_flag_cf, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_cf), extract<0, 1>(output)));
            policy.writeFlag(x86_flag_of, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_of), policy.xor_(extract<0, 1>(output), extract<31, 32>(output))));
            write32(operands[0], output);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }
      case x86_ror: {
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) op = read8(operands[0]);
            Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
            Word(8) output = policy.rotateRight(op, shiftCount);
            policy.writeFlag(x86_flag_cf, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_cf), extract<7, 8>(output)));
            policy.writeFlag(x86_flag_of, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_of), policy.xor_(extract<6, 7>(output), extract<7, 8>(output))));
            write8(operands[0], output);
            break;
          }
          case 2: {
            Word(16) op = read16(operands[0]);
            Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
            Word(16) output = policy.rotateRight(op, shiftCount);
            policy.writeFlag(x86_flag_cf, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_cf), extract<15, 16>(output)));
            policy.writeFlag(x86_flag_of, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_of), policy.xor_(extract<14, 15>(output), extract<15, 16>(output))));
            write16(operands[0], output);
            break;
          }
          case 4: {
            Word(32) op = read32(operands[0]);
            Word(5) shiftCount = extract<0, 5>(read8(operands[1]));
            Word(32) output = policy.rotateRight(op, shiftCount);
            policy.writeFlag(x86_flag_cf, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_cf), extract<31, 32>(output)));
            policy.writeFlag(x86_flag_of, policy.ite(policy.equalToZero(shiftCount), policy.readFlag(x86_flag_of), policy.xor_(extract<30, 31>(output), extract<31, 32>(output))));
            write32(operands[0], output);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }
      case x86_shld: {
        Word(5) shiftCount = extract<0, 5>(read8(operands[2]));
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 2: {
            Word(16) op1 = read16(operands[0]);
            Word(16) op2 = read16(operands[1]);
            Word(16) output1 = policy.shiftLeft(op1, shiftCount);
            Word(16) output2 = policy.ite(policy.equalToZero(shiftCount),
                                          number<16>(0),
                                          policy.shiftRight(op2, policy.negate(shiftCount)));
            Word(16) output = policy.or_(output1, output2);
            Word(1) newCf = policy.ite(policy.equalToZero(shiftCount),
                                       policy.readFlag(x86_flag_cf),
                                       extract<15, 16>(policy.shiftLeft(op1, policy.add(shiftCount, number<5>(15)))));
            policy.writeFlag(x86_flag_cf, newCf);
            Word(1) newOf = policy.ite(policy.equalToZero(shiftCount),
                                       policy.readFlag(x86_flag_of), 
                                       policy.xor_(extract<15, 16>(output), newCf));
            policy.writeFlag(x86_flag_of, newOf);
            write16(operands[0], output);
            setFlagsForResult<16>(output);
            policy.writeFlag(x86_flag_af, policy.ite(policy.equalToZero(shiftCount),
                                                     policy.readFlag(x86_flag_af),
                                                     policy.undefined_())); // Undefined
            break;
          }
          case 4: {
            Word(32) op1 = read32(operands[0]);
            Word(32) op2 = read32(operands[1]);
            Word(5) shiftCount = extract<0, 5>(read8(operands[2]));
            Word(32) output1 = policy.shiftLeft(op1, shiftCount);
            Word(32) output2 = policy.ite(policy.equalToZero(shiftCount),
                                          number<32>(0),
                                          policy.shiftRight(op2, policy.negate(shiftCount)));
            Word(32) output = policy.or_(output1, output2);
            Word(1) newCf = policy.ite(policy.equalToZero(shiftCount),
                                       policy.readFlag(x86_flag_cf),
                                       extract<31, 32>(policy.shiftLeft(op1, policy.add(shiftCount, number<5>(31)))));
            policy.writeFlag(x86_flag_cf, newCf);
            Word(1) newOf = policy.ite(policy.equalToZero(shiftCount),
                                       policy.readFlag(x86_flag_of), 
                                       policy.xor_(extract<31, 32>(output), newCf));
            policy.writeFlag(x86_flag_of, newOf);
            write32(operands[0], output);
            setFlagsForResult<32>(output);
            policy.writeFlag(x86_flag_af, policy.ite(policy.equalToZero(shiftCount),
                                                     policy.readFlag(x86_flag_af),
                                                     policy.undefined_())); // Undefined
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }
      case x86_shrd: {
        Word(5) shiftCount = extract<0, 5>(read8(operands[2]));
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 2: {
            Word(16) op1 = read16(operands[0]);
            Word(16) op2 = read16(operands[1]);
            Word(16) output1 = policy.shiftRight(op1, shiftCount);
            Word(16) output2 = policy.ite(policy.equalToZero(shiftCount),
                                          number<16>(0),
                                          policy.shiftLeft(op2, policy.negate(shiftCount)));
            Word(16) output = policy.or_(output1, output2);
            Word(1) newCf = policy.ite(policy.equalToZero(shiftCount),
                                       policy.readFlag(x86_flag_cf),
                                       extract<0, 1>(policy.shiftRight(op1, policy.add(shiftCount, number<5>(15)))));
            policy.writeFlag(x86_flag_cf, newCf);
            Word(1) newOf = policy.ite(policy.equalToZero(shiftCount),
                                       policy.readFlag(x86_flag_of), 
                                       policy.xor_(extract<15, 16>(output),
                                                   extract<15, 16>(op1)));
            policy.writeFlag(x86_flag_of, newOf);
            write16(operands[0], output);
            setFlagsForResult<16>(output);
            policy.writeFlag(x86_flag_af, policy.ite(policy.equalToZero(shiftCount),
                                                     policy.readFlag(x86_flag_af),
                                                     policy.undefined_())); // Undefined
            break;
          }
          case 4: {
            Word(32) op1 = read32(operands[0]);
            Word(32) op2 = read32(operands[1]);
            Word(32) output1 = policy.shiftRight(op1, shiftCount);
            Word(32) output2 = policy.ite(policy.equalToZero(shiftCount),
                                          number<32>(0),
                                          policy.shiftLeft(op2, policy.negate(shiftCount)));
            Word(32) output = policy.or_(output1, output2);
            Word(1) newCf = policy.ite(policy.equalToZero(shiftCount),
                                       policy.readFlag(x86_flag_cf),
                                       extract<0, 1>(policy.shiftRight(op1, policy.add(shiftCount, number<5>(31)))));
            policy.writeFlag(x86_flag_cf, newCf);
            Word(1) newOf = policy.ite(policy.equalToZero(shiftCount),
                                       policy.readFlag(x86_flag_of), 
                                       policy.xor_(extract<31, 32>(output),
                                                   extract<31, 32>(op1)));
            policy.writeFlag(x86_flag_of, newOf);
            write32(operands[0], output);
            setFlagsForResult<32>(output);
            policy.writeFlag(x86_flag_af, policy.ite(policy.equalToZero(shiftCount),
                                                     policy.readFlag(x86_flag_af),
                                                     policy.undefined_())); // Undefined
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }
      case x86_bsf: {
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.undefined_()); // Undefined
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 2: {
            Word(16) op = read16(operands[1]);
            policy.writeFlag(x86_flag_zf, policy.equalToZero(op));
            Word(16) result = policy.ite(policy.readFlag(x86_flag_zf),
                                         read16(operands[0]),
                                         policy.leastSignificantSetBit(op));
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) op = read32(operands[1]);
              policy.writeFlag(x86_flag_zf, policy.equalToZero(op));
              Word(32) result = policy.ite(policy.readFlag(x86_flag_zf),
                                           read32(operands[0]),
                                           policy.leastSignificantSetBit(op));
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }
      case x86_bsr: {
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.undefined_()); // Undefined
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 2: {
            Word(16) op = read16(operands[1]);
            policy.writeFlag(x86_flag_zf, policy.equalToZero(op));
            Word(16) result = policy.ite(policy.readFlag(x86_flag_zf),
                                         read16(operands[0]),
                                         policy.mostSignificantSetBit(op));
            write16(operands[0], result);
            break;
          }
          case 4: {
            Word(32) op = read32(operands[1]);
            policy.writeFlag(x86_flag_zf, policy.equalToZero(op));
            Word(32) result = policy.ite(policy.readFlag(x86_flag_zf),
                                         read32(operands[0]),
                                         policy.mostSignificantSetBit(op));
            write32(operands[0], result);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        break;
      }

      case x86_bts: {
        ROSE_ASSERT (operands.size() == 2);
        // All flags except CF are undefined
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_zf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        if (isSgAsmMemoryReferenceExpression(operands[0]) && isSgAsmx86RegisterReferenceExpression(operands[1])) { // Special case allowing multi-word offsets into memory
          Word(32) addr = readEffectiveAddress(operands[0]);
          int numBytes = numBytesInAsmType(operands[1]->get_type());
          Word(32) bitnum = numBytes == 2 ? signExtend<16, 32>(read16(operands[1])) : read32(operands[1]);
          Word(32) adjustedAddr = policy.add(addr, signExtend<29, 32>(extract<3, 32>(bitnum)));
          Word(8) val = readMemory<8>(getSegregFromMemoryReference(isSgAsmMemoryReferenceExpression(operands[0])), adjustedAddr, policy.true_());
          Word(1) bitval = extract<0, 1>(policy.rotateRight(val, extract<0, 3>(bitnum)));
          Word(8) result = policy.or_(val, policy.rotateLeft(number<8>(1), extract<0, 3>(bitnum)));
          policy.writeFlag(x86_flag_cf, bitval);
          policy.writeMemory(getSegregFromMemoryReference(isSgAsmMemoryReferenceExpression(operands[0])), adjustedAddr, result, policy.true_());
        } else { // Simple case
          switch (numBytesInAsmType(operands[0]->get_type())) {
            case 2: {
              Word(16) op0 = read16(operands[0]);
              Word(4) bitnum = extract<0, 4>(read16(operands[1]));
              Word(1) bitval = extract<0, 1>(policy.rotateRight(op0, bitnum));
              Word(16) result = policy.or_(op0, policy.rotateLeft(number<16>(1), bitnum));
              policy.writeFlag(x86_flag_cf, bitval);
              write16(operands[0], result);
              break;
            }
            case 4: {
              Word(32) op0 = read32(operands[0]);
              Word(5) bitnum = extract<0, 5>(read32(operands[1]));
              Word(1) bitval = extract<0, 1>(policy.rotateRight(op0, bitnum));
              Word(32) result = policy.or_(op0, policy.rotateLeft(number<32>(1), bitnum));
              policy.writeFlag(x86_flag_cf, bitval);
              write32(operands[0], result);
              break;
            }
            default: ROSE_ASSERT (!"Bad size");
          }
        }
        break;
      }

      case x86_imul: {
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) op0 = extract<0, 8>(policy.readGPR(x86_gpr_ax));
            Word(8) op1 = read8(operands[0]);
            Word(16) mulResult = policy.signedMultiply(op0, op1);
            updateGPRLowWord(x86_gpr_ax, mulResult);
            Word(1) carry = policy.invert(policy.or_(policy.equalToZero(policy.invert(extract<7, 16>(mulResult))), policy.equalToZero(extract<7, 16>(mulResult))));
            policy.writeFlag(x86_flag_cf, carry);
            policy.writeFlag(x86_flag_of, carry);
            break;
          }
          case 2: {
            Word(16) op0 = operands.size() == 1 ? extract<0, 16>(policy.readGPR(x86_gpr_ax)) : read16(operands[operands.size() - 2]);
            Word(16) op1 = read16(operands[operands.size() - 1]);
            Word(32) mulResult = policy.signedMultiply(op0, op1);
            if (operands.size() == 1) {
              updateGPRLowWord(x86_gpr_ax, extract<0, 16>(mulResult));
              updateGPRLowWord(x86_gpr_dx, extract<16, 32>(mulResult));
            } else {
              write16(operands[0], extract<0, 16>(mulResult));
            }
            Word(1) carry = policy.invert(policy.or_(policy.equalToZero(policy.invert(extract<7, 32>(mulResult))), policy.equalToZero(extract<7, 32>(mulResult))));
            policy.writeFlag(x86_flag_cf, carry);
            policy.writeFlag(x86_flag_of, carry);
            break;
          }
          case 4: {
            Word(32) op0 = operands.size() == 1 ? policy.readGPR(x86_gpr_ax) : read32(operands[operands.size() - 2]);
            Word(32) op1 = read32(operands[operands.size() - 1]);
            Word(64) mulResult = policy.signedMultiply(op0, op1);
            if (operands.size() == 1) {
              policy.writeGPR(x86_gpr_ax, extract<0, 32>(mulResult));
              policy.writeGPR(x86_gpr_dx, extract<32, 64>(mulResult));
            } else {
              write32(operands[0], extract<0, 32>(mulResult));
            }
            Word(1) carry = policy.invert(policy.or_(policy.equalToZero(policy.invert(extract<7, 64>(mulResult))), policy.equalToZero(extract<7, 64>(mulResult))));
            policy.writeFlag(x86_flag_cf, carry);
            policy.writeFlag(x86_flag_of, carry);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_zf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        break;
      }
      case x86_mul: {
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(8) op0 = extract<0, 8>(policy.readGPR(x86_gpr_ax));
            Word(8) op1 = read8(operands[0]);
            Word(16) mulResult = policy.unsignedMultiply(op0, op1);
            updateGPRLowWord(x86_gpr_ax, mulResult);
            Word(1) carry = policy.invert(policy.equalToZero(extract<8, 16>(mulResult)));
            policy.writeFlag(x86_flag_cf, carry);
            policy.writeFlag(x86_flag_of, carry);
            break;
          }
          case 2: {
            Word(16) op0 = extract<0, 16>(policy.readGPR(x86_gpr_ax));
            Word(16) op1 = read16(operands[0]);
            Word(32) mulResult = policy.unsignedMultiply(op0, op1);
            updateGPRLowWord(x86_gpr_ax, extract<0, 16>(mulResult));
            updateGPRLowWord(x86_gpr_dx, extract<16, 32>(mulResult));
            Word(1) carry = policy.invert(policy.equalToZero(extract<16, 32>(mulResult)));
            policy.writeFlag(x86_flag_cf, carry);
            policy.writeFlag(x86_flag_of, carry);
            break;
          }
          case 4: {
            Word(32) op0 = policy.readGPR(x86_gpr_ax);
            Word(32) op1 = read32(operands[0]);
            Word(64) mulResult = policy.unsignedMultiply(op0, op1);
            policy.writeGPR(x86_gpr_ax, extract<0, 32>(mulResult));
            policy.writeGPR(x86_gpr_dx, extract<32, 64>(mulResult));
            Word(1) carry = policy.invert(policy.equalToZero(extract<32, 64>(mulResult)));
            policy.writeFlag(x86_flag_cf, carry);
            policy.writeFlag(x86_flag_of, carry);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_zf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        break;
      }
      case x86_idiv: {
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(16) op0 = extract<0, 16>(policy.readGPR(x86_gpr_ax));
            Word(8) op1 = read8(operands[0]);
            // if op1 == 0, we should trap
            Word(16) divResult = policy.signedDivide(op0, op1);
            Word(8) modResult = policy.signedModulo(op0, op1);
            // if result overflows, we should trap
            updateGPRLowWord(x86_gpr_ax, policy.concat(extract<0, 8>(divResult), modResult));
            break;
          }
          case 2: {
            Word(32) op0 = policy.concat(extract<0, 16>(policy.readGPR(x86_gpr_ax)), extract<0, 16>(policy.readGPR(x86_gpr_dx)));
            Word(16) op1 = read16(operands[0]);
            // if op1 == 0, we should trap
            Word(32) divResult = policy.signedDivide(op0, op1);
            Word(16) modResult = policy.signedModulo(op0, op1);
            // if result overflows, we should trap
            updateGPRLowWord(x86_gpr_ax, extract<0, 16>(divResult));
            updateGPRLowWord(x86_gpr_dx, modResult);
            break;
          }
          case 4: {
            Word(64) op0 = policy.concat(policy.readGPR(x86_gpr_ax), policy.readGPR(x86_gpr_dx));
            Word(32) op1 = read32(operands[0]);
            // if op1 == 0, we should trap
            Word(64) divResult = policy.signedDivide(op0, op1);
            Word(32) modResult = policy.signedModulo(op0, op1);
            // if result overflows, we should trap
            policy.writeGPR(x86_gpr_ax, extract<0, 32>(divResult));
            policy.writeGPR(x86_gpr_dx, modResult);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_zf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        break;
      }
      case x86_div: {
        switch (numBytesInAsmType(operands[0]->get_type())) {
          case 1: {
            Word(16) op0 = extract<0, 16>(policy.readGPR(x86_gpr_ax));
            Word(8) op1 = read8(operands[0]);
            // if op1 == 0, we should trap
            Word(16) divResult = policy.unsignedDivide(op0, op1);
            Word(8) modResult = policy.unsignedModulo(op0, op1);
            // if extract<8, 16> of divResult is non-zero (overflow), we should trap
            updateGPRLowWord(x86_gpr_ax, policy.concat(extract<0, 8>(divResult), modResult));
            break;
          }
          case 2: {
            Word(32) op0 = policy.concat(extract<0, 16>(policy.readGPR(x86_gpr_ax)), extract<0, 16>(policy.readGPR(x86_gpr_dx)));
            Word(16) op1 = read16(operands[0]);
            // if op1 == 0, we should trap
            Word(32) divResult = policy.unsignedDivide(op0, op1);
            Word(16) modResult = policy.unsignedModulo(op0, op1);
            // if extract<16, 32> of divResult is non-zero (overflow), we should trap
            updateGPRLowWord(x86_gpr_ax, extract<0, 16>(divResult));
            updateGPRLowWord(x86_gpr_dx, modResult);
            break;
          }
          case 4: {
            Word(64) op0 = policy.concat(policy.readGPR(x86_gpr_ax), policy.readGPR(x86_gpr_dx));
            Word(32) op1 = read32(operands[0]);
            // if op1 == 0, we should trap
            Word(64) divResult = policy.unsignedDivide(op0, op1);
            Word(32) modResult = policy.unsignedModulo(op0, op1);
            // if extract<32, 64> of divResult is non-zero (overflow), we should trap
            policy.writeGPR(x86_gpr_ax, extract<0, 32>(divResult));
            policy.writeGPR(x86_gpr_dx, modResult);
            break;
          }
          default: ROSE_ASSERT (!"Bad size");
        }
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_zf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        break;
      }

      case x86_aaa: {
        ROSE_ASSERT (operands.size() == 0);
        Word(1) incAh = policy.or_(policy.readFlag(x86_flag_af),
                                   greaterOrEqualToTen(extract<0, 4>(policy.readGPR(x86_gpr_ax))));
        updateGPRLowWord(x86_gpr_ax,
          policy.concat(
            policy.add(policy.ite(incAh, number<4>(6), number<4>(0)),
                       extract<0, 4>(policy.readGPR(x86_gpr_ax))),
            policy.concat(
              number<4>(0),
              policy.add(policy.ite(incAh, number<8>(1), number<8>(0)),
                         extract<8, 16>(policy.readGPR(x86_gpr_ax))))));
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_zf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, incAh);
        policy.writeFlag(x86_flag_cf, incAh);
        break;
      }

      case x86_aas: {
        ROSE_ASSERT (operands.size() == 0);
        Word(1) decAh = policy.or_(policy.readFlag(x86_flag_af),
                                   greaterOrEqualToTen(extract<0, 4>(policy.readGPR(x86_gpr_ax))));
        updateGPRLowWord(x86_gpr_ax,
          policy.concat(
            policy.add(policy.ite(decAh, number<4>(-6), number<4>(0)),
                       extract<0, 4>(policy.readGPR(x86_gpr_ax))),
            policy.concat(
              number<4>(0),
              policy.add(policy.ite(decAh, number<8>(-1), number<8>(0)),
                         extract<8, 16>(policy.readGPR(x86_gpr_ax))))));
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_sf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_zf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_pf, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, decAh);
        policy.writeFlag(x86_flag_cf, decAh);
        break;
      }

      case x86_aam: {
        ROSE_ASSERT (operands.size() == 1);
        Word(8) al = extract<0, 8>(policy.readGPR(x86_gpr_ax));
        Word(8) divisor = read8(operands[0]);
        Word(8) newAh = policy.unsignedDivide(al, divisor);
        Word(8) newAl = policy.unsignedModulo(al, divisor);
        updateGPRLowWord(x86_gpr_ax, policy.concat(newAl, newAh));
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.undefined_()); // Undefined
        setFlagsForResult<8>(newAl);
        break;
      }

      case x86_aad: {
        ROSE_ASSERT (operands.size() == 1);
        Word(8) al = extract<0, 8>(policy.readGPR(x86_gpr_ax));
        Word(8) ah = extract<8, 16>(policy.readGPR(x86_gpr_ax));
        Word(8) divisor = read8(operands[0]);
        Word(8) newAl = policy.add(al, extract<0, 8>(policy.unsignedMultiply(ah, divisor)));
        updateGPRLowWord(x86_gpr_ax, policy.concat(newAl, number<8>(0)));
        policy.writeFlag(x86_flag_of, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_af, policy.undefined_()); // Undefined
        policy.writeFlag(x86_flag_cf, policy.undefined_()); // Undefined
        setFlagsForResult<8>(newAl);
        break;
      }

      case x86_bswap: {
        ROSE_ASSERT (operands.size() == 1);
        Word(32) oldVal = read32(operands[0]);
        Word(32) newVal = policy.concat(extract<24, 32>(oldVal), policy.concat(extract<16, 24>(oldVal), policy.concat(extract<8, 16>(oldVal), extract<0, 8>(oldVal))));
        write32(operands[0], newVal);
        break;
      }

      case x86_push: {
        ROSE_ASSERT (operands.size() == 1);
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        ROSE_ASSERT (insn->get_operandSize() == x86_insnsize_32);
        Word(32) oldSp = policy.readGPR(x86_gpr_sp);
        Word(32) newSp = policy.add(oldSp, number<32>(-4));
        policy.writeMemory(x86_segreg_ss, newSp, read32(operands[0]), policy.true_());
        policy.writeGPR(x86_gpr_sp, newSp);
        break;
      }
      case x86_pushad: {
        ROSE_ASSERT (operands.size() == 0);
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        Word(32) oldSp = policy.readGPR(x86_gpr_sp);
        Word(32) newSp = policy.add(oldSp, number<32>(-32));
        policy.writeMemory(x86_segreg_ss, newSp, policy.readGPR(x86_gpr_di), policy.true_());
        policy.writeMemory(x86_segreg_ss, policy.add(newSp, number<32>(4)), policy.readGPR(x86_gpr_si), policy.true_());
        policy.writeMemory(x86_segreg_ss, policy.add(newSp, number<32>(8)), policy.readGPR(x86_gpr_bp), policy.true_());
        policy.writeMemory(x86_segreg_ss, policy.add(newSp, number<32>(12)), oldSp, policy.true_());
        policy.writeMemory(x86_segreg_ss, policy.add(newSp, number<32>(16)), policy.readGPR(x86_gpr_bx), policy.true_());
        policy.writeMemory(x86_segreg_ss, policy.add(newSp, number<32>(20)), policy.readGPR(x86_gpr_dx), policy.true_());
        policy.writeMemory(x86_segreg_ss, policy.add(newSp, number<32>(24)), policy.readGPR(x86_gpr_cx), policy.true_());
        policy.writeMemory(x86_segreg_ss, policy.add(newSp, number<32>(28)), policy.readGPR(x86_gpr_ax), policy.true_());
        policy.writeGPR(x86_gpr_sp, newSp);
        break;
      }
      case x86_pop: {
        ROSE_ASSERT (operands.size() == 1);
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        ROSE_ASSERT (insn->get_operandSize() == x86_insnsize_32);
        Word(32) oldSp = policy.readGPR(x86_gpr_sp);
        Word(32) newSp = policy.add(oldSp, number<32>(4));
        write32(operands[0], readMemory<32>(x86_segreg_ss, oldSp, policy.true_()));
        policy.writeGPR(x86_gpr_sp, newSp);
        break;
      }
      case x86_popad: {
        ROSE_ASSERT (operands.size() == 0);
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        Word(32) oldSp = policy.readGPR(x86_gpr_sp);
        Word(32) newSp = policy.add(oldSp, number<32>(32));
        policy.writeGPR(x86_gpr_di, readMemory<32>(x86_segreg_ss, oldSp, policy.true_()));
        policy.writeGPR(x86_gpr_si, readMemory<32>(x86_segreg_ss, policy.add(oldSp, number<32>(4)), policy.true_()));
        policy.writeGPR(x86_gpr_bp, readMemory<32>(x86_segreg_ss, policy.add(oldSp, number<32>(8)), policy.true_()));
        policy.writeGPR(x86_gpr_bx, readMemory<32>(x86_segreg_ss, policy.add(oldSp, number<32>(16)), policy.true_()));
        policy.writeGPR(x86_gpr_dx, readMemory<32>(x86_segreg_ss, policy.add(oldSp, number<32>(20)), policy.true_()));
        policy.writeGPR(x86_gpr_cx, readMemory<32>(x86_segreg_ss, policy.add(oldSp, number<32>(24)), policy.true_()));
        policy.writeGPR(x86_gpr_ax, readMemory<32>(x86_segreg_ss, policy.add(oldSp, number<32>(28)), policy.true_()));
        readMemory<32>(x86_segreg_ss, policy.add(oldSp, number<32>(12)), policy.true_()); // Read and ignore old SP
        policy.writeGPR(x86_gpr_sp, newSp);
        break;
      }
      case x86_leave: {
        ROSE_ASSERT (operands.size() == 0);
        policy.writeGPR(x86_gpr_sp, policy.readGPR(x86_gpr_bp));
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        ROSE_ASSERT (insn->get_operandSize() == x86_insnsize_32);
        Word(32) oldSp = policy.readGPR(x86_gpr_sp);
        Word(32) newSp = policy.add(oldSp, number<32>(4));
        policy.writeGPR(x86_gpr_bp, readMemory<32>(x86_segreg_ss, oldSp, policy.true_()));
        policy.writeGPR(x86_gpr_sp, newSp);
        break;
      }
      case x86_call: {
        ROSE_ASSERT (operands.size() == 1);
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        ROSE_ASSERT (insn->get_operandSize() == x86_insnsize_32);
        Word(32) oldSp = policy.readGPR(x86_gpr_sp);
        Word(32) newSp = policy.add(oldSp, number<32>(-4));
        policy.writeMemory(x86_segreg_ss, newSp, policy.readIP(), policy.true_());
        policy.writeIP(policy.filterCallTarget(read32(operands[0])));
        policy.writeGPR(x86_gpr_sp, newSp);
        break;
      }
      case x86_ret: {
        ROSE_ASSERT (operands.size() <= 1);
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        ROSE_ASSERT (insn->get_operandSize() == x86_insnsize_32);
        Word(32) extraBytes = (operands.size() == 1 ? read32(operands[0]) : number<32>(0));
        Word(32) oldSp = policy.readGPR(x86_gpr_sp);
        Word(32) newSp = policy.add(oldSp, policy.add(number<32>(4), extraBytes));
        policy.writeIP(policy.filterReturnTarget(readMemory<32>(x86_segreg_ss, oldSp, policy.true_())));
        policy.writeGPR(x86_gpr_sp, newSp);
        break;
      }

      case x86_loop: {
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        ROSE_ASSERT (insn->get_operandSize() == x86_insnsize_32);
        ROSE_ASSERT (operands.size() == 1);
        Word(32) oldCx = policy.readGPR(x86_gpr_cx);
        Word(32) newCx = policy.add(number<32>(-1), oldCx);
        policy.writeGPR(x86_gpr_cx, newCx);
        Word(1) doLoop = policy.invert(policy.equalToZero(newCx));
        policy.writeIP(policy.ite(doLoop, read32(operands[0]), policy.readIP()));
        break;
      }
      case x86_loopz: {
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        ROSE_ASSERT (insn->get_operandSize() == x86_insnsize_32);
        ROSE_ASSERT (operands.size() == 1);
        Word(32) oldCx = policy.readGPR(x86_gpr_cx);
        Word(32) newCx = policy.add(number<32>(-1), oldCx);
        policy.writeGPR(x86_gpr_cx, newCx);
        Word(1) doLoop = policy.and_(policy.invert(policy.equalToZero(newCx)), policy.readFlag(x86_flag_zf));
        policy.writeIP(policy.ite(doLoop, read32(operands[0]), policy.readIP()));
        break;
      }
      case x86_loopnz: {
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32);
        ROSE_ASSERT (insn->get_operandSize() == x86_insnsize_32);
        ROSE_ASSERT (operands.size() == 1);
        Word(32) oldCx = policy.readGPR(x86_gpr_cx);
        Word(32) newCx = policy.add(number<32>(-1), oldCx);
        policy.writeGPR(x86_gpr_cx, newCx);
        Word(1) doLoop = policy.and_(policy.invert(policy.equalToZero(newCx)), policy.invert(policy.readFlag(x86_flag_zf)));
        policy.writeIP(policy.ite(doLoop, read32(operands[0]), policy.readIP()));
        break;
      }

      case x86_jmp: {
        ROSE_ASSERT (operands.size() == 1);
        policy.writeIP(policy.filterIndirectJumpTarget(read32(operands[0])));
        break;
      }
#define FLAGCOMBO_ne policy.invert(policy.readFlag(x86_flag_zf))
#define FLAGCOMBO_e policy.readFlag(x86_flag_zf)
#define FLAGCOMBO_no policy.invert(policy.readFlag(x86_flag_of))
#define FLAGCOMBO_o policy.readFlag(x86_flag_of)
#define FLAGCOMBO_ns policy.invert(policy.readFlag(x86_flag_sf))
#define FLAGCOMBO_s policy.readFlag(x86_flag_sf)
#define FLAGCOMBO_po policy.invert(policy.readFlag(x86_flag_pf))
#define FLAGCOMBO_pe policy.readFlag(x86_flag_pf)
#define FLAGCOMBO_ae policy.invert(policy.readFlag(x86_flag_cf))
#define FLAGCOMBO_b policy.readFlag(x86_flag_cf)
#define FLAGCOMBO_be policy.or_(FLAGCOMBO_b, FLAGCOMBO_e)
#define FLAGCOMBO_a policy.and_(FLAGCOMBO_ae, FLAGCOMBO_ne)
#define FLAGCOMBO_l policy.xor_(policy.readFlag(x86_flag_sf), policy.readFlag(x86_flag_of))
#define FLAGCOMBO_ge policy.invert(policy.xor_(policy.readFlag(x86_flag_sf), policy.readFlag(x86_flag_of)))
#define FLAGCOMBO_le policy.or_(FLAGCOMBO_e, FLAGCOMBO_l)
#define FLAGCOMBO_g policy.and_(FLAGCOMBO_ge, FLAGCOMBO_ne)
#define FLAGCOMBO_cxz policy.equalToZero(extract<0, 16>(policy.readGPR(x86_gpr_cx)))
#define FLAGCOMBO_ecxz policy.equalToZero(policy.readGPR(x86_gpr_cx))
#define JUMP(tag) case x86_j##tag: {ROSE_ASSERT(operands.size() == 1); policy.writeIP(policy.ite(FLAGCOMBO_##tag, read32(operands[0]), policy.readIP())); break;}
      JUMP(ne)
      JUMP(e)
      JUMP(no)
      JUMP(o)
      JUMP(po)
      JUMP(pe)
      JUMP(ns)
      JUMP(s)
      JUMP(ae)
      JUMP(b)
      JUMP(be)
      JUMP(a)
      JUMP(le)
      JUMP(g)
      JUMP(ge)
      JUMP(l)
      JUMP(cxz)
      JUMP(ecxz)
#undef JUMP
#define SET(tag) case x86_set##tag: {ROSE_ASSERT (operands.size() == 1); write8(operands[0], policy.concat(FLAGCOMBO_##tag, number<7>(0))); break;}
      SET(ne)
      SET(e)
      SET(no)
      SET(o)
      SET(po)
      SET(pe)
      SET(ns)
      SET(s)
      SET(ae)
      SET(b)
      SET(be)
      SET(a)
      SET(le)
      SET(g)
      SET(ge)
      SET(l)
#undef SET
#define CMOV(tag) case x86_cmov##tag: { \
                    ROSE_ASSERT (operands.size() == 2); \
                    switch (numBytesInAsmType(operands[0]->get_type())) { \
                      case 2: write16(operands[0], policy.ite(FLAGCOMBO_##tag, read16(operands[1]), read16(operands[0]))); break; \
                      case 4: write32(operands[0], policy.ite(FLAGCOMBO_##tag, read32(operands[1]), read32(operands[0]))); break; \
                      default: ROSE_ASSERT ("Bad size"); break; \
                    } \
                    break; \
                  }
      CMOV(ne)
      CMOV(e)
      CMOV(no)
      CMOV(o)
      CMOV(po)
      CMOV(pe)
      CMOV(ns)
      CMOV(s)
      CMOV(ae)
      CMOV(b)
      CMOV(be)
      CMOV(a)
      CMOV(le)
      CMOV(g)
      CMOV(ge)
      CMOV(l)
#undef CMOV
#undef FLAGCOMBO_ne
#undef FLAGCOMBO_e
#undef FLAGCOMBO_ns
#undef FLAGCOMBO_s
#undef FLAGCOMBO_ae
#undef FLAGCOMBO_b
#undef FLAGCOMBO_be
#undef FLAGCOMBO_a
#undef FLAGCOMBO_l
#undef FLAGCOMBO_ge
#undef FLAGCOMBO_le
#undef FLAGCOMBO_g
#undef FLAGCOMBO_cxz
#undef FLAGCOMBO_ecxz
      case x86_cld: {
        ROSE_ASSERT (operands.size() == 0);
        policy.writeFlag(x86_flag_df, policy.false_());
        break;
      }
      case x86_std: {
        ROSE_ASSERT (operands.size() == 0);
        policy.writeFlag(x86_flag_df, policy.true_());
        break;
      }
      case x86_clc: {
        ROSE_ASSERT (operands.size() == 0);
        policy.writeFlag(x86_flag_cf, policy.false_());
        break;
      }
      case x86_stc: {
        ROSE_ASSERT (operands.size() == 0);
        policy.writeFlag(x86_flag_cf, policy.true_());
        break;
      }
      case x86_cmc: {
        ROSE_ASSERT (operands.size() == 0);
        policy.writeFlag(x86_flag_cf, policy.invert(policy.readFlag(x86_flag_cf)));
        break;
      }
      case x86_nop: break;
#define STRINGOP_SETUP_LOOP \
                    Word(1) ecxNotZero = policy.invert(policy.equalToZero(policy.readGPR(x86_gpr_cx))); \
                    policy.writeGPR(x86_gpr_cx, policy.add(policy.readGPR(x86_gpr_cx), policy.ite(ecxNotZero, number<32>(-1), number<32>(0))));
#define STRINGOP_UPDATE_REG(reg, amount) \
                    policy.writeGPR(reg, policy.add(policy.readGPR(reg), policy.ite(policy.readFlag(x86_flag_df), number<32>(-amount), number<32>(amount))));
#define STRINGOP_UPDATE_REG_COND(reg, amount) \
                    policy.writeGPR(reg, policy.add(policy.readGPR(reg), policy.ite(ecxNotZero, policy.ite(policy.readFlag(x86_flag_df), number<32>(-amount), number<32>(amount)), number<32>(0))));
#define STRINGOP_LOAD_SI(len, cond) readMemory<(8 * (len))>((insn->get_segmentOverride() == x86_segreg_none ? x86_segreg_ds : insn->get_segmentOverride()), policy.readGPR(x86_gpr_si), (cond))
#define STRINGOP_LOAD_DI(len, cond) readMemory<(8 * (len))>(x86_segreg_es, policy.readGPR(x86_gpr_di), (cond))
#define STRINGOP_STORE_DI(len, cond, val) policy.writeMemory(x86_segreg_es, policy.readGPR(x86_gpr_di), (val), (cond))
#define STRINGOP_LOOP \
        policy.writeIP(policy.ite(ecxNotZero, /* If true, repeat this instruction, otherwise go to the next one */ \
                                 number<32>((uint32_t)(insn->get_address())), \
                                 policy.readIP()));
#define STRINGOP_LOOP_E \
        policy.writeIP(policy.ite(policy.and_(ecxNotZero, policy.readFlag(x86_flag_zf)), /* If true, repeat this instruction, otherwise go to the next one */ \
                                 number<32>((uint32_t)(insn->get_address())), \
                                 policy.readIP()));
#define STRINGOP_LOOP_NE \
        policy.writeIP(policy.ite(policy.and_(ecxNotZero, policy.invert(policy.readFlag(x86_flag_zf))), /* If true, repeat this instruction, otherwise go to the next one */ \
                                 number<32>((uint32_t)(insn->get_address())), \
                                 policy.readIP()));
#define REP_SCAS(suffix, len, repsuffix, loopmacro) \
      case x86_rep##repsuffix##_scas##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        STRINGOP_SETUP_LOOP \
        doAddOperation<(len * 8)>(extract<0, (len * 8)>(policy.readGPR(x86_gpr_ax)), policy.invert(STRINGOP_LOAD_DI(len, ecxNotZero)), true, policy.false_(), ecxNotZero); \
        STRINGOP_UPDATE_REG_COND(x86_gpr_di, (len)) \
        loopmacro \
        break; \
      }
      REP_SCAS(b, 1, ne, STRINGOP_LOOP_NE)
      REP_SCAS(w, 2, ne, STRINGOP_LOOP_NE)
      REP_SCAS(d, 4, ne, STRINGOP_LOOP_NE)
      REP_SCAS(b, 1, e, STRINGOP_LOOP_E)
      REP_SCAS(w, 2, e, STRINGOP_LOOP_E)
      REP_SCAS(d, 4, e, STRINGOP_LOOP_E)
#undef REP_SCAS
#define SCAS(suffix, len) \
      case x86_scas##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        doAddOperation<(len * 8)>(extract<0, (len * 8)>(policy.readGPR(x86_gpr_ax)), policy.invert(STRINGOP_LOAD_DI(len, policy.true_())), true, policy.false_(), policy.true_()); \
        STRINGOP_UPDATE_REG(x86_gpr_di, (len)) \
        break; \
      }
      SCAS(b, 1)
      SCAS(w, 2)
      SCAS(d, 4)
#undef SCAS
#define REP_CMPS(suffix, len, repsuffix, loopmacro) \
      case x86_rep##repsuffix##_cmps##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        STRINGOP_SETUP_LOOP \
        doAddOperation<(len * 8)>(STRINGOP_LOAD_SI(len, ecxNotZero), policy.invert(STRINGOP_LOAD_DI(len, ecxNotZero)), true, policy.false_(), ecxNotZero); \
        STRINGOP_UPDATE_REG_COND(x86_gpr_si, (len)); \
        STRINGOP_UPDATE_REG_COND(x86_gpr_di, (len)); \
        loopmacro \
        break; \
      }
      REP_CMPS(b, 1, ne, STRINGOP_LOOP_NE)
      REP_CMPS(w, 2, ne, STRINGOP_LOOP_NE)
      REP_CMPS(d, 4, ne, STRINGOP_LOOP_NE)
      REP_CMPS(b, 1, e, STRINGOP_LOOP_E)
      REP_CMPS(w, 2, e, STRINGOP_LOOP_E)
      REP_CMPS(d, 4, e, STRINGOP_LOOP_E)
#undef REP_CMPS
#define CMPS(suffix, len) \
      case x86_cmps##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        doAddOperation<(len * 8)>(STRINGOP_LOAD_SI(len, policy.true_()), policy.invert(STRINGOP_LOAD_DI(len, policy.true_())), true, policy.false_(), policy.true_()); \
        STRINGOP_UPDATE_REG(x86_gpr_si, (len)); \
        STRINGOP_UPDATE_REG(x86_gpr_di, (len)); \
        break; \
      }
      CMPS(b, 1)
      CMPS(w, 2)
      CMPS(d, 4)
#undef CMPS
#define MOVS(suffix, len) \
      case x86_movs##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        STRINGOP_STORE_DI(len, policy.true_(), STRINGOP_LOAD_SI(len, policy.true_())); \
        STRINGOP_UPDATE_REG(x86_gpr_si, len) \
        STRINGOP_UPDATE_REG(x86_gpr_di, len) \
        break; \
      } \
      case x86_rep_movs##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        STRINGOP_SETUP_LOOP \
        STRINGOP_STORE_DI(len, ecxNotZero, STRINGOP_LOAD_SI(len, ecxNotZero)); \
        STRINGOP_UPDATE_REG_COND(x86_gpr_si, len) \
        STRINGOP_UPDATE_REG_COND(x86_gpr_di, len) \
        STRINGOP_LOOP \
        break; \
      }
      MOVS(b, 1)
      MOVS(w, 2)
      MOVS(d, 4)
#undef MOVS
#define STOS(suffix, len) \
      case x86_stos##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        STRINGOP_STORE_DI(len, policy.true_(), (extract<0, (8 * (len))>(policy.readGPR(x86_gpr_ax)))); \
        STRINGOP_UPDATE_REG(x86_gpr_di, len) \
        break; \
      } \
      case x86_rep_stos##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        STRINGOP_SETUP_LOOP \
        STRINGOP_STORE_DI(len, ecxNotZero, (extract<0, (8 * (len))>(policy.readGPR(x86_gpr_ax)))); \
        STRINGOP_UPDATE_REG_COND(x86_gpr_di, len) \
        STRINGOP_LOOP \
        break; \
      }
      STOS(b, 1)
      STOS(w, 2)
      STOS(d, 4)
#undef STOS
#define LODS(suffix, len, regupdate) \
      case x86_lods##suffix: { \
        ROSE_ASSERT (operands.size() == 0); \
        ROSE_ASSERT (insn->get_addressSize() == x86_insnsize_32); \
        regupdate(x86_gpr_ax, STRINGOP_LOAD_SI(len, policy.true_())); \
        STRINGOP_UPDATE_REG(x86_gpr_si, len) \
        break; \
      }
      LODS(b, 1, updateGPRLowByte)
      LODS(w, 2, updateGPRLowWord)
      LODS(d, 4, policy.writeGPR)
#undef LODS
#undef STRINGOP_SETUP_LOOP
#undef STRINGOP_UPDATE_REG
#undef STRINGOP_UPDATE_REG_COND
#undef STRINGOP_LOAD_SI
#undef STRINGOP_LOAD_DI
#undef STRINGOP_STORE_DI
#undef STRINGOP_UPDATE_CX
#undef STRINGOP_LOOP
#undef STRINGOP_LOOP_E
#undef STRINGOP_LOOP_NE
      case x86_hlt: {
        ROSE_ASSERT (operands.size() == 0);
        policy.hlt();
        policy.writeIP(number<32>((uint32_t)(insn->get_address()))); // Infinite loop (for use in model checking)
        break;
      }
      case x86_rdtsc: {
        ROSE_ASSERT (operands.size() == 0);
        Word(64) tsc = policy.rdtsc();
        policy.writeGPR(x86_gpr_ax, extract<0, 32>(tsc));
        policy.writeGPR(x86_gpr_dx, extract<32, 64>(tsc));
        break;
      }
      case x86_int: {
        ROSE_ASSERT (operands.size() == 1);
        SgAsmByteValueExpression* bv = isSgAsmByteValueExpression(operands[0]);
        ROSE_ASSERT (bv);
        policy.interrupt(bv->get_value());
        break;
      }
      // This is a dummy version that should be replaced later FIXME
      case x86_fnstcw: {
        ROSE_ASSERT (operands.size() == 1);
        write16(operands[0], number<16>(0x37f));
        break;
      }
      case x86_fldcw: {
        ROSE_ASSERT (operands.size() == 1);
        read16(operands[0]); // To catch access control violations
        break;
      }
      default: fprintf(stderr, "Bad instruction %s\n", toString(kind).c_str()); abort();
    }
  }

  void processInstruction(SgAsmx86Instruction* insn) {
    ROSE_ASSERT (insn);
    policy.startInstruction(insn);
    translate(insn);
    policy.finishInstruction(insn);
  }

  void processBlock(const SgAsmStatementPtrList& stmts, size_t begin, size_t end) {
    if (begin == end) return;
    policy.startBlock(stmts[begin]->get_address());
    for (size_t i = begin; i < end; ++i) {
      processInstruction(isSgAsmx86Instruction(stmts[i]));
    }
    policy.finishBlock(stmts[begin]->get_address());
  }

  static bool isRepeatedStringOp(SgAsmStatement* s) {
    SgAsmx86Instruction* insn = isSgAsmx86Instruction(s);
    if (!insn) return false;
    switch (insn->get_kind()) {
      case x86_repe_cmpsb: return true;
      case x86_repe_cmpsd: return true;
      case x86_repe_cmpsq: return true;
      case x86_repe_cmpsw: return true;
      case x86_repe_scasb: return true;
      case x86_repe_scasd: return true;
      case x86_repe_scasq: return true;
      case x86_repe_scasw: return true;
      case x86_rep_insb: return true;
      case x86_rep_insd: return true;
      case x86_rep_insw: return true;
      case x86_rep_lodsb: return true;
      case x86_rep_lodsd: return true;
      case x86_rep_lodsq: return true;
      case x86_rep_lodsw: return true;
      case x86_rep_movsb: return true;
      case x86_rep_movsd: return true;
      case x86_rep_movsq: return true;
      case x86_rep_movsw: return true;
      case x86_repne_cmpsb: return true;
      case x86_repne_cmpsd: return true;
      case x86_repne_cmpsq: return true;
      case x86_repne_cmpsw: return true;
      case x86_repne_scasb: return true;
      case x86_repne_scasd: return true;
      case x86_repne_scasq: return true;
      case x86_repne_scasw: return true;
      case x86_rep_outsb: return true;
      case x86_rep_outsd: return true;
      case x86_rep_outsw: return true;
      case x86_rep_stosb: return true;
      case x86_rep_stosd: return true;
      case x86_rep_stosq: return true;
      case x86_rep_stosw: return true;
      default: return false;
    }
  }

  static bool isHltOrInt(SgAsmStatement* s) {
    SgAsmx86Instruction* insn = isSgAsmx86Instruction(s);
    if (!insn) return false;
    switch (insn->get_kind()) {
      case x86_hlt: return true;
      case x86_int: return true;
      default: return false;
    }
  }

  void processBlock(SgAsmBlock* b) {
    const SgAsmStatementPtrList& stmts = b->get_statementList();
    if (stmts.empty()) return;
    if (!isSgAsmInstruction(stmts[0])) return; // A block containing functions or something
    size_t i = 0;
    while (i < stmts.size()) {
      size_t oldI = i;
      // Advance until either i points to a repeated string op or it is just after a hlt or int
      while (i < stmts.size() && !isRepeatedStringOp(stmts[i]) && (i == oldI || !isHltOrInt(stmts[i - 1]))) ++i;
      processBlock(stmts, oldI, i);
      if (i >= stmts.size()) break;
      if (isRepeatedStringOp(stmts[i])) {
        processBlock(stmts, i, i + 1);
        ++i;
      }
      ROSE_ASSERT (i != oldI);
    }
  }

};

#undef Word

#endif // ROSE_X86INSTRUCTIONSEMANTICS_H
