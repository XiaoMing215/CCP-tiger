#include "tiger/liveness/liveness.h"

/*
活跃变量分析（Liveness Analysis）：计算每条指令前后的“活跃寄存器集合”，即哪些变量在该点之后还会被使用。

构建干涉图（Interference Graph）：基于活跃信息，建立变量之间的冲突（干涉）关系，表示哪些变量不能共用同一个寄存器。


*/
extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](const std::pair<INodePtr, INodePtr> &move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = std::find_if(move_list_.begin(), move_list_.end(),
                              [src, dst](const std::pair<INodePtr, INodePtr> &move) {
                                return move.first == src && move.second == dst;
                              });
  if (move_it != move_list_.end()) {
    move_list_.erase(move_it);
  }
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (const auto &move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (const auto &move : list->GetList()) {
    if (!res->Contain(move.first, move.second)) {
      res->move_list_.push_back(move);
    }
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (const auto &move : list->GetList()) {
    if (Contain(move.first, move.second)) {
      res->move_list_.push_back(move);
    }
  }
  return res;
}
//

bool SameSet(const std::set<temp::Temp *> &first,
             const std::set<temp::Temp *> &second) {
  return first == second;
}

std::set<temp::Temp *> ToSet(const std::list<temp::Temp *> &origin) {
  std::set<temp::Temp *> res;
  for (const auto &it : origin) {
    res.insert(it);
  }
  return res;
}

temp::TempList *ToTempList(const std::set<temp::Temp *> &origin) {
  auto *res = new temp::TempList();
  for (const auto &it : origin) {
    res->Append(it);
  }
  return res;
}

//LiveMap 是对 整个函数/基本块列表 做的
void LiveGraphFactory::LiveMap() {
  // 初始化每个节点的 in 和 out 集合为空
  for (fg::FNodePtr node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
  }

  bool changed = true;
  while (changed) {
    changed = false;

    for (fg::FNodePtr node : flowgraph_->Nodes()->GetList()) {
      assem::Instr *instr = node->NodeInfo();

      // 当前节点的 use 和 def 集合
      std::set<temp::Temp *> use_set = ToSet(instr->Use()->GetList());
      std::set<temp::Temp *> def_set = ToSet(instr->Def()->GetList());

      // 计算 out[node] = 联合所有后继节点的 in 集合
      std::set<temp::Temp *> new_out_set;
      for (fg::FNodePtr succ : node->Succ()->GetList()) {
        std::set<temp::Temp *> succ_in_set = ToSet(in_->Look(succ)->GetList());
        new_out_set.merge(std::move(succ_in_set));
      }

      // 计算 in[node] = use[node] ∪ (out[node] - def[node])
      std::list<temp::Temp *> out_minus_def;
      std::set_difference(new_out_set.begin(), new_out_set.end(),
                          def_set.begin(), def_set.end(),
                          std::back_inserter(out_minus_def));
      std::set<temp::Temp *> new_in_set = use_set;
      new_in_set.merge(ToSet(out_minus_def));

      // 获取当前存储的 in/out
      std::set<temp::Temp *> old_in_set = ToSet(in_->Look(node)->GetList());
      std::set<temp::Temp *> old_out_set = ToSet(out_->Look(node)->GetList());

      // 如果 in 或 out 有变化，更新并标记继续迭代
      //循环结构在CFG中表现为有向环，分析过程中通过多次迭代不断传播变量活跃信息，直到所有节点的 in 和 out 集合稳定不再变化。
      if (!SameSet(new_in_set, old_in_set) || !SameSet(new_out_set, old_out_set)) {
        changed = true;
        in_->Set(node, ToTempList(new_in_set));
        out_->Set(node, ToTempList(new_out_set));
      }
    }
  }
}


void LiveGraphFactory::InterfGraph() {

  // 构建预着色的干涉图
  auto precolored_temps = reg_manager->Registers()->GetList();
  // 为所有预着色寄存器创建节点
  for (auto precolored_temp : precolored_temps) {
    INodePtr node = live_graph_.interf_graph->NewNode(precolored_temp);
    temp_node_map_->Enter(precolored_temp, node);
  }
  // 在所有预着色寄存器之间添加边 真实的物理储存器
  for (temp::Temp *temp1 : precolored_temps) {
    for (temp::Temp *temp2 : precolored_temps) {
      if (temp1 != temp2) {
        INodePtr node1 = temp_node_map_->Look(temp1);
        INodePtr node2 = temp_node_map_->Look(temp2);
        live_graph_.interf_graph->AddEdge(node1, node2);
      }
    }
  }

  // 为所有临时寄存器创建节点
  for (auto node : flowgraph_->Nodes()->GetList()) {
    assem::Instr *instr = node->NodeInfo();

    // 检查指令的使用寄存器
    for (auto use : instr->Use()->GetList()) {
      if (temp_node_map_->Look(use))
        continue;
      INodePtr new_node = live_graph_.interf_graph->NewNode(use);
      temp_node_map_->Enter(use, new_node);
    }

    // 检查指令的定义寄存器
    for (auto def : instr->Def()->GetList()) {
      if (temp_node_map_->Look(def))
        continue;
      INodePtr new_node = live_graph_.interf_graph->NewNode(def);
      temp_node_map_->Enter(def, new_node);
    }
  }

  // 添加干涉边
  for (auto node : flowgraph_->Nodes()->GetList()) {
    assem::Instr *instr = node->NodeInfo();

    // 对于定义了变量a，活跃出口为b1, ..., bj的指令，添加干涉边

    if (typeid(*instr) == typeid(assem::MoveInstr)) {
      // 如果是move指令 a ← c，添加 (a, b1), ..., (a, bj)，其中 bj != c
      //如果编译器在图着色时把 a 和 c 分配到同一个寄存器，那么这条 move 指令可以 完全删除（被 coalesce）
      for (auto def : instr->Def()->GetList()) {
        INodePtr def_node = temp_node_map_->Look(def);
        auto out_set = ToSet(out_->Look(node)->GetList());
        auto use_set = ToSet(instr->Use()->GetList());
        std::list<temp::Temp *> b_list;  // out - use
        std::set_difference(out_set.begin(), out_set.end(),
                            use_set.begin(), use_set.end(),
                            back_inserter(b_list));
        for(auto b : b_list){
          INodePtr b_node = temp_node_map_->Look(b);
          live_graph_.interf_graph->AddEdge(def_node, b_node);
          live_graph_.interf_graph->AddEdge(b_node, def_node);
        }

        // move指令将对应节点加入moves集合 为了后续寄存器分配时做 合并判断
        for (temp::Temp *use : instr->Use()->GetList()) {
          INodePtr use_node = temp_node_map_->Look(use);
          live_graph_.moves->Append(use_node, def_node);
        }
      }

    } else {
      // 对于非move指令，添加 (a, b1), ..., (a, bj) 边
      for (auto def : instr->Def()->GetList()) {
        INodePtr def_node = temp_node_map_->Look(def);
        for (auto out : out_->Look(node)->GetList()) {
          INodePtr out_node = temp_node_map_->Look(out);
          live_graph_.interf_graph->AddEdge(def_node, out_node);
          live_graph_.interf_graph->AddEdge(out_node, def_node);
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live