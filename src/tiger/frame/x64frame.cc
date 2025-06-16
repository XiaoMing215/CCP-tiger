#include <sstream>
#include "tiger/frame/x64frame.h"
#include "frame.h"

extern frame::RegManager *reg_manager;

namespace frame {

int X64Frame::AllocLocal() { //在栈帧中为局部变量分配空间 返回该变量的访问方式
  offset_ -= reg_manager->WordSize();
  return offset_;
}//这是frame的alloc不是tmp的 只负责计算 不管逃逸问题

X64Frame::X64Frame(temp::Label *name, std::list<bool> formals) : Frame(name) {
  formals_ = new std::list<frame::Access *>();
  for (auto formal_escape : formals) {
    formals_->push_back(frame::Access::AllocLocal(this, formal_escape));
  }//逐个存在应该存的地方
}

std::list<frame::Access *> *X64Frame::Formals() { return formals_; }

int X64Frame::Size() {
  const int arg_num = formals_->size();
  const int arg_reg_num = reg_manager->ArgRegs()->GetList().size(); 
  return -offset_ + std::max(arg_num - arg_reg_num, 0) * reg_manager->WordSize();
  //参数中“溢出”到栈上的那部分参数空间
}

temp::TempList *X64RegManager::Registers() {
  return new temp::TempList({
      regs_[RAX],
      regs_[RBX],
      regs_[RCX],
      regs_[RDX],
      regs_[RSI],
      regs_[RDI],
      regs_[RBP],
      regs_[R8],
      regs_[R9],
      regs_[R10],
      regs_[R11],
      regs_[R12],
      regs_[R13],
      regs_[R14],
      regs_[R15],
  });
}

temp::TempList *X64RegManager::ArgRegs() {
  return new temp::TempList({
      regs_[RDI],
      regs_[RSI],
      regs_[RDX],
      regs_[RCX],
      regs_[R8],
      regs_[R9],
  });
}

temp::TempList *X64RegManager::CallerSaves() {
  return new temp::TempList({
      regs_[RAX],
      regs_[RCX],
      regs_[RDX],
      regs_[RSI],
      regs_[RDI],
      regs_[R8],
      regs_[R9],
      regs_[R10],
      regs_[R11],
  });
}

temp::TempList *X64RegManager::CalleeSaves() {
  return new temp::TempList({
      regs_[RBX],
      regs_[RBP],
      regs_[R12],
      regs_[R13],
      regs_[R14],
      regs_[R15],
  });
}

temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *temp_list = CalleeSaves();
  temp_list->Append(StackPointer());
  temp_list->Append(ReturnValue());
  return temp_list;
}

int X64RegManager::WordSize() {
  return WORD_SIZE;
}

temp::Temp *X64RegManager::FramePointer() {
  return regs_[RBP];
}

temp::Temp *X64RegManager::StackPointer() {
  return regs_[RSP];
}

temp::Temp *X64RegManager::ReturnValue() {
  return regs_[RAX];
}

X64RegManager::X64RegManager() : RegManager() { //为每个 x64 的寄存器生成一个 temp::Temp
  for (std::string reg_name : X64RegNames) {
    temp::Temp *reg = temp::TempFactory::NewTemp();
    temp_map_->Enter(reg, new std::string("%" + reg_name));
    regs_.push_back(reg);
  }
}

tree::Exp *ExternalCall(std::string s, tree::ExpList *args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)),//找到已经被命名的名称对应的标签
                           args);
}//CALL NAME("malloc"), args

tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm) {
  tree::Stm *res_stm = nullptr;

  tree::Stm *save_callee_stm = new tree::ExpStm(new tree::ConstExp(0));//占位
  temp::TempList *callee_saved = new temp::TempList();

  //保存 Callee 保存寄存器（被调用者需要保持不变的寄存器）
  for (auto reg : reg_manager->CalleeSaves()->GetList()) {
    temp::Temp *dst = temp::TempFactory::NewTemp();
    save_callee_stm =
        new tree::SeqStm(
        save_callee_stm, new tree::MoveStm(new tree::TempExp(dst),
                                                     new tree::TempExp(reg)));
    callee_saved->Append(dst);
  }

  auto arg_reg_num = reg_manager->ArgRegs()->GetList().size();
  auto arg_num = frame->formals_->size();
  int formal_idx = 0; //当前第几个
  tree::SeqStm *view_shift = nullptr, *tail = nullptr;

  // 处理参数传递  希望把这些值 写入我们函数帧中分配好的变量位置（Access）
  for (Access *formal : *(frame->formals_)) {
    // 构造目标表达式 
    tree::Exp *dst =
        formal->ToExp(new tree::TempExp(reg_manager->FramePointer()));
    // 构造源表达式
    tree::Exp *src = nullptr;
    if (formal_idx < arg_reg_num) {
      // 参数在寄存器里
      src = new tree::TempExp(reg_manager->ArgRegs()->NthTemp(formal_idx));
    } else {
      // 参数在栈里（调用者压进去的）
      src = new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, new tree::TempExp(reg_manager->FramePointer()),
          new tree::ConstExp((arg_num - formal_idx) *
                             reg_manager->WordSize())));
    }
    // 生成 MoveStm
    tree::MoveStm *move_stm = new tree::MoveStm(dst, src);
    // 将每个 MoveStm 拼接成一个串联的 SeqStm 语句树
    if (!tail) {
      view_shift = tail = new tree::SeqStm(move_stm, nullptr);
    } else {
      tail->right_ = new tree::SeqStm(move_stm, nullptr);
      tail = static_cast<tree::SeqStm *>(tail->right_);
    }
    ++formal_idx;
  }

  // 接上主函数体 stm
  if (view_shift) {//函数有参数 把函数体 stm 接到末尾
    tail->right_ = stm;
    res_stm = view_shift;
  } else { //否则直接用 stm
    res_stm = stm;
  }

  res_stm = new tree::SeqStm(save_callee_stm, res_stm);

  auto saved = callee_saved->GetList().cbegin();
   for (auto reg : reg_manager->CalleeSaves()->GetList()) { //遍历所有需要恢复的 callee-saved 寄存器
    res_stm = new tree::SeqStm(res_stm, new tree::MoveStm(new tree::TempExp(reg),
                                                  new tree::TempExp(*saved)));
    ++saved;
  }
  delete callee_saved; //否则会造成内存泄漏

  return res_stm;
}


//负责在函数的汇编指令序列末尾添加保存返回值寄存器以及“返回点”相关的指令
assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}

//给函数加上完整的函数入口（prologue）和函数出口（epilogue）代码。
assem::Proc *BuildCompleteProcedure(frame::Frame *frame, assem::InstrList *body) {
  const std::string labelName = temp::LabelFactory::LabelString(frame->frameLabel_);
  const int frameSize = frame->Size();

  std::ostringstream prologueStream;
  prologueStream << ".set " << labelName << "_framesize, " << frameSize << "\n";
  prologueStream << labelName << ":\n";

  //是 tigermain 主函数，需要先给栈指针 rsp 留 8 字节空间（对齐栈），
  //并保存当前帧指针 rbp 到栈顶，便于后面恢复。
  bool isMainFunc = (frame->frameLabel_->Name() == "tigermain");
  if (isMainFunc) {
    prologueStream << "subq $8, %rsp\n";
    prologueStream << "movq %rbp, (%rsp)\n";
  }
  prologueStream << "subq $" << frameSize << ", %rsp\n";

  std::ostringstream epilogueStream;
  epilogueStream << "addq $" << frameSize << ", %rsp\n";
  if (isMainFunc) {
    epilogueStream << "movq (%rsp), %rbp\n";
    epilogueStream << "addq $8, %rsp\n";
  }
  epilogueStream << "retq\n";

  return new assem::Proc(prologueStream.str(), body, epilogueStream.str());
}

Access *Access::AllocLocal(Frame *frame, bool escape) { //真正做出判断该存在哪里的函数
  if (escape) {//存入栈当中
    int offset = frame->AllocLocal();
    return new frame::InFrameAccess(offset);
  } else {//搞个虚拟寄存器丢进去
    temp::Temp *newTemp = temp::TempFactory::NewTemp();
    return new frame::InRegAccess(newTemp);
  }
}


//返回当前变量在运行时的位置（可以是内存地址也可以是寄存器），
//并构造成一个 tree::Exp* 抽象表达式，用于参与后续语法树生成（如赋值、取值等）。
tree::Exp *InFrameAccess::ToExp(tree::Exp *framePtr) const {
  auto offsetExp = new tree::ConstExp(offset);
  auto addressExp = new tree::BinopExp(tree::BinOp::PLUS_OP, framePtr, offsetExp);
  return new tree::MemExp(addressExp);
}//存在栈则需要构造式子：*(framePtr + offset)

tree::Exp *InRegAccess::ToExp(tree::Exp *framePtr) const {
  return new tree::TempExp(reg);
}//存在寄存器当中直接用tree构造寄存器所在的位置

} // namespace frame
