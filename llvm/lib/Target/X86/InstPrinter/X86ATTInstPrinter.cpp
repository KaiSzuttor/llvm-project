//===-- X86ATTInstPrinter.cpp - AT&T assembly instruction printing --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file includes code for rendering MCInst instances as AT&T-style
// assembly.
//
//===----------------------------------------------------------------------===//

#include "X86ATTInstPrinter.h"
#include "MCTargetDesc/X86BaseInfo.h"
#include "X86InstComments.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cinttypes>
#include <cstdint>

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "X86GenAsmWriter.inc"

void X86ATTInstPrinter::printRegName(raw_ostream &OS, unsigned RegNo) const {
  OS << markup("<reg:") << '%' << getRegisterName(RegNo) << markup(">");
}

void X86ATTInstPrinter::printInst(const MCInst *MI, raw_ostream &OS,
                                  StringRef Annot, const MCSubtargetInfo &STI) {
  // If verbose assembly is enabled, we can print some informative comments.
  if (CommentStream)
    HasCustomInstComment = EmitAnyX86InstComments(MI, *CommentStream, MII);

  printInstFlags(MI, OS);

  // Output CALLpcrel32 as "callq" in 64-bit mode.
  // In Intel annotation it's always emitted as "call".
  //
  // TODO: Probably this hack should be redesigned via InstAlias in
  // InstrInfo.td as soon as Requires clause is supported properly
  // for InstAlias.
  if (MI->getOpcode() == X86::CALLpcrel32 &&
      (STI.getFeatureBits()[X86::Mode64Bit])) {
    OS << "\tcallq\t";
    printPCRelImm(MI, 0, OS);
  }
  // data16 and data32 both have the same encoding of 0x66. While data32 is
  // valid only in 16 bit systems, data16 is valid in the rest.
  // There seems to be some lack of support of the Requires clause that causes
  // 0x66 to be interpreted as "data16" by the asm printer.
  // Thus we add an adjustment here in order to print the "right" instruction.
  else if (MI->getOpcode() == X86::DATA16_PREFIX &&
           STI.getFeatureBits()[X86::Mode16Bit]) {
   OS << "\tdata32";
  }
  // Try to print any aliases first.
  else if (!printAliasInstr(MI, OS) &&
           !printVecCompareInstr(MI, OS))
    printInstruction(MI, OS);

  // Next always print the annotation.
  printAnnotation(OS, Annot);
}

