#include "tiger/regalloc/regalloc.h"
#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
Result::~Result() {
}

RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assem_instr)
    : frame_(frame), assem_instr_(assem_instr->GetInstrList()) {
  Init();
  K = reg_manager->Registers()->GetList().size();
}

void RegAllocator::RegAlloc() {
  LivenessAnalysis();
  Build();
  MakeWorklist();
  do {
    if (!simplify_worklist->GetList().empty())
      Simplify();
    else if (!worklist_moves->GetList().empty())
      Coalesce();
    else if (!freeze_worklist->GetList().empty())
      Freeze();
    else if (!spill_worklist->GetList().empty())
      SelectSpill();
  } while (!(simplify_worklist->GetList().empty() &&
             worklist_moves->GetList().empty() &&
             freeze_worklist->GetList().empty() &&
             spill_worklist->GetList().empty()));

  auto color_assign_result = AssignColor();

  if (!spilled_nodes->GetList().empty()) {
    RewriteProgram();
    RegAlloc();
  } else {
    result_ = std::make_unique<Result>(
        color_assign_result.coloring,
        RemoveRedundantMove(color_assign_result.coloring));
  }
}

bool RegAllocator::IsRedundant(assem::Instr *instr, temp::Map *coloring) {
  if (typeid(*instr) != typeid(assem::MoveInstr)) return false;

  auto move_instr = static_cast<assem::MoveInstr *>(instr);
  if (!move_instr->src_ || !move_instr->dst_) return false;

  auto &src_list = move_instr->src_->GetList();
  auto &dst_list = move_instr->dst_->GetList();
  if (src_list.size() != 1 || dst_list.size() != 1) return false;

  auto src_color = coloring->Look(src_list.front());
  auto dst_color = coloring->Look(dst_list.front());

  return src_color == dst_color;
}


assem::InstrList *RegAllocator::RemoveRedundantMove(temp::Map *coloring) {
  auto new_instr_list = new assem::InstrList();
  for (auto *instr : assem_instr_->GetList()) {
    if (!IsRedundant(instr, coloring)) {
      new_instr_list->Append(instr);
    }
  }
  return new_instr_list;
}


void RegAllocator::LivenessAnalysis() {
  fg::FlowGraphFactory flow_graph_factory(assem_instr_);
  flow_graph_factory.AssemFlowGraph();

  live::LiveGraphFactory live_graph_factory(flow_graph_factory.GetFlowGraph());
  live_graph_factory.Liveness();

  auto live_graph = live_graph_factory.GetLiveGraph();
  interf_graph = live_graph.interf_graph;
  moves = live_graph.moves;
  temp_node_map = live_graph_factory.GetTempNodeMap();

  worklist_moves = moves;
}



void RegAllocator::Build() {
  ClearAndInit();

  auto temp_map = reg_manager->temp_map_;
  for (auto &node : interf_graph->Nodes()->GetList()) {
    degree->Enter(node, new int(node->OutDegree()));

    auto related_moves = new live::MoveList();
    for (const auto &move : worklist_moves->GetList()) {
      if (move.first == node || move.second == node) {
        related_moves->Append(move.first, move.second);
      }
    }
    move_list->Enter(node, related_moves);

    alias->Enter(node, node);

    if (temp_map->Look(node->NodeInfo())) {
      precolored->Append(node);
    } else {
      initial->Append(node);
    }
  }
}


void RegAllocator::AddEdge(live::INodePtr u, live::INodePtr v) {
  if (u == v || u->Adj(v)) return;

  if (!precolored->Contain(u)) {
    interf_graph->AddEdge(u, v);
    ++(*degree->Look(u));
  }
  if (!precolored->Contain(v)) {
    interf_graph->AddEdge(v, u);
    ++(*degree->Look(v));
  }
}


void RegAllocator::MakeWorklist() {
  for (auto &node : initial->GetList()) {
    int deg = *degree->Look(node);
    if (deg >= K) {
      spill_worklist->Union(node);
    } else if (MoveRelated(node)) {
      freeze_worklist->Union(node);
    } else {
      simplify_worklist->Union(node);
    }
  }
  initial->Clear();
}


