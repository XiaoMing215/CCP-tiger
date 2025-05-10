#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>

#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"

namespace frame {

class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
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

  virtual ~Access() = default;
  virtual tree::Exp *ToExp(tree::Exp *framePtr) const = 0;
  //统一生成中间表示的 tree::Exp
  //translate 阶段统一调用 access->ToExp(fp)
};

class Frame {
  /* TODO: Put your lab5 code here */
  public:
    // Frame构造函数：由x64frame当中的Frame(8, 0, name, formals)得到 用的formals是指针类型
    Frame(int word_size, int offset, temp::Label *name, std::list<Access *> *formals)
      : word_size_(word_size), offset_(offset), name_(name), formals_(formals), total_size_(0), local_count_(0), out_args_(0) {}

    virtual ~Frame() = default;

    virtual std::string GetLabel() const = 0;
    virtual temp::Label *Name() const = 0;
    virtual std::list<frame::Access *> *Formals() const = 0;
    //上三种全是x64的要求
    virtual Access *AllocLocal(bool escape) = 0;
    virtual void AllocOutgoSpace(int size) = 0;

    virtual std::list<Access *> *Formals() { return formals_; }

    virtual int WordSize() { return word_size_; }
    virtual int TotalSize() { return total_size_; }

    //translate处需要的函数
    frame::Access *StaticLink(){return formals_->front();}  
    [[nodiscard]] const std::list<frame::Access *> &GetFormalList() const { return *formals_; }
    [[nodiscard]] const std::list<tree::Stm*> &GetVSList() const { return view_shift_stm; }
  protected:
    int word_size_;
    int offset_;
    int out_args_ = 0;
    temp::Label *name_;
    std::list<Access *> *formals_;
    std::vector<Access *> locals_; // 需要使用unique_ptr，保持性质一致
    std::list<tree::Stm*> view_shift_stm;
    int total_size_;
    int local_count_;
};


/**
 * Fragments
 */


//frag：作为中间代码生成阶段的产物，用于保存翻译后的函数体和字符串常量，供后续生成汇编时使用。
class Frag {
public:
  virtual ~Frag() = default;
};

class StringFrag : public Frag {
public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}
};

class ProcFrag : public Frag {
public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.push_back(frag); }
  const std::list<Frag*> &GetList() { return frags_; }

private:
  std::list<Frag*> frags_;
};

/* TODO: Put your lab5 code here */

tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm);
// x64frame当中的函数
/* End for lab5 code */

Frame *NewFrame(temp::Label *name, std::list<bool> formals);

tree::Exp *externalCall(std::string s,tree::ExpList *args);
} // namespace frame

#endif