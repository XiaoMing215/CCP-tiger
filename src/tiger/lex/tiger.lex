%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */


digit [0-9]
letter [a-zA-Z]

%x COMMENT STR IGNORE
//%x创建了一个独立状态 这三者也是我们需要实现的大菜

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* reserved words */
"array" {adjust(); return Parser::ARRAY;}


 /* TODO: Put your lab2 code here */

"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"do" {adjust(); return Parser::DO;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"of" {adjust(); return Parser::OF;}
"break" {adjust(); return Parser::BREAK;}
"nil" {adjust(); return Parser::NIL;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}

{letter}({letter}|{digit}|_)* { adjust(); return Parser::ID; }


//adjustStr 似乎指明了字符串的开始位置
//adjust需要把头尾的”识别
//可以使用char_position位置调试 
//adjuststr和普通的只有报错与否区别 怀疑str版本为了处理\n诞生
//string_buf_用于承接读取到的串 可以使用c代码对其操作
//more 是接受目前匹配的结果 并需求下一个 
//两个adjust函数会更新char_pos 这是标记每个词在哪个位置的指针 在处理\n类时，输入两个字符却在matched当中只“输出”一个 因此要微调
//matched用于查看目前匹配到的结果 setmatched用于设置。注意改变内容时不可全部清空之前接受的match

\"  {
        adjust();
        begin(StartCondition__::STR);
    } //asjust的长度为一 只负责双引号

<STR>{
\"  {   //结束标志
        //std::cout<<char_pos_;
        //std::cout<<" ";

        std::string str = matched();  // 获取完整的匹配结果
        str = str.substr(0, str.size() - 1);  // 去掉两端的引号
        setMatched(str);
        adjustStr();//长度为加上后半个双引号的字符串长度
        char_pos_++;
        begin(StartCondition__::INITIAL);
        return Parser::STRING;
    }
}

<STR>\\[0-9][0-9][0-9] {
    //匹配\ddd类型
    more();
    string_buf_ = matched();
    int offset = string_buf_.size() - 3;
    std::string tmp = string_buf_.substr(offset,string_buf_.size());
    string_buf_ = string_buf_.substr(0, string_buf_.size() - 4);
    char c = std::stoi(tmp);
    string_buf_+= c;
    setMatched(string_buf_);
    char_pos_+=3; //四个字符只代表了一个字符的值 人为干预
}

<STR> \\[ \t\n] { //匹配\f___f\
    string_buf_ = matched();
    //std::cout<<char_pos_;
    string_buf_ = string_buf_.substr(0, string_buf_.size() - 2);
    char_pos_-=1;//斜杠被读到但应该被忽略
    begin(StartCondition__::IGNORE);
    } 

<STR> \\\^[@A-Z[\]^_] { //匹配\^c类 其中c只能是A-Z
    string_buf_ = matched();
    int offset = string_buf_.size() - 1;
    char tmp = string_buf_[offset]; //要处理的字符
    string_buf_ = string_buf_.substr(0, string_buf_.size() - 3); //原先的匹配内容
    int asciinum = tmp - 'A' + 1 ;
    if(1<= asciinum && 26>= asciinum)
    {
        char real = asciinum;
        string_buf_ += real;
        //std::cout<<asciinum; 
        setMatched(string_buf_);
        char_pos_+=2; //三位表示一位 即：写作三位 但表示只有一位 adjust缺两位
    }
    else
    {
        string_buf_ = string_buf_ + "\\^" + tmp ; //这是除了A-Z之外的情况 
        setMatched(string_buf_);
    }
    more();

}
        

<STR>\\[^\n]   {  //没有搞懂 在这里处理后对于adjust的用处 我知道我少录了一项 可是这里调整最后被哪个adjust承接？
    string_buf_ = matched();  // 获取当前匹配的转义字符
    int offset = string_buf_.size() - 1;
    char escape = string_buf_[offset];  // 获取转义字符 但是前面的也被获取和更改了！
    string_buf_ = string_buf_.substr(0, string_buf_.size() - 2);//保留前端
    switch (escape) {
        case 'n': string_buf_+= "\n"; setMatched(string_buf_); break;  // 转换为换行符
        case 't': string_buf_+= "\t"; setMatched(string_buf_); break;  // 转换为制表符
        case '"': string_buf_+= "\""; setMatched(string_buf_); break;  // 转换为双引号
        case '\\': string_buf_+= "\\"; setMatched(string_buf_); break;  // 转换为反斜杠
        default:  string_buf_+=escape;setMatched(string_buf_); break;  // 其他转义字符 还没改！！！
    }
    
    char_pos_++;//转义字符只被当作一个 但在文中是两个
    more();  // 继续累加后续字符
}

<STR>[^\\\"\n]+ { more();}  // 普通匹配 继续累加后续字符
    //这里不可以adjust这是因为adjust把目前为止读到的加在了char上

<IGNORE>{ //处理\f___f当中内容
    \\ {
        setMatched(string_buf_);//将前面半段拿回来
        begin(StartCondition__::STR);
        adjust();
        char_pos_--; //应该是末尾的\要去掉
        more();
        }

    [ \t\n] {};
}


"/*"  {adjust();comment_level_=1;begin(StartCondition__::COMMENT);}  //进入是成功的

<COMMENT>{
"*/"  {
            comment_level_--;
            if(comment_level_==0)
            {
                adjust();
                //std::cout<<matched();
                begin(StartCondition__::INITIAL);
            }
            else
            {
                more();
            }
        }

"/*"  {comment_level_++;more();}

\n {more();}

\\.|. {more();}

}



"//"[^\n]*\n {adjust();} //单行注释只需要一个表达式 ignore不是给他用的


[0-9]+ {adjust(); return Parser::INT;}
"," {adjust(); return Parser::COMMA;}
":" {adjust(); return Parser::COLON;}
";" {adjust(); return Parser::SEMICOLON;}
"(" {adjust(); return Parser::LPAREN;}
")" {adjust(); return Parser::RPAREN;}
"[" {adjust(); return Parser::LBRACK;}
"]" {adjust(); return Parser::RBRACK;}
"{" {adjust(); return Parser::LBRACE;}
"}" {adjust(); return Parser::RBRACE;}
"." {adjust(); return Parser::DOT;}
"+" {adjust(); return Parser::PLUS;}
"-" {adjust(); return Parser::MINUS;}
"*" {adjust(); return Parser::TIMES;}
"/" {adjust(); return Parser::DIVIDE;}
"=" {adjust(); return Parser::EQ;}
"<>" {adjust(); return Parser::NEQ;}
"<" {adjust(); return Parser::LT;}
"<=" {adjust(); return Parser::LE;}
">" {adjust(); return Parser::GT;}
">=" {adjust(); return Parser::GE;}
"&" {adjust(); return Parser::AND;}
"|" {adjust(); return Parser::OR;}
":=" {adjust(); return Parser::ASSIGN;}


 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
