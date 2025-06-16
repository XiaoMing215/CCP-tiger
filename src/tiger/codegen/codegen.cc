#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::PushRegOnStack(assem::InstrList &list, temp::Temp *reg) {
  //生成函数调用保存寄存器现场代码，保护寄存器值。
  auto sp = reg_manager->StackPointer();
  int word_size = reg_manager->WordSize();

  frame_->offset_ -= word_size;

  list.Append(new assem::OperInstr(
      "subq $" + std::to_string(word_size) + ", `d0",
      new temp::TempList(sp), nullptr, nullptr));

  list.Append(new assem::OperInstr(
      "movq `s0, (`d0)", new temp::TempList(sp),
      new temp::TempList(reg), nullptr));
}

void CodeGen::PopRegFromStack(assem::InstrList &list, temp::Temp *reg) {
  //生成函数调用保存寄存器现场代码，保护寄存器值。
  auto sp = reg_manager->StackPointer();
  int word_size = reg_manager->WordSize();

  list.Append(new assem::OperInstr(
      "movq (`s0), `d0", new temp::TempList(reg),
      new temp::TempList(sp), nullptr));

  list.Append(new assem::OperInstr(
      "addq $" + std::to_string(word_size) + ", `d0",
      new temp::TempList(sp), nullptr, nullptr));
}

void CodeGen::PushRegToPos(assem::InstrList &list, temp::Temp *pos, temp::Temp *val) {
  //使用的是pos指向的寄存器（比如栈指针）
  int size = reg_manager->WordSize();
  frame_->offset_ -= size;

  list.Append(new assem::OperInstr(
      "subq $" + std::to_string(size) + ", `d0",
      new temp::TempList(pos), nullptr, nullptr));

  list.Append(new assem::OperInstr(
      "movq `s0, (`d0)",
      new temp::TempList(pos),
      new temp::TempList(val), nullptr));
}

void CodeGen::PopRegFromPos(assem::InstrList &list, temp::Temp *pos, temp::Temp *dst) {
  //从pos指向的栈位置弹出值到寄存器dst
  int size = reg_manager->WordSize();

  list.Append(new assem::OperInstr(
      "subq $" + std::to_string(size) + ", `d0",
      new temp::TempList(pos), nullptr, nullptr));

  list.Append(new assem::OperInstr(
      "movq (`s0), `d0",
      new temp::TempList(dst),
      new temp::TempList(pos), nullptr));
}

//主代码生成入口，遍历中间代码stmts，
//调用每条语句的Munch方法生成汇编指令，并封装成ProcEntryExit2的函数体。
void CodeGen::Codegen() {
  auto &stmts = traces_->GetStmList()->GetList();
  auto ilist = new assem::InstrList();

  fs_ = frame_->GetFrameLabel() + "_framesize";

  for (auto stmt : stmts)
    stmt->Munch(*ilist, fs_);

  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(ilist));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto *inst : instr_list_->GetList())
    inst->Print(out, map);
  fputc('\n', out);
}

temp::TempList *MunchOperand(tree::Exp *e, OperandRole role,
                             std::string &assem, assem::InstrList &list,
                             std::string_view fs) {
  if (auto c = dynamic_cast<const tree::ConstExp *>(e)) {
    assem = "$" + std::to_string(c->consti_);
    return new temp::TempList();
  }

  if (auto mem = dynamic_cast<const tree::MemExp *>(e)) {
    tree::Exp *addr = mem->exp_;
    assem = role == SRC ? "(`s0)" : "(`d0)";

    if (auto binop = dynamic_cast<tree::BinopExp *>(addr)) {
      tree::Exp *l = binop->left_, *r = binop->right_;

      if (auto lc = dynamic_cast<const tree::ConstExp *>(l)) {
        assem = std::to_string(lc->consti_) + assem;
        return new temp::TempList(r->Munch(list, fs));
      }

      if (auto rc = dynamic_cast<const tree::ConstExp *>(r)) {
        assem = std::to_string(rc->consti_) + assem;
        return new temp::TempList(l->Munch(list, fs));
      }
    }

    return new temp::TempList(addr->Munch(list, fs));
  }

  // fallback: register
  assem = (role == SRC) ? "`s0" : "`d0";
  return new temp::TempList(e->Munch(list, fs));
}

} // namespace cg


namespace tree {

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::LabelInstr(
      temp::LabelFactory::LabelString(label_), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto target = exp_->name_->Name();
  instr_list.Append(new assem::OperInstr(
      "jmp " + target, nullptr, nullptr, new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto *lhs = left_->Munch(instr_list, fs);
  auto *rhs = right_->Munch(instr_list, fs);

  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0", nullptr, new temp::TempList({lhs, rhs}), nullptr));

  static const std::unordered_map<RelOp, std::string> relop_mnemonics = {
      {EQ_OP, "je"},   {NE_OP, "jne"}, {LT_OP, "jl"},   {GT_OP, "jg"},
      {LE_OP, "jle"},  {GE_OP, "jge"}, {ULT_OP, "jnb"}, {ULE_OP, "jnbe"},
      {UGT_OP, "jna"}, {UGE_OP, "jnae"}};

  auto it = relop_mnemonics.find(op_);
  assert(it != relop_mnemonics.end() && "Unknown RelOp in CjumpStm");

  instr_list.Append(new assem::OperInstr(
      it->second + " `j0", nullptr, nullptr,
      new assem::Targets(new std::vector<temp::Label *>{
          true_label_, false_label_})));
}


void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *src = src_->Munch(instr_list, fs); // IR 表达式翻译成目标汇编中的某个寄存器，并把生成的指令加入 instr_list

  if (typeid(*dst_) == typeid(tree::MemExp)) {
    auto *mem_dst = static_cast<tree::MemExp *>(dst_);
    temp::Temp *addr = mem_dst->exp_->Munch(instr_list, fs);

    instr_list.Append(new assem::OperInstr(
        "movq `s0, (`s1)",
        nullptr,
        new temp::TempList({src, addr}),
        nullptr));
  } else {
    temp::Temp *dst = dst_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({dst}),
        new temp::TempList({src})));
  }
}



void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  static const std::unordered_map<BinOp, std::string> binop_instr_map = {
      {PLUS_OP,    "addq"},
      {MINUS_OP,   "subq"},
      {AND_OP,     "andq"},
      {OR_OP,      "orq"},
      {LSHIFT_OP,  "salq"},
      {RSHIFT_OP,  "shrq"},
      {ARSHIFT_OP, "sarq"},
      {XOR_OP,     "xorq"},
  };

  temp::Temp *rax = reg_manager->GetRegister(frame::X64RegManager::X64Reg::RAX);
  temp::Temp *rdx = reg_manager->GetRegister(frame::X64RegManager::X64Reg::RDX);

  // 特殊处理 DIV
  if (op_ == DIV_OP) {
    temp::Temp *reg = temp::TempFactory::NewTemp();
    temp::Temp *lhs = left_->Munch(instr_list, fs);
    temp::Temp *rhs = right_->Munch(instr_list, fs);

    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", new temp::TempList(rax), new temp::TempList(lhs)));
    instr_list.Append(new assem::OperInstr("cqto", new temp::TempList({rax, rdx}), new temp::TempList(rax), nullptr));
    instr_list.Append(new assem::OperInstr("idivq `s0", new temp::TempList({rax, rdx}), new temp::TempList({rhs, rax, rdx}), nullptr));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", new temp::TempList(reg), new temp::TempList(rax)));
    return reg;
  }

  // 特殊处理 MUL
  if (op_ == MUL_OP) {
    temp::Temp *reg = temp::TempFactory::NewTemp();
    temp::Temp *lhs = left_->Munch(instr_list, fs);
    temp::Temp *rhs = right_->Munch(instr_list, fs);

    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", new temp::TempList(rax), new temp::TempList(lhs)));
    instr_list.Append(new assem::OperInstr("imulq `s0", new temp::TempList({rax, rdx}), new temp::TempList({rhs, rax}), nullptr));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", new temp::TempList(reg), new temp::TempList(rax)));
    return reg;
  }

  // 一般二元运算（左值 mov 到目标寄存器，再执行 op 指令）
  auto it = binop_instr_map.find(op_);
  assert(it != binop_instr_map.end());  // 相当于 switch default 中 assert(0)

  std::string op_instr = it->second;
  std::string left_assem, right_assem;
  temp::TempList *left = cg::MunchOperand(left_, cg::OperandRole::SRC, left_assem, instr_list, fs);
  temp::TempList *right = cg::MunchOperand(right_, cg::OperandRole::SRC, right_assem, instr_list, fs);
  temp::Temp *result = temp::TempFactory::NewTemp();

  // 目标寄存器也作为操作数右侧
  right->Append(result);
  temp::TempList *dst = new temp::TempList(result);

  if (typeid(*left_) == typeid(tree::MemExp)) {
    instr_list.Append(new assem::OperInstr("movq " + left_assem + ", `d0", dst, left, nullptr));
  } else {
    instr_list.Append(new assem::MoveInstr("movq " + left_assem + ", `d0", dst, left));
  }

  instr_list.Append(new assem::OperInstr(op_instr + " " + right_assem + ", `d0", dst, right, nullptr));
  return result;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // only deal with src is mem
  temp::Temp *reg = temp::TempFactory::NewTemp();
  temp::Temp *exp = exp_->Munch(instr_list, fs);
  instr_list.Append(new assem::OperInstr("movq (`s0), `d0",
                                         new temp::TempList(reg),
                                         new temp::TempList(exp), nullptr));
  return reg;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view frame_offset) {
  auto *fp_temp = reg_manager->FramePointer();
  if (temp_ != fp_temp) {
    return temp_;
  }

  // 没有真实的帧指针，改用栈指针加偏移替代
  temp::Temp *result_temp = temp::TempFactory::NewTemp();

  std::string instruction = "leaq " + std::string(frame_offset) + "(`s0), `d0";
  instr_list.Append(new assem::OperInstr(
      instruction,
      new temp::TempList(result_temp),
      new temp::TempList(reg_manager->StackPointer()),
      nullptr));

  return result_temp;
}


temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *dst = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::OperInstr("leaq " + name_->Name() + "(%rip), `d0",
                           new temp::TempList(dst), nullptr, nullptr));
  return dst;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *dst = temp::TempFactory::NewTemp();
  // Imm
  instr_list.Append(
      new assem::OperInstr("movq $" + std::to_string(consti_) + ", `d0",
                           new temp::TempList(dst), nullptr, nullptr));
  return dst;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view frame_offset) {
  // 生成参数传递代码，获得传递的寄存器列表
  temp::TempList *arg_regs = args_->MunchArgs(instr_list, frame_offset);

  // 生成CALL指令，调用函数名
  std::string func_name = static_cast<NameExp *>(fun_)->name_->Name();
  instr_list.Append(new assem::OperInstr(
      "callq " + func_name,
      reg_manager->CallerSaves(),  // CALL指令会破坏这些寄存器
      arg_regs,
      nullptr));

  // 分配一个临时保存函数返回值
  temp::Temp *return_temp = temp::TempFactory::NewTemp();

  // 将返回值寄存器的值移到return_temp中
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList(return_temp),
      new temp::TempList(reg_manager->ReturnValue())));

  // 栈上参数需要恢复栈指针
  const int arg_regs_count = reg_manager->ArgRegs()->GetList().size();
  int total_args_count = static_cast<int>(args_->GetList().size());
  int stack_args_count = std::max(total_args_count - arg_regs_count, 0);

  if (stack_args_count > 0) {
    int stack_bytes = stack_args_count * reg_manager->WordSize();
    instr_list.Append(new assem::OperInstr(
        "addq $" + std::to_string(stack_bytes) + ", `d0",
        new temp::TempList(reg_manager->StackPointer()),
        nullptr,
        nullptr));
  }

  return return_temp;
}


temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view frame_offset) {
  // 用于保存传递参数使用的寄存器列表
  temp::TempList *arg_regs = new temp::TempList();

  const int reg_arg_count = reg_manager->ArgRegs()->GetList().size();
  int idx = 0;

  for (Exp *arg_exp : exp_list_) {
    temp::Temp *src_temp = arg_exp->Munch(instr_list, frame_offset);

    if (idx < reg_arg_count) {
      // 使用寄存器传参
      temp::Temp *arg_reg = reg_manager->ArgRegs()->NthTemp(idx);
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0",
          new temp::TempList(arg_reg),
          new temp::TempList(src_temp)));
      arg_regs->Append(arg_reg);
    } else {
      // 栈上传参，先调整栈指针，再存值
      instr_list.Append(new assem::OperInstr(
          "subq $" + std::to_string(reg_manager->WordSize()) + ", `d0",
          new temp::TempList(reg_manager->StackPointer()),
          nullptr,
          nullptr));
      instr_list.Append(new assem::OperInstr(
          "movq `s0, (`d0)",
          new temp::TempList(reg_manager->StackPointer()),
          new temp::TempList(src_temp),
          nullptr));
    }
    ++idx;
  }

  return arg_regs;
}


} // namespace tree