#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
public:
  enum Reg : unsigned long {
    RAX,
    RBX,
    RCX,
    RDX,
    RSI,
    RDI,
    RBP,
    RSP,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    FP,
    REG_COUNT,
    SP = RSP, // use RSP as SP
    RV = RAX, // use RAX as RV
  };

  X64RegManager();

  [[nodiscard]] temp::TempList *Registers() override;

  [[nodiscard]] temp::TempList *ArgRegs() override;

  [[nodiscard]] temp::TempList *CallerSaves() override;

  [[nodiscard]] temp::TempList *CalleeSaves() override;

  [[nodiscard]] temp::TempList *ReturnSink() override;

  [[nodiscard]] int WordSize() override;

  [[nodiscard]] temp::Temp *FramePointer() override;

  [[nodiscard]] temp::Temp *StackPointer() override;

  [[nodiscard]] temp::Temp *ReturnValue() override;
  
};

//must be declared in .h instead of .cc
class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  X64Frame(temp::Label *name,std::list<bool> *f);
  Access *AllocLocal(bool escape)override;
};


//must be declared in .h instead of .cc
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::MemExp(
      new tree::BinopExp(
        tree::PLUS_OP,
        framePtr,
        new tree::ConstExp(offset)
      )
    );
  }

};


class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }

};


} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
