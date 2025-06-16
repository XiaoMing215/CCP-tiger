#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

// 寄存器分配结果，包含寄存器映射和重写后的指令列表
class Result {
public:
  temp::Map *coloring_;        // 临时寄存器到物理寄存器的映射
  assem::InstrList *il_;       // 重写后的汇编指令链表

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result();
};

class RegAllocator {
private:
  std::unique_ptr<ra::Result> result_;    // 分配结果
  frame::Frame *frame_;                    // 当前函数的栈帧信息
  assem::InstrList *assem_instr_;         // 原始汇编指令列表

  live::IGraphPtr interf_graph;            // 干扰图
  live::MoveList *moves;                   // 需要合并的移动指令列表
  tab::Table<temp::Temp, live::INode> *temp_node_map;  // 临时寄存器到干扰图节点的映射

  live::INodeListPtr precolored;           // 已经预着色的寄存器节点（物理寄存器）

  // 工作列表、集合和栈，分类管理干扰图节点
  live::INodeListPtr simplify_worklist;    
  live::INodeListPtr freeze_worklist;      
  live::INodeListPtr spill_worklist;       
  live::INodeListPtr spilled_nodes;        
  live::INodeListPtr initial;            
  live::INodeListPtr coalesced_nodes;      
  live::INodeListPtr select_stack;          
  live::INodeListPtr colored_nodes;         

  live::INodeListPtr no_spill_temps;        // 标记为不允许溢出的寄存器节点

  // 移动指令集，管理合并状态
  live::MoveList *worklist_moves;        
  live::MoveList *active_moves;           
  live::MoveList *coalesced_moves;        
  live::MoveList *constrained_moves;       
  live::MoveList *frozen_moves;            

  tab::Table<live::INode, live::MoveList> *move_list; 
  tab::Table<live::INode, int> *degree;                
  tab::Table<live::INode, live::INode> *alias;         

  // K表示寄存器的数量（颜色数）
  int K;

  // 以下为寄存器分配算法的主要步骤函数声明
  void LivenessAnalysis();
  void Init();
  void ClearAndInit();

  void Build();
  void AddEdge(live::INodePtr u, live::INodePtr v);
  void MakeWorklist();
  live::INodeListPtr Adjacent(live::INodePtr n);
  live::MoveList *NodeMoves(live::INodePtr n);
  bool MoveRelated(live::INodePtr n);
  void Simplify();
  void DecrementDegree(live::INodePtr m);
  void EnableMoves(live::INodeListPtr nodes);
  void Coalesce();
  void AddWorkList(live::INodePtr u);
  bool OK(live::INodePtr t, live::INodePtr r);
  bool Conservative(live::INodeListPtr nodes);
  live::INodePtr GetAlias(live::INodePtr n);
  void Combine(live::INodePtr u, live::INodePtr v);
  void Freeze();
  void FreezeMoves(live::INodePtr u);
  void SelectSpill();
  col::Result AssignColor();
  void RewriteProgram();

  assem::InstrList *RemoveRedundantMove(temp::Map *coloring);
  bool IsRedundant(assem::Instr *instr, temp::Map *coloring);

public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr);
  void RegAlloc();
  std::unique_ptr<ra::Result> BuildAllocationResult() { return std::move(result_); }
};

} // namespace ra

#endif
