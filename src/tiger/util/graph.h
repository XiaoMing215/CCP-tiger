#ifndef TIGER_UTIL_GRAPH_H_
#define TIGER_UTIL_GRAPH_H_

#include "tiger/util/table.h"

//Graph 类：管理图的整体结构，包含所有节点和节点数量，支持创建节点和添加边。
namespace graph {

template <typename T> class Node;
template <typename T> class NodeList;

template <typename T> class Graph {
public:
  Graph() : nodecount_(0), my_nodes_(new NodeList<T>()) {}
  NodeList<T> *Nodes();
  Node<T> *NewNode(T *info);
  void AddEdge(Node<T> *from, Node<T> *to);
  static void Show(FILE *out, NodeList<T> *p, std::function<void(T *)> show_info);
  int nodecount_;
  ~Graph();

private:
  NodeList<T> *my_nodes_;
};
// Node 类：表示图中的单个节点，维护节点之间的前驱后继关系，存储节点信息。
template <typename T> class Node {
  template <typename NodeType> friend class Graph;

public:
  bool GoesTo(Node<T> *n);
  bool Adj(Node<T> *n);
  int InDegree();
  int OutDegree();
  NodeList<T> *Adj();
  NodeList<T> *Succ();
  NodeList<T> *Pred();
  int Degree();
  T *NodeInfo();
  int Key();
  ~Node<T>() {
    delete succs_;
    delete preds_;
  }

private:
  Graph<T> *my_graph_;
  int my_key_;
  NodeList<T> *succs_;
  NodeList<T> *preds_;
  T *info_;
  Node<T>()
      : my_graph_(nullptr), my_key_(0), succs_(nullptr), preds_(nullptr),
        info_(nullptr) {}
};

// NodeList 类：用链表管理一组节点，支持集合操作（并、差等）和节点插入删除。
template <typename T> class NodeList {
  friend class Graph<T>;
  friend class Node<T>;

public: 
  NodeList<T>() = default;
  ~NodeList<T>() = default;
  bool Contain(Node<T> *n);
  void CatList(NodeList<T> *nl);
  void DeleteNode(Node<T> *n);
  void Clear() { node_list_.clear(); }
  void Prepend(Node<T> *n) { node_list_.push_front(n); }
  void Append(Node<T> *n) { node_list_.push_back(n); }
  void Union(Node<T> *n);
  NodeList<T> *Union(NodeList<T> *nl);
  NodeList<T> *Diff(NodeList<T> *nl);
  [[nodiscard]] const std::list<Node<T> *> &GetList() const {
    return node_list_;
  }
  bool SameInfo(Node<T> *n);

private:
  std::list<Node<T> *> node_list_{};
};

// NewNode 函数：创建节点，初始化其基本属性，加入图的节点列表，并分配前驱后继列表。
template <typename T> Node<T> *Graph<T>::NewNode(T *info) {
  auto n = new Node<T>();
  n->my_graph_ = this;
  n->my_key_ = nodecount_++;
  my_nodes_->node_list_.push_back(n);
  n->succs_ = new NodeList<T>();
  n->preds_ = new NodeList<T>();
  n->info_ = info;
  return n;
}

template <typename T> bool Node<T>::GoesTo(Node<T> *n) {
  return succs_->Contain(n);
}

template <typename T> bool Node<T>::Adj(Node<T> *n) {
  return succs_->Contain(n) || preds_->Contain(n);
}

template <typename T>
void Graph<T>::AddEdge(Node<T> *from, Node<T> *to) {
  assert(from && to);
  assert(from->my_graph_ == this && to->my_graph_ == this);
  if (from->GoesTo(to)) return;
  from->succs_->Append(to);
  to->preds_->Append(from);
}

template <typename T>
Graph<T>::~Graph() {
  for (auto node : my_nodes_->GetList()) {
    delete node;
  }
  delete my_nodes_;
}


template <typename T> int Node<T>::InDegree() {
  return preds_->node_list_.size();
}

template <typename T> int Node<T>::OutDegree() {
  return succs_->node_list_.size();
}

template <typename T> int Node<T>::Degree() { return InDegree() + OutDegree(); }

template <typename T> NodeList<T> *Node<T>::Adj() {
  NodeList<T> *adj_list = new NodeList<T>();
  adj_list->CatList(succs_);
  adj_list->CatList(preds_);
  return adj_list;
}

template <typename T> NodeList<T> *Node<T>::Succ() { return succs_; }

template <typename T> NodeList<T> *Node<T>::Pred() { return preds_; }

template <typename T> T *Node<T>::NodeInfo() { return info_; }

template <typename T> int Node<T>::Key() { return my_key_; }

template <typename T> NodeList<T> *Graph<T>::Nodes() { return my_nodes_; }

template <typename T>
bool NodeList<T>::Contain(Node<T> *n) {
  return std::find(node_list_.begin(), node_list_.end(), n) != node_list_.end();
}

template <typename T>
bool NodeList<T>::SameInfo(Node<T> *n) {
  return std::any_of(node_list_.begin(), node_list_.end(),
                     [n](Node<T> *node) { return node->NodeInfo() == n->NodeInfo(); });
}

template <typename T>
void NodeList<T>::DeleteNode(Node<T> *n) {
  assert(n);
  auto it = std::find(node_list_.begin(), node_list_.end(), n);
  if (it != node_list_.end())
    node_list_.erase(it);
}

template <typename T>
void NodeList<T>::CatList(NodeList<T> *nl) {
  if (nl)
    node_list_.insert(node_list_.end(), nl->node_list_.begin(), nl->node_list_.end());
}

template <typename T>
void NodeList<T>::Union(Node<T> *n) {
  if (!Contain(n))
    node_list_.push_back(n);
}

template <typename T>
NodeList<T> *NodeList<T>::Union(NodeList<T> *nl) {
  auto res = new NodeList<T>();
  for (auto node : node_list_)
    res->Append(node);
  for (auto node : nl->GetList())
    if (!res->Contain(node))
      res->Append(node);
  return res;
}


template <typename T> NodeList<T> *NodeList<T>::Diff(NodeList<T> *nl) {
  NodeList<T> *res = new NodeList<T>();
  for (auto node : node_list_) {
    if (!nl->Contain(node))
      res->node_list_.push_back(node);
  }
  return res;
}

// The type of "tables" mapping graph-nodes to information
template <typename T, typename ValueType>
using Table = tab::Table<Node<T>, ValueType>;

/**
 * Print a human-readable dump for debugging.
 * @tparam T node type
 * @param out output FILE object
 * @param p node_list_ to print
 * @param show_info show-information function
 */
template <typename T>
void Graph<T>::Show(FILE *out, NodeList<T> *p, std::function<void(T *)> show_info) {
  for (auto *n : p->node_list_) {
    assert(n);
    if (show_info) show_info(n->NodeInfo());
    fprintf(out, " (%d): ", n->Key());

    auto *succs = n->Succ();
    for (auto *q : succs->GetList())
      fprintf(out, "%d ", q->Key());

    auto *preds = n->Pred();
    for (auto *q : preds->GetList())
      fprintf(out, "%d ", q->Key());

    fprintf(out, "\n");
  }
}


} // namespace graph

#endif