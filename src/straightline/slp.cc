#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(stm1->MaxArgs(),stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  t = stm1->Interp(t);
  t = stm2->Interp(t);
  return t;
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  // printf("%d",exp->MaxArgs());
  return exp->MaxArgs();//赋值只要管右半部分
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable* tmp = exp->InterpExp(t);
  int num = tmp->i;
  Table* newt = tmp->t;
  // // printf("%d",num);
  newt = newt->Update(id,num);
  // int a = t->Lookup("a");
  // printf("%d",a);
  return newt;//传递正常
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(exps->NumExps(),exps->MaxArgs());
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table* tmpt = (exps->Interp(t))->t;
  return tmpt;//这一步的interp没有记录下之前的table
}




int IdExp::MaxArgs() const {
  return 0; // should be 0 for none print stm testcase
}

IntAndTable *IdExp::InterpExp(Table *t) const {
  return new IntAndTable(t->Lookup(id), t);//找到id对应的int值
}

int NumExp::MaxArgs() const {
  return 0; // should be 0 for none print stm testcase
}

IntAndTable *NumExp::InterpExp(Table *t) const {
  return new IntAndTable(num, t);//值和对应的没有更新的t
}

int OpExp::MaxArgs() const {
  int args1 = left->MaxArgs();
  int args2 = right->MaxArgs();
  // std::cout << args1 << " " << args2 << std::endl;
  return args1 > args2 ? args1 : args2;
}

IntAndTable *OpExp::InterpExp(Table *t) const {
  IntAndTable *expValue1 = left->InterpExp(t);
  IntAndTable *expValue2 = right->InterpExp(t);
  switch (oper) {
  case PLUS:
    return new IntAndTable(expValue1->i + expValue2->i, expValue2->t);
  case MINUS:
    return new IntAndTable(expValue1->i - expValue2->i, expValue2->t);
  case TIMES:
    return new IntAndTable(expValue1->i * expValue2->i, expValue2->t);
  case DIV:
    return new IntAndTable(expValue1->i / expValue2->i, expValue2->t);
  default: // add default for robustness
    break;
  }
  return nullptr;
}

int EseqExp::MaxArgs() const {
  int args1 = stm->MaxArgs();
  int args2 = exp->MaxArgs();
  return args1 > args2 ? args1 : args2;
}

IntAndTable *EseqExp::InterpExp(Table *t) const {
  return exp->InterpExp(stm->Interp(t));
}

int PairExpList::MaxArgs() const {
  int num1 = exp->MaxArgs();
  int num2 = tail->MaxArgs();
  return num1 > num2 ? num1 : num2;//exp里面才会有print 找到两个里面更大就行
}

int PairExpList::NumExps() const { return 1 + tail->NumExps(); }

IntAndTable *PairExpList::Interp(Table *t) const {
  IntAndTable *temp_table = exp->InterpExp(t);
  printf("%d ", temp_table->i);
  return tail->Interp(temp_table->t);
}

int LastExpList::MaxArgs() const { return exp->MaxArgs(); }

int LastExpList::NumExps() const { return 1; }

IntAndTable *LastExpList::Interp(Table *t) const {
  // return exp->InterpExp(t);
  IntAndTable *int_and_table = exp->InterpExp(t);
  printf("%d\n", int_and_table->i);
  return int_and_table;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
} // namespace A
