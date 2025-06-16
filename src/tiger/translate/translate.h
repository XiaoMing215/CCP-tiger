#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include <list>
#include <memory>

#include "tiger/absyn/absyn.h"
#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/semant/types.h"

namespace tr {

class Exp;
class ExpAndTy;
class Level;

class PatchList { //处理跳转指令中未确定目标的标签列表
public:
  void DoPatch(temp::Label *label) {
    for (auto &patch : patch_list_)
      *patch = label;
  }

  static PatchList JoinPatch(const PatchList &first, const PatchList &second) {
    PatchList ret(first.GetList());
    for (auto &patch : second.patch_list_) {
      ret.patch_list_.push_back(patch);
    }
    return ret;
  }

  explicit PatchList(std::list<temp::Label **> patch_list)
      : patch_list_(patch_list) {}
  PatchList() = default;

  [[nodiscard]] const std::list<temp::Label **> &GetList() const {
    return patch_list_;
  }

private:
  std::list<temp::Label **> patch_list_;
};

//封装变量的访问方式（存在栈帧中，还是寄存器中）
class Access {
public:
  Level *level_;  
  frame::Access *access_; 

  Access(Level *level, frame::Access *access)
      : level_(level), access_(access) {}

  static Access *AllocLocal(Level *level, bool escape);

  tree::Exp *ToExp(Level *currentLevel);
};

// 表示静态嵌套函数的层级结构。
class Level {
public:
  frame::Frame *frame_;
  Level *parent_;

  Level(Level *parent, temp::Label *name,
        std::list<bool> formals);
  Level(frame::Frame *frame, Level *parent) : frame_(frame), parent_(parent) {}

  std::list<Access *> *Formals();

  // Get the static link from current level to the target level
  tree::Exp *StaticLink(Level *targetLevel);
};

//翻译主类
class ProgTr {
public:
  // TODO: Put your lab5 code here */
  ProgTr(std::unique_ptr<absyn::AbsynTree> absyn_tree,
         std::unique_ptr<err::ErrorMsg> errormsg)
      : absyn_tree_(std::move(absyn_tree)), errormsg_(std::move(errormsg)),
        tenv_(std::make_unique<env::TEnv>()),
        venv_(std::make_unique<env::VEnv>()){};

  void Translate();

  std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }

private:
  std::unique_ptr<absyn::AbsynTree> absyn_tree_;
  std::unique_ptr<err::ErrorMsg> errormsg_;
  std::unique_ptr<Level> main_level_;
  std::unique_ptr<env::TEnv> tenv_;
  std::unique_ptr<env::VEnv> venv_;

  void FillBaseVEnv();
  void FillBaseTEnv();
};

} // namespace tr

#endif