bool X86ATTInstPrinter::printVecCompareInstr(const MCInst *MI,
                                             raw_ostream &OS) {
  if (MI->getNumOperands() == 0 ||
      !MI->getOperand(MI->getNumOperands() - 1).isImm())
    return false;

  int64_t Imm = MI->getOperand(MI->getNumOperands() - 1).getImm();

  const MCInstrDesc &Desc = MII.get(MI->getOpcode());

  // Custom print the vector compare instructions to get the immediate
  // translated into the mnemonic.
  switch (MI->getOpcode()) {
  case X86::VPCOMBmi:  case X86::VPCOMBri:
  case X86::VPCOMDmi:  case X86::VPCOMDri:
  case X86::VPCOMQmi:  case X86::VPCOMQri:
  case X86::VPCOMUBmi: case X86::VPCOMUBri:
  case X86::VPCOMUDmi: case X86::VPCOMUDri:
  case X86::VPCOMUQmi: case X86::VPCOMUQri:
  case X86::VPCOMUWmi: case X86::VPCOMUWri:
  case X86::VPCOMWmi:  case X86::VPCOMWri:
    if (Imm >= 0 && Imm <= 7) {
      OS << '\t';
      printVPCOMMnemonic(MI, OS);

      if ((Desc.TSFlags & X86II::FormMask) == X86II::MRMSrcMem)
        printxmmwordmem(MI, 2, OS);
      else
        printOperand(MI, 2, OS);

      OS << ", ";
      printOperand(MI, 1, OS);
      OS << ", ";
      printOperand(MI, 0, OS);
      return true;
    }
    break;

  case X86::VPCMPBZ128rmi:   case X86::VPCMPBZ128rri:
  case X86::VPCMPBZ256rmi:   case X86::VPCMPBZ256rri:
  case X86::VPCMPBZrmi:      case X86::VPCMPBZrri:
  case X86::VPCMPDZ128rmi:   case X86::VPCMPDZ128rri:
  case X86::VPCMPDZ256rmi:   case X86::VPCMPDZ256rri:
  case X86::VPCMPDZrmi:      case X86::VPCMPDZrri:
  case X86::VPCMPQZ128rmi:   case X86::VPCMPQZ128rri:
  case X86::VPCMPQZ256rmi:   case X86::VPCMPQZ256rri:
  case X86::VPCMPQZrmi:      case X86::VPCMPQZrri:
  case X86::VPCMPUBZ128rmi:  case X86::VPCMPUBZ128rri:
  case X86::VPCMPUBZ256rmi:  case X86::VPCMPUBZ256rri:
  case X86::VPCMPUBZrmi:     case X86::VPCMPUBZrri:
  case X86::VPCMPUDZ128rmi:  case X86::VPCMPUDZ128rri:
  case X86::VPCMPUDZ256rmi:  case X86::VPCMPUDZ256rri:
  case X86::VPCMPUDZrmi:     case X86::VPCMPUDZrri:
  case X86::VPCMPUQZ128rmi:  case X86::VPCMPUQZ128rri:
  case X86::VPCMPUQZ256rmi:  case X86::VPCMPUQZ256rri:
  case X86::VPCMPUQZrmi:     case X86::VPCMPUQZrri:
  case X86::VPCMPUWZ128rmi:  case X86::VPCMPUWZ128rri:
  case X86::VPCMPUWZ256rmi:  case X86::VPCMPUWZ256rri:
  case X86::VPCMPUWZrmi:     case X86::VPCMPUWZrri:
  case X86::VPCMPWZ128rmi:   case X86::VPCMPWZ128rri:
  case X86::VPCMPWZ256rmi:   case X86::VPCMPWZ256rri:
  case X86::VPCMPWZrmi:      case X86::VPCMPWZrri:
  case X86::VPCMPBZ128rmik:  case X86::VPCMPBZ128rrik:
  case X86::VPCMPBZ256rmik:  case X86::VPCMPBZ256rrik:
  case X86::VPCMPBZrmik:     case X86::VPCMPBZrrik:
  case X86::VPCMPDZ128rmik:  case X86::VPCMPDZ128rrik:
  case X86::VPCMPDZ256rmik:  case X86::VPCMPDZ256rrik:
  case X86::VPCMPDZrmik:     case X86::VPCMPDZrrik:
  case X86::VPCMPQZ128rmik:  case X86::VPCMPQZ128rrik:
  case X86::VPCMPQZ256rmik:  case X86::VPCMPQZ256rrik:
  case X86::VPCMPQZrmik:     case X86::VPCMPQZrrik:
  case X86::VPCMPUBZ128rmik: case X86::VPCMPUBZ128rrik:
  case X86::VPCMPUBZ256rmik: case X86::VPCMPUBZ256rrik:
  case X86::VPCMPUBZrmik:    case X86::VPCMPUBZrrik:
  case X86::VPCMPUDZ128rmik: case X86::VPCMPUDZ128rrik:
  case X86::VPCMPUDZ256rmik: case X86::VPCMPUDZ256rrik:
  case X86::VPCMPUDZrmik:    case X86::VPCMPUDZrrik:
  case X86::VPCMPUQZ128rmik: case X86::VPCMPUQZ128rrik:
  case X86::VPCMPUQZ256rmik: case X86::VPCMPUQZ256rrik:
  case X86::VPCMPUQZrmik:    case X86::VPCMPUQZrrik:
  case X86::VPCMPUWZ128rmik: case X86::VPCMPUWZ128rrik:
  case X86::VPCMPUWZ256rmik: case X86::VPCMPUWZ256rrik:
  case X86::VPCMPUWZrmik:    case X86::VPCMPUWZrrik:
  case X86::VPCMPWZ128rmik:  case X86::VPCMPWZ128rrik:
  case X86::VPCMPWZ256rmik:  case X86::VPCMPWZ256rrik:
  case X86::VPCMPWZrmik:     case X86::VPCMPWZrrik:
  case X86::VPCMPDZ128rmib:  case X86::VPCMPDZ128rmibk:
  case X86::VPCMPDZ256rmib:  case X86::VPCMPDZ256rmibk:
  case X86::VPCMPDZrmib:     case X86::VPCMPDZrmibk:
  case X86::VPCMPQZ128rmib:  case X86::VPCMPQZ128rmibk:
  case X86::VPCMPQZ256rmib:  case X86::VPCMPQZ256rmibk:
  case X86::VPCMPQZrmib:     case X86::VPCMPQZrmibk:
  case X86::VPCMPUDZ128rmib: case X86::VPCMPUDZ128rmibk:
  case X86::VPCMPUDZ256rmib: case X86::VPCMPUDZ256rmibk:
  case X86::VPCMPUDZrmib:    case X86::VPCMPUDZrmibk:
  case X86::VPCMPUQZ128rmib: case X86::VPCMPUQZ128rmibk:
  case X86::VPCMPUQZ256rmib: case X86::VPCMPUQZ256rmibk:
  case X86::VPCMPUQZrmib:    case X86::VPCMPUQZrmibk:
    if ((Imm >= 0 && Imm <= 2) || (Imm >= 4 && Imm <= 6)) {
      OS << '\t';
      printVPCMPMnemonic(MI, OS);

      unsigned CurOp = (Desc.TSFlags & X86II::EVEX_K) ? 3 : 2;

      if ((Desc.TSFlags & X86II::FormMask) == X86II::MRMSrcMem) {
        if (Desc.TSFlags & X86II::EVEX_B) {
          // Broadcast form.
          // Load size is based on W-bit as only D and Q are supported.
          if (Desc.TSFlags & X86II::VEX_W)
            printqwordmem(MI, CurOp--, OS);
          else
            printdwordmem(MI, CurOp--, OS);

          // Print the number of elements broadcasted.
          unsigned NumElts;
          if (Desc.TSFlags & X86II::EVEX_L2)
            NumElts = (Desc.TSFlags & X86II::VEX_W) ? 8 : 16;
          else if (Desc.TSFlags & X86II::VEX_L)
            NumElts = (Desc.TSFlags & X86II::VEX_W) ? 4 : 8;
          else
            NumElts = (Desc.TSFlags & X86II::VEX_W) ? 2 : 4;
          OS << "{1to" << NumElts << "}";
        } else {
          if (Desc.TSFlags & X86II::EVEX_L2)
            printzmmwordmem(MI, CurOp--, OS);
          else if (Desc.TSFlags & X86II::VEX_L)
            printymmwordmem(MI, CurOp--, OS);
          else
            printxmmwordmem(MI, CurOp--, OS);
        }
      } else {
        printOperand(MI, CurOp--, OS);
      }

      OS << ", ";
      printOperand(MI, CurOp--, OS);
      OS << ", ";
      printOperand(MI, 0, OS);
      if (CurOp > 0) {
        // Print mask operand.
        OS << " {";
        printOperand(MI, CurOp--, OS);
        OS << "}";
      }

      return true;
    }
    break;
  }

  return false;
}