live::INodeListPtr RegAllocator::Adjacent(live::INodePtr n) {
  auto adj_list = n->Succ();
  return adj_list->Diff(select_stack->Union(coalesced_nodes));
}

live::MoveList *RegAllocator::NodeMoves(live::INodePtr n) {
  return move_list->Look(n)->Intersect(active_moves->Union(worklist_moves));
}

bool RegAllocator::MoveRelated(live::INodePtr n) {
  return !NodeMoves(n)->GetList().empty();
}

void RegAllocator::Simplify() {
  if (simplify_worklist->GetList().empty())
    return;
  auto n = simplify_worklist->GetList().front();
  simplify_worklist->DeleteNode(n);
  select_stack->Prepend(n);

  for (auto m : Adjacent(n)->GetList()) {
    DecrementDegree(m);
  }
}
//
void RegAllocator::DecrementDegree(live::INodePtr m) {
  if (precolored->Contain(m)) {
    return;
  }

  auto d = degree->Look(m);
  --(*d);

  if (*d == K) {
    live::INodeListPtr m_set = new live::INodeList();
    m_set->Append(m);
    EnableMoves(m_set->Union(Adjacent(m)));
    delete m_set;

    spill_worklist->DeleteNode(m);

    if (MoveRelated(m)) {
      freeze_worklist->Union(m);
    } else {
      simplify_worklist->Union(m);
    }
  }
}

void RegAllocator::EnableMoves(live::INodeListPtr nodes) {
  for (auto n : nodes->GetList()) {
    for (auto m : NodeMoves(n)->GetList()) {
      if (active_moves->Contain(m.first, m.second)) {
        active_moves->Delete(m.first, m.second);
        worklist_moves->Union(m.first, m.second);
      }
    }
  }
}

void RegAllocator::Coalesce() {
  if (worklist_moves->GetList().empty()) return;

  auto move = worklist_moves->GetList().front();
  worklist_moves->Delete(move.first, move.second);

  auto u = GetAlias(move.first);
  auto v = GetAlias(move.second);

  if (precolored->Contain(v)) std::swap(u, v);

  auto IsSameNode = [](auto a, auto b) { return a == b; };

  auto IsConstrained = [this](auto a, auto b) {
    return precolored->Contain(b) || a->Adj(b);
  };

  auto CanCoalesce = [this](auto a, auto b) {
    if (precolored->Contain(a)) {
      auto adj_b = Adjacent(b);
      if (!adj_b) return true;
      for (auto t : adj_b->GetList()) {
        if (!OK(t, a)) return false;
      }
      return true;
    } else {
      return Conservative(Adjacent(a)->Union(Adjacent(b)));
    }
  };

  auto MarkCoalesced = [this](auto x, auto y) {
    coalesced_moves->Union(x, y);
  };

  auto MarkConstrained = [this](auto x, auto y) {
    constrained_moves->Union(x, y);
  };

  auto MarkActive = [this](auto x, auto y) {
    active_moves->Union(x, y);
  };

  if (IsSameNode(u, v)) {
    MarkCoalesced(u, v);
    AddWorkList(u);
  } else if (IsConstrained(u, v)) {
    MarkConstrained(u, v);
    AddWorkList(u);
    AddWorkList(v);
  } else if (CanCoalesce(u, v)) {
    MarkCoalesced(u, v);
    Combine(u, v);
    AddWorkList(u);
  } else {
    MarkActive(u, v);
  }
}




void RegAllocator::AddWorkList(live::INodePtr u) {
  if (!precolored->Contain(u) && !MoveRelated(u) && (*(degree->Look(u)) < K)) {
    freeze_worklist->DeleteNode(u);
    simplify_worklist->Union(u);
  }
}

bool RegAllocator::OK(live::INodePtr t, live::INodePtr r) {
  return *(degree->Look(t)) < K || precolored->Contain(t) || t->Adj(r);
}

bool RegAllocator::Conservative(live::INodeListPtr nodes) {
  int k = 0;
  for (auto n : nodes->GetList()) {
    if (*(degree->Look(n)) >= K) {
      k = k + 1;
    }
  }
  return (k < K);
}

