#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"
#include <unordered_set>
#include <algorithm>

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //树本质上就是root root是exp 本质是exp的analyse。而exp需要labelcount 此处为0
  int labelcount = 0;
  root_->SemAnalyze(venv,tenv,labelcount,errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  // 检查变量是否存在
  if (!entry) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().c_str());
    return type::IntTy::Instance();  // 返回一个合法类型防止崩溃(也可能引起更多报错)
  }
  // 检查是否为一个变量（而不是函数等其他类型）
  if (entry && typeid(*entry) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "%s is not a variable", sym_->Name().c_str());
    return type::IntTy::Instance();  
  }else{
    type::Ty *ty = static_cast<env::VarEntry *>(entry)->ty_->ActualTy();
    // 检查是否获取到了有效的类型
    if (!ty) {
      errormsg->Error(pos_, "invalid type for variable %s", sym_->Name().c_str());
      return type::IntTy::Instance();  
    }
    return ty;
  }
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //a.b类型的检查。这类表达式的语义是：a 必须是一个 record 类型，b 是其中的某个字段。
    // 分析变量 var 的类型（即 a）
  type::Ty *ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  ty = ty->ActualTy();  // 解引用别名类型

  // 判断是否是记录类型。dynamiccast在不能转换时会返回nullptr
  if (auto *recordTy = dynamic_cast<type::RecordTy *>(ty)) {
    // 遍历记录的字段，找名字匹配的那个字段
    for (const auto &field : recordTy->fields_->GetList()) {
      if (field->name_ == sym_) {
        return field->ty_->ActualTy();
      }
    }
    // 没有找到对应的字段名
    errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
    return type::IntTy::Instance();  // 错误恢复：返回个合法类型防止崩溃
  } else {
    // 不是记录类型，不能访问字段
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();  // 同样错误恢复
  }
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount, err::ErrorMsg *errormsg) const {
  auto var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto actual_ty = var_ty->ActualTy();  // ← 必须写这句！！

  auto subscript_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!type::IntTy::Instance()->IsSameType(subscript_ty)) {
    errormsg->Error(subscript_->pos_, "array index must be integer");
  }

  auto *array_ty = dynamic_cast<type::ArrayTy *>(actual_ty);
  if (!array_ty) {
    errormsg->Error(pos_, "array type required");
    return new type::ArrayTy(type::IntTy::Instance()); 
  }

  return array_ty->ty_->ActualTy();  // 返回数组元素类型
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //VarExp 表示“取变量的值”，并且它会进行类型检查，确保变量的类型是合法的。
  return var_->SemAnalyze(venv,tenv,labelcount,errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
    return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //这不是函数定义！只需要保证函数存在且参数正确 还有返回值类型需要对的上
  // f 1,2
  auto entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::VoidTy::Instance();  // 默认函数返回为空
  }

  auto fun_entry = static_cast<env::FunEntry *>(entry);
  auto formals = fun_entry->formals_->GetList();  // 形参类型列表
  auto actuals = args_->GetList();                // 实参表达式列表
  auto result_ty = fun_entry->result_->ActualTy();

  // 先判断数量
  if (actuals.size() < formals.size()) {
    errormsg->Error(pos_, "too few params in function %s", func_->Name().data());
  }
  if (actuals.size() > formals.size()) {
    errormsg->Error(pos_, "too many params in function %s", func_->Name().data());
  }

  // 再判断类型
  auto f_it = formals.begin();
  auto a_it = actuals.begin();
  for (; f_it != formals.end(); ++f_it, ++a_it) {
    auto actual_ty = (*a_it)->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!(*f_it)->IsSameType(actual_ty)) {
      errormsg->Error((*a_it)->pos_, "para type mismatch");
    }
  }

  return result_ty;
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //照抄ppt
    auto left_ty = left_->SemAnalyze(venv,tenv,labelcount,errormsg)->ActualTy();
  auto right_ty = right_->SemAnalyze(venv,tenv,labelcount,errormsg)->ActualTy();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP || 
      oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_,"integer required");
    }
    if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_,"integer required");
    }
    return type::IntTy::Instance();
  }
  if (!left_ty->IsSameType(right_ty)) {
    errormsg->Error(pos_, "same type required");
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //形如person{name = "Tom", age = 18}
  //不用做递归检查：person已经被定义了
  //查找对应的 record 类型定义（比如 person）。检查字段是否存在、名字是否匹配、类型是否一致。返回整个 record 的类型（即 RecordTy 类型）。
  type::Ty *ty = tenv->Look(typ_);  
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }

  ty = ty->ActualTy();  // 展开 NameTy 有可能最后actual不是真record类

  if (typeid(*ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "type %s is not a record type", typ_->Name().data());
    return type::IntTy::Instance();
  }

  auto record_ty = static_cast<type::RecordTy *>(ty);
  auto fields = record_ty->fields_->GetList();
  auto records = fields_->GetList();

  if (fields.size() != records.size()) { //检查字段个数
    errormsg->Error(pos_, "record field number mismatch");
    return type::IntTy::Instance();
  }

  auto f_it = fields.begin();
  auto r_it = records.begin();
  for (; f_it != fields.end(); ++f_it, ++r_it) { //一一对应
    if ((*f_it)->name_ != (*r_it)->name_) {
      errormsg->Error(pos_, "field name mismatch: expected %s but got %s",
                      (*f_it)->name_->Name().data(), (*r_it)->name_->Name().data());
    }

    type::Ty *exp_ty = (*r_it)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!exp_ty->IsSameType((*f_it)->ty_)) {
      errormsg->Error(pos_, "field type mismatch for %s", (*r_it)->name_->Name().data());
    }
  }

  return ty;

}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //(a := 1; b := a + 1; b)
  //只关心最后一个表达式的类型，因为前面的只是为了副作用（如赋值、打印等）。
    type::Ty *result = type::VoidTy::Instance(); 
  for(const auto &exp: seq_->GetList()){
    result = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return result;
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //标准的 Tiger 语言没有 elseif 语法
  //if e1 then e2：e1 必须是 int，e2 必须是 void
  //if e1 then e2 else e3：e1 是 int，e2 和 e3 必须类型一致，整体返回该类型。
  auto test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  auto then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  //e1判断
  if (!test_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(test_->pos_, "if test expression must be integer");
  }

  //e3
  if (elsee_) {
    auto else_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!then_ty->IsSameType(else_ty)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
    }
    return then_ty;
  } else {//只有e2
    if (!then_ty->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  }
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //while test do body
  auto test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  //test是 int（布尔值）
  if (!test_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(test_->pos_, "while test must be integer");
  }
  //labelcount+1表明进入循环语句
  auto body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  //检查body是 void
  if (!body_ty->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(body_->pos_, "while body must produce no value");
  }

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
   
  auto low_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  auto high_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (typeid(*low_ty) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_,"for exp's range type is not integer");
  }
  if (typeid(*high_ty) != typeid(type::IntTy)) {
    errormsg->Error(hi_->pos_,"for exp's range type is not integer");
  }
  venv->BeginScope();
  venv->Enter(var_,new env::VarEntry(type::IntTy::Instance(),true));
  auto ty = body_->SemAnalyze(venv, tenv, labelcount+1, errormsg)->ActualTy();
  if (typeid(*ty) != typeid(type::VoidTy)) {
    errormsg->Error(hi_->pos_,"for body must produce no value");
  }
  venv->EndScope();
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //break是检查嵌套层数的checkpoint
    if(labelcount==0){
    errormsg->Error(pos_,"break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  /*
  let
    declarations
  in
    expression
  end
  */
  //新的let要进入新的作用域！
  venv->BeginScope();
  tenv->BeginScope();

  // 所有声明
  for (const auto &dec : decs_->GetList()) {
    dec->SemAnalyze(venv, tenv, labelcount,errormsg);  // 注意声明不需要返回值
  }

  // 3. 分析主体表达式
  type::Ty *ret_ty;
  if (!body_){
    ret_ty = type::VoidTy::Instance();
  }else{ 
    ret_ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  // 4. 退出作用域
  venv->EndScope();
  tenv->EndScope();

  return ret_ty;
}
//let没有需要检查的东西 但是它里面可能出错

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //和arrayty完全不一样 var arr = array[10] of 0
  auto ty = tenv->Look(typ_);
  if (!ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return type::VoidTy::Instance();  
  }

  ty = ty->ActualTy(); //这一步很重要 有了它才能正确的返回类型
  //ty 是 type::NameTy，要 ActualTy() 一次才是真正的 ArrayTy！

  if (typeid(*ty) != typeid(type::ArrayTy)) {
      errormsg->Error(pos_, "not an array type");
      return type::VoidTy::Instance(); 
  }

  auto size_ty = size_->SemAnalyze(venv,tenv,labelcount,errormsg)->ActualTy();
  if(typeid(*size_ty)!=typeid(type::IntTy)){
    errormsg->Error(pos_, "size should be int");
    return type::VoidTy::Instance();  
  }

  auto init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!init_ty->IsSameType(static_cast<type::ArrayTy*>(ty)->ty_)) {
      errormsg->Error(init_->pos_, "type mismatch");
      return type::VoidTy::Instance();  
  }

  return ty;
}



