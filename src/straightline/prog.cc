#include "straightline/prog.h"
#include "straightline/slp.h"

// a := 1 / 2;
A::Stm *SimpleAssign() {
  // a := 1 / 2;
  A::Stm *ass_stm = new A::AssignStm(
      "a", new A::OpExp(new A::NumExp(1), A::DIV, new A::NumExp(2)));
  return ass_stm;
}

// print(5201314);
A::Stm *SimplePrint() {
  // print(5201314);
  A::Stm *prt_stm = new A::PrintStm(new A::LastExpList(new A::NumExp(5201314)));
  return prt_stm;
}

// x := 1 + 1 + 2 + 3 + 5; y := 1 * 2 * 4 / 8;
A::Stm *DoubleAssign() {
  // x := 1 + 1 + 2 + 3 + 5;
  A::Stm *ass_stm_x = new A::AssignStm(
      "x", new A::OpExp(
               new A::OpExp(new A::OpExp(new A::OpExp(new A::NumExp(1), A::PLUS,
                                                      new A::NumExp(1)),
                                         A::PLUS, new A::NumExp(2)),
                            A::PLUS, new A::NumExp(3)),
               A::PLUS, new A::NumExp(5)));

  // y := 1 * 2 * 4 / 8;
  A::Stm *ass_stm_y = new A::AssignStm(
      "y", new A::OpExp(new A::OpExp(new A::OpExp(new A::NumExp(1), A::TIMES,
                                                  new A::NumExp(2)),
                                     A::TIMES, new A::NumExp(4)),
                        A::DIV, new A::NumExp(8)));

  return new A::CompoundStm(ass_stm_x, ass_stm_y);
}

// print(255); print(512, 1314);
A::Stm *DoublePrint() {
  // print(255);
  A::Stm *prt_stm_255 = new A::PrintStm(new A::LastExpList(new A::NumExp(255)));

  // print(512, 1314);
  A::PairExpList *exp_list_num_pair = new A::PairExpList(
      new A::NumExp(512), new A::LastExpList(new A::NumExp(1314)));

  A::Stm *prt_stm_pair = new A::PrintStm(exp_list_num_pair);

  return new A::CompoundStm(prt_stm_255, prt_stm_pair);
}

// a := 5 + 3; b := (print(a, a - 1), 10 * a); print b;
A::Stm *Prog() {
  // a := 5 + 3;
  A::Stm *ass_stm_a = new A::AssignStm(
      "a", new A::OpExp(new A::NumExp(5), A::PLUS, new A::NumExp(3)));

  // b := (print(a, a - 1), 10 * a);
  A::Exp *op_exp_a_minus_1 =
      new A::OpExp(new A::IdExp("a"), A::MINUS, new A::NumExp(1));

  A::PairExpList *exp_list_pair = new A::PairExpList(
      new A::IdExp("a"), new A::LastExpList(op_exp_a_minus_1));
  A::Stm *prt_stm_pair = new A::PrintStm(exp_list_pair);

  A::Exp *op_exp_10_mul_a =
      new A::OpExp(new A::NumExp(10), A::TIMES, new A::IdExp("a"));
  A::Stm *ass_stm_b =
      new A::AssignStm("b", new A::EseqExp(prt_stm_pair, op_exp_10_mul_a));

  // print b;
  A::Stm *prt_stm_b = new A::PrintStm(new A::LastExpList(new A::IdExp("b")));

  // b := (print(a, a - 1), 10 * a); print b;
  A::Stm *com_stm = new A::CompoundStm(ass_stm_b, prt_stm_b);

  return new A::CompoundStm(ass_stm_a, com_stm);
}

// a := 5 + 3; b := (print(a, a - 1), 10 * a); print b;
// a := 5 + b; b := (print(a, a, a - 1), 10 * a); print b;
A::Stm *ProgProg() {
  // a := 5 + 3; b := (print(a, a - 1), 10 * a); print b;
  A::Stm *stm1 = Prog();

  // a := 5 + b;
  A::Stm *ass_stm_a = new A::AssignStm(
      "a", new A::OpExp(new A::NumExp(5), A::PLUS, new A::IdExp("b")));

  // print(a, a, a-1)
  A::Exp *op_exp_a_minus_1 =
      new A::OpExp(new A::IdExp("a"), A::MINUS, new A::NumExp(1));

  A::PairExpList *triple_explist = new A::PairExpList(
      new A::IdExp("a"),
      new A::PairExpList(new A::IdExp("a"),
                         new A::LastExpList(op_exp_a_minus_1)));

  // 10 * a
  A::Exp *op_exp_10_mul_a =
      new A::OpExp(new A::NumExp(10), A::TIMES, new A::IdExp("a"));

  // b := (print(a, a, a - 1), 10 * a);
  A::Stm *ass_stm_b = new A::AssignStm(
      "b", new A::EseqExp(new A::PrintStm(triple_explist), op_exp_10_mul_a));

  // print b;
  A::Stm *prt_stm_b = new A::PrintStm(new A::LastExpList(new A::IdExp("b")));

  return new A::CompoundStm(
      stm1,
      new A::CompoundStm(ass_stm_a, new A::CompoundStm(ass_stm_b, prt_stm_b)));
}

// a := 5 + 3; b := (print(a, a - 1), 10 * a); print b;
// a := 5 + b; b := (print(a, a, a - 1), 10 * a); print b;
// a := (a := a + b, a);
A::Stm *RightProg() {
  A::Stm *stm1 = ProgProg();
  return new A::CompoundStm(
      stm1,
      new A::AssignStm(
          "a", new A::EseqExp(new A::AssignStm(
                                  "a", new A::OpExp(new A::IdExp("a"), A::PLUS,
                                                    new A::IdExp("b"))),
                              new A::IdExp("a"))));
}
