extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int b=10;
int f(int x) {
  int a=100;
  if (x > 0)
  	return 6 + f(x-5);
  else 
    return 0;
}
int main() {
	int c;
	int a;
	c = 100;
	a = f(b);
	PRINT(a);
}

// |-FunctionDecl 0x55f80ff189e0 <test00.c:1:1, col:16> col:12 GET 'int ()' extern
// |-FunctionDecl 0x55f80ff18bc0 <line:2:1, col:25> col:15 MALLOC 'void *(int)' extern
// | `-ParmVarDecl 0x55f80ff18af8 <col:22> col:25 'int'
// |-FunctionDecl 0x55f80ff18d58 <line:3:1, col:24> col:13 FREE 'void (void *)' extern
// | `-ParmVarDecl 0x55f80ff18c98 <col:18, col:23> col:24 'void *'
// |-FunctionDecl 0x55f80ff18ef8 <line:4:1, col:22> col:13 used PRINT 'void (int)' extern
// | `-ParmVarDecl 0x55f80ff18e30 <col:19> col:22 'int'
// |-VarDecl 0x55f80ff18fd0 <line:6:1, col:7> col:5 used b 'int' cinit
// | `-IntegerLiteral 0x55f80ff19038 <col:7> 'int' 10
// |-FunctionDecl 0x55f80ff19170 <line:7:1, line:12:1> line:7:5 used f 'int (int)'
// | |-ParmVarDecl 0x55f80ff19088 <col:7, col:11> col:11 used x 'int'
// | `-CompoundStmt 0x55f80ff19450 <col:14, line:12:1>
// |   `-IfStmt 0x55f80ff19428 <line:8:3, line:11:12> has_else
// |     |-BinaryOperator 0x55f80ff19288 <line:8:7, col:11> 'int' '>'
// |     | |-ImplicitCastExpr 0x55f80ff19270 <col:7> 'int' <LValueToRValue>
// |     | | `-DeclRefExpr 0x55f80ff19230 <col:7> 'int' lvalue ParmVar 0x55f80ff19088 'x' 'int'
// |     | `-IntegerLiteral 0x55f80ff19250 <col:11> 'int' 0
// |     |-ReturnStmt 0x55f80ff193e8 <line:9:4, col:22>
// |     | `-BinaryOperator 0x55f80ff193c8 <col:11, col:22> 'int' '+'
// |     |   |-IntegerLiteral 0x55f80ff192a8 <col:11> 'int' 5
// |     |   `-CallExpr 0x55f80ff193a0 <col:15, col:22> 'int'
// |     |     |-ImplicitCastExpr 0x55f80ff19388 <col:15> 'int (*)(int)' <FunctionToPointerDecay>
// |     |     | `-DeclRefExpr 0x55f80ff192c8 <col:15> 'int (int)' Function 0x55f80ff19170 'f' 'int (int)'
// |     |     `-BinaryOperator 0x55f80ff19340 <col:17, col:21> 'int' '-'
// |     |       |-ImplicitCastExpr 0x55f80ff19328 <col:17> 'int' <LValueToRValue>
// |     |       | `-DeclRefExpr 0x55f80ff192e8 <col:17> 'int' lvalue ParmVar 0x55f80ff19088 'x' 'int'
// |     |       `-IntegerLiteral 0x55f80ff19308 <col:21> 'int' 5
// |     `-ReturnStmt 0x55f80ff19418 <line:11:5, col:12>
// |       `-IntegerLiteral 0x55f80ff193f8 <col:12> 'int' 0
// `-FunctionDecl 0x55f80ff19490 <line:13:1, line:15:1> line:13:5 main 'int ()'
//   `-CompoundStmt 0x55f80ff19668 <col:12, line:15:1>
//     `-CallExpr 0x55f80ff19640 <line:14:4, col:14> 'void'
//       |-ImplicitCastExpr 0x55f80ff19628 <col:4> 'void (*)(int)' <FunctionToPointerDecay>
//       | `-DeclRefExpr 0x55f80ff19548 <col:4> 'void (int)' Function 0x55f80ff18ef8 'PRINT' 'void (int)'
//       `-CallExpr 0x55f80ff195c0 <col:10, col:13> 'int'
//         |-ImplicitCastExpr 0x55f80ff195a8 <col:10> 'int (*)(int)' <FunctionToPointerDecay>
//         | `-DeclRefExpr 0x55f80ff19568 <col:10> 'int (int)' Function 0x55f80ff19170 'f' 'int (int)'
//         `-ImplicitCastExpr 0x55f80ff195e8 <col:12> 'int' <LValueToRValue>
//           `-DeclRefExpr 0x55f80ff19588 <col:12> 'int' lvalue Var 0x55f80ff18fd0 'b' 'int'