void X86ATTInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    printRegName(O, Op.getReg());
  } else if (Op.isImm()) {
    // Print immediates as signed values.
    int64_t Imm = Op.getImm();
    O << markup("<imm:") << '$' << formatImm(Imm) << markup(">");

    // TODO: This should be in a helper function in the base class, so it can
    // be used by other printers.

    // If there are no instruction-specific comments, add a comment clarifying
    // the hex value of the immediate operand when it isn't in the range
    // [-256,255].
    if (CommentStream && !HasCustomInstComment && (Imm > 255 || Imm < -256)) {
      // Don't print unnecessary hex sign bits.
      if (Imm == (int16_t)(Imm))
        *CommentStream << format("imm = 0x%" PRIX16 "\n", (uint16_t)Imm);
      else if (Imm == (int32_t)(Imm))
        *CommentStream << format("imm = 0x%" PRIX32 "\n", (uint32_t)Imm);
      else
        *CommentStream << format("imm = 0x%" PRIX64 "\n", (uint64_t)Imm);
    }
  } else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    O << markup("<imm:") << '$';
    Op.getExpr()->print(O, &MAI);
    O << markup(">");
  }
}

void X86ATTInstPrinter::printMemReference(const MCInst *MI, unsigned Op,
                                          raw_ostream &O) {
  const MCOperand &BaseReg = MI->getOperand(Op + X86::AddrBaseReg);
  const MCOperand &IndexReg = MI->getOperand(Op + X86::AddrIndexReg);
  const MCOperand &DispSpec = MI->getOperand(Op + X86::AddrDisp);

  O << markup("<mem:");

  // If this has a segment register, print it.
  printOptionalSegReg(MI, Op + X86::AddrSegmentReg, O);

  if (DispSpec.isImm()) {
    int64_t DispVal = DispSpec.getImm();
    if (DispVal || (!IndexReg.getReg() && !BaseReg.getReg()))
      O << formatImm(DispVal);
  } else {
    assert(DispSpec.isExpr() && "non-immediate displacement for LEA?");
    DispSpec.getExpr()->print(O, &MAI);
  }

  if (IndexReg.getReg() || BaseReg.getReg()) {
    O << '(';
    if (BaseReg.getReg())
      printOperand(MI, Op + X86::AddrBaseReg, O);

    if (IndexReg.getReg()) {
      O << ',';
      printOperand(MI, Op + X86::AddrIndexReg, O);
      unsigned ScaleVal = MI->getOperand(Op + X86::AddrScaleAmt).getImm();
      if (ScaleVal != 1) {
        O << ',' << markup("<imm:") << ScaleVal // never printed in hex.
          << markup(">");
      }
    }
    O << ')';
  }

  O << markup(">");
}

