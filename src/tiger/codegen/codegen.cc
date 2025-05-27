#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {
  // 将 reg 寄存器中的值压入当前的栈顶
void CodeGen::SaveRegToAddress(assem::InstrList &list, temp::Temp *base,
                               temp::Temp *src) {
  frame_->offset_ -= reg_manager->WordSize();
  int word_size = reg_manager->WordSize();

  list.Append(new assem::OperInstr(
      "subq $" + std::to_string(word_size) + ", `d0",
      new temp::TempList(base), nullptr, nullptr));

  list.Append(new assem::OperInstr(
      "movq `s0, (`d0)",
      new temp::TempList(base),
      new temp::TempList(src), nullptr));
}

// 用 LEA 指令将 "label(%reg)" 地址加载到目标寄存器
void CodeGen::AppendLeaWithOffset(assem::InstrList* list, temp::Temp* dst,
                     const std::string& base_reg, const std::string& label) {
  list->Append(new assem::OperInstr(
      "leaq " + label + "(" + base_reg + "), `d0",
      new temp::TempList(dst),
      nullptr,
      nullptr));
}


// 向目标寄存器添加立即数
void CodeGen::AppendAddImmediate(assem::InstrList* list, temp::Temp* dst, 
                                                                int imm) {
  list->Append(new assem::OperInstr(
      "addq $" + std::to_string(imm) + ", `d0",
      new temp::TempList(dst),
      nullptr,
      nullptr));
}

//对应的恢复
void CodeGen::RestoreRegFromAddress(assem::InstrList &list, temp::Temp *base,
                                    temp::Temp *dst) {
  int word_size = reg_manager->WordSize();

  list.Append(new assem::OperInstr(
      "subq $" + std::to_string(word_size) + ", `d0",
      new temp::TempList(base), nullptr, nullptr));

  list.Append(new assem::OperInstr(
      "movq (`s0), `d0",
      new temp::TempList(dst),
      new temp::TempList(base), nullptr));
}

// 设置栈帧、保存和恢复 callee-saved 寄存器、生成指令列表。
void CodeGen::Codegen() {
  // 帧大小标签，例如函数名为f_sum时得到f_sum_framesize
  fs_ = frame_->GetLabel() + "_framesize";

  // 生成地址：RAX = fs_ + %rsp
  auto instr_list = new assem::InstrList();
  auto addr_temp = reg_manager->GetRegister(frame::X64RegManager::X64Reg::RAX);
  AppendLeaWithOffset(instr_list, addr_temp, "%rsp", fs_);
  // 加上偏移量（offset_ 表示当前帧的总偏移）
  AppendAddImmediate(instr_list, addr_temp, frame_->offset_);

  // 保存被调用者需要保存的寄存器（callee-saved）
  auto callee_saved = reg_manager->CalleeSaves()->GetList();
  for (auto reg : callee_saved) {
    SaveRegToAddress(*instr_list, addr_temp, reg);
  }

  // 设置帧指针 FP = SP + fs
  auto sp = reg_manager->StackPointer();
  auto fp = reg_manager->FramePointer();
  instr_list->Append(new assem::OperInstr(
      "leaq " + fs_ + "(`s0), `d0",
      new temp::TempList(fp),
      new temp::TempList(sp),
      nullptr));  // 无跳转目标

  // 遍历中间代码 trace，生成汇编
  for (auto stm : traces_->GetStmList()->GetList()) {
    stm->Munch(*instr_list, fs_);
  }

  // 用另一个寄存器（这里用的是 RBX）重新计算恢复地址
  auto restore_temp = reg_manager->GetRegister(frame::X64RegManager::X64Reg::RBX);
  int restore_offset = frame_->offset_ + static_cast<int>(callee_saved.size()) * reg_manager->WordSize();

  AppendLeaWithOffset(instr_list, restore_temp, "%rsp", fs_);
  AppendAddImmediate(instr_list, restore_temp, restore_offset);

  // 恢复寄存器
  for (auto reg : callee_saved) {
    RestoreRegFromAddress(*instr_list, restore_temp, reg);
  }

  // 包装为最终的 AssemInstr 对象
  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(instr_list));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */
