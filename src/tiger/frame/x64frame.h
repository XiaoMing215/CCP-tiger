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
//这一块对应cc文件的第一大段 不用我们修改 是寄存器分配相关的

//由于translate要用到inframeaccess 这里要申明：
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset);
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override;
};


} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
