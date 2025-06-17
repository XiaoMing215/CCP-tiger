#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  return new Access(level, frame::Access::AllocLocal(level->frame_, escape));
} //外层的 Access 类是对 frame::Access 的包装 判断的内容放在了frame access当中

tree::Exp *Access::ToExp(Level *currentLevel) {
  //需要从当前帧通过一连串的静态链（static link）回溯
  tree::Exp *framePtr = currentLevel->StaticLink(level_);
  return access_->ToExp(framePtr);
}

class Cx { //条件表达式的补丁表示
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp { //三个转化接口
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0; //表达式翻译为有值表达式
  [[nodiscard]] virtual tree::Stm *UnNx() = 0; //表达式翻译为语句（无值）
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0; //表达式翻译为控制流布尔条件
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp { //代表一个有值的表达式
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override { //exp2bool
    /* TODO: Put your lab5 code here */
    tree::CjumpStm *stm = new tree::CjumpStm(
        tree::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr); //先空跳转
    std::list<temp::Label **> true_patch_list{&(stm->true_label_)};
    std::list<temp::Label **> false_patch_list{&(stm->false_label_)};
    return {PatchList(true_patch_list), PatchList(false_patch_list), stm};
  }
};

class NxExp : public Exp { //没有值的语句
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    // 这是不应该存在的分支
    assert(0);
  }
};

class CxExp : public Exp { //布尔表达式的控制流表示
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  //此处的跳转还是空的
  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    //把一个布尔控制流表达式（CxExp）转换成一个有值表达式（返回 1 或 0）
    /*
    Temp r;
    r := 1;
    if (cond) goto t else goto f;
    f:
      r := 0;
    t:
      return r;
    */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    //把布尔表达式中“还不知道跳哪儿”的地方，补上真正要跳的标签
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),//先将结果寄存器 r 置为 1，即假设布尔表达式为真。
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r),
                                                    new tree::ConstExp(0)),
                                  new tree::EseqExp(new tree::LabelStm(t),
                                                    new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return cx_.stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

void ProgTr::Translate() {

  FillBaseTEnv();
  FillBaseVEnv();
  /* TODO: Put your lab5 code here */
  temp::Label *main_label_ = temp::LabelFactory::NamedLabel("tigermain");
  main_level_ =
      std::make_unique<Level>(nullptr, main_label_, std::list<bool>()); //调用的是 Level 的构造函数

  tr::ExpAndTy *main =
      absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(),
                             main_label_, errormsg_.get());
  frags->PushBack(new frame::ProcFrag(main->exp_->UnNx(), main_level_->frame_));
//ProcFrag 是一个过程片段，表示“某个函数的语义内容”。

}

static tr::ExExp *getVoidExp() { return new tr::ExExp(new tree::ConstExp(0)); }

static tr::ExpAndTy *getVoidExpAndNilTy() {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

static tr::ExpAndTy *getVoidExpAndVoidTy() {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::VoidTy::Instance());
}

static tree::ExpStm *getVoidStm() {
  return new tree::ExpStm(new tree::ConstExp(0));
}

tree::Exp *Level::StaticLink(Level *targetLevel) { //通过链找到目标所在的level
  Level *currentLevel = this;
  tree::Exp *framePtr = new tree::TempExp(reg_manager->FramePointer());//初始化帧指针为当前函数的帧指针
  while (currentLevel && currentLevel != targetLevel) {
    framePtr = currentLevel->frame_->formals_->front()->ToExp(framePtr);
    currentLevel = currentLevel->parent_;
  }
  return framePtr;
}

//后续的 frame::Access* 包装成 tr::Access* 并放到新的列表里返回
std::list<Access *> *Level::Formals() {
  std::list<frame::Access *> *formal_list = frame_->formals_;
  std::list<tr::Access *> *formal_list_with_level =
      new std::list<tr::Access *>();
  bool first = true;
  for (frame::Access *formal : *formal_list) {
    if (first) {
      first = false;
      continue;
    }
    formal_list_with_level->push_back(new tr::Access(this, formal));
  }
  return formal_list_with_level;
}

Level::Level(Level *parent, temp::Label *name, std::list<bool> formals)
    : parent_(parent) {
  formals.push_front(true);//代表给静态链（static link）预留了一个额外的参数。
  frame_ = new frame::X64Frame(name, formals);
}

} // namespace tr


