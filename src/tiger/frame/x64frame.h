//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
public:
  enum X64Reg {
    RAX = 0,
    RBX,
    RCX,
    RDX,
    RSI,
    RDI,
    RSP,
    RBP,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15
  };

  const std::string X64RegNames[16] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi",
                                       "rsp", "rbp", "r8",  "r9",  "r10", "r11",
                                       "r12", "r13", "r14", "r15"};
  const int WORD_SIZE = 8;

public:
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

  explicit InFrameAccess(int offset) : offset(offset) {}

  tree::Exp *ToExp(tree::Exp *framePtr) const override;
};

// 变量访问（Access）的子类，表示变量保存在寄存器中的情况。
class InRegAccess : public Access {
public:
  temp::Temp *reg; 
  //temp::Temp *reg：这个 Temp 代表一个虚拟寄存器，和最终的物理寄存器通过寄存器分配（regalloc）映射。
  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  tree::Exp *ToExp(tree::Exp *framePtr) const override;
  //返回一个抽象语法树（tree::Exp），表示访问这个变量的表达式。
};

//表示一个具体函数的栈帧（frame）信息，适用于 x86-64 架构。
class X64Frame : public Frame {
public:
  X64Frame(temp::Label *name, std::list<bool> formals);
  int AllocLocal();
  std::list<frame::Access *> *Formals();
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