type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
   /* TODO: Put your lab4 code here */
   //int := 10
  auto left_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto right_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);

  // 左右类型需匹配
  if (!left_ty->IsSameType(right_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
    return type::VoidTy::Instance();
  }
  if(typeid(*var_)==typeid(SimpleVar)){
    // x := 5 中的 x
    auto sym = static_cast<SimpleVar*>(var_)->sym_;
    auto entry = venv->Look(sym);
    if (!entry) {
      errormsg->Error(var_->pos_, "undefined variable %s", sym->Name().data());
    }else{
      if(entry->readonly_){
        errormsg->Error(var_->pos_,"loop variable can't be assigned");
      }else{
        //变量没有问题，更新为赋值右边的表达式类型
        venv->Enter(sym,new env::VarEntry(right_ty));
      }
    }
  }
  return type::VoidTy::Instance();
  //赋值操作本身没有返回值
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  /*
  检查函数名是否已被定义：在 venv 中查找函数的符号。如果函数已经存在，则需要报错，因为一个函数不能重复声明。
  进入函数作用域：创建一个新的作用域来处理函数体内的变量声明（即局部变量）。
  处理函数参数：检查函数的参数，并将它们添加到当前的作用域中。
  检查函数体：递归调用函数体的 SemAnalyze 来进行函数内部的语义分析。
  更新符号表：在完成所有检查后，将函数的返回类型和其他相关信息更新到符号表中。
  */
  auto func_list = functions_->GetList();
  std::unordered_set<sym::Symbol*> current_scope_names; //记录当前这组函数声明中已经出现过的函数名

  for (const auto &function : func_list) {
      if (current_scope_names.count(function->name_)) {
          errormsg->Error(pos_, "two functions have the same name");
      }
      current_scope_names.insert(function->name_);//不能使用害人的look！！！
      //这是因为内外的命名可能是相同的 内部遮蔽外部即可

      auto params = function->params_; //参数列表
      type::Ty *result_ty = type::VoidTy::Instance(); //函数声明中的返回类型
      if (function->result_) {
          result_ty = tenv->Look(function->result_); //保证返回类型有效，look报错
      }


      auto formals = params->MakeFormalTyList(tenv, errormsg);
      // 提前注册函数名称以及返回值（为了支持递归）
      venv->Enter(function->name_, new env::FunEntry(formals, result_ty));
      //依次将定义列表的函数注册到当中
  }

  //开始分析他们的内容
  for (const auto &function : func_list) {
      auto params = function->params_;
      auto formals = params->MakeFormalTyList(tenv, errormsg);
      venv->BeginScope();
      //依次将参数加入venv
      auto formal_it = formals->GetList().begin();
      auto param_it = params->GetList().begin();
      for (; param_it != params->GetList().end(); formal_it++, param_it++) {
          venv->Enter((*param_it)->name_, new env::VarEntry(*formal_it));
      }

      // 递归分析函数体 第一遍循环保证了这个东西正常
      auto res = function->body_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

      type::Ty *result_ty = type::VoidTy::Instance();
      if (function->result_) {
          result_ty = tenv->Look(function->result_);//获取其返回类型 如果不对会在look报错
      }

      
      if (typeid(*result_ty->ActualTy()) == typeid(type::VoidTy)//返回应为viod但结果不是
          && typeid(*res) != typeid(type::VoidTy)) {
          errormsg->Error(pos_, "procedure returns value");
      } else {
          if (!result_ty->IsSameType(res)) {//返回不是void但type不同
              errormsg->Error(pos_, "function return value mismatch");
          }
      }//判断返回值有无及类型对应

      venv->EndScope();
      //对每个函数独立开栈分析
  }

}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  //var x: int := 10
  /* TODO: Put your lab4 code here */

  //考察“int”与右侧初始化式子关系 需要类型推断
  auto init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if(typ_){
    auto ty = tenv->Look(typ_);
    if(!ty->IsSameType(init_ty)){
      errormsg->Error(pos_, "type mismatch");
    }
  }else{
    if(typeid(*init_ty)==typeid(type::NilTy)){
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
  }
  venv->Enter(var_, new env::VarEntry(init_ty));//注册
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //type student = { name: string, score: int }
  //类型声明不仅限于记录类型，还可以是类型别名、数组类型等。
  //type integer = int

  //不使用look 注册所有type 与函数一样 这是先占坑的过程
  auto type_list = types_->GetList();
  std::unordered_set<sym::Symbol *> current_scope_names;

  for (const auto &type : type_list) {
    if (current_scope_names.count(type->name_)) {
      errormsg->Error(pos_, "two types have the same name");
    }
    current_scope_names.insert(type->name_);
    tenv->Enter(type->name_, new type::NameTy(type->name_, nullptr));
  }

  //给NameTy补ty_ 此时不知道右边的具体类型
  for (const auto &type : type_list) {
    auto ty = type->ty_->SemAnalyze(tenv, errormsg);
    auto entry = tenv->Look(type->name_);
    if (entry && typeid(*entry) == typeid(type::NameTy)) {
      auto name_ty = static_cast<type::NameTy *>(entry);
      name_ty->ty_ = ty;//对namety解引用导致了可能的“环”被连起来  类型指针被指向非真实的对方
    }
  }
  //

  //循环检测
  for (const auto &type : type_list) {
    type::Ty *ty = tenv->Look(type->name_);
    type::Ty *actual = ty;
    std::unordered_set<type::Ty*> visited;  // 改为存指针 
    //存名字可能的问题：type a = b type b = array of int 报错

    while (actual && typeid(*actual) == typeid(type::NameTy)) {
      if (visited.count(actual)) {//访问过了第二遍证明遇到环路
        errormsg->Error(pos_, "illegal type cycle");
        return;//一次报错就返回 否则通不过测试
      }
      visited.insert(actual);

      auto name_ty = static_cast<type::NameTy *>(actual);
      if (!name_ty->ty_) break;
      actual = name_ty->ty_;
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  /*
  这是用于将被定义新名字的类型拆包
  假设我们有如下的类型定义：
  type student = { name: string, score: int }
  NameTy 会通过类型名 student 来查找该类型是否已定义。
  */
  auto ty = tenv->Look(name_);
  if(ty) {
    return new type::NameTy(name_,ty);
  }
  errormsg->Error(pos_, "undefined type %s", name_->Name().data());
  return type::VoidTy::Instance();
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //根据 record_ 中定义的字段信息来生成并返回一个新的 RecordTy 对象。
  return new type::RecordTy(record_->MakeFieldList(tenv,errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  //array of int
  //array的申明需要检查声称的每个item是否是合法的type 即上方“int”
  auto item_ty = tenv->Look(array_);
  if(item_ty) {
    return new type::ArrayTy(item_ty);
  }
  errormsg->Error(pos_, "undefined type %s", array_->Name().data());
  return new type::ArrayTy(type::IntTy::Instance());
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
