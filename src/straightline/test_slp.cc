#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "straightline/prog.h"

int main(int argc, char **argv) {
  int args;
  int test_num;

  assert(argc == 2);
  test_num = atoi(argv[1]);

  switch (test_num) {
  case CASE_A:
    printf("SimpleAssign\n");
    args = SimpleAssign()->MaxArgs();
    printf("args: %d\n", args);
    SimpleAssign()->Interp(nullptr);
    break;
  case CASE_B:
    printf("SimplePrint\n");
    args = SimplePrint()->MaxArgs();
    printf("args: %d\n", args);
    SimplePrint()->Interp(nullptr);
    break;
  case CASE_C:
    printf("DoubleAssign\n");
    args = DoubleAssign()->MaxArgs();
    printf("args: %d\n", args);
    DoubleAssign()->Interp(nullptr);
    break;
  case CASE_D:
    printf("DoublePrint\n");
    args = DoublePrint()->MaxArgs();
    printf("args: %d\n", args);
    DoublePrint()->Interp(nullptr);
    break;
  case CASE_E:
    printf("Prog\n");
    args = Prog()->MaxArgs();
    printf("args: %d\n", args);
    Prog()->Interp(nullptr);
    break;
  case CASE_F:
    printf("ProgProg\n");
    args = ProgProg()->MaxArgs();
    printf("args: %d\n", args);
    ProgProg()->Interp(nullptr);
    break;
  case CASE_G:
    printf("RightProg\n");
    args = RightProg()->MaxArgs();
    printf("args: %d\n", args);
    RightProg()->Interp(nullptr);
    break;
  default:
    printf("unexpected case\n");
    exit(-1);
  }
  return 0;
}