live::INodePtr RegAllocator::GetAlias(live::INodePtr n) {
  if (coalesced_nodes->Contain(n))
    return GetAlias(alias->Look(n));
  else
    return n;
}

void RegAllocator::Combine(live::INodePtr u, live::INodePtr v) {
  auto RemoveFromWorklist = [this](live::INodePtr node) {
    if (freeze_worklist->Contain(node)) {
      freeze_worklist->DeleteNode(node);
    } else {
      spill_worklist->DeleteNode(node);
    }
  };

  auto UpdateAliasAndMoves = [this](live::INodePtr u, live::INodePtr v) {
    coalesced_nodes->Union(v);
    alias->Set(v, u);
    move_list->Set(u, move_list->Look(u)->Union(move_list->Look(v)));
  };

  auto EnableMovesOnNodes = [this](live::INodeList *nodes) {
    EnableMoves(nodes);
  };

  auto AddEdgesAndDecrement = [this, u](live::INodeList *adj_nodes) {
    for (auto t : adj_nodes->GetList()) {
      AddEdge(t, u);
      DecrementDegree(t);
    }
  };

  auto MoveIfNeeded = [this](live::INodePtr u) {
    if (*(degree->Look(u)) >= K && freeze_worklist->Contain(u)) {
      freeze_worklist->DeleteNode(u);
      spill_worklist->Union(u);
    }
  };

  // 主逻辑
  RemoveFromWorklist(v);
  UpdateAliasAndMoves(u, v);

  auto v_list = new live::INodeList();
  v_list->Append(v);
  EnableMovesOnNodes(v_list);

  AddEdgesAndDecrement(Adjacent(v));
  MoveIfNeeded(u);
}


void RegAllocator::Freeze() {
  if (freeze_worklist->GetList().empty()) return;

  auto u = freeze_worklist->GetList().front();
  freeze_worklist->DeleteNode(u);
  simplify_worklist->Union(u);
  FreezeMoves(u);
}

void RegAllocator::FreezeMoves(live::INodePtr u) {
  auto ProcessMove = [this, u](const std::pair<live::INodePtr, live::INodePtr> &m) {
    auto x = m.first;
    auto y = m.second;

    live::INodePtr v = (GetAlias(y) == GetAlias(u)) ? GetAlias(x) : GetAlias(y);

    active_moves->Delete(x, y);
    frozen_moves->Union(x, y);

    if (NodeMoves(v)->GetList().empty() && *(degree->Look(v)) < K) {
      freeze_worklist->DeleteNode(v);
      simplify_worklist->Union(v);
    }
  };

  for (auto m : NodeMoves(u)->GetList()) {
    ProcessMove(m);
  }
}

void RegAllocator::SelectSpill() {
  if (spill_worklist->GetList().empty()) return;

  // 选度数最大的节点溢出
  live::INodePtr u = nullptr;
  int max_degree = -1;
  for (auto t : spill_worklist->GetList()) {
    int d = t->Degree();
    if (d > max_degree) {
      max_degree = d;
      u = t;
    }
  }
  spill_worklist->DeleteNode(u);
  simplify_worklist->Union(u);
  FreezeMoves(u);
}

col::Result RegAllocator::AssignColor() {
  col::Color color;

  while (!select_stack->GetList().empty()) {
    auto n = select_stack->GetList().front();
    select_stack->DeleteNode(n);

    color.InitOkColors();

    for (auto w : n->Succ()->GetList()) {
      auto alias_w = GetAlias(w);
      if ((colored_nodes->Union(precolored))->Contain(alias_w)) {
        color.RemoveOkColor(alias_w);
      }
    }

    if (color.OkColorsEmpty()) {
      spilled_nodes->Union(n);
    } else {
      colored_nodes->Union(n);
      color.AssignColor(n);
    }
  }

  for (live::INodePtr n : coalesced_nodes->GetList()) {
    color.AssignSameColor(GetAlias(n), n);
  }

  return color.BuildAndGetResult();
}

///