///////////////////////////////////////////////////////////////////////
//translate大军来咯

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  auto entry = venv->Look(sym_);
  if (!entry || typeid(*entry) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().c_str());
    return tr::getVoidExpAndNilTy();
  }

  auto varEntry = static_cast<env::VarEntry *>(entry);
  auto exp = varEntry->access_->ToExp(level); //只是负责找到他被定义的地方 
  return new tr::ExpAndTy(new tr::ExExp(exp), varEntry->ty_->ActualTy());
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  auto baseExpAndTy = var_->Translate(venv, tenv, level, label, errormsg);
  auto baseTy = baseExpAndTy->ty_->ActualTy();

  if (typeid(*baseTy) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "expression is not a record type");
    return tr::getVoidExpAndNilTy();
  }

  auto recordTy = static_cast<type::RecordTy *>(baseTy);
  int offsetIndex = 0;
  for (auto field : recordTy->fields_->GetList()) {//只是找到结构体的对应字段
    if (field->name_->Name() == sym_->Name()) {
      auto fieldAddr = new tree::BinopExp(
          tree::BinOp::PLUS_OP,
          baseExpAndTy->exp_->UnEx(),
          new tree::ConstExp(offsetIndex * reg_manager->WordSize()));
      auto memExp = new tree::MemExp(fieldAddr);
      return new tr::ExpAndTy(new tr::ExExp(memExp), field->ty_);
    }
    ++offsetIndex; //通过字段在记录中的序号（offsetIndex）计算字段偏移量
  }

  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
  return tr::getVoidExpAndNilTy();
}
//

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const { //a[5]
  auto arrayExpAndTy = var_->Translate(venv, tenv, level, label, errormsg);
  auto indexExpAndTy = subscript_->Translate(venv, tenv, level, label, errormsg);
  auto arrayTy = arrayExpAndTy->ty_->ActualTy();

  if (typeid(*arrayTy) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return tr::getVoidExpAndNilTy();
  }

  auto elementTy = static_cast<type::ArrayTy *>(arrayTy)->ty_->ActualTy();
  auto offsetExp = new tree::BinopExp(tree::BinOp::MUL_OP, indexExpAndTy->exp_->UnEx(),
                                     new tree::ConstExp(reg_manager->WordSize()));
  auto addrExp = new tree::BinopExp(tree::BinOp::PLUS_OP, arrayExpAndTy->exp_->UnEx(), offsetExp);
  auto memExp = new tree::MemExp(addrExp);

  return new tr::ExpAndTy(new tr::ExExp(memExp), elementTy);
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *lab = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(lab, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(lab)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  auto entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
    return tr::getVoidExpAndVoidTy();
  }

  auto func = static_cast<env::FunEntry *>(entry);
  const auto &formals = func->formals_->GetList(); //函数定义时的参数列表

  const auto &argsList = args_->GetList(); //函数调用时的实际参数表达式列表

  if (formals.size() != argsList.size()) {
    if (argsList.size() > formals.size()) {
      errormsg->Error(pos_ - 1, "too many params in function %s", func_->Name().c_str());
    } else {
      errormsg->Error(pos_ - 1, "para type mismatch");
    }
    return tr::getVoidExpAndVoidTy();
  }

  auto args_exp_list = new tree::ExpList();
  if (func->level_) {  // level不为0 需要静态链作为第一个参数
    args_exp_list->Append(level->StaticLink(func->level_)); 
    //对于嵌套定义的函数，调用时要传入静态链（即“外层帧的帧指针”）作为第一个隐式参数。
  }

  auto formal_it = formals.cbegin();
  for (auto arg : argsList) {
    auto arg_exp_ty = arg->Translate(venv, tenv, level, label, errormsg); //传入时计算
    if (!arg_exp_ty->ty_->IsSameType(*formal_it)) {
      errormsg->Error(arg->pos_, "para type mismatch");
      return tr::getVoidExpAndVoidTy();
    }
    args_exp_list->Append(arg_exp_ty->exp_->UnEx());
    ++formal_it;
  }

  auto call_exp = new tree::CallExp(new tree::NameExp(func_), args_exp_list);
  return new tr::ExpAndTy(new tr::ExExp(call_exp), func->result_);
}

