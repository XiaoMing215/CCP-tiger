%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */

%union {
  int ival;
  std::string* sval;
  sym::Symbol *sym;
  absyn::Exp *exp;
  absyn::ExpList *explist;
  absyn::Var *var;
  absyn::DecList *declist;
  absyn::Dec *dec;
  absyn::EFieldList *efieldlist;
  absyn::EField *efield;
  absyn::NameAndTyList *tydeclist;
  absyn::NameAndTy *tydec;
  absyn::FieldList *fieldlist;
  absyn::Field *field;
  absyn::FunDecList *fundeclist;
  absyn::FunDec *fundec;
  absyn::Ty *ty;
  }

%token <sym> ID
%token <sval> STRING
%token <ival> INT

/* %token [ <type> ] terminal token(s) */
/* %token 尖括号内接的是union当中定义的 */
/* union全部被token和type分干净了但是！！还可以创建新的 4.5.29 */
/* 有很多没见过的explist 和 declist的关系？使用type申明因为他们是非终结符*/

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF
  BREAK NIL
  FUNCTION VAR TYPE

 /* token priority */
 /* TODO: Put your lab3 code here */
/*申明左右结合规则 并排序优先级。优先级越高越在后面*/
/*上面的token可能不全 PLUS等都不在？*/
/*没错。token当中没有被定义的都是需要left/right定义的。有谁？*/
/*PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE AND OR*/

%left OR
%left AND
%left EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE


%type <exp> exp expseq opexp ifexp whileexp callexp recordexp
%type <explist> actuals nonemptyactuals sequencing sequencing_exps
%type <var> lvalue one oneormore
%type <declist> decs decs_nonempty
%type <dec> decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec
%type <tydec> tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <field> tyfield
%type <ty> ty
%type <fundeclist> fundec
%type <fundec> fundec_one
/*type申明非终结符*/


%start program
/*若没有上面这一行，默认启动符是文法文件中第一个非空的规则*/

%%
program:  exp  {absyn_tree_ = std::make_unique<absyn::AbsynTree>($1);};

/* TODO: Put your lab3 code here */
/* $1表示产生式规则的第一个；exp '+' exp{ $$ = $1 + $3;}*/
/* | 用于分割两条规则*/
/*%type、%left 和 %right 都是用于声明的指令*/

/*<fundeclist>fundec <fundec>fundec_one */
/*只要有可能使用左递归 如：expseq1 ',' exp*/

/*lets go!*/

decs:
  decs_nonempty_s decs {if($2)$$=$2->Prepend($1);else $$ = new absyn::DecList($1);}
| //empty
;
//decs_nonempty_s 是Dec类型的 才能符合Declist的append

decs_nonempty:
  decs_nonempty_s decs {if($2) $$ = $2->Prepend($1); else $$ = new absyn::DecList($1);}
;

decs_nonempty_s:
  tydec {$$ = new absyn::TypeDec(scanner_.GetTokPos(),$1);}
| vardec {$$ = $1;}
| fundec {$$ = new absyn::FunctionDec(scanner_.GetTokPos(),$1);}
;

/*id与typeid不可混用 需要{}当中定义规则找到符合的typeid*/


tydec:
  tydec_one {$$ = new absyn::NameAndTyList($1);} //不能直接使用$$ = $1 因为返回类型不同
| tydec_one tydec {if($2) $$=$2->Prepend($1);else $$ = new absyn::NameAndTyList($1);}
|  //empty
;
//tydec 是 tydeclist类型的 这才有append
//$$也是tydeclist类型的 返回的是NameAndTyList类

tydec_one:
  TYPE ID EQ ty {$$ = new absyn::NameAndTy($2,$4);} 
;

//NameAndTy两个参数分别为symbol和ty

ty:
  ID {$$ = new absyn::NameTy(scanner_.GetTokPos(),$1);}
| LBRACE tyfields_nonempty RBRACE {$$ = new absyn::RecordTy(scanner_.GetTokPos(),$2);}
| ARRAY OF ID {$$ = new absyn::ArrayTy(scanner_.GetTokPos(),$3);}
;

//return Ty类？ ty是一个抽象类:该返回子类

tyfields:
  {$$ = new absyn::FieldList();} //empty
| tyfield {$$ = new absyn::FieldList($1);}
| tyfield COMMA tyfields {$$=$3->Prepend($1);}
;

tyfields_nonempty:
  tyfield COMMA tyfields {$$ = $3->Prepend($1);}
| tyfield {$$ = new absyn::FieldList($1);}
;
//Fieldlist双大写

//有:的需要保证前面不空
tyfield:
  ID COLON ID {$$ = new absyn::Field(scanner_.GetTokPos(),$1,$3);}
;

vardec:
  VAR ID ASSIGN exp {$$ = new absyn::VarDec(scanner_.GetTokPos(),$2,nullptr,$4);}
| VAR ID COLON ID ASSIGN exp {$$ = new absyn::VarDec(scanner_.GetTokPos(),$2,$4,$6);}
;
//nullptr_t专指一个指向 nullptr 的值的类型

fundec: //不会为空！他在decs_nonempty_s下
 fundec_one fundec {if($2) $$ = $2->Prepend($1); else $$ = new absyn::FunDecList($1);}
| //empty
;

fundec_one:
  FUNCTION ID LPAREN tyfields RPAREN EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(),$2,$4,nullptr,$7);}
| FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(),$2,$4,$7,$9);}
;

lvalue: //var类型
  ID {$$ = new absyn::SimpleVar(scanner_.GetTokPos(),$1);}
