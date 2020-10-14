extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main(int argc, char *argv[]) {
	int a = 3;
	int d = 4;
	int c = 5;
	//BinaryOperator DeclRefExpr IntegerLiteral
	a = 1 + d;
	d = 2;
	c = 3;
	PRINT(a);
	//MALLOC(c);
}

// |-TypedefDecl 0x564d8c290020 <<invalid sloc>> <invalid sloc> implicit __int128_t '__int128'
// | `-BuiltinType 0x564d8c28fd20 '__int128'
// |-TypedefDecl 0x564d8c290090 <<invalid sloc>> <invalid sloc> implicit __uint128_t 'unsigned __int128'
// | `-BuiltinType 0x564d8c28fd40 'unsigned __int128'
// |-TypedefDecl 0x564d8c2903a8 <<invalid sloc>> <invalid sloc> implicit __NSConstantString 'struct __NSConstantString_tag'
// | `-RecordType 0x564d8c290180 'struct __NSConstantString_tag'
// |   `-Record 0x564d8c2900e8 '__NSConstantString_tag'
// |-TypedefDecl 0x564d8c290440 <<invalid sloc>> <invalid sloc> implicit __builtin_ms_va_list 'char *'
// | `-PointerType 0x564d8c290400 'char *'
// |   `-BuiltinType 0x564d8c28f820 'char'
// |-TypedefDecl 0x564d8c2d4280 <<invalid sloc>> <invalid sloc> implicit __builtin_va_list 'struct __va_list_tag [1]'
// | `-ConstantArrayType 0x564d8c2906f0 'struct __va_list_tag [1]' 1 
// |   `-RecordType 0x564d8c290530 'struct __va_list_tag'
// |     `-Record 0x564d8c290498 '__va_list_tag'
// |-FunctionDecl 0x564d8c2d43b8 <test00.c:4:1, col:22> col:13 used PRINT 'void (int)' extern
// | `-ParmVarDecl 0x564d8c2d42f0 <col:19> col:22 'int'
// `-FunctionDecl 0x564d8c2d4510 <line:6:1, line:16:1> line:6:5 main 'int ()'
//   `-CompoundStmt 0x564d8c2d4a20 <col:12, line:16:1>
//     |-DeclStmt 0x564d8c2d4668 <line:7:2, col:13>
//     | `-VarDecl 0x564d8c2d45e0 <col:2, col:10> col:6 used a 'int' cinit
//     |   `-IntegerLiteral 0x564d8c2d4648 <col:10> 'int' 200
//     |-DeclStmt 0x564d8c2d4720 <line:8:2, col:13>
//     | `-VarDecl 0x564d8c2d4698 <col:2, col:10> col:6 used d 'int' cinit
//     |   `-IntegerLiteral 0x564d8c2d4700 <col:10> 'int' 300
//     |-DeclStmt 0x564d8c2d47d8 <line:9:2, col:13>
//     | `-VarDecl 0x564d8c2d4750 <col:2, col:10> col:6 used c 'int' cinit
//     |   `-IntegerLiteral 0x564d8c2d47b8 <col:10> 'int' 400
//     |-BinaryOperator 0x564d8c2d4848 <line:11:2, col:6> 'int' '='
//     | |-DeclRefExpr 0x564d8c2d47f0 <col:2> 'int' lvalue Var 0x564d8c2d45e0 'a' 'int'
//     | `-IntegerLiteral 0x564d8c2d4828 <col:6> 'int' 1
//     |-BinaryOperator 0x564d8c2d48c0 <line:12:2, col:6> 'int' '='
//     | |-DeclRefExpr 0x564d8c2d4868 <col:2> 'int' lvalue Var 0x564d8c2d4698 'd' 'int'
//     | `-IntegerLiteral 0x564d8c2d48a0 <col:6> 'int' 2
//     |-BinaryOperator 0x564d8c2d4938 <line:13:2, col:6> 'int' '='
//     | |-DeclRefExpr 0x564d8c2d48e0 <col:2> 'int' lvalue Var 0x564d8c2d4750 'c' 'int'
//     | `-IntegerLiteral 0x564d8c2d4918 <col:6> 'int' 3
//     `-CallExpr 0x564d8c2d49e0 <line:14:2, col:9> 'void'
//       |-ImplicitCastExpr 0x564d8c2d49c8 <col:2> 'void (*)(int)' <FunctionToPointerDecay>
//       | `-DeclRefExpr 0x564d8c2d4958 <col:2> 'void (int)' Function 0x564d8c2d43b8 'PRINT' 'void (int)'
//       `-ImplicitCastExpr 0x564d8c2d4a08 <col:8> 'int' <LValueToRValue>
//         `-DeclRefExpr 0x564d8c2d4978 <col:8> 'int' lvalue Var 0x564d8c2d45e0 'a' 'int'