/*
递归调用 left_->Translate() 和 right_->Translate() 获得左右操作数的表达式及类型信息。
算术运算（加减乘除）强制要求整型，类型不符报错，构造 tree::BinopExp。
逻辑运算（AND、OR）用条件跳转（CxExp）表达式实现短路，拼接左右表达式的真假链。
关系运算（==, !=, <, <=, >, >=）构造条件跳转语句。对于字符串相等，调用外部函数 "string_equal" 代替。
*/
tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  auto left = left_->Translate(venv, tenv, level, label, errormsg);
  auto right = right_->Translate(venv, tenv, level, label, errormsg);

  auto left_ty = left->ty_;
  auto right_ty = right->ty_;

  // 算术运算符：要求整型
  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP ||
      oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required");
      return new tr::ExpAndTy(tr::getVoidExp(), type::IntTy::Instance());
    }
    if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_, "integer required");
      return new tr::ExpAndTy(tr::getVoidExp(), type::IntTy::Instance());
    }

    tree::BinOp bin_op;
    switch (oper_) {
    case absyn::PLUS_OP: bin_op = tree::BinOp::PLUS_OP; break;
    case absyn::MINUS_OP: bin_op = tree::BinOp::MINUS_OP; break;
    case absyn::TIMES_OP: bin_op = tree::BinOp::MUL_OP; break;
    case absyn::DIVIDE_OP: bin_op = tree::BinOp::DIV_OP; break;
    default: /* 不应到这 */ break;
    }

    return new tr::ExpAndTy(new tr::ExExp(new tree::BinopExp(bin_op, left->exp_->UnEx(), right->exp_->UnEx())), 
                            type::IntTy::Instance());
  }

  // 非算术运算符先检查类型一致
  if (!left_ty->IsSameType(right_ty)) {
    errormsg->Error(pos_, "same type required");
    return left;
  }

  // 逻辑运算符 AND/OR

  /*
  if (a) {
  if (b) {
    goto TRUE;
  } else {
    goto FALSE;
  }
} else {
  goto FALSE;
}

  */
  if (oper_ == absyn::AND_OP || oper_ == absyn::OR_OP) {
    temp::Label *mid_label = temp::LabelFactory::NewLabel();
    auto left_cx = left->exp_->UnCx(errormsg);
    auto right_cx = right->exp_->UnCx(errormsg);//转换为控制流语义

    if (oper_ == absyn::AND_OP) {
      left_cx.trues_.DoPatch(mid_label);//如果左侧为假，直接跳 falses_
      auto true_list = right_cx.trues_;
      auto false_list = tr::PatchList::JoinPatch(left_cx.falses_, right_cx.falses_); //只要 left 或 right 为假，都应该跳 false
      auto stm = new tree::SeqStm(left_cx.stm_, new tree::SeqStm(new tree::LabelStm(mid_label), right_cx.stm_));
      return new tr::ExpAndTy(new tr::CxExp(true_list, false_list, stm), type::IntTy::Instance());
    } else {  // OR_OP
      left_cx.falses_.DoPatch(mid_label);
      auto true_list = tr::PatchList::JoinPatch(left_cx.trues_, right_cx.trues_);
      auto false_list = right_cx.falses_;
      auto stm = new tree::SeqStm(left_cx.stm_, new tree::SeqStm(new tree::LabelStm(mid_label), right_cx.stm_));
      return new tr::ExpAndTy(new tr::CxExp(true_list, false_list, stm), type::IntTy::Instance());
    }
  }

  // 关系运算符
  tree::RelOp rel_op;
  tree::CjumpStm *cjump_stm = nullptr;
  switch (oper_) {
  case absyn::EQ_OP:
  case absyn::NEQ_OP://两者共用
    rel_op = (oper_ == absyn::EQ_OP) ? tree::RelOp::EQ_OP : tree::RelOp::NE_OP;
    if (left_ty->IsSameType(type::StringTy::Instance())) {//字符串的比较没有大于 需要调用函数
      auto args = new tree::ExpList();
      args->Append(left->exp_->UnEx());
      args->Append(right->exp_->UnEx());
      auto expected = new tree::ConstExp(oper_ == absyn::EQ_OP ? 1 : 0);
      cjump_stm = new tree::CjumpStm(tree::RelOp::EQ_OP, frame::ExternalCall("string_equal", args), expected, nullptr, nullptr);
    }
    break;
  case absyn::LT_OP: rel_op = tree::RelOp::LT_OP; break;
  case absyn::LE_OP: rel_op = tree::RelOp::LE_OP; break;
  case absyn::GT_OP: rel_op = tree::RelOp::GT_OP; break;
  case absyn::GE_OP: rel_op = tree::RelOp::GE_OP; break;
  default: rel_op = tree::RelOp::REL_OPER_COUNT; break;
  }

  if (!cjump_stm) {//不是字符串
    cjump_stm = new tree::CjumpStm(rel_op, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
  }//关键的构造CJUMP(LT, a, b, ??, ??)

  auto true_list = tr::PatchList({&(cjump_stm->true_label_)});
  auto false_list = tr::PatchList({&(cjump_stm->false_label_)});//跳转目标地址封装成补丁列表（PatchList）以待后续绑定标签。
  return new tr::ExpAndTy(new tr::CxExp(true_list, false_list, cjump_stm), type::IntTy::Instance());
}


tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  auto rec_type = tenv->Look(typ_);
  if (!rec_type || typeid(*rec_type->ActualTy()) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return tr::getVoidExpAndNilTy();
  }

  auto recordTy = static_cast<type::RecordTy *>(rec_type->ActualTy());
  auto field_count = recordTy->fields_->GetList().size();//字段数
  auto allocated_size = field_count * reg_manager->WordSize();

  // 在堆上分配一块足够大的内存
  temp::Temp *record_temp = temp::TempFactory::NewTemp();
  tree::ExpList *alloc_args = new tree::ExpList();
  alloc_args->Append(new tree::ConstExp(allocated_size));
  tree::MoveStm *alloc_stm = new tree::MoveStm(
      new tree::TempExp(record_temp), frame::ExternalCall("alloc_record", alloc_args));

  // 链表构建初始化语句序列
  tree::SeqStm *stm_seq = new tree::SeqStm(alloc_stm, nullptr);
  tree::SeqStm *tail = stm_seq;

  // 字段数检查
  if (fields_->GetList().size() != field_count) {
    errormsg->Error(pos_, "num of field doesn't match");
  }

  auto defined_fields_it = recordTy->fields_->GetList().cbegin();
  int index = 0;
  for (auto actual_field : fields_->GetList()) {//对每一个字段
    auto field_exp_ty = actual_field->exp_->Translate(venv, tenv, level, label, errormsg);

    // 字段名和类型检查
    if (actual_field->name_->Name() != (*defined_fields_it)->name_->Name()) {
      errormsg->Error(pos_, "field %s doesn't exist", actual_field->name_->Name().data());
    }
    if (!field_exp_ty->ty_->IsSameType((*defined_fields_it)->ty_)) {
      errormsg->Error(pos_, "type of field %s doesn't match", actual_field->name_->Name().data());
    }

    // 生成赋值语句
    tree::Exp *addr = new tree::BinopExp(
        tree::BinOp::PLUS_OP,
        new tree::TempExp(record_temp),
        new tree::ConstExp(index * reg_manager->WordSize()));
    tree::MoveStm *move_stm = new tree::MoveStm(
        new tree::MemExp(addr), field_exp_ty->exp_->UnEx());

    tree::SeqStm *new_seq = new tree::SeqStm(move_stm, nullptr);
    tail->right_ = new_seq;
    tail = new_seq;

    ++index;
    ++defined_fields_it;
  }

  tail->right_ = tr::getVoidStm();

  auto eseq = new tree::EseqExp(stm_seq, new tree::TempExp(record_temp));
  if (fields_->GetList().empty()) {
    return new tr::ExpAndTy(new tr::ExExp(eseq), type::NilTy::Instance());
  }
  return new tr::ExpAndTy(new tr::ExExp(eseq), rec_type->ActualTy());
}



tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  auto expressions = seq_->GetList();
  absyn::Exp *last_exp = expressions.back(); //尾部负责输出值
  expressions.pop_back();

  if (expressions.empty()) {
    return last_exp->Translate(venv, tenv, level, label, errormsg);
  }

  tree::SeqStm *head = nullptr;
  tree::SeqStm *current = nullptr;

  for (auto exp : expressions) {
    auto exp_and_ty = exp->Translate(venv, tenv, level, label, errormsg);
    tree::SeqStm *new_seq = new tree::SeqStm(exp_and_ty->exp_->UnNx(), nullptr);

    if (!head) {
      head = new_seq;
      current = new_seq;
    } else {
      current->right_ = new_seq;
      current = new_seq;
    }//串成链表 方便后续用一个 SeqStm 头节点整体表示整个语句序列
  }

  auto last_exp_and_ty = last_exp->Translate(venv, tenv, level, label, errormsg);
  current->right_ = tr::getVoidStm();

  tree::Exp *eseq_exp = new tree::EseqExp(head, last_exp_and_ty->exp_->UnEx());

  return new tr::ExpAndTy(new tr::ExExp(eseq_exp), last_exp_and_ty->ty_);
}


