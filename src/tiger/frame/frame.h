#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>

#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"
#include "tiger/codegen/assem.h"


namespace frame {

class Access;
class Frame;

class RegManager { //负责管理目标机器的寄存器信息 在x64才有具体的实现。此处只有大类
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }
  [[nodiscard]] virtual temp::TempList *Registers() = 0;
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;
  /*
  ReturnSink() 返回的寄存器集合通常包含：
  函数返回值所在的寄存器（如 rax）
  可能被调用过程修改的寄存器
  用于确保调用结束时寄存器状态一致性的寄存器集合
  */
  [[nodiscard]] virtual int WordSize() = 0;
  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;
  [[nodiscard]] virtual temp::Temp *StackPointer() = 0;
  [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

  temp::Map *temp_map_;

protected:
  std::vector<temp::Temp *> regs_;
};


class Access {
public:
  /* TODO: Put your lab5 code here */
  Access() {}
  virtual tree::Exp *ToExp(tree::Exp *framePtr)
      const = 0; 
  static Access *AllocLocal(Frame *frame, bool escape);

  virtual ~Access() = default;
  
};

class Frame { //代表一个函数调用时的栈帧结构。抽象类 每个函数对应一个
  /* TODO: Put your lab5 code here */
public:

  std::list<frame::Access *> *formals_; //函数的形式参数在运行时存放的位置。
  int offset_ = 0;                      //用来分配栈空间

  temp::Label *frameLabel_ = nullptr;   //函数的入口标签
public:
  Frame() {}
  Frame(temp::Label *name) : offset_(0), frameLabel_(name) {}
  ~Frame() {}
  [[nodiscard]] virtual int Size() { return -offset_; } //栈向下增长 于是大小应该是相反数
  [[nodiscard]] std::string GetFrameLabel() { return frameLabel_->Name(); } //返回当前函数的名字
  [[nodiscard]] std::list<frame::Access *> *GetFormals() {return formals_;} //返回该函数的形参列表
  virtual int AllocLocal() = 0;  //给局部变量分配栈空间
};

class Frag { //用于保存程序片段，方便后续生成汇编
public:
  virtual ~Frag() = default;

  enum OutputPhase { //OutputPhase 说明输出的是什么阶段
    Proc,
    String,
  };

  virtual void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const = 0;
};//提供统一接口 OutputAssem，用于后续输出汇编代码。

class StringFrag : public Frag { //表示一条静态字符串
public:
  temp::Label *label_; //是这个字符串的唯一标识名
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {//一个函数（过程）的代码体，
public:
  tree::Stm *body_; //树状中间表示
  Frame *frame_;    //函数的栈帧信息

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};
//后续通过 ProcEntryExit1/2/3 和 codegen 把它翻译为汇编。

class Frags { //存储一个程序中所有的片段（字符串 + 函数体）。
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag*> &GetList() { return frags_; }

private:
  std::list<Frag *> frags_;
};

/* TODO: Put your lab5 code here */
tree::Exp *ExternalCall(std::string s, tree::ExpList *args);
tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm);
assem::InstrList *ProcEntryExit2(assem::InstrList *body);
assem::Proc *BuildCompleteProcedure(frame::Frame *frame, assem::InstrList *body);

} // namespace frame

#endif