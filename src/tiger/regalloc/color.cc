#include "tiger/regalloc/color.h"

extern frame::RegManager *reg_manager;

namespace col {


/* TODO: Put your lab6 code here */
col::Result Color::BuildAndGetResult() {
  return {coloring, new live::INodeList()};
}

Color::Color() {
  coloring = temp::Map::Empty();
  auto regs = reg_manager->Registers()->GetList();
  for (auto r : regs) {
    auto *name = reg_manager->temp_map_->Look(r);
    okColors.insert(name);            // 初始化可用颜色集合
    coloring->Enter(r, name);         // 为预着色寄存器设定颜色
  }
}

void Color::RemoveOkColor(live::INodePtr n) {
  // 移除已被使用的颜色
  okColors.erase(coloring->Look(n->NodeInfo()));
}

bool Color::AssignColor(live::INodePtr n) {
  if (okColors.empty()) return false;
  auto c = okColors.begin();          // 选择一个可用颜色
  coloring->Enter(n->NodeInfo(), *c); // 分配颜色
  return true;
}

void Color::AssignSameColor(live::INodePtr color_src,
                            live::INodePtr color_dst) {
  auto c = coloring->Look(color_src->NodeInfo());
  assert(c);                          // 确保源结点已有颜色
  coloring->Enter(color_dst->NodeInfo(), c); // 复用相同颜色
}

void Color::InitOkColors() {
  okColors.clear();
  // 重新初始化可用颜色集合
  for (temp::Temp *temp : reg_manager->Registers()->GetList()) {
    std::string *color = reg_manager->temp_map_->Look(temp);
    okColors.insert(color);
  }
}

bool Color::OkColorsEmpty() {
  return okColors.empty();
}

} // namespace col