#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace {
frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body);
}

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  frame::Access *access = level->frame_->AllocLocal(escape);
  return new Access(level, access);
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
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
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

  [[nodiscard]] tree::Exp *UnEx() const override { 
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    tree::CjumpStm *stm = new tree::CjumpStm(
        tree::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr);
    std::list<temp::Label**> true_list = {&stm->true_label_};
    PatchList trues(true_list);
    std::list<temp::Label**> false_list = {&stm->false_label_};
    PatchList falses(false_list);
    return tr::Cx(trues, falses, stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override { 
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    std::list<temp::Label**> empty_list;
    PatchList empty(empty_list);
    return tr::Cx(empty, empty, nullptr);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    temp::Label *done = temp::LabelFactory::NewLabel();

    const_cast<PatchList&>(cx_.trues_).DoPatch(t);
    const_cast<PatchList&>(cx_.falses_).DoPatch(f);

    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(
                    new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0)),
                    new tree::EseqExp(
                        new tree::LabelStm(t),
                        new tree::TempExp(r))))));
    
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    temp::Label *done = temp::LabelFactory::NewLabel();
    const_cast<PatchList&>(cx_.trues_).DoPatch(done);
    const_cast<PatchList&>(cx_.falses_).DoPatch(done);
    return new tree::SeqStm(cx_.stm_, new tree::LabelStm(done));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override { 
    /* TODO: Put your lab5 code here */
     return cx_;
  }
};

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  temp::Label *main_label = temp::LabelFactory::NamedLabel("tigermain");
  std::list<bool> formals_escape;
  formals_escape.push_back(false);  // static link
  frame::Frame *frame = frame::NewFrame(main_label, formals_escape);
  tr::Level *main_level = tr::Level::NewLevel(nullptr, main_label, formals_escape);

  venv_ = std::make_unique<env::VEnv>();
  tenv_ = std::make_unique<env::TEnv>();

  FillBaseVEnv();
  FillBaseTEnv();

  tr::ExpAndTy *exp_ty =
      absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level, main_label, errormsg_.get());

  frame::ProcFrag *main_frag = ProcEntryExit(main_level, exp_ty->exp_);
  frags->PushBack(main_frag);
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
  frame::Frame *frame = level->frame_;
  tree::Stm *stm = new tree::MoveStm(
      new tree::TempExp(reg_manager->ReturnValue()),
      body->UnEx());
  frame::ProcFrag *frag = new frame::ProcFrag(stm, frame);
  frags->PushBack(frag);
  return frag;
}
} // namespace

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
  /* TODO: Put your lab5 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if (!entry || typeid(*entry) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }

  auto var_entry = static_cast<env::VarEntry *>(entry);
  tree::Exp *fp = new tree::TempExp(reg_manager->FramePointer());
  tree::Exp *access = var_entry->access_->access_->ToExp(fp);
  return new tr::ExpAndTy(new tr::ExExp(access), var_entry->ty_);
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
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
    tr::ExpAndTy *index = subscript_->Translate(venv, tenv, level, label, errormsg);

    if (typeid(*(var->ty_->ActualTy())) == typeid(type::ArrayTy)) {
        tree::Exp *base = var->exp_->UnEx();
        tree::Exp *offset = new tree::BinopExp(
            tree::MUL_OP, 
            index->exp_->UnEx(),
            new tree::ConstExp(reg_manager->WordSize()));
        tree::Exp *addr = new tree::BinopExp(
            tree::PLUS_OP,
            base,
            offset);
        return new tr::ExpAndTy(
            new tr::ExExp(new tree::MemExp(addr)),
            static_cast<type::ArrayTy*>(var->ty_->ActualTy())->ty_);
    } else {
        errormsg->Error(pos_, "array type required");
        return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
    }

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
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::NilTy::Instance());

}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)), type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *str_label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(str_label, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(str_label)), type::StringTy::Instance());

}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }

  auto fun_entry = static_cast<env::FunEntry *>(entry);
  std::list<tr::Exp *> args_exp;
  for (Exp *exp : args_->GetList()) {
    tr::ExpAndTy *exp_ty = exp->Translate(venv, tenv, level, label, errormsg);
    args_exp.push_back(exp_ty->exp_);
  }

  tree::ExpList *args_list = new tree::ExpList();
  for (tr::Exp *exp : args_exp) {
    args_list->Append(exp->UnEx());
  }

  tree::Exp *call_exp = new tree::CallExp(
    new tree::NameExp(func_),  
    args_list);

  return new tr::ExpAndTy(new tr::ExExp(call_exp), fun_entry->result_);
  /* End for lab5 code */
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *left = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right = right_->Translate(venv, tenv, level, label, errormsg);

  switch (oper_) {
    case PLUS_OP:
      return new tr::ExpAndTy(
          new tr::ExExp(new tree::BinopExp(tree::PLUS_OP, left->exp_->UnEx(), right->exp_->UnEx())),
          type::IntTy::Instance());
    case MINUS_OP:
      return new tr::ExpAndTy(
          new tr::ExExp(new tree::BinopExp(tree::MINUS_OP, left->exp_->UnEx(), right->exp_->UnEx())),
          type::IntTy::Instance());
    case TIMES_OP:
      return new tr::ExpAndTy(
          new tr::ExExp(new tree::BinopExp(tree::MUL_OP, left->exp_->UnEx(), right->exp_->UnEx())),
          type::IntTy::Instance());
    case DIVIDE_OP:
      return new tr::ExpAndTy(
          new tr::ExExp(new tree::BinopExp(tree::DIV_OP, left->exp_->UnEx(), right->exp_->UnEx())),
          type::IntTy::Instance());
    case LT_OP: {
      tree::CjumpStm *cjump = new tree::CjumpStm(tree::LT_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
      std::list<temp::Label**> trues = {&cjump->true_label_};
      std::list<temp::Label**> falses = {&cjump->false_label_};
      return new tr::ExpAndTy(
          new tr::CxExp(tr::PatchList(trues), tr::PatchList(falses), cjump),
          type::IntTy::Instance());
    }
    case LE_OP: {
      tree::CjumpStm *cjump = new tree::CjumpStm(tree::LE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
      std::list<temp::Label**> trues = {&cjump->true_label_};
      std::list<temp::Label**> falses = {&cjump->false_label_};
      return new tr::ExpAndTy(
          new tr::CxExp(tr::PatchList(trues), tr::PatchList(falses), cjump),
          type::IntTy::Instance());
    }
    case GT_OP: {
      tree::CjumpStm *cjump = new tree::CjumpStm(tree::GT_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
      std::list<temp::Label**> trues = {&cjump->true_label_};
      std::list<temp::Label**> falses = {&cjump->false_label_};
      return new tr::ExpAndTy(
          new tr::CxExp(tr::PatchList(trues), tr::PatchList(falses), cjump),
          type::IntTy::Instance());
    }
    case GE_OP: {
      tree::CjumpStm *cjump = new tree::CjumpStm(tree::GE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
      std::list<temp::Label**> trues = {&cjump->true_label_};
      std::list<temp::Label**> falses = {&cjump->false_label_};
      return new tr::ExpAndTy(
          new tr::CxExp(tr::PatchList(trues), tr::PatchList(falses), cjump),
          type::IntTy::Instance());
    }
    case EQ_OP: {
      tree::CjumpStm *cjump = new tree::CjumpStm(tree::EQ_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
      std::list<temp::Label**> trues = {&cjump->true_label_};
      std::list<temp::Label**> falses = {&cjump->false_label_};
      return new tr::ExpAndTy(
          new tr::CxExp(tr::PatchList(trues), tr::PatchList(falses), cjump),
          type::IntTy::Instance());
    }
    case NEQ_OP: {
      tree::CjumpStm *cjump = new tree::CjumpStm(tree::NE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
      std::list<temp::Label**> trues = {&cjump->true_label_};
      std::list<temp::Label**> falses = {&cjump->false_label_};
      return new tr::ExpAndTy(
          new tr::CxExp(tr::PatchList(trues), tr::PatchList(falses), cjump),
          type::IntTy::Instance());
    }
    case AND_OP: {
      temp::Label *t = temp::LabelFactory::NewLabel();
      temp::Label *f = temp::LabelFactory::NewLabel();
      temp::Label *join = temp::LabelFactory::NewLabel();
      
      tree::CjumpStm *cjump = new tree::CjumpStm(tree::NE_OP, left->exp_->UnEx(), new tree::ConstExp(0), t, f);
      std::list<temp::Label**> trues = {&cjump->true_label_};
      std::list<temp::Label**> falses = {&cjump->false_label_};
      
      tree::Stm *then_stm = new tree::SeqStm(
          new tree::LabelStm(t),
          new tree::SeqStm(
              new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), right->exp_->UnEx()),
              new tree::JumpStm(new tree::NameExp(join), new std::vector<temp::Label *>{join})));
      
      tree::Stm *else_stm = new tree::SeqStm(
          new tree::LabelStm(f),
          new tree::SeqStm(
              new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), new tree::ConstExp(0)),
              new tree::JumpStm(new tree::NameExp(join), new std::vector<temp::Label *>{join})));
      
      tree::Stm *join_stm = new tree::SeqStm(
          new tree::LabelStm(join),
          new tree::ExpStm(new tree::TempExp(reg_manager->ReturnValue())));
      
      return new tr::ExpAndTy(
          new tr::ExExp(new tree::EseqExp(
              cjump,
              new tree::EseqExp(
                  then_stm,
                  new tree::EseqExp(
                      else_stm,
                      new tree::EseqExp(
                          join_stm,
                          new tree::TempExp(reg_manager->ReturnValue())
                      )
                  )
              )
          )),
          type::IntTy::Instance());
    }
    case OR_OP: {
      temp::Label *t = temp::LabelFactory::NewLabel();
      temp::Label *f = temp::LabelFactory::NewLabel();
      temp::Label *join = temp::LabelFactory::NewLabel();
      
      tree::CjumpStm *cjump = new tree::CjumpStm(tree::NE_OP, left->exp_->UnEx(), new tree::ConstExp(0), t, f);
      std::list<temp::Label**> trues = {&cjump->true_label_};
      std::list<temp::Label**> falses = {&cjump->false_label_};
      
      tree::Stm *then_stm = new tree::SeqStm(
          new tree::LabelStm(t),
          new tree::SeqStm(
              new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), new tree::ConstExp(1)),
              new tree::JumpStm(new tree::NameExp(join), new std::vector<temp::Label *>{join})));
      
      tree::Stm *else_stm = new tree::SeqStm(
          new tree::LabelStm(f),
          new tree::SeqStm(
              new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), right->exp_->UnEx()),
              new tree::JumpStm(new tree::NameExp(join), new std::vector<temp::Label *>{join})));
      
      tree::Stm *join_stm = new tree::SeqStm(
          new tree::LabelStm(join),
          new tree::ExpStm(new tree::TempExp(reg_manager->ReturnValue())));
      
      return new tr::ExpAndTy(
          new tr::ExExp(new tree::EseqExp(
              cjump,
              new tree::EseqExp(
                  then_stm,
                  new tree::EseqExp(
                      else_stm,
                      new tree::EseqExp(
                          join_stm,
                          new tree::TempExp(reg_manager->ReturnValue())
                      )
                  )
              )
          )),
          type::IntTy::Instance());
    }
    default:
      errormsg->Error(pos_, "unknown operator");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(typ_);