//////////////////

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp = exp_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *varTy = var->ty_->ActualTy();
  type::Ty *expTy = exp->ty_->ActualTy();

  // check if var and exp has the same type
  if (varTy->IsSameType(expTy)) {

    // check if loop variable (readonly) is assigned
    if (typeid(*var_) == typeid(absyn::SimpleVar)) {
      absyn::SimpleVar *var = static_cast<absyn::SimpleVar *>(var_);
      env::VarEntry *entry =
          static_cast<env::VarEntry *>(venv->Look(var->sym_));
      if (entry->readonly_) {
        errormsg->Error(pos_, "loop variable can't be assigned");
      }
    }

  } else {
    errormsg->Error(pos_, "unmatched assign exp");
  }
  tree::MoveStm *stm = new tree::MoveStm(var->exp_->UnEx(), exp->exp_->UnEx()); //赋值语句
  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance()); //没有返回值的语句 且类型是woid
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *test_exp_and_ty =
      test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then_exp_and_ty =
      then_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *thenTy = then_exp_and_ty->ty_;
  if (errormsg->AnyErrors()) {
    return new tr::ExpAndTy(tr::getVoidExp(), thenTy);
  }


  temp::Label *t = temp::LabelFactory::NewLabel();
  temp::Label *f = temp::LabelFactory::NewLabel();

  temp::Label *joint = temp::LabelFactory::NewLabel();

  temp::Temp *r = temp::TempFactory::NewTemp();

  tr::Cx test_cx = test_exp_and_ty->exp_->UnCx(errormsg);

  test_cx.trues_.DoPatch(t);
  test_cx.falses_.DoPatch(f);

  if (!elsee_) {//没有else
    if (!thenTy->IsSameType(type::VoidTy::Instance())) {//但是产生了值
      errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
      return new tr::ExpAndTy(tr::getVoidExp(), thenTy);
    }

    tree::SeqStm *stm = new tree::SeqStm(
        test_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(t),
                         new tree::SeqStm(then_exp_and_ty->exp_->UnNx(),
                                          new tree::LabelStm(f))));

    return new tr::ExpAndTy(new tr::NxExp(stm), thenTy);
  } else {//有else
    tr::ExpAndTy *else_exp_and_ty =
        elsee_->Translate(venv, tenv, level, label, errormsg);
    type::Ty *elseTy = else_exp_and_ty->ty_;
    if (!elseTy->IsSameType(thenTy)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
      return new tr::ExpAndTy(tr::getVoidExp(), thenTy);
    }
    //构建跳转
    tree::SeqStm *true_stm = new tree::SeqStm(
        new tree::LabelStm(t),
        new tree::SeqStm(
            new tree::MoveStm(new tree::TempExp(r),
                              then_exp_and_ty->exp_->UnEx()),
            new tree::JumpStm(new tree::NameExp(joint),
                              new std::vector<temp::Label *>{joint})));
                              //当条件成立时，就跳转到标签 t，计算 then 分支的值，并跳到 joint 汇合点。

    tree::SeqStm *false_stm = new tree::SeqStm(
        new tree::LabelStm(f),
        new tree::SeqStm(
            new tree::MoveStm(new tree::TempExp(r),
                              else_exp_and_ty->exp_->UnEx()),
            new tree::JumpStm(new tree::NameExp(joint),
                              new std::vector<temp::Label *>{joint})));

    tree::EseqExp *exp = new tree::EseqExp(
        new tree::SeqStm(
            test_cx.stm_,
            new tree::SeqStm(
                true_stm,
                new tree::SeqStm(false_stm, new tree::LabelStm(joint)))),//顺序拼起
        new tree::TempExp(r)); //所有 then 和 else 的计算结果都存进这个临时寄存器 r，最后表达式就等价于读取 r。
    return new tr::ExpAndTy(new tr::ExExp(exp), thenTy);
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  //Tiger 语言中，while 语句的语义决定它不引入新的作用域
  temp::Label *doneLabel = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *test_exp_and_ty =
      test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *body_exp_and_ty =
      body_->Translate(venv, tenv, level, doneLabel, errormsg);
  type::Ty *bodyTy = body_exp_and_ty->ty_;
  if (!bodyTy->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "while body must produce no value");
  }

  temp::Label *testLabel = temp::LabelFactory::NewLabel();
  temp::Label *bodyLabel = temp::LabelFactory::NewLabel();
  tr::Cx test_cx = test_exp_and_ty->exp_->UnCx(errormsg);

  test_cx.trues_.DoPatch(bodyLabel);
  test_cx.falses_.DoPatch(doneLabel);

  //  test:
  //    if not(condition) goto done
  //  body:
  //    body
  //    goto test
  //  done:
  tree::SeqStm *seq_stm = new tree::SeqStm(
      new tree::LabelStm(testLabel),
      new tree::SeqStm(
          test_cx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(bodyLabel), //满足条件后进入循环体
              new tree::SeqStm(
                  body_exp_and_ty->exp_->UnNx(), //循环体代码
                  new tree::SeqStm(
                      new tree::JumpStm(
                          new tree::NameExp(testLabel),
                          new std::vector<temp::Label *>{testLabel}), //跳回 testLabel 重新判断
                      new tree::LabelStm(doneLabel))))));

  return new tr::ExpAndTy(new tr::NxExp(seq_stm), type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(tr::Access::AllocLocal(level, false),
                                      type::IntTy::Instance(), true));
  tr::ExpAndTy *lo_exp_and_ty =
      lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *hi_exp_and_ty =
      hi_->Translate(venv, tenv, level, label, errormsg);
  // check if lo and hi are int type
  if (!(lo_exp_and_ty->ty_)->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  if (!(hi_exp_and_ty->ty_)->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }

  // temp::Label *loop_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();
  temp::Label *inc_label = temp::LabelFactory::NewLabel();

  // check if body produce value
  tr::ExpAndTy *body_exp_and_ty =
      body_->Translate(venv, tenv, level, done_label, errormsg);
  type::Ty *bodyTy = body_exp_and_ty->ty_;
  if (!bodyTy->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "for body should produce no value");
  }

  temp::Temp *limit = temp::TempFactory::NewTemp();
  env::VarEntry *loop_i_entry = static_cast<env::VarEntry *>(venv->Look(var_));
  temp::Temp *loop_i =
      (static_cast<frame::InRegAccess *>(loop_i_entry->access_->access_))->reg;

  // 初始化语句
  auto loop_i_init_stmt =
      new tree::MoveStm(new tree::TempExp(loop_i), lo_exp_and_ty->exp_->UnEx());
  // init limit with hi_
  auto limit_init_stmt =
      new tree::MoveStm(new tree::TempExp(limit), hi_exp_and_ty->exp_->UnEx());

  // if i > limit 直接结束循环
  auto i_gt_limit_cjump_stmt =
      new tree::CjumpStm(tree::RelOp::GT_OP, new tree::TempExp(loop_i),
                         new tree::TempExp(limit), done_label, body_label);

  // if i == limit 直接结束循环
  auto i_eq_limit_cjump_stmt =
      new tree::CjumpStm(tree::RelOp::EQ_OP, new tree::TempExp(loop_i),
                         new tree::TempExp(limit), done_label, inc_label);

  // i := i + 1
  auto loop_i_increase_stmt = new tree::MoveStm(
      new tree::TempExp(loop_i),
      new tree::BinopExp(tree::BinOp::PLUS_OP, new tree::TempExp(loop_i),
                         new tree::ConstExp(1)));

  tree::Stm *stm = new tree::SeqStm(
      loop_i_init_stmt,
      new tree::SeqStm(
          limit_init_stmt,
          new tree::SeqStm(
              i_gt_limit_cjump_stmt,
              new tree::SeqStm(
                  new tree::LabelStm(body_label), // 循环体开始 + 
                  new tree::SeqStm(
                       body_exp_and_ty->exp_->UnNx(), //执行用户提供的 body 代码
                      new tree::SeqStm(
                          i_eq_limit_cjump_stmt,
                          new tree::SeqStm(
                              new tree::LabelStm(inc_label),
                              new tree::SeqStm(
                                  loop_i_increase_stmt,//增量
                                  new tree::SeqStm(
                                      new tree::JumpStm(
                                          new tree::NameExp(body_label),
                                          new std::vector<temp::Label *>(
                                              {body_label})),
                                      new tree::LabelStm(done_label))))))))));


  venv->EndScope();
  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tree::JumpStm *jump_stm = new tree::JumpStm(
      new tree::NameExp(label), new std::vector<temp::Label *>{label});
  return new tr::ExpAndTy(new tr::NxExp(jump_stm), type::VoidTy::Instance());
}


