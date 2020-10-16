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
using namespace std;

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
		// llvm::errs() << "[*] getDeclVal first  : "<< mVars.find(decl)->first <<  " second : "<< mVars.find(decl)->second << "\n";
      	// [*] getDeclVal first  : 0x55b2fdd04f48 second : 3
		// 这里的first和second就是(vardecl, val)
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
   	int Get(int addr);
};
*/
class Heap {
   // The map of mBufs[address] = size
   std::map<int64_t, int> mBufs;
   // The map of mContents[address] = value
   std::map<int64_t, int> mContents;
public:
	Heap() : mBufs(), mContents(){
   }
   //allocate a buffer with the size of size and return the start pointer of the buffer
   int64_t Malloc(int size) {
      	/// malloc the buffer
	  	int64_t *p = (int64_t *)std::malloc(size);
      	mBufs.insert(std::make_pair((int64_t)p, size));

      	/// init the content to zero
      	for (int i=0; i<size; i ++) {
         	mContents.insert(std::make_pair((int64_t)(p+i), 0));
      	}
      	return (int64_t)p;
   }
   //Free the buffer, clear the content to zero.
   void Free (int64_t addr) {
		// check the address first.
   		assert(mBufs.find(addr) != mBufs.end());
		//get the buffer address
      	int64_t *buf = (int64_t *)addr;
	  	// get the buffer size
      	int size = mBufs.find(addr)->second;
	  	// delete the addr's iterator
      	mBufs.erase(mBufs.find(addr));
		
		// delete the addr's iterator
      	for (int i = 0; i < size; i++) {
      		assert(mContents.find((int64_t)(buf+i)) != mContents.end());
        	mContents.erase((int64_t)(buf+i));
      	}
        // Free the buffer
      	std::free(buf);
   }

   //Update the value of address in the buffer
   void Update(int64_t addr, int val) {
      assert(mContents.find(addr) != mContents.end());
      mContents[addr] = val;
   }

