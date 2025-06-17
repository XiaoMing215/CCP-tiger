#include "tiger/liveness/flowgraph.h"
#include "tiger/codegen/assem.h"


#include "tiger/liveness/flowgraph.h"

namespace fg {

static bool IsLabel(const assem::Instr *instr) {
  return typeid(*instr) == typeid(assem::LabelInstr);
}

static bool IsOper(const assem::Instr *instr) {
  return typeid(*instr) == typeid(assem::OperInstr);
}

static bool HasJumps(const assem::Instr *instr) {
  if (const auto *oper = dynamic_cast<const assem::OperInstr *>(instr)) {
    return oper->jumps_ != nullptr;
  }
  return false;
}

static temp::Label *GetLabel(const assem::Instr *instr) {
  return static_cast<const assem::LabelInstr *>(instr)->label_;
}

static assem::Targets *GetJumps(const assem::Instr *instr) {
  return static_cast<const assem::OperInstr *>(instr)->jumps_;
}

void FlowGraphFactory::AssemFlowGraph() { //遍历汇编指令列表，创建 CFG 节点
  assem::Instr *prev_instr = nullptr;
  FNodePtr prev_node = nullptr;

  for (assem::Instr *instr : instr_list_->GetList()) {
    FNodePtr node = flowgraph_->NewNode(instr);

    if (prev_instr) {
      if (IsOper(prev_instr)) { //普通顺序控制流边
        if (!HasJumps(prev_instr)) {//大于等于类
          flowgraph_->AddEdge(prev_node, node);
        }
      } else {
        if (IsLabel(prev_instr)) {//就是跳转
          label_map_->Enter(GetLabel(prev_instr), node);
        }
        flowgraph_->AddEdge(prev_node, node); //给上一条指令加边指向当前指令。
      }
    }

    prev_instr = instr;
    prev_node = node;
  }

  for (FNodePtr node : flowgraph_->Nodes()->GetList()) {
    assem::Instr *instr = node->NodeInfo();

    if (IsOper(instr) && HasJumps(instr)) {//加入跳转边
      for (temp::Label *target : *(GetJumps(instr)->labels_)) {
        FNodePtr target_node = label_map_->Look(target);
        if (target_node) {
          flowgraph_->AddEdge(node, target_node);
        }
      }
    }
  }
}
// 图构建完后，这个 CFG 就是下一步做活跃变量分析（liveness 模块）的输入。
}  // namespace fg

// 每个汇编指令都有两个属性：
// Use()：这条指令读了哪些临时寄存器（temp）；
// Def()：这条指令写了哪些 temp；
// TempList 是一个链表结构，通常表示寄存器（Temp）集合。
namespace assem {

temp::TempList *LabelInstr::Def() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Def() const {
  if (dst_)
    return dst_;
  else
    return new temp::TempList();
}

temp::TempList *OperInstr::Def() const {
  if (dst_)
    return dst_;
  else
    return new temp::TempList();
}

temp::TempList *LabelInstr::Use() const { return new temp::TempList(); }



temp::TempList *MoveInstr::Use() const {
  if (src_)
    return src_;
  else
    return new temp::TempList();
}


temp::TempList *OperInstr::Use() const {
  if (src_)
    return src_;
  else
    return new temp::TempList();
}

} // namespace assem