| lvalue DOT ID { $$ = new absyn::FieldVar(scanner_.GetTokPos(),$1,$3);}
| lvalue LBRACK exp RBRACK  { $$ = new absyn::SubscriptVar(scanner_.GetTokPos(), $1, $3); }
| one {$$ = $1;}
;
//one : 解决 a[10] of 1

one:
  ID LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), new absyn::SimpleVar(scanner_.GetTokPos(), $1),$3);}
;

exp://lab5-1新增三个：和minus exp
    lvalue {$$ = new absyn::VarExp(scanner_.GetTokPos(),$1);}                    
  | NIL {$$ = new absyn::NilExp(scanner_.GetTokPos());} 
  | MINUS exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::MINUS_OP,new absyn::IntExp(scanner_.GetTokPos(),0),$2); }                      
  | exp AND exp {$$ = new absyn::OpExp(scanner_.GetTokPos(),absyn::AND_OP,$1,$3);}
  | INT { $$ = new absyn::IntExp(scanner_.GetTokPos(),$1); }                        
  | STRING { $$ = new absyn::StringExp(scanner_.GetTokPos(),$1); }                     
  | callexp {$$ = $1;}
  | opexp {$$ = $1;}
  | recordexp {$$ = $1;}    
  | LPAREN sequencing_exps RPAREN {$$ = new absyn::SeqExp(scanner_.GetTokPos(),$2);}           
  | lvalue ASSIGN exp {$$ = new absyn::AssignExp(scanner_.GetTokPos(),$1,$3);}
  | ifexp {$$ = $1;} 
  | whileexp {$$ = $1;}
  | FOR ID ASSIGN exp TO exp DO exp {$$ = new absyn::ForExp(scanner_.GetTokPos(),$2,$4,$6,$8);}
  | BREAK { $$ = new absyn::BreakExp(scanner_.GetTokPos()); }                   
  | LET decs_nonempty IN sequencing_exps END { $$ = new absyn::LetExp(scanner_.GetTokPos(),$2,new absyn::SeqExp(scanner_.GetTokPos(),$4)); } 
  | LPAREN exp RPAREN {$$ = $2;} //外包括号
  | one {$$ = new absyn::VarExp(scanner_.GetTokPos(),$1);}
  | {$$ = new absyn::VoidExp(scanner_.GetTokPos());}//empty
  | one OF exp {auto scriptvar = static_cast<absyn::SubscriptVar*>($1);
    auto simplevar = static_cast<absyn::SimpleVar*>(scriptvar->var_);
    $$ = new absyn::ArrayExp(scanner_.GetTokPos(),simplevar->sym_,scriptvar->subscript_,$3);}
  ;
  //最后一个操作是arr[10] of 0 类型；
  //one 是数组的取一个的操作；
  //然而在 one OF exp 里不是要访问数组元素 是要创建；
  //$1 是Var类型，需要人为指定 SubscriptVar就是指数组下标；
  //ArrayExp(int pos, sym::Symbol *typ, Exp *size, Exp *init) 数据类型（来自scriptvar当中的“数组名”再找到其所属属性“变量名”），数组大小（来自scriptvar当中存储的下标值），初始化数值（来自exp）


opexp:
    exp PLUS exp     { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::PLUS_OP, $1, $3); }
  | exp MINUS exp    { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::MINUS_OP, $1, $3); }
  | exp TIMES exp    { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::TIMES_OP, $1, $3); }
  | exp DIVIDE exp   { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::DIVIDE_OP, $1, $3); }
  | exp EQ exp       { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::EQ_OP, $1, $3); }
  | exp NEQ exp      { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::NEQ_OP, $1, $3); }
  | exp LT exp       { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::LT_OP, $1, $3); }
  | exp LE exp       { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::LE_OP, $1, $3); }
  | exp GT exp       { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::GT_OP, $1, $3); }
  | exp GE exp       { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::GE_OP, $1, $3); }
  | exp AND exp      { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::AND_OP, $1, $3); }
  | exp OR exp       { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::OR_OP, $1, $3); }
;
  
ifexp:
  IF exp THEN exp {$$ = new absyn::IfExp(scanner_.GetTokPos(),$2,$4,nullptr);}
| IF exp THEN exp ELSE exp {$$ = new absyn::IfExp(scanner_.GetTokPos(),$2,$4,$6);}
;

whileexp:
  WHILE exp DO exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(),$2,$4);}
;

callexp:
  ID LPAREN actuals RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(),$1,$3);}
| ID LPAREN RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(),$1,new absyn::ExpList());} //无参数的函数调用
;

recordexp:
  ID LBRACE rec RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(),$1,$3);}
;

rec:
 {$$ = new absyn::EFieldList();}//empty
| rec_nonempty {$$ = $1;}
;
// rec_nonempty 和 rec是同类型的

rec_nonempty:
  rec_one COMMA rec {if($3) $$ = $3->Prepend($1); else $$ = new absyn::EFieldList($1); }
| rec_one {$$ = new absyn::EFieldList($1);}
;

rec_one : ID EQ exp {$$ = new absyn::EField($1,$3);};

//actuals 是函数参数
actuals: 
  nonemptyactuals {$$ = $1;}
| {$$ = new absyn::ExpList();}
;

nonemptyactuals:
  exp COMMA actuals {$$ = $3->Prepend($1);}
| exp {$$ = new absyn::ExpList($1);}
;

sequencing_exps : 
  exp SEMICOLON sequencing  {$$ = $3->Prepend($1);}
| exp {$$ = new absyn::ExpList($1);}
;

sequencing : 
  exp SEMICOLON sequencing {$$ = $3->Prepend($1);}
| exp {$$ = new absyn::ExpList($1);}
| {$$ = new absyn::ExpList();}
;