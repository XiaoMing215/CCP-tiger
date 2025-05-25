#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {

class X64RegManager : public RegManager {
public:
  enum X64Reg {
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
  
  //用于x64frame.cc
  const std::string X64RegNames[16] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi",
                                      "rsp", "rbp", "r8",  "r9",  "r10", "r11",
                                      "r12", "r13", "r14", "r15"};
  const int WORD_SIZE = 8;
  //用于x64frame.cc
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
//这一块对应cc文件的第一大段 不用我们修改 是寄存器分配相关的

//由于translate要用到inframeaccess 这里要申明：
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset);
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override;
};

//这个新增的意义？
// visiting var in reg
class InRegAccess : public Access {
public:
  temp::Temp *reg; // Temp is a data structure represents virtual registers
  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override;
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  X64Frame(temp::Label *name, std::list<bool> formals);
  int AllocLocal();
  std::list<frame::Access *> *Formals();
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