if (!ty) {
  errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
}

auto record_ty = static_cast<type::RecordTy *>(ty->ActualTy());
type::FieldList *fields = record_ty->fields_;

tree::ExpList *field_exps = new tree::ExpList();
for (EField *efield : fields_->GetList()) {
  tr::ExpAndTy *exp_ty = efield->exp_->Translate(venv, tenv, level, label, errormsg);
  field_exps->Append(exp_ty->exp_->UnEx());
}

temp::Temp *r = temp::TempFactory::NewTemp();
tree::ExpList *alloc_args = new tree::ExpList();
alloc_args->Append(new tree::ConstExp(fields->GetList().size() * reg_manager->WordSize()));
tree::Stm *alloc_stm = new tree::MoveStm(
  new tree::TempExp(r),
  new tree::CallExp(
      new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")),
      alloc_args
  )
);

tree::Stm *init_stm = alloc_stm;
int i = 0;
for (tree::Exp *exp : field_exps->GetList()) {
  init_stm = new tree::SeqStm(
      init_stm,
      new tree::MoveStm(
          new tree::MemExp(
              new tree::BinopExp(tree::PLUS_OP, new tree::TempExp(r), 
              new tree::ConstExp(i * reg_manager->WordSize()))),
          exp));
  i++;
}

return new tr::ExpAndTy(
    new tr::ExExp(new tree::EseqExp(init_stm, new tree::TempExp(r))),
    record_ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  if (seq_->GetList().empty()) {
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  tree::Stm *stm = nullptr;
  tr::ExpAndTy *last_exp_ty = nullptr;
  for (Exp *exp : seq_->GetList()) {
    last_exp_ty = exp->Translate(venv, tenv, level, label, errormsg);
    if (stm == nullptr) {
      stm = last_exp_ty->exp_->UnNx();
    } else {
      stm = new tree::SeqStm(stm, last_exp_ty->exp_->UnNx());
    }
  }

  return new tr::ExpAndTy(
      new tr::ExExp(new tree::EseqExp(stm, last_exp_ty->exp_->UnEx())),
      last_exp_ty->ty_);

}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp = exp_->Translate(venv, tenv, level, label, errormsg);

  return new tr::ExpAndTy(
      new tr::NxExp(new tree::MoveStm(var->exp_->UnEx(), exp->exp_->UnEx())),
      type::VoidTy::Instance());

}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *test = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then = then_->Translate(venv, tenv, level, label, errormsg);

  if (!elsee_) {
    return new tr::ExpAndTy(
        new tr::NxExp(new tree::SeqStm(
            test->exp_->UnNx(),
            then->exp_->UnNx())),
        type::VoidTy::Instance());
  }

  tr::ExpAndTy *elsee = elsee_->Translate(venv, tenv, level, label, errormsg);
  temp::Label *t = temp::LabelFactory::NewLabel();
  temp::Label *f = temp::LabelFactory::NewLabel();
  temp::Label *join = temp::LabelFactory::NewLabel();

  tree::CjumpStm *cjump = new tree::CjumpStm(
      tree::NE_OP, test->exp_->UnEx(), new tree::ConstExp(0), t, f);

  tree::Stm *then_stm = new tree::SeqStm(
      new tree::LabelStm(t),
      new tree::SeqStm(
          new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), then->exp_->UnEx()),
          new tree::JumpStm(new tree::NameExp(join), new std::vector<temp::Label *>{join})));
  
  tree::Stm *else_stm = new tree::SeqStm(
      new tree::LabelStm(f),
      new tree::SeqStm(
          new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), elsee->exp_->UnEx()),
          new tree::JumpStm(new tree::NameExp(join), new std::vector<temp::Label *>{join})));

  tree::Stm *join_stm = new tree::SeqStm(
      new tree::LabelStm(join),
      new tree::ExpStm(new tree::TempExp(reg_manager->ReturnValue())));

  return new tr::ExpAndTy(
    new tr::ExExp(new tree::EseqExp(
        cjump,
        new tree::EseqExp(
            then_stm,
            new tree::EseqExp(
                else_stm,
                new tree::EseqExp(
                    join_stm,
                    new tree::TempExp(reg_manager->ReturnValue())
                )
            )
        )
    )),
    then->ty_);

}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *test = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, label, errormsg);

  tree::Stm *test_stm = new tree::SeqStm(
      new tree::LabelStm(test_label),
      new tree::CjumpStm(
          tree::NE_OP, test->exp_->UnEx(), new tree::ConstExp(0), body_label, done_label));

  tree::Stm *body_stm = new tree::SeqStm(
      new tree::LabelStm(body_label),
      new tree::SeqStm(
          body->exp_->UnNx(),
          new tree::JumpStm(new tree::NameExp(test_label), new std::vector<temp::Label *>{test_label})));

  tree::Stm *done_stm = new tree::LabelStm(done_label);

  return new tr::ExpAndTy(
      new tr::NxExp(new tree::SeqStm(test_stm, new tree::SeqStm(body_stm, done_stm))),
      type::VoidTy::Instance());

}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *lo = lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *hi = hi_->Translate(venv, tenv, level, label, errormsg);

  tr::Access *access = tr::Access::AllocLocal(level, false); 
  venv->Enter(var_, new env::VarEntry(access, type::IntTy::Instance()));

  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();

  tree::Stm *init_stm = new tree::MoveStm(
      access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),
      lo->exp_->UnEx());

  tree::Stm *test_stm = new tree::SeqStm(
      new tree::LabelStm(test_label),
      new tree::CjumpStm(
          tree::LE_OP, 
          access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),
          hi->exp_->UnEx(),
          body_label,
          done_label));

  tree::Stm *body_stm = new tree::SeqStm(
      new tree::LabelStm(body_label),
      new tree::SeqStm(
          body_->Translate(venv, tenv, level, label, errormsg)->exp_->UnNx(),
          new tree::SeqStm(
              new tree::MoveStm(
                  access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),
                  new tree::BinopExp(
                      tree::PLUS_OP,
                      access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),
                      new tree::ConstExp(1))),
              new tree::JumpStm(new tree::NameExp(test_label), new std::vector<temp::Label *>{test_label}))));

  tree::Stm *done_stm = new tree::LabelStm(done_label);

  return new tr::ExpAndTy(
      new tr::NxExp(new tree::SeqStm(
          init_stm,
          new tree::SeqStm(
              test_stm,
              new tree::SeqStm(
                  body_stm,
                  done_stm)))),
      type::VoidTy::Instance());
 
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
    new tr::NxExp(new tree::JumpStm(new tree::NameExp(label), new std::vector<temp::Label *>{label})),
    type::VoidTy::Instance());

}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tree::Stm *stm = nullptr;
  
  for (Dec *dec : decs_->GetList()) {
    tr::Exp *exp = dec->Translate(venv, tenv, level, label, errormsg);
    if (stm == nullptr) {
      stm = exp->UnNx();
    } else {
      stm = new tree::SeqStm(stm, exp->UnNx());
    }
  }

  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, label, errormsg);
  
  if (stm == nullptr) {
    return body;
  } else {
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(stm, body->exp_->UnEx())),
        body->ty_);
  }

}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::IntTy::Instance());
  }

  auto array_ty = static_cast<type::ArrayTy *>(ty->ActualTy());
  tr::ExpAndTy *size = size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init = init_->Translate(venv, tenv, level, label, errormsg);

  tree::ExpList *args = new tree::ExpList();
  args->Append(size->exp_->UnEx());
  args->Append(init->exp_->UnEx());

  tree::Exp *call_exp = new tree::CallExp(
    new tree::NameExp(temp::LabelFactory::NamedLabel("init_array")),
    args);
  
  return new tr::ExpAndTy(
      new tr::ExExp(call_exp),
      array_ty);

}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());

}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  for (FunDec *fundec : functions_->GetList()) {
    type::TyList *formals = fundec->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *result = fundec->result_ ? tenv->Look(fundec->result_) : type::VoidTy::Instance();

    temp::Label *fun_label = temp::LabelFactory::NamedLabel(fundec->name_->Name());

    std::list<bool> escapes;  
    for (auto param : fundec->params_->GetList()) {
      escapes.push_back(false);  
    }
    tr::Level *fun_level = tr::Level::NewLevel(level, fun_label, escapes);

    venv->Enter(fundec->name_, new env::FunEntry(fun_level, fun_label, formals, result));
  }

  for (FunDec *fundec : functions_->GetList()) {
    env::EnvEntry *entry = venv->Look(fundec->name_);
    auto fun_entry = static_cast<env::FunEntry *>(entry);

    venv->BeginScope();

    auto formal_it = fundec->params_->GetList().begin();
    auto access_it = fun_entry->level_->frame_->Formals()->begin();
    for (; formal_it != fundec->params_->GetList().end(); ++formal_it, ++access_it) {
      venv->Enter((*formal_it)->name_, 
                 new env::VarEntry(new tr::Access(fun_entry->level_, *access_it), 
                 (*formal_it)->typ_ ? tenv->Look((*formal_it)->typ_) : type::IntTy::Instance()));
    }

    tr::ExpAndTy *body = fundec->body_->Translate(venv, tenv, fun_entry->level_, fun_entry->label_, errormsg);

    venv->EndScope();

    frame::ProcFrag *frag = ProcEntryExit(fun_entry->level_, body->exp_);
    frags->PushBack(frag);
  }

  return new tr::ExExp(new tree::ConstExp(0));

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
  for (NameAndTy *ty : types_->GetList()) {
    tenv->Enter(ty->name_, new type::NameTy(ty->name_, nullptr));
  }

  for (NameAndTy *ty : types_->GetList()) {
    type::Ty *ty_ty = tenv->Look(ty->name_);
    auto name_ty = static_cast<type::NameTy *>(ty_ty);
    name_ty->ty_ = ty->ty_->Translate(tenv, errormsg);
  }

  for (NameAndTy *ty : types_->GetList()) {
    type::Ty *ty_ty = tenv->Look(ty->name_);
    if (ty_ty->ActualTy() == ty_ty) {
      errormsg->Error(pos_, "illegal type cycle");
    }
  }

  return new tr::ExExp(new tree::ConstExp(0));
 
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::NameTy(name_, ty);

}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::FieldList *fields = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fields);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn
