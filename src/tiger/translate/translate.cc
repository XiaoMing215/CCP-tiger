#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

#define NOP (new tr::ExExp(new tree::ConstExp(0)))
extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace {
frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body);
}

namespace tr {
//语义翻译层

//给当前的 Level（一个作用域，对应一个函数）分配一个新的局部变量，并返回一个 tr::Access*
// 为什么 frame::Frame::AllocLocal 和 tr::Access::AllocLocal 都存在？
// 它们虽然名字相似，但职责完全不同——它们属于不同抽象层级。

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  // 从 level 中拿到底层的 frame:每个level对应一个scope他创建时就会有空间的分配
  frame::Frame *f = level->frame_;
  // 调用 frame 的 AllocLocal 分配一个局部变量的存储空间（栈上或寄存器）
  frame::Access *frame_access = f->AllocLocal(escape);
  // 用 tr::Access 封装 frame_access 和 level
  return new Access(level, frame_access);
}



class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
//这里的const需要清除
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

//将表达式转换为三种 IR 表达式类型（Exp / Nx / Cx）
  [[nodiscard]] tree::Exp *UnEx() override { 
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
  // 构造跳转语句：CJUMP(NE, exp_, CONST(0), nullptr, nullptr)
  // 注意：true 和 false 分支标签先设为 nullptr，后续由 PatchList 进行补丁填充
  auto *cjmp = new tree::CjumpStm(tree::RelOp::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr);

  // PatchList 接受 Label** 类型，所以传入的是地址的地址
  PatchList trues({&(cjmp->true_label_)}); //初始化列表
  PatchList falses({&(cjmp->false_label_)});

  return Cx(trues, falses, cjmp);
  //Cx 是 tr::Exp 类的一个返回类型，用于表达条件表达式的“真假跳转逻辑”。
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}
  //用于表示不返回值的语句 
  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
    //不返回值 则只能返回一个0
  }
  [[nodiscard]] tree::Stm *UnNx() override { 
    /* TODO: Put your lab5 code here */
    return stm_;//返回自己的语句即可
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
  //执行某些操作 不直接涉及控制流（即不涉及条件跳转等），
  return {{},{},stm_};
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
    // [[nodiscard]] tree::Exp *UnEx() const override { 
    [[nodiscard]] tree::Exp *UnEx() override { 
    /* TODO: Put your lab5 code here */
    //将控制流表达式（即包含条件跳转的表达式）转换为一个常规的表达式。
    temp::Temp *r = temp::TempFactory::NewTemp();  // 创建一个新的临时变量 r，用于存储计算结果
    temp::Label *t = temp::LabelFactory::NewLabel();  // 创建一个新的标签 t，用于跳转的 true 分支
    temp::Label *f = temp::LabelFactory::NewLabel();  // 创建一个新的标签 f，用于跳转的 false 分支

    // 将 PatchList 中的标签填充
    cx_.trues_.DoPatch(t);  // 把 true 分支的标签填充到跳转语句中
    cx_.falses_.DoPatch(f);  // 把 false 分支的标签填充到跳转语句中

    // 返回一个复合的 EseqExp 表达式
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),  // 首先执行一个 MoveStm，将常数 1 移动到临时变量 r 中
        new tree::EseqExp(
            cx_.stm_,  // 然后执行条件跳转语句（即 cx_.stm_）
            new tree::EseqExp(
                new tree::LabelStm(f),  // 如果 false 分支执行，跳转到标签 f
                new tree::EseqExp(
                    new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0)),  // 将 0 移到 r 中，表示 false 分支的结果
                    new tree::EseqExp(
                        new tree::LabelStm(t),  // 如果 true 分支执行，跳转到标签 t
                        new tree::TempExp(r))))));  // 返回 r，这里 r 被设置为 1 表示 true 分支的结果
  }
  
  [[nodiscard]] tree::Stm *UnNx() override {
    //cx_.stm_) 还没有完成跳转（即没有填充 PatchList），我们不能直接返回 cx_.stm_，
    //因此需要先调用 UnEx 来生成表达式，然后将其封装成一个 ExpStm 语句。
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(UnEx());
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override { 
    /* TODO: Put your lab5 code here */
    //本来就是cx 直接返回即可
    return cx_;
  }
};