//实现一堆munch：Munch 是编译器后端中用来把中间代码（IR）翻译成汇编代码的函数。

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  //序列只需要依次处理
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  //标签是跳转（jmp、je、jne 等）语句的目标
  //比如 IR 中的 LABEL(label_) => 汇编中的 .Lx:
  instr_list.Append(
      new assem::LabelInstr(temp::LabelFactory::LabelString(label_), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto dst_label = exp_->name_->Name();
  assem::Instr *instr = new assem::OperInstr(
      "jmp " + dst_label, nullptr, nullptr, new assem::Targets(jumps_));
  instr_list.Append(instr);
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left = left_->Munch(instr_list, fs);
  temp::Temp *right = right_->Munch(instr_list, fs);
  //跳转指令 先得到左右的数值
  //在比较数值大小
  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0", nullptr, new temp::TempList({left, right}), nullptr));
  std::string cjump_instr;
  switch (op_) {
  case EQ_OP:
    cjump_instr = "je";
    break;
  case NE_OP:
    cjump_instr = "jne";
    break;
  case LT_OP:
    cjump_instr = "jl";
    break;
  case GT_OP:
    cjump_instr = "jg";
    break;
  case LE_OP:
    cjump_instr = "jle";
    break;
  case GE_OP:
    cjump_instr = "jge";
    break;
  case ULT_OP:
    cjump_instr = "jnb";
    break;
  case ULE_OP:
    cjump_instr = "jnbe";
    break;
  case UGT_OP:
    cjump_instr = "jna";
    break;
  case UGE_OP:
    cjump_instr = "jnae";
    break;
  case REL_OPER_COUNT:
  default:
    assert(0);
  }
  //生成跳转指令：有两个跳转对象
  instr_list.Append(
      new assem::OperInstr(cjump_instr + " `j0", nullptr, nullptr,
                           new assem::Targets(new std::vector<temp::Label *>{
                               true_label_, false_label_})));
}

//作用是将中间代码中的赋值语句（Move语句）翻译成对应的汇编指令，并加入到汇编指令列表中。
void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *src = src_->Munch(instr_list, fs);
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    // 递归调用 dst_->exp_->Munch 获得内存地址表达式对应的寄存器 dst
    temp::Temp *dst = ((MemExp *)dst_)->exp_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq `s0, (`s1)", nullptr,
                                           new temp::TempList({src, dst})));
  } else {
    // 目标是寄存器
    temp::Temp *dst = dst_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({dst}), new temp::TempList({src})));
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // 只计算表达式，但不保存结果的语句
  exp_->Munch(instr_list, fs);
}

temp::Temp* BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // 根据操作符映射到对应汇编指令名
  std::string op_instr;
  switch (op_) {
  case PLUS_OP: op_instr = "addq"; break;
  case MINUS_OP: op_instr = "subq"; break;
  case MUL_OP: op_instr = "imulq"; break;
  case DIV_OP: op_instr = "idivq"; break;
  case AND_OP: op_instr = "andq"; break;
  case OR_OP: op_instr = "orq"; break;
  case LSHIFT_OP: op_instr = "salq"; break;
  case RSHIFT_OP: op_instr = "shrq"; break;
  case ARSHIFT_OP: op_instr = "sarq"; break;
  case XOR_OP: op_instr = "xorq"; break;
  default: assert(false);
  }

  // 递归生成左、右表达式，获得对应寄存器
  temp::Temp* left = left_->Munch(instr_list, fs);
  temp::Temp* right = right_->Munch(instr_list, fs);

  // 新建临时寄存器保存结果
  temp::Temp* reg = temp::TempFactory::NewTemp();

  // 通用寄存器别名，便于调用
  temp::Temp* rax = reg_manager->GetRegister(frame::X64RegManager::X64Reg::RAX);
  temp::Temp* rdx = reg_manager->GetRegister(frame::X64RegManager::X64Reg::RDX);

  if (op_ == MUL_OP) {
    // 乘法特殊逻辑：用rax和rdx寄存器，结果存在rax
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                                           new temp::TempList(rax), 
                                           new temp::TempList(left)));
    instr_list.Append(new assem::OperInstr("imulq `s0", 
                                           new temp::TempList({rax, rdx}), 
                                           new temp::TempList(right), nullptr));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                                           new temp::TempList(reg), 
                                           new temp::TempList(rax)));
    return reg;
  } else if (op_ == DIV_OP) {
    // 除法特殊逻辑：rax/rdx寄存器，商放rax
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                                           new temp::TempList(rax), 
                                           new temp::TempList(left)));
    instr_list.Append(new assem::OperInstr("cqto", 
                                           new temp::TempList(rdx), 
                                           new temp::TempList(rax), nullptr));
    instr_list.Append(new assem::OperInstr("idivq `s0", 
                                           new temp::TempList({rax, rdx}), 
                                           new temp::TempList(right), nullptr));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                                           new temp::TempList(reg), 
                                           new temp::TempList(rax)));
    return reg;
  } else {
    // 其他操作，先将左操作数放到reg，再用op_instr进行运算
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                                           new temp::TempList(reg), 
                                           new temp::TempList(left)));
    instr_list.Append(new assem::OperInstr(op_instr + " `s0, `d0", 
                                           new temp::TempList(reg), 
                                           new temp::TempList(right), nullptr));
    return reg;
  }
}


temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // *(addr)
  temp::Temp *reg = temp::TempFactory::NewTemp();
  temp::Temp *exp = exp_->Munch(instr_list, fs);
  //生成汇编指令 movq (exp), reg
  instr_list.Append(new assem::OperInstr("movq (`s0), `d0",
                                         new temp::TempList(reg),
                                         new temp::TempList(exp), nullptr));
  return reg;
}

//TempExp 代表一个临时寄存器或变量。通常直接返回它对应的寄存器 temp_
temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (temp_ != reg_manager->FramePointer()) {
    return temp_;
  }

// 如果它代表帧指针（FP），而实际实现中没有帧指针，只有栈指针（SP）和栈大小 fs，
// 就通过指令 leaq fs(%rsp), reg 计算出“虚拟帧指针”。
  temp::Temp *fp = temp::TempFactory::NewTemp();
  std::stringstream assem;
  assem << "leaq " << fs << "(`s0), `d0";
  instr_list.Append(new assem::OperInstr(
      assem.str(), new temp::TempList(fp),
      new temp::TempList(reg_manager->StackPointer()), nullptr));
  return fp;
}

//EseqExp 是“先执行一段语句，再计算一个表达式”
temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}


//符号名称（比如全局变量、函数名）的地址加载到一个新申请的临时寄存器中
temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *dst = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::OperInstr("leaq " + name_->Name() + "(%rip), `d0",
                           new temp::TempList(dst), nullptr, nullptr));
  return dst;
}

//将一个立即数常量装载到一个新的临时寄存器中。
temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *dst = temp::TempFactory::NewTemp();
  // Imm
  instr_list.Append(
      new assem::OperInstr("movq $" + std::to_string(consti_) + ", `d0",
                           new temp::TempList(dst), nullptr, nullptr));
  return dst;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // Step 1: evaluate arguments, generate code, and collect argument registers
  auto *arg_regs = args_->MunchArgs(instr_list, fs);
  const auto &arg_list = args_->GetList();

  // Step 2: emit callq instruction
  instr_list.Append(new assem::OperInstr(
      "callq " + (static_cast<NameExp *>(fun_))->name_->Name(),
      reg_manager->CallerSaves(), arg_regs, nullptr));

  // Step 3: fetch return value (%rax)
  auto *ret_temp = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0", 
      new temp::TempList(ret_temp), 
      new temp::TempList(reg_manager->ReturnValue())));

  // Step 4: stack cleanup if extra arguments passed via stack
  int reg_arg_count = reg_manager->ArgRegs()->GetList().size();
  int stack_arg_count = std::max(int(arg_list.size()) - reg_arg_count, 0);
  if (stack_arg_count > 0) {
    int offset = stack_arg_count * reg_manager->WordSize();
    instr_list.Append(new assem::OperInstr(
        "addq $" + std::to_string(offset) + ", `d0",
        new temp::TempList(reg_manager->StackPointer()), nullptr, nullptr));
  }

  return ret_temp;
}


temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  auto *arg_regs = new temp::TempList();
  const auto &arg_reg_list = reg_manager->ArgRegs()->GetList();
  int reg_count = arg_reg_list.size();

  int idx = 0;
  for (Exp *arg : exp_list_) {
    temp::Temp *src = arg->Munch(instr_list, fs);

    if (idx < reg_count) {
      // Use argument register
      temp::Temp *reg = reg_manager->ArgRegs()->NthTemp(idx);
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0",
          new temp::TempList(reg),
          new temp::TempList(src)));
      arg_regs->Append(reg);
    } else {
      // Stack-passed argument
      int word_size = reg_manager->WordSize();
      temp::Temp *sp = reg_manager->StackPointer();

      instr_list.Append(new assem::OperInstr(
          "subq $" + std::to_string(word_size) + ", `d0",
          new temp::TempList(sp), nullptr, nullptr));

      instr_list.Append(new assem::OperInstr(
          "movq `s0, (`d0)",
          new temp::TempList(sp),
          new temp::TempList(src), nullptr));
    }
    ++idx;
  }

  return arg_regs;
}


} // namespace tree