void RegAllocator::RewriteProgram() {
  live::INodeListPtr new_temps = new live::INodeList();
  no_spill_temps->Clear();

  // 辅助函数：生成fetch指令（从内存加载到寄存器）
  auto makeFetchInstr = [&](int offset, temp::Temp* vi) -> assem::OperInstr* {
    std::string ins = "movq (" + frame_->frameLabel_->Name() + "_framesize" +
                      std::to_string(offset) + ")(`s0), `d0";
    return new assem::OperInstr(ins, new temp::TempList(vi),
                               new temp::TempList(reg_manager->StackPointer()), nullptr);
  };

  // 辅助函数：生成store指令（从寄存器保存到内存）
  auto makeStoreInstr = [&](int offset, temp::Temp* vi) -> assem::OperInstr* {
    std::string ins = "movq `s0, (" + frame_->frameLabel_->Name() + "_framesize" +
                      std::to_string(offset) + ")(`d0)";
    return new assem::OperInstr(ins, new temp::TempList(reg_manager->StackPointer()),
                               new temp::TempList(vi), nullptr);
  };

  for (live::INodePtr v : spilled_nodes->GetList()) {
    // 为溢出节点分配栈空间
    frame::InFrameAccess* access = static_cast<frame::InFrameAccess*>(
        frame::Access::AllocLocal(frame_, true));
    
    // 保存分配偏移
    int offset = frame_->offset_;

    temp::Temp* old_temp = v->NodeInfo();
    temp::Temp* vi = temp::TempFactory::NewTemp();

    auto new_instr_list = new assem::InstrList();

    for (auto instr_it = assem_instr_->GetList().begin(); 
         instr_it != assem_instr_->GetList().end(); ++instr_it) {
      assem::Instr* instr = *instr_it;
      assert(!precolored->Contain(v));

      instr->ReplaceTemp(old_temp, vi);

      if (instr->Use()->Contain(vi)) {
        new_instr_list->Append(makeFetchInstr(offset, vi));
      }

      new_instr_list->Append(instr);

      if (instr->Def()->Contain(vi)) {
        new_instr_list->Append(makeStoreInstr(offset, vi));
      }
    }

    assem_instr_ = new_instr_list;

    live::INodePtr new_node = interf_graph->NewNode(vi);
    new_temps->Union(new_node);
    no_spill_temps->Append(new_node);
  }

  spilled_nodes->Clear();
  initial->Clear();
  initial = colored_nodes->Union(coalesced_nodes->Union(new_temps));
  colored_nodes->Clear();
  coalesced_nodes->Clear();
}


void RegAllocator::Init() {
  precolored = new live::INodeList();
  simplify_worklist = new live::INodeList();
  freeze_worklist = new live::INodeList();
  spill_worklist = new live::INodeList();
  spilled_nodes = new live::INodeList();
  initial = new live::INodeList();
  coalesced_nodes = new live::INodeList();
  select_stack = new live::INodeList();
  colored_nodes = new live::INodeList();
  no_spill_temps = new live::INodeList();

  worklist_moves = new live::MoveList();
  active_moves = new live::MoveList();
  coalesced_moves = new live::MoveList();
  constrained_moves = new live::MoveList();
  frozen_moves = new live::MoveList();

  move_list = new tab::Table<live::INode, live::MoveList>();
  degree = new tab::Table<live::INode, int>();
  alias = new tab::Table<live::INode, live::INode>();
}
void RegAllocator::ClearAndInit() {
  precolored->Clear();
  simplify_worklist->Clear();
  freeze_worklist->Clear();
  spill_worklist->Clear();
  spilled_nodes->Clear();
  initial->Clear();
  coalesced_nodes->Clear();
  select_stack->Clear();
  colored_nodes->Clear();
//  no_spill_temps->Clear();

  //  delete worklist_moves;
  //  worklist_moves = new live::MoveList();
  delete active_moves;
  active_moves = new live::MoveList();
  delete coalesced_moves;
  coalesced_moves = new live::MoveList();
  delete constrained_moves;
  constrained_moves = new live::MoveList();
  delete frozen_moves;
  frozen_moves = new live::MoveList();

  delete move_list;
  delete degree;
  delete alias;
  move_list = new tab::Table<live::INode, live::MoveList>();
  degree = new tab::Table<live::INode, int>();
  alias = new tab::Table<live::INode, live::INode>();
}

} // namespace ra