//辅助函数：
tree::Exp *staticLink(tr::Level *level_now,tr::Level *level_target){
  tree::Exp * framePtr = new tree::TempExp(reg_manager->FramePointer());
  while(level_now != level_target){
    //only main dont have static link formal
    framePtr = level_now->frame_->StaticLink()->ToExp(framePtr);
    level_now = level_now->parent_;
  }
  return framePtr;
}

tree::Stm *list2tree(std::list<tree::Stm*> stm_list){
  tree::Stm* stm = nullptr;
  for(auto it = stm_list.rbegin();it!=stm_list.rend();it++){
    if(!*it){
      continue;
    }
    if(stm){
      stm = new tree::SeqStm(*it,stm);
    }else{
      stm = *it;
    }
  }
  return stm;
}

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
//将 absyn::AbsynTree 语法树转换为若干条 tree::Stm 形式的中间代码语句，
//并封装成 frags::Frag 链表，用于后续 IR 的处理与优化。
  FillBaseVEnv();
  FillBaseTEnv();
  auto main = absyn_tree_->Translate(venv_.get(),tenv_.get(),main_level_.get(),nullptr,errormsg_.get());
  auto frag = new frame::ProcFrag(tr::list2tree({frame::ProcEntryExit1(main_level_->frame_,main->exp_->UnNx())}),main_level_->frame_);
  frags->PushBack(frag);
}

} // namespace tr

