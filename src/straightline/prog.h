#ifndef STRAIGHTLINE_PROG1_H_
#define STRAIGHTLINE_PROG1_H_

#include "straightline/slp.h"

constexpr int CASE_A = 0;
constexpr int CASE_B = 1;
constexpr int CASE_C = 2;
constexpr int CASE_D = 3;
constexpr int CASE_E = 4;
constexpr int CASE_F = 5;
constexpr int CASE_G = 6;


A::Stm *SimpleAssign();
A::Stm *SimplePrint();
A::Stm *DoubleAssign();
A::Stm *DoublePrint();
A::Stm *Prog();
A::Stm *ProgProg();
A::Stm *RightProg();


#endif // STRAIGHTLINE_PROG1_H_
