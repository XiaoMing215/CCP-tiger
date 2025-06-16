#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
public:
  enum X64Reg {
    RBX = 0,
    RCX,
    RDX,
    RSI,
    RDI,
    RBP,
    RSP,
    RAX,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15
  };

  const std::string X64RegNames[16] = {"rbx", "rcx", "rdx", "rsi", "rdi", "rbp",
                                       "rsp", "rax", "r8",  "r9",  "r10", "r11",
                                       "r12", "r13", "r14", "r15"};
  const int WORD_SIZE = 8;

public:
  X64RegManager();

  temp::TempList *Registers();
  temp::TempList *ArgRegs();
  temp::TempList *CallerSaves();
  temp::TempList *CalleeSaves();
  temp::TempList *ReturnSink();
  int WordSize();
  temp::Temp *FramePointer();
  temp::Temp *StackPointer();
  temp::Temp *ReturnValue();
};

/* TODO: Put your lab5 code here */
class InFrameAccess : public Access { //表示变量不放在寄存器里
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */

  tree::Exp *ToExp(tree::Exp *framePtr) const override;  //返回形如 Mem(BinOp(PLUS, framePtr, offset)) 的树
};

class InRegAccess : public Access { //变量存在寄存器中
public:
  temp::Temp *reg; 
  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override; //直接返回 Temp(reg)
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  X64Frame(temp::Label *name, std::list<bool> formals);//formals表示每个参数是否可能逃逸
  int AllocLocal();
  std::list<frame::Access *> *Formals();
  int Size() override;
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H