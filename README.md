# ast-interpreter

## 具体要求：

```
变量类型 : int | char | void | *
运算符号: * | + | - | * | / | &lt; | &gt; | == | = | [ ] 
语句：IfStmt | WhileStmt | ForStmt | DeclStmt
Expr：BinaryOperator | UnaryOperator | DeclRefExpr | CallExpr | CastExpr
```

我们还需要支持4个外部函数int GET（），void * MALLOC（int），void FREE（void *），void PRINT（int），这4个函数的语义是self-explanatory。

提供了一个kelton实现ast-interpreter.tgz，欢迎您对实现进行任何更改。 提供的实现能够解释以下简单程序：

```
 extern int GET();
 extern void * MALLOC(int);
 extern void FREE(void *);
 extern void PRINT(int);

 int main() {
    int a;
    a = GET();
    PRINT(a);
 }
```

我们为接受的语言提供了更正式的定义，如下所示：

```
 Program : DeclList
 DeclList : Declaration DeclList | empty
 Declaration : VarDecl FuncDecl
 VarDecl : Type VarList;
 Type : BaseType | QualType
 BaseType : int | char | void
 QualType : Type * 
 VarList : ID, VarList |  | ID[num], VarList | emtpy
 FuncDecl : ExtFuncDecl | FuncDefinition
 ExtFuncDecl : extern int GET(); | extern void * MALLOC(int); | extern void FREE(void *); | extern void PRINT(int);
 FuncDefinition : Type ID (ParamList) { StmtList }
 ParamList : Param, ParamList | empty
 Param : Type ID
 StmtList : Stmt, StmtList | empty
 Stmt : IfStmt | WhileStmt | ForStmt | DeclStmt | CompoundStmt | CallStmt | AssignStmt | 
 IfStmt : if (Expr) Stmt | if (Expr) Stmt else Stmt
 WhileStmt : while (Expr) Stmt
 DeclStmt : Type VarList;
 AssignStmt : DeclRefExpr = Expr;
 CallStmt : CallExpr;
 CompoundStmt : { StmtList }
 ForStmt : for ( Expr; Expr; Expr) Stmt
 Expr : BinaryExpr | UnaryExpr | DeclRefExpr | CallExpr | CastExpr | ArrayExpr | DerefExpr | (Expr) | num
 BinaryExpr : Expr BinOP Expr
 BinaryOP : + | - | * | / | &lt; | &gt; | ==
 UnaryExpr : - Expr
 DeclRefExpr : ID
 CallExpr : DeclRefExpr (ExprList)
 ExprList : Expr, ExprList | empty
 CastExpr : (Type) Expr
 ArrayExpr : DeclRefExpr [Expr]
 DerefExpr : * DeclRefExpr
```