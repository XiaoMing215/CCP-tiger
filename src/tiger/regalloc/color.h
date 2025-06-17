#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"
#include <set>
/*
输入：Liveness 分析之后构建的 干扰图；
输出：
一个 temp::Map *：记录每个虚拟寄存器被分配到哪个实际寄存器；
*/
namespace col {
struct Result {
  Result() : coloring(nullptr), spills(nullptr) {}
  Result(temp::Map *coloring, live::INodeListPtr spills)
      : coloring(coloring), spills(spills) {}
  temp::Map *coloring;
  live::INodeListPtr spills; //其中的每个 node 表示一个需要溢出的临时变量
};

class Color {
  /* TODO: Put your lab6 code here */
private:
  temp::Map *coloring;
  std::set<std::string *> okColors;
public:
  Color();
  void InitOkColors(); //初始化 okColors（即哪些物理寄存器可以用于分配）
  void RemoveOkColor(live::INodePtr n); //表示某个邻接节点已经用了这个寄存器
  bool OkColorsEmpty();
  bool AssignColor(live::INodePtr n);
  void AssignSameColor(live::INodePtr color_src, live::INodePtr color_dst);
  col::Result BuildAndGetResult();
};
} // namespace col

#endif // TIGER_COMPILER_COLOR_H
