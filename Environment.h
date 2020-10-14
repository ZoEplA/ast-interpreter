//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//
#include <stdio.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

class StackFrame {
   	/// StackFrame maps Variable Declaration to Value
   	/// Which are either integer or addresses (also represented using an Integer value)
   	std::map<Decl*, int> mVars;
   	std::map<Stmt*, int> mExprs;
   	/// The current stmt
   	Stmt * mPC;
public:

   	StackFrame() : mVars(), mExprs(), mPC() {
   	}

  	void bindDecl(Decl* decl, int val) {
      	mVars[decl] = val;
   	}    
   	int getDeclVal(Decl * decl) {
      	assert (mVars.find(decl) != mVars.end());
      	return mVars.find(decl)->second;
   	}
   	void bindStmt(Stmt * stmt, int val) {
	   	mExprs[stmt] = val;
   	}
   	int getStmtVal(Stmt * stmt) {
	   	assert (mExprs.find(stmt) != mExprs.end());
	   	return mExprs[stmt];
   	}
   	void setPC(Stmt * stmt) {
	   	mPC = stmt;
   	}
   	Stmt * getPC() {
	   	return mPC;
   	}
};

/// Heap maps address to a value
/*
class Heap {
public:
   	int Malloc(int size) ;
   	void Free (int addr) ;
   	void Update(int addr, int val) ;
   	int get(int addr);
};
*/

class Environment {
   	std::vector<StackFrame> mStack;

   	FunctionDecl * mFree;				/// Declartions to the built-in functions
   	FunctionDecl * mMalloc;
   	FunctionDecl * mInput;
   	FunctionDecl * mOutput;

   	FunctionDecl * mEntry;
public:
   	/// Get the declartions to the built-in functions
   	Environment() : mStack(), mFree(NULL), mMalloc(NULL), mInput(NULL), mOutput(NULL), mEntry(NULL) {
   	}


   	/// Initialize the Environment
   	void init(TranslationUnitDecl * unit) {
	   	for (TranslationUnitDecl::decl_iterator i =unit->decls_begin(), e = unit->decls_end(); i != e; ++ i) {
		   	// bind global vardecl to stack
			// if (VarDecl * vdecl = dyn_cast<VarDecl>(*i)) {
            //     llvm::errs() << "global var decl: " << vdecl << "\n";
            //     int val = 0;
			// }

			// extract functions defined by ourself
		   	if (FunctionDecl * fdecl = dyn_cast<FunctionDecl>(*i) ) {
			   	if (fdecl->getName().equals("FREE")) mFree = fdecl;
			   	else if (fdecl->getName().equals("MALLOC")) mMalloc = fdecl;
			   	else if (fdecl->getName().equals("GET")) mInput = fdecl;
			   	else if (fdecl->getName().equals("PRINT")) mOutput = fdecl;
			   	else if (fdecl->getName().equals("main")) mEntry = fdecl;
		   	}
	   	}
	   	mStack.push_back(StackFrame());
   	}

   	FunctionDecl * getEntry() {
	   	return mEntry;
   	}

   	/// !TODO Support comparison operation
	// 二进制运算符
   	void binop(BinaryOperator *bop) {
	   	Expr * left = bop->getLHS();
	   	Expr * right = bop->getRHS();
        // binOpcode Opc = bop->getOpcode();
		
		llvm::errs() << "binop left : " << left->getStmtClassName() << "\n";	
		llvm::errs() << "binop right : " << right->getStmtClassName() << "\n";

		// assign op
	   	if (bop->isAssignmentOp()) {
		   	int rval = mStack.back().getStmtVal(right);
		   	mStack.back().bindStmt(left, rval);
			//if left expr is a refered expr, bind the right value to it
		   	if (DeclRefExpr * declexpr = dyn_cast<DeclRefExpr>(left)) {
				llvm::errs() << "binop left : " << left->getStmtClassName() << "\n";
				llvm::errs() << "binop left : "  << dyn_cast<DeclRefExpr>(left)->getFoundDecl()->getNameAsString() << "\n";
				//获取发生此引用的NamedDecl
			   	Decl * decl = declexpr->getFoundDecl();
				//return the 
			   	mStack.back().bindDecl(decl, rval);
		   	}
	   	}
   	}

	//CFG: 表示源级别的过程内CFG，它表示Stmt的控制流。
	//DeclStmt-用于将声明与语句和表达式混合的适配器类
   	void decl(DeclStmt * declstmt) {
	   	for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end(); it != ie; ++ it) {
			//in ast, the sub-node is usually VarDecl
			Decl * decl = *it;
			int val = 0;
			//VarDecl 表示变量声明或定义
		   	if (VarDecl * vardecl = dyn_cast<VarDecl>(decl)) {
				llvm::errs() << "decl: " << (vardecl->getType()).getAsString() << "\n";
				llvm::errs() << "global var decl: " << vardecl << "\n";
				if (Expr * expr = vardecl->getInit()) {
                    if (IntegerLiteral * iliteral = dyn_cast<IntegerLiteral>(expr)) {
                        val = (int)iliteral->getValue().getLimitedValue();  
                    }
                }
				mStack.back().bindDecl(vardecl, val);
		   	}
	   	}
   	}

    // 对已声明的变量，函数，枚举等的引用
   	void declref(DeclRefExpr * declref) {
		llvm::errs() << "declref : " << declref->getFoundDecl()->getNameAsString() << "\n";
	   	mStack.back().setPC(declref);
	   	if (declref->getType()->isIntegerType()) {
		   	Decl* decl = declref->getFoundDecl();

		   	int val = mStack.back().getDeclVal(decl);
        	llvm::errs() << " declref's int: " << val << "\n";
		   	mStack.back().bindStmt(declref, val);
	   	}
   	}

	//类型转换的基类，包括隐式转换（ImplicitCastExpr）和在源代码中具有某种表示形式的显式转换（ExplicitCastExpr的派生类）
   	void cast(CastExpr * castexpr) {
	   	mStack.back().setPC(castexpr);
	   	if (castexpr->getType()->isIntegerType()) {
		   	Expr * expr = castexpr->getSubExpr();
		   	int val = mStack.back().getStmtVal(expr);
        	llvm::errs() << "------CastExpr expr val: " << val <<" getSubExpr expr: " << expr->getStmtClassName() << "\n";
		   	mStack.back().bindStmt(castexpr, val );
	   	}
   	}
    void intliteral(IntegerLiteral * intliteral) {
        int val = (int)intliteral->getValue().getLimitedValue(); // intliteral->getValue().getSExtValue()
        llvm::errs() << " intliteral:\n    " << val << "\n";
        mStack.back().bindStmt(dyn_cast<Expr>(intliteral), val);
    }

   	/// !TODO Support Function Call
   	void call(CallExpr * callexpr) {
	   	mStack.back().setPC(callexpr);
	   	int val = 0;
	   	FunctionDecl * callee = callexpr->getDirectCallee();
	   	if (callee == mInput) {
		  	llvm::errs() << "Please Input an Integer Value : ";
		  	scanf("%d", &val);

		  	mStack.back().bindStmt(callexpr, val);
	   	} else if (callee == mOutput) {
		   	Expr * decl = callexpr->getArg(0);
		   	val = mStack.back().getStmtVal(decl);
		   	llvm::errs() << "callee : " << val << "\n";
	   	} else {
		   	/// You could add your code here for Function call Return
	   	}
   	}
};


