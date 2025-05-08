#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc
//对应了最大的void Traverse(esc::EscEnvPtr env);

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  /* TODO: Put your lab5 code here */
  root_->Traverse(env,0);//absyn承接的必然是exp 调用Traverse(esc::EscEnvPtr env, int depth)
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  //当前使用深度 > 定义深度，则逃逸。
  auto entry = env->Look(sym_); //entry需不为空
  if (entry && entry->depth_ < depth) {
    *(entry->escape_) = true;
  }
  //escape_表明是否逃逸
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  //p.age sym表示其名称 var表示本身，它是一个存储在某个作用域中的变量。
  // var_ 存储的是一个 基变量，而不是具体的数值。这个基变量表示的是字段所属的容器，即它指向一个记录（或结构体、对象）的实例。
  var_->Traverse(env,depth);
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env,depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env,depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}
//上三类不会产生逃逸，表达式的结果仅仅是一个常量值

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */

}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  left_->Traverse(env,depth);
  right_->Traverse(env,depth);
  //递归神力
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto efields = fields_->GetList();
  for(auto &efield:efields){
    efield->exp_->Traverse(env,depth);
  }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto exps = seq_->GetList();//逐个处理
  for(auto &exp:exps){
    exp->Traverse(env,depth);
  }
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env,depth);
  exp_->Traverse(env,depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  test_->Traverse(env,depth); // 先遍历条件表达式
  then_->Traverse(env,depth); // 然后遍历 then 分支
  if (elsee_) {               // 如果有 else 分支
    elsee_->Traverse(env,depth); // 遍历 else 分支
  }
//分别对三个部分逃逸分析

}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  test_->Traverse(env,depth);
  body_->Traverse(env,depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  //var是创建的新变量 需要登记 初始化escape
  lo_->Traverse(env,depth);
  hi_->Traverse(env,depth);
  escape_ = false;
  env->Enter(var_,new esc::EscapeEntry(depth,&escape_));
  body_->Traverse(env,depth);
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto declist = decs_->GetList();
  for(auto &dec:declist){
    dec->Traverse(env,depth);
  }
  body_->Traverse(env,depth);
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  size_->Traverse(env,depth);
  init_->Traverse(env,depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  auto funclist = functions_->GetList();
  for(auto &funcdec : funclist){ //遍历每一个参数，由于进入新的scope 需要重新注册新的名字
    auto fieldlist = funcdec->params_->GetList();
    for(auto &field : fieldlist){
      field->escape_ = false;
      env->Enter(field->name_, new esc::EscapeEntry(depth+1, &(field->escape_)));
    }
    //funcdec 是一个 FunctionDec 类型的对象
    // 需要遍历的部分是函数体 body_
    funcdec->body_->Traverse(env, depth+1);
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  //声明新变量
  init_->Traverse(env,depth); //用于声明的不能逃逸
  escape_ = false;
  env->Enter(var_,new esc::EscapeEntry(depth,&escape_)); //新产生的要登记
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  //类型定义而非变量的声明和使用，因此它不涉及常规的变量逃逸分析。
  return;
}

} // namespace absyn