namespace {

/**
 * Wrapper for `ProcExitEntry1`, which deals with the return value of the
 * function body
 * @param level current level
 * @param body function body
 * @return statements after `ProcExitEntry1`
 */
frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body) {
  /* TODO: Put your lab5 code here */
  //有什么用？
    tree::Stm *body_stm = body->UnNx();  // 将函数体转换成语句形式

    // 创建函数的返回语句（假设返回值存放在一个临时寄存器中）
    temp::Temp *result = temp::TempFactory::NewTemp();
    tree::Stm *return_stm = new tree::MoveStm(new tree::TempExp(result), new tree::TempExp(result));

    // 生成函数的入口和出口代码
    tree::Stm *entry_exit_stm = frame::ProcEntryExit1(level->frame_, body_stm);
    
    // 连接入口、函数体和返回语句
    tree::Stm *final_stm = new tree::SeqStm(entry_exit_stm, return_stm);

    // 将所有语句包装成一个ProcFrag
    return new frame::ProcFrag(final_stm, level->frame_);


}
} // namespace

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv,tenv,level,label,errormsg);

}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
auto entry = static_cast<env::VarEntry*>(venv->Look(sym_));
  //entry must not be null in type checking
  auto access = entry->access_->access_;
  tree::Exp *framePtr = nullptr;
  if(typeid(*access)==typeid(frame::InFrameAccess)){
      framePtr = tr::staticLink(level,entry->access_->level_);
  }
  return new tr::ExpAndTy(
    new tr::ExExp(entry->access_->access_->ToExp(framePtr)),
    entry->ty_->ActualTy()
  );
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *exp_ty =
    var_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *exp = exp_ty->exp_;
  type::Ty *ty = exp_ty->ty_->ActualTy();

  if (typeid(*ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  if (typeid(*exp) != typeid(tr::ExExp)) {
    errormsg->Error(pos_, "field var's exp must be an expression");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }
  auto record_ty = static_cast<type::RecordTy *>(ty);
  type::FieldList *field_list = record_ty->fields_;
  int order = 0;
  for (auto field : field_list->GetList()) {
    if (field->name_ == sym_) {
      tree::Exp *texp = new tree::MemExp(new tree::BinopExp(
          tree::PLUS_OP, exp->UnEx(),
          new tree::ConstExp(order * reg_manager->WordSize())));
      return new tr::ExpAndTy(new tr::ExExp(texp), field->ty_->ActualTy());
    }
    order++;
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return new tr::ExpAndTy(nullptr, type::IntTy::Instance());

  /*TODO end*/
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto var_exp_ty = var_->Translate(venv,tenv,level,label,errormsg);
  auto exp_exp_ty = subscript_->Translate(venv,tenv,level,label,errormsg);
  auto base = var_exp_ty->exp_->UnEx();
  auto offset = exp_exp_ty->exp_->UnEx();
  auto size = new tree::ConstExp(reg_manager->WordSize());
  
  return new tr::ExpAndTy(
    new tr::ExExp(
      new tree::MemExp(
        new tree::BinopExp(
          tree::PLUS_OP,
          base,
          new tree::BinopExp(
            tree::MUL_OP,
            offset,size
          )
        )
      )
    ),
    static_cast<type::ArrayTy*>(var_exp_ty->ty_)->ty_->ActualTy()
  );
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto exp_ty = var_->Translate(venv,tenv,level,label,errormsg);

  return new tr::ExpAndTy(
    new tr::ExExp(
      exp_ty->exp_->UnEx()
    ),
    exp_ty->ty_
  );
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
    new tr::ExExp(
      new tree::ConstExp(0)
    ),
    type::NilTy::Instance()
  );
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
    new tr::ExExp(
      new tree::ConstExp(val_)
    ),
    type::IntTy::Instance()
  );
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto str_label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(str_label,str_));
  return new tr::ExpAndTy(
    new tr::ExExp(
      new tree::NameExp(str_label)
    ),
    type::StringTy::Instance()
  );

}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto exp_list = args_->GetList();
  auto arg_list = new tree::ExpList();
  for(const auto &exp:exp_list){
    auto arg_exp_ty = exp->Translate(venv,tenv,level,label,errormsg);
    arg_list->Append(arg_exp_ty->exp_->UnEx());
  }

  auto func_entry = static_cast<env::FunEntry*>(venv->Look(func_));
  auto func_label = func_entry->label_;
  tree::Exp *call_exp;
  if(func_label){
    //func->entry->level is the level of func itself, parent is the level defines func
    arg_list->Insert(staticLink(level,func_entry->level_->parent_));
    call_exp = new tree::CallExp(new tree::NameExp(func_label),arg_list);
  }else{//env.cc externalcall label=nullptr
    //new NamedLabel
    call_exp = frame::externalCall(func_->Name(),arg_list);
  }
  auto res_ty = func_entry->result_;
  return new tr::ExpAndTy(
    new tr::ExExp(call_exp),
    res_ty
  );
  /* End for lab5 code */
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto left_exp_ty = left_->Translate(venv,tenv,level,label,errormsg);
  auto right_exp_ty = right_->Translate(venv,tenv,level,label,errormsg);
  {
    tree::BinOp op = tree::BinOp::BIN_OPER_COUNT;
    switch (oper_)
    {
    case PLUS_OP:op = tree::PLUS_OP;break;
    case MINUS_OP:op = tree::MINUS_OP;break;
    case TIMES_OP:op = tree::MUL_OP;break;
    case DIVIDE_OP:op = tree::DIV_OP;break;
    default:
      break;
    }
    if(op != tree::BIN_OPER_COUNT){
      return new tr::ExpAndTy(
        new tr::ExExp(
          new tree::BinopExp(
            op,
            left_exp_ty->exp_->UnEx(),
            right_exp_ty->exp_->UnEx()
          )
        ),
        left_exp_ty->ty_
      );
    }
  }

  {
    tree::RelOp op = tree::RelOp::REL_OPER_COUNT;
    switch (oper_)
    {
    case LT_OP:op = tree::LT_OP;break;
    case LE_OP:op = tree::LE_OP;break;
    case GT_OP:op = tree::GT_OP;break;
    case GE_OP:op = tree::GE_OP;break;
    default:
      break;
    }
    if(op != tree::REL_OPER_COUNT){
      auto cj = new tree::CjumpStm(op,left_exp_ty->exp_->UnEx(),right_exp_ty->exp_->UnEx(),nullptr,nullptr);
      tr::PatchList trues{{&cj->true_label_}};
      tr::PatchList falses{{&cj->false_label_}};
      return new tr::ExpAndTy(
        new tr::CxExp(trues,falses,cj),
        type::IntTy::Instance()
      );
    }
  }

  {
    tree::RelOp op = tree::RelOp::REL_OPER_COUNT;
    switch (oper_)
    {
    case EQ_OP:op = tree::EQ_OP;break;
    case NEQ_OP:op = tree::NE_OP;break;
    default:
      break;
    }
    if(op != tree::REL_OPER_COUNT){
      tree::CjumpStm* cj = nullptr;
      if(left_exp_ty->ty_->IsSameType(type::StringTy::Instance())){
        auto str_cmp = frame::externalCall("string_equal",new tree::ExpList({left_exp_ty->exp_->UnEx(),right_exp_ty->exp_->UnEx()}));
        //1 eq  0 neq  order does not matter
        cj = new tree::CjumpStm(op,str_cmp,new tree::ConstExp(1),nullptr,nullptr);
      }else{
        cj = new tree::CjumpStm(op,left_exp_ty->exp_->UnEx(),right_exp_ty->exp_->UnEx(),nullptr,nullptr);
      }
      tr::PatchList trues{{&cj->true_label_}};
      tr::PatchList falses{{&cj->false_label_}};
      return new tr::ExpAndTy(
        new tr::CxExp(trues,falses,cj),
        type::IntTy::Instance()
      );
    }
  }

  if(oper_ == AND_OP){
    auto lcx = left_exp_ty->exp_->UnCx(errormsg);
    auto rcx = right_exp_ty->exp_->UnCx(errormsg);
    
    auto right_label = temp::LabelFactory::NewLabel();
    lcx.trues_.DoPatch(right_label);
    auto stm = tr::list2tree({
      lcx.stm_,
      new tree::LabelStm(right_label),
      rcx.stm_
    });
    return new tr::ExpAndTy(
      new tr::CxExp(
        rcx.trues_,
        tr::PatchList::JoinPatch(lcx.falses_,rcx.falses_),
        stm
      ),
      type::IntTy::Instance()
    );
  }

  if(oper_ == OR_OP){
    auto lcx = left_exp_ty->exp_->UnCx(errormsg);
    auto rcx = right_exp_ty->exp_->UnCx(errormsg);
    
    auto right_label = temp::LabelFactory::NewLabel();
    lcx.falses_.DoPatch(right_label);
    auto stm = tr::list2tree({
      lcx.stm_,
      new tree::LabelStm(right_label),
      rcx.stm_
    });
    return new tr::ExpAndTy(
      new tr::CxExp(
        tr::PatchList::JoinPatch(lcx.trues_,rcx.trues_),
        rcx.falses_,
        stm
      ),
      type::IntTy::Instance()
    );
  }
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto ty = static_cast<type::RecordTy*>(tenv->Look(typ_));
  auto field_list = ty->fields_->GetList();
  auto wordsize = reg_manager->WordSize();
  auto record_len = field_list.size();
  auto alloc_record = frame::externalCall("alloc_record",new tree::ExpList({new tree::ConstExp(record_len*wordsize)}));

  auto r = temp::TempFactory::NewTemp();
  auto move_addr_to_r = new tree::MoveStm(
    new tree::TempExp(r),
    alloc_record
  );

  auto offset = (record_len-1)*wordsize;
  auto efield_list = fields_->GetList();
  auto efield_it = efield_list.rbegin();

  tree::Stm * stm = nullptr;
  for(auto efield_it = efield_list.rbegin();efield_it!=efield_list.rend();efield_it++){
    auto exp_ty = (*efield_it)->exp_->Translate(venv,tenv,level,label,errormsg);
    
    auto move = new tree::MoveStm(
                  new tree::MemExp(
                    new tree::BinopExp(
                      tree::PLUS_OP,
                      new tree::TempExp(r),
                      new tree::ConstExp(offset)
                    )
                  ),
                  exp_ty->exp_->UnEx()
                );
    offset -= wordsize;
    if(stm){
      stm = new tree::SeqStm(move,stm);
    }else{
      stm = move;
    }
  }

  return new tr::ExpAndTy(
    new tr::ExExp(
      new tree::EseqExp(
        new tree::SeqStm(
          move_addr_to_r,
          stm
        ),
        new tree::TempExp(r)
      )
    ),
    ty
  );
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto exp_list = seq_->GetList();
  auto exp_it = exp_list.begin();
  std::list<tree::Stm*> stm_list;
  for(;std::next(exp_it)!=exp_list.end();exp_it++){
    auto exp_ty = (*exp_it)->Translate(venv,tenv,level,label,errormsg);
    stm_list.push_back(exp_ty->exp_->UnNx());
  }
  auto exp_ty = (*exp_it)->Translate(venv,tenv,level,label,errormsg);
  if(stm_list.empty()){
    return exp_ty;
  }
  return new tr::ExpAndTy(
    new tr::ExExp(
      new tree::EseqExp(
        tr::list2tree(stm_list),
        exp_ty->exp_->UnEx()
      )
    ),
    exp_ty->ty_
  );
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto var_exp_ty = var_->Translate(venv,tenv,level,label,errormsg);
  auto exp_exp_ty = exp_->Translate(venv,tenv,level,label,errormsg);
  return new tr::ExpAndTy(
    new tr::NxExp(
      new tree::MoveStm(
        var_exp_ty->exp_->UnEx(),
        exp_exp_ty->exp_->UnEx()
      )
    ),
    type::VoidTy::Instance()
  );
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
auto test_exp_ty = test_->Translate(venv,tenv,level,label,errormsg);
  auto then_exp_ty = then_->Translate(venv,tenv,level,label,errormsg);

  auto t = temp::LabelFactory::NewLabel();
  auto f = temp::LabelFactory::NewLabel();
  auto done = temp::LabelFactory::NewLabel();

  auto cx = test_exp_ty->exp_->UnCx(errormsg);
  cx.trues_.DoPatch(t);
  cx.falses_.DoPatch(f);

  if(!elsee_){//if then
    auto stm = tr::list2tree({
      cx.stm_,
      new tree::LabelStm(t),
      then_exp_ty->exp_->UnNx(),
      new tree::LabelStm(f)
    });
    return new tr::ExpAndTy(
      new tr::NxExp(stm),
      type::VoidTy::Instance()
    );
  }
  // if then else
  auto r = temp::TempFactory::NewTemp();
  auto else_exp_ty = elsee_->Translate(venv,tenv,level,label,errormsg);
  auto stm = tr::list2tree({
    cx.stm_,
    new tree::LabelStm(t),
    new tree::MoveStm(
      new tree::TempExp(r),
      then_exp_ty->exp_->UnEx()
    ),
    new tree::JumpStm(new tree::NameExp(done),new std::vector<temp::Label*>{done}),
    new tree::LabelStm(f),
    new tree::MoveStm(
      new tree::TempExp(r),
      else_exp_ty->exp_->UnEx()
    ),
    new tree::LabelStm(done)
  });
  
  return new tr::ExpAndTy(
    new tr::ExExp(
      new tree::EseqExp(
        stm,
        new tree::TempExp(r)
      )
    ),
    then_exp_ty->ty_
  );
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
auto test = temp::LabelFactory::NewLabel();
  auto done = temp::LabelFactory::NewLabel();
  auto body = temp::LabelFactory::NewLabel();
  auto test_exp_ty = test_->Translate(venv,tenv,level,label,errormsg);
  auto body_exp_ty = body_->Translate(venv,tenv,level,done,errormsg);
  auto cx = test_exp_ty->exp_->UnCx(errormsg);
  cx.trues_.DoPatch(body);
  cx.falses_.DoPatch(done);

  auto seq = tr::list2tree({
    new tree::LabelStm(test),
    cx.stm_,
    new tree::LabelStm(body),
    body_exp_ty->exp_->UnNx(),
    new tree::JumpStm(new tree::NameExp(test),new std::vector<temp::Label*>{test}),
    new tree::LabelStm(done)
  });
  return new tr::ExpAndTy(
    new tr::NxExp(
      seq
    ),
    type::VoidTy::Instance()
  );
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
 auto low_exp_ty = lo_->Translate(venv,tenv,level,label,errormsg);
  auto high_exp_ty = hi_->Translate(venv,tenv,level,label,errormsg);

  venv->BeginScope();
  auto access = tr::Access::AllocLocal(level,escape_);
  auto access_limit = tr::Access::AllocLocal(level,false);
  auto i = access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer()));
  auto limit = access_limit->access_->ToExp(nullptr);


  auto body = temp::LabelFactory::NewLabel();
  auto loop = temp::LabelFactory::NewLabel();
  auto done = temp::LabelFactory::NewLabel();
  venv->Enter(var_,new env::VarEntry(access,low_exp_ty->ty_,true));

  auto body_exp_ty = body_->Translate(venv,tenv,level,done,errormsg);

  venv->EndScope();


  auto cj1 = new tree::CjumpStm(tree::GT_OP,i,limit,done,body);
  auto cj2 = new tree::CjumpStm(tree::EQ_OP,i,limit,done,loop);
  auto cj3 = new tree::CjumpStm(tree::LT_OP,i,limit,loop,done);

  auto body_stm = body_exp_ty->exp_->UnNx();
  auto seq = tr::list2tree({
    new tree::MoveStm(
      i,low_exp_ty->exp_->UnEx()
    ),
    new tree::MoveStm(
      limit,high_exp_ty->exp_->UnEx()
    ),
    cj1,
    new tree::LabelStm(body),
    body_stm,
    cj2,
    new tree::LabelStm(loop),
    new tree::MoveStm(
      i,
      new tree::BinopExp(tree::PLUS_OP,i,new tree::ConstExp(1))
    ),
    body_stm,
    cj3,
    new tree::LabelStm(done)
  });
  return new tr::ExpAndTy(
    new tr::NxExp(seq),
    type::VoidTy::Instance()
  );
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new::tr::ExpAndTy(
    new tr::NxExp(
      new tree::JumpStm(
        new tree::NameExp(label),new std::vector<temp::Label*>{label}
      )
    ),
    type::VoidTy::Instance()
  );
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  if (!body_){
    return new tr::ExpAndTy(
      new tr::ExExp(new tree::ConstExp(0)),
      type::VoidTy::Instance()
    );
  }

  venv->BeginScope();
  tenv->BeginScope();
  auto decslist = decs_->GetList();
  std::list<tree::Stm*> dec_stm_list;
  for (const auto &dec : decslist){
    auto dec_exp = dec->Translate(venv, tenv, level,label, errormsg);
    dec_stm_list.push_back(dec_exp->UnNx());
  }

  auto body_exp_ty = body_->Translate(venv, tenv, level,label, errormsg);
  
  tenv->EndScope();
  venv->EndScope();

  auto seqstm = tr::list2tree(dec_stm_list);
  if(!seqstm){
    return body_exp_ty;
  }
  return new tr::ExpAndTy(
    new tr::ExExp(
      new tree::EseqExp(
        seqstm,
        body_exp_ty->exp_->UnEx()
      )
    ),
    body_exp_ty->ty_
  );
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto ty = static_cast<type::ArrayTy*>(tenv->Look(typ_));
  auto size_exp_ty = size_->Translate(venv,tenv,level,label,errormsg);
  auto init_exp_ty = init_->Translate(venv,tenv,level,label,errormsg);
  auto init_array = frame::externalCall("init_array",new tree::ExpList({size_exp_ty->exp_->UnEx(),init_exp_ty->exp_->UnEx()}));

  //externalcall already mov init value
  return new tr::ExpAndTy(
    new tr::ExExp(
      init_array
    ),
    ty
  );
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
    new tr::NxExp(
      nullptr
    ),
    type::VoidTy::Instance()
  );
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto func_list = functions_->GetList();

  for(const auto&function:func_list){
    auto params = function->params_;
    auto escape = new std::list<bool>;
    //dont add static link here ,Level will add it in constructor
    for(const auto&arg:params->GetList()){
      escape->push_back(arg->escape_);
    }
    type::Ty *result_ty = type::VoidTy::Instance();
    if(function->result_){
      result_ty = tenv->Look(function->result_); 
    }
    auto formals = params->MakeFormalTyList(tenv, errormsg);
    auto name = function->name_;
    auto f_label = temp::LabelFactory::NamedLabel(name->Name());
    auto f_level = new tr::Level(frame::NewFrame(f_label, {escape}), level);//有更改
    //must add level of the function to env instead of the level defines it
    venv->Enter(name,new env::FunEntry(f_level,f_label,formals,result_ty));
  }
  for(const auto&function:func_list){
    auto entry = static_cast<env::FunEntry*>(venv->Look(function->name_));
    auto f_level = entry->level_;
    auto params = function->params_;
    auto formals = params->MakeFormalTyList(tenv, errormsg);
    venv->BeginScope();

    //after new frame,params will be allocated
    auto formal_it = entry->level_->frame_->GetFormalList().begin();
    formal_it++;//static link
    auto ty_it = entry->formals_->GetList().begin();//not contain static link
    auto param_it = params->GetList().begin();
    for (; param_it != params->GetList().end(); formal_it++, param_it++){
      //this access shows the level of where formal defines is the same as the function
      venv->Enter((*param_it)->name_,new env::VarEntry(new tr::Access(f_level,*formal_it),*ty_it));
    }
    
    auto res_exp_ty = function->body_->Translate(venv,tenv,f_level,entry->label_,errormsg);
    venv->EndScope();

    auto ret = new tree::MoveStm(
      new tree::TempExp(reg_manager->ReturnValue()),
      res_exp_ty->exp_->UnEx()
    );

    auto frag = new frame::ProcFrag(tr::list2tree({frame::ProcEntryExit1(f_level->frame_,ret)}),f_level->frame_);
    frags->PushBack(frag);
  }

  return NOP;
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *init_exp_ty =
      init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *init_ty = init_exp_ty->ty_;

  if (typ_) {
    type::Ty *ty = tenv->Look(typ_);
    if (!ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    }

    if (!ty->IsSameType(init_ty)) {
      errormsg->Error(pos_, "type and init type mismatch");
    }
  } else {
    auto actual_init_ty = init_ty->ActualTy();
    if (typeid(*actual_init_ty) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
  }

  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, init_ty));

  return new tr::NxExp(
      new tree::MoveStm(access->access_->ToExp(new tree::TempExp(
                            reg_manager->FramePointer())),
                        init_exp_ty->exp_->UnEx()));
  /*TODO end*/
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto type_list = types_->GetList();
  for(const auto &type:type_list){
    tenv->Enter(type->name_, new type::NameTy(type->name_,nullptr)); 
  }
  for(const auto &type:type_list){
    
    auto ty = type->ty_->Translate(tenv, errormsg);
    tenv->Enter(type->name_, ty); 
    //type list = { first: int, rest: list }
    if(typeid(*ty)==typeid(type::RecordTy)){
      auto fields_list = static_cast<type::RecordTy*>(ty)->fields_->GetList();
      for(auto&field:fields_list){
          if(typeid(*field->ty_)==typeid(type::NameTy)){
            auto ty_ = static_cast<type::NameTy*>(field->ty_);
            if(ty_->sym_ == type->name_ && !ty_->ty_){
              field->ty_ = ty;
            }
          }
      }
    }
  }

  return NOP;
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::NameTy(
    name_,tenv->Look(name_)
  );
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::RecordTy(
    record_->MakeFieldList(tenv,errormsg)
  );
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::ArrayTy(
    tenv->Look(array_)
  );
}

} // namespace absyn
