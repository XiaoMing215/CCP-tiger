#include "tiger/frame/x64frame.h"
#include "frame.h"
#include <sstream>

extern frame::RegManager *reg_manager;

namespace frame {

int X64Frame::AllocLocal() {
  // Keep away from the return address on the top of the frame
  offset_ -= reg_manager->WordSize();
  return offset_;
}

tree::Exp *ExternalCall(std::string s, tree::ExpList *args) {
  // Prepend a magic exp at first arg, indicating do not pass static link on
  // stack
  // args->Insert(new tree::NameExp(temp::LabelFactory::NamedLabel("staticLink")));
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)),
                           args);
}

X64RegManager::X64RegManager() : RegManager() {
    /* TODO: Put your lab5 code here */
  for (std::string reg_name : X64RegNames) {
    temp::Temp *reg = temp::TempFactory::NewTemp();
    temp_map_->Enter(reg, new std::string("%" + reg_name));
    regs_.push_back(reg);
  }
}

temp::TempList *X64RegManager::Registers() {
    /* TODO: Put your lab5 code here */
      return new temp::TempList({
      regs_[RAX],
      regs_[RBX],
      regs_[RCX],
      regs_[RDX],
      regs_[RDI],
      regs_[RBP],
      regs_[RSP],
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
    /* TODO: Put your lab5 code here */
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
    /* TODO: Put your lab5 code here */
      return new temp::TempList({
      regs_[R10],
      regs_[R11],
  });
}

temp::TempList *X64RegManager::CalleeSaves() {
    /* TODO: Put your lab5 code here */
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
    /* TODO: Put your lab5 code here */
    temp::TempList *temp_list = CalleeSaves();
    temp_list->Append(StackPointer());
    temp_list->Append(ReturnValue());
    return temp_list;
}

int X64RegManager::WordSize() { return WORD_SIZE; /* TODO: Put your lab5 code here */ }

temp::Temp *X64RegManager::FramePointer() { return regs_[RBP]; /* TODO: Put your lab5 code here */ }

temp::Temp *X64RegManager::StackPointer() { return regs_[RSP]; /* TODO: Put your lab5 code here */ }

temp::Temp *X64RegManager::ReturnValue() { return regs_[RAX]; /* TODO: Put your lab5 code here */ }

//以上是x86frame.h当中的定义实现 要管的 5-1没写竟然也过了

//escape = true
//定义部分在.h文件当中
InFrameAccess::InFrameAccess(int offset) : offset(offset) {}

tree::Exp *InFrameAccess::ToExp(tree::Exp *frame_ptr) const {
  return new tree::MemExp(
    new tree::BinopExp(tree::PLUS_OP, frame_ptr, new tree::ConstExp(offset))
  );
}

//escape = false
// class InRegAccess : public Access {
// public:
//   temp::Temp *reg;
//   //Temp 对象只是一个“逻辑寄存器”占位符，

//   explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
//   /* TODO: Put your lab5 code here */
//   tree::Exp *ToExp(tree::Exp *framePtr) const override {
//     //这个变量是保存在寄存器里的，所以访问它的表达式就是直接返回对应的 Temp 表达式即可。
//     return new tree::TempExp(reg);
//     //传入的参数并没有用 但是为了frame当中抽象定义所以传入了
//   } 
//   /* End for lab5 code */
// };
//以上是access类的实现

// class X64Frame : public Frame {
//   /* TODO: Put your lab5 code here */
// public:
//   tree::Stm *view_shift;

//   X64Frame(temp::Label *name, std::list<frame::Access *> *formals)
//       : Frame(8, 0, name, formals), view_shift(nullptr) {}

//   [[nodiscard]] std::string GetLabel() const override { return name_->Name(); }
//   [[nodiscard]] temp::Label *Name() const override { return name_; }
//   [[nodiscard]] std::list<frame::Access *> *Formals() const override {
//     return formals_;
//   }
//   frame::Access *AllocLocal(bool escape) override {
//     /* TODO: Put your lab5 code here */
//     // 根据变量是否逃逸，分配对应的局部变量（寄存器或栈上）。
//     if (escape) {
//         // 从栈上分配，按 8 字节对齐
//         offset_ -= 8;
//         return new InFrameAccess(offset_);
//       } else {
//         // 从寄存器分配
//         return new InRegAccess(temp::TempFactory::NewTemp());
//     }
//   }
//   void AllocOutgoSpace(int size) override {
//     /* TODO: Put your lab5 code here */
//     //记录该函数调用其它函数时，需要为“传出参数”分配的最大栈空间。
//     if (size > out_args_) {
//       out_args_ = size;
//       //更新帧的信息
//     }
//   }
//   /* End for lab5 code */
// };
// 在frame当中定义即可
/* TODO: Put your lab5 code here */

///////////////////////////////////////////////////////////////////////////
// frame::Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
//   /* TODO: Put your lab5 code here */
//   auto *formals_access = new std::list<frame::Access *>();
//   // 创建一个 X64Frame 实例，用于调用 AllocLocal
//   auto *frame = new X64Frame(name, formals_access);
//   // 为每个形式参数分配 Access，并加入 formals_access 列表
//   for (bool escape : formals) {
//     frame::Access *access = frame->AllocLocal(escape);
//     formals_access->push_back(access);
//   }
//   return frame;
// }//对吗？

//似乎不对
///////////////////////////////////////////////////////////////////////////

/**
 * Moving incoming formal parameters, the saving and restoring of callee-save
 * Registers
 * @param frame curruent frame
 * @param stm statements
 * @return statements with saving, restoring and view shift
 */
///////////////////////////////////////////////////////////////////////////

// tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm) {
//   auto x64_frame = dynamic_cast<frame::X64Frame *>(frame);
//   assert(x64_frame);

//   auto callee_list = new tree::ExpList();

//   // Save callee-saved register
//   tree::Stm *save_stm = nullptr;
//   temp::TempList *callees = reg_manager->CalleeSaves();
//   for (auto callee : callees->GetList()) {
//     temp::Temp *r = temp::TempFactory::NewTemp();
//     if (!save_stm)
//       save_stm =
//           new tree::MoveStm(new tree::TempExp(r), new tree::TempExp(callee));
//     else
//       save_stm = new tree::SeqStm(
//           save_stm,
//           new tree::MoveStm(new tree::TempExp(r), new tree::TempExp(callee)));
//     callee_list->Append(new tree::TempExp(r));
//   }

//   // Restore callee-saved register
//   tree::Stm *restore_stm = nullptr;
//   callees = reg_manager->CalleeSaves();
//   auto callee_it = callee_list->GetList().begin();
//   for (auto callee : callees->GetList()) {
//     assert(callee_it != callee_list->GetList().end());
//     if (!restore_stm)
//       restore_stm = new tree::MoveStm(new tree::TempExp(callee), *callee_it++);
//     else
//       restore_stm = new tree::SeqStm(
//           restore_stm,
//           new tree::MoveStm(new tree::TempExp(callee), *callee_it++));
//   }

//   // Add view shift for arguments
//   tree::Stm *exit_stm;
//   if (x64_frame->view_shift == nullptr) {
//     // Outermost frame and functions with no formals do not have formal access_
//     // list and view shift
//     exit_stm = new tree::SeqStm(save_stm, new tree::SeqStm(stm, restore_stm));
//   } else
//     exit_stm = new tree::SeqStm(
//         save_stm, new tree::SeqStm(x64_frame->view_shift,
//                                    new tree::SeqStm(stm, restore_stm)));
//   return exit_stm;
// }
//上面版本正确性有待考证
///////////////////////////////////////////////////////////////////////////
tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm) {
  /* TODO: Put your lab5 code here */
  auto arg_reg_num = reg_manager->ArgRegs()->GetList().size();
  // num of arg of proc
  auto arg_num = frame->formals_->size();
  int formal_idx = 0;  // current processing formal index
  tree::SeqStm *view_shift = nullptr, *tail = nullptr;
  for (Access *formal : *(frame->formals_)) {
    tree::Exp *dst =
        formal->ToExp(new tree::TempExp(reg_manager->FramePointer()));
    tree::Exp *src;
    if (formal_idx < arg_reg_num) {
      // in reg
      src = new tree::TempExp(reg_manager->ArgRegs()->NthTemp(formal_idx));
    } else {
      // in stack
      // TODO: may have bugs in offset
      src = new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, new tree::TempExp(reg_manager->FramePointer()),
          new tree::ConstExp((arg_num - formal_idx) *
                             reg_manager->WordSize())));
    }
    tree::MoveStm *move_stm = new tree::MoveStm(dst, src);
    if (!tail) {
      view_shift = tail = new tree::SeqStm(move_stm, nullptr);
    } else {
      tail->right_ = new tree::SeqStm(move_stm, nullptr);
      tail = static_cast<tree::SeqStm *>(tail->right_);
    }
    ++formal_idx;
  }
  if (view_shift) {
    tail->right_ = stm;
    return view_shift;
  }
  return stm;
}

//5-2新增两个函数：
assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  /* TODO: Put your lab5 code here */
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}

assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
  /* TODO: Put your lab5 code here */
  // TODO: may have bugs

  // prolog part
  std::stringstream prologue;
  const std::string name = temp::LabelFactory::LabelString(frame->name_);
  const int rsp_offset = frame->Size();
  prologue << ".set " << name << "_framesize, " << rsp_offset << std::endl;
  prologue << name << ":" << std::endl;
  prologue << "subq $" << rsp_offset << ", %rsp" << std::endl;

  // epilog part
  std::stringstream epilogue;
  epilogue << "addq $" << rsp_offset << ", %rsp" << std::endl;
  epilogue << "retq" << std::endl << ".END" << std::endl;
  return new assem::Proc(prologue.str(), body, epilogue.str());
}

/* End for lab5 code */

} // namespace frame