void X86ATTInstPrinter::printSrcIdx(const MCInst *MI, unsigned Op,
                                    raw_ostream &O) {
  O << markup("<mem:");

  // If this has a segment register, print it.
  printOptionalSegReg(MI, Op + 1, O);

  O << "(";
  printOperand(MI, Op, O);
  O << ")";

  O << markup(">");
}

void X86ATTInstPrinter::printDstIdx(const MCInst *MI, unsigned Op,
                                    raw_ostream &O) {
  O << markup("<mem:");

  O << "%es:(";
  printOperand(MI, Op, O);
  O << ")";

  O << markup(">");
}

void X86ATTInstPrinter::printMemOffset(const MCInst *MI, unsigned Op,
                                       raw_ostream &O) {
  const MCOperand &DispSpec = MI->getOperand(Op);

  O << markup("<mem:");

  // If this has a segment register, print it.
  printOptionalSegReg(MI, Op + 1, O);

  if (DispSpec.isImm()) {
    O << formatImm(DispSpec.getImm());
  } else {
    assert(DispSpec.isExpr() && "non-immediate displacement?");
    DispSpec.getExpr()->print(O, &MAI);
  }

  O << markup(">");
}

void X86ATTInstPrinter::printU8Imm(const MCInst *MI, unsigned Op,
                                   raw_ostream &O) {
  if (MI->getOperand(Op).isExpr())
    return printOperand(MI, Op, O);

  O << markup("<imm:") << '$' << formatImm(MI->getOperand(Op).getImm() & 0xff)
    << markup(">");
}

void X86ATTInstPrinter::printSTiRegOperand(const MCInst *MI, unsigned OpNo,
                                           raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  unsigned Reg = Op.getReg();
  // Override the default printing to print st(0) instead st.
  if (Reg == X86::ST0)
    OS << markup("<reg:") << "%st(0)" << markup(">");
  else
    printRegName(OS, Reg);
}