/*
在一个新的作用域中执行 decs（定义变量、类型、函数等）；

然后执行 body 表达式；

并返回 body 的值；

离开 let 后，decs 中定义的绑定失效（即作用域结束）；
*/
tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  tenv->BeginScope();
  tree::Stm *dec_list_stm = nullptr;
  //将 let 表达式中的所有声明（decs）翻译成一串顺序执行的 tree::Stm 指令（SeqStm 链），用于后续构造 EseqExp。
  for (Dec *dec : decs_->GetList()) {
    tree::Stm *dec_stm =
        dec->Translate(venv, tenv, level, label, errormsg)->UnNx();
    if (dec_list_stm) {
      dec_list_stm = new tree::SeqStm(dec_list_stm, dec_stm);
    } else {
      dec_list_stm = dec_stm;
    }
  }

  tr::ExpAndTy *body_exp_and_ty = new tr::ExpAndTy(nullptr, nullptr);
  if (!body_) {
    body_exp_and_ty->ty_ = type::VoidTy::Instance();
    body_exp_and_ty->exp_ = tr::getVoidExp();
  } else
    body_exp_and_ty = body_->Translate(venv, tenv, level, label, errormsg);

  tenv->EndScope();
  venv->EndScope();

  tree::Exp *result_exp;
  if (dec_list_stm) {
    result_exp = new tree::EseqExp(dec_list_stm, body_exp_and_ty->exp_->UnEx());
  } else {
    result_exp = body_exp_and_ty->exp_->UnEx();
  }

  return new tr::ExpAndTy(new tr::ExExp(result_exp),
                          body_exp_and_ty->ty_->ActualTy());
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  type::Ty *type = tenv->Look(typ_);

  if (type && typeid(*(type->ActualTy())) == typeid(type::ArrayTy)) {

    // check if size is int
    tr::ExpAndTy *size_exp_and_ty =
        size_->Translate(venv, tenv, level, label, errormsg);
    
    type::Ty *size_ty = size_exp_and_ty->ty_;
    if (typeid(*size_ty) != typeid(type::IntTy)) {
      errormsg->Error(pos_, "size of array should be int");
      return new tr::ExpAndTy(tr::getVoidExp(), type::VoidTy::Instance());
    }

    // check if type of init is same as array
    type::Ty *arrayTy = static_cast<type::ArrayTy *>(type->ActualTy())->ty_;
    tr::ExpAndTy *init_exp_and_ty =
        init_->Translate(venv, tenv, level, label, errormsg);
    type::Ty *initTy = init_exp_and_ty->ty_;
    if (!initTy->IsSameType(arrayTy)) {
      errormsg->Error(pos_, "type mismatch");
      return init_exp_and_ty;
    }

    tree::ExpList *args = new tree::ExpList();
    // runtime.c: long *init_array(int size, long init)
    args->Append(size_exp_and_ty->exp_->UnEx());
    args->Append(init_exp_and_ty->exp_->UnEx());
    temp::Temp *r = temp::TempFactory::NewTemp();
    // returns the pointer into a new temporary r
    // The result of the whole expression is r
    tree::EseqExp *exp = new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r),
                          frame::ExternalCall("init_array", args)),//runtime.c
        new tree::TempExp(r));
    return new tr::ExpAndTy(new tr::ExExp(exp), type->ActualTy());
  } else {
    errormsg->Error(pos_, "undefined array %s", typ_);
    return new tr::ExpAndTy(tr::getVoidExp(), type::VoidTy::Instance());
  }
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return tr::getVoidExpAndVoidTy();
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {

  // For FunctionDec Node there are two passes for its children nodes
  for (absyn::FunDec *function : functions_->GetList()) {

    temp::Label *function_label =
        temp::LabelFactory::NamedLabel(function->name_->Name());

    type::Ty *result_ty = function->result_ ? tenv->Look(function->result_)
                                            : type::VoidTy::Instance();
    type::TyList *formals_ty =
        function->params_->MakeFormalTyList(tenv, errormsg);

    for (absyn::FunDec *anotherFunction : functions_->GetList()) {
      if (function != anotherFunction &&
          function->name_ == anotherFunction->name_) {
        errormsg->Error(pos_, "two functions have the same name");
        return new tr::ExExp(new tree::ConstExp(0));
      }
    }

    venv->Enter(function->name_, new env::FunEntry(level, function_label,
                                                   formals_ty, result_ty));
  }

  for (absyn::FunDec *function : functions_->GetList()) {

    // get FunEntry
    env::FunEntry *entry =
        static_cast<env::FunEntry *>(venv->Look(function->name_));

    // build escape list of function formal parameters
    std::list<bool> formal_escapes;
    for (Field *param : function->params_->GetList()) {
      formal_escapes.push_back(param->escape_);
    }

    tr::Level *function_level =
        new tr::Level(level, entry->label_, formal_escapes);
    // Suppose function f(x,y) is nesting inside function g (Level for g is
    // levelg) call tr::Level::NewLevel(levelg, f, {false, false})

    venv->BeginScope();

    type::Ty *result_ty = function->result_ ? tenv->Look(function->result_)
                                            : type::VoidTy::Instance();
    type::TyList *formals_ty =
        function->params_->MakeFormalTyList(tenv, errormsg);

    std::list<tr::Access *> *access_list = function_level->Formals();
    // entering params as env::VarEntry
    auto formal_ty = formals_ty->GetList().cbegin();
    auto access = access_list->cbegin();
    for (absyn::Field *param : function->params_->GetList()) {
      venv->Enter(param->name_, new env::VarEntry(*access, *formal_ty));
      ++formal_ty;
      ++access;
    }

    tr::ExpAndTy *body_exp_and_ty = function->body_->Translate(
        venv, tenv, function_level, entry->label_, errormsg);
    // check if body_ty is same as result_ty
    if (!body_exp_and_ty->ty_->IsSameType(result_ty)) {
      errormsg->Error(pos_, "procedure returns value");
    }

    venv->EndScope();

    // 打包成一个 ProcFrag 放入 frags
    frags->PushBack(new frame::ProcFrag(
        frame::ProcEntryExit1(
            function_level->frame_,
            new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()),
                              body_exp_and_ty->exp_->UnEx())),
        function_level->frame_));
  }
  return tr::getVoidExp();
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *init_exp_and_ty =
      init_->Translate(venv, tenv, level, label, errormsg);
  auto init_ty = init_exp_and_ty->ty_;

  if (typ_) {
    // var x : type_id := exp
    type::Ty *type_id = tenv->Look(typ_);

    // check if type exist
    if (!type_id) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return new tr::NxExp(tr::getVoidStm());
    }

    // check that type_id and type of exp are compatible
    if (!init_ty->ActualTy()->IsSameType(type_id)) {
      errormsg->Error(init_->pos_, "type mismatch");
      return new tr::NxExp(tr::getVoidStm());
    }

    tr::Access *access = tr::Access::AllocLocal(level, escape_);
    venv->Enter(var_, new env::VarEntry(access, type_id));
    return new tr::NxExp(new tree::MoveStm(
        access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),//两次才能找到计算
        init_exp_and_ty->exp_->UnEx()));
  } else {
    // var x := exp
    // if the type of exp is NilTy, must have type_id
    if (init_ty->ActualTy()->IsSameType(type::NilTy::Instance()) &&
        typeid(*(init_ty->ActualTy())) != typeid(type::RecordTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
      return new tr::NxExp(tr::getVoidStm());
    }
    // creates a “new location” tr::Access for each variable at level level
    // by calling tr::Access::AllocLocal(level, escape_);
    tr::Access *access = tr::Access::AllocLocal(level, escape_);
    // Semant records this tr::Access in its VarEntry
    venv->Enter(var_, new env::VarEntry(access, init_ty->ActualTy()));
    return new tr::NxExp(new tree::MoveStm(
        access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),
        init_exp_and_ty->exp_->UnEx()));
  }
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  for (NameAndTy *type : types_->GetList()) {
    for (NameAndTy *anotherType : types_->GetList()) {
      if (type != anotherType && type->name_ == anotherType->name_) {
        errormsg->Error(pos_, "two types have the same name");
        return tr::getVoidExp();
      }
    }

    // Let the body to be NULL at first
    tenv->Enter(type->name_, new type::NameTy(type->name_, nullptr));
  }

  for (NameAndTy *type : types_->GetList()) {
    // find name_ in tenv
    type::NameTy *name_ty =
        static_cast<type::NameTy *>(tenv->Look(type->name_));

    // modify the ty_ field of the type::NameTy class in the tenv for which is
    // NULL now
    name_ty->ty_ = type->ty_->Translate(tenv, errormsg);

    // doesn't get type
    if (!name_ty->ty_) {
      errormsg->Error(pos_, "undefined type %s", type->name_);
      break;
    }


    // check if type declarations form illegal cycle from current one
    type::Ty *tmp = tenv->Look(type->name_), *next, *start = tmp;
    while (tmp) {

      // break if not a name type
      if (typeid(*tmp) != typeid(type::NameTy))
        break;

      next = (static_cast<type::NameTy *>(tmp))->ty_;
      if (next == start) {
        errormsg->Error(pos_, "illegal type cycle");
        return tr::getVoidExp();
      }

      tmp = next;
    }
  }
  return tr::getVoidExp();
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *type = tenv->Look(name_);
  if (!type) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::NilTy::Instance();
  }
  return type;
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  //类型检查阶段解析用户写的数组类型标识符，得到实际的元素类型，构造出数组类型的抽象表示。
  /* TODO: Put your lab5 code here */
  type::Ty *type = tenv->Look(array_);
  if (!type) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return type::NilTy::Instance();
  }
  return new type::ArrayTy(type);
}

} // namespace absyn