   //Get the value of address in the buffer
   int Get(int64_t addr) {
      assert(mContents.find(addr) != mContents.end());
      return mContents.find(addr)->second;
    }
};


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
		// put it in first ,otherwise th process global will segmentfault because no StackFrame.
		mStack.push_back(StackFrame()); 
	   	for (TranslationUnitDecl::decl_iterator i =unit->decls_begin(), e = unit->decls_end(); i != e; ++ i) {
		   	// bind global vardecl to stack
            if (VarDecl * vdecl = dyn_cast<VarDecl>(*i)) {
                llvm::errs() << "global var decl: " << vdecl << "\n";
                if (vdecl->getType().getTypePtr()->isIntegerType() || vdecl->getType().getTypePtr()->isCharType() ||
					vdecl->getType().getTypePtr()->isPointerType())
				{
					if (vdecl->hasInit())
						mStack.back().bindDecl(vdecl, expr(vdecl->getInit()));
					else
						mStack.back().bindDecl(vdecl, 0);
				}
				else
				{ // todo array
				}
            } else if (FunctionDecl * fdecl = dyn_cast<FunctionDecl>(*i) ) { // extract functions defined by ourself
			   	if (fdecl->getName().equals("FREE")) mFree = fdecl;
			   	else if (fdecl->getName().equals("MALLOC")) mMalloc = fdecl;
			   	else if (fdecl->getName().equals("GET")) mInput = fdecl;
			   	else if (fdecl->getName().equals("PRINT")) mOutput = fdecl;
			   	else if (fdecl->getName().equals("main")) mEntry = fdecl;
		   	}
	   	}
		   
   	}

   	FunctionDecl * getEntry() {
	   	return mEntry;
   	}

   	/// !TODO Support comparison operation
	// 二进制运算符
   	void binop(BinaryOperator *bop) {
	   	Expr * left = bop->getLHS();
	   	Expr * right = bop->getRHS();
        BinaryOperatorKind Opcode = bop->getOpcode();
		
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
	   	}else{
			int val;
			int lval = mStack.back().getStmtVal(left);
			int rval = mStack.back().getStmtVal(right);
			switch (Opcode)
			{
			case BO_Add: // + 
				mStack.back().bindStmt(bop, lval + rval);
				break;
			case BO_Sub: // -
				mStack.back().bindStmt(bop, lval - rval);
				break;
			case BO_Mul: // *
				mStack.back().bindStmt(bop, lval * rval);
				break;
			case BO_Div: //  / ; check the b can not be 0
				if (rval == 0){
					llvm::errs() << "the BinaryOperator /, can not div 0 " << "\n";
					exit(0);
				}
				mStack.back().bindStmt(bop, lval / rval);
				break;
			case BO_LT: // <
				val = (lval < rval) ? 1:0;
				mStack.back().bindStmt(bop, val);
				break;
			case BO_GT: // >
				val = (lval > rval) ? 1:0;
				mStack.back().bindStmt(bop, val);
				break;
			case BO_EQ: // ==
				val = (lval == rval) ? 1:0;
				mStack.back().bindStmt(bop, val);
				break;
			case BO_GE:  //>=
				if( lval >= rval )
					mStack.back().bindStmt(bop, 1);
				else
					mStack.back().bindStmt(bop, 0);
				break;
			case BO_LE:  //>=
				if( lval <= rval )
					mStack.back().bindStmt(bop, 1);
				else
					mStack.back().bindStmt(bop, 0);
				break;
			case BO_NE: // !=
				if( lval != rval )
					mStack.back().bindStmt(bop,1);
				else
					mStack.back().bindStmt(bop,0);
				break;
			default:
				llvm::errs() << "process binaryOp error" << "\n";
				exit(0);
				break;
			}
		}
	}

	//CFG: 表示源级别的过程内CFG，它表示Stmt的控制流。
	//DeclStmt-用于将声明与语句和表达式混合的适配器类
	// 声明的变量，函数，枚举
   	void decl(DeclStmt * declstmt) {
	   	for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end(); it != ie; ++ it) {
			//in ast, the sub-node is usually VarDecl
			Decl * decl = *it;
			int val = 0;
			//VarDecl 表示变量声明或定义
		   	if (VarDecl * vardecl = dyn_cast<VarDecl>(decl)) {
				llvm::errs() << "decl: " << (vardecl->getType()).getAsString() << "\n";
				llvm::errs() << "global var decl: " << vardecl << "\n";
				if (Expr * expr_tmp = vardecl->getInit()) {
					val = expr(expr_tmp);
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
	   	}else if (declref->getType()->isPointerType()){
			Decl *decl = declref->getFoundDecl();
			int val = mStack.back().getDeclVal(decl);
        	llvm::errs() << " declref's Pointer: " << val << "\n";
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

	int64_t expr(Expr *exp)
	{
		exp = exp->IgnoreImpCasts();
		if (auto decl = dyn_cast<DeclRefExpr>(exp))
		{
			declref(decl);
			int64_t result = mStack.back().getStmtVal(decl);
			return result;
		}
		else if (auto intLiteral = dyn_cast<IntegerLiteral>(exp))
		{ //a = 12
			llvm::APInt result = intLiteral->getValue();
			return result.getLimitedValue(); // intliteral->getValue().getSExtValue()
		}
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
		}else{ // other callee
			/// You could add your code here for Function call Return
			// vector<int64_t> args;
			// for (auto i = callexpr->arg_begin(), e = callexpr->arg_end(); i != e; i++)
			// {
			// 	args.push_back(expr(*i));
			// }
			// mStack.push_back(StackFrame());
			// int j = 0;
			// for (auto i = callee->param_begin(), e = callee->param_end(); i != e; i++, j++)
			// {
			// 	mStack.back().bindDecl(*i, args[j]);
			// }
		   	
	   	}
   	}
};


