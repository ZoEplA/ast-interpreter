//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//
#include <stdio.h>
#include <iostream>

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
		llvm::errs() << "[*] bindStmt : " << stmt->getStmtClassName() << " " << stmt << " " << val << "\n";
	   	mExprs[stmt] = val;
   	}
   	int getStmtVal(Stmt * stmt) {
		llvm::errs() << "[*] getstmtval first  : "<< mExprs.find(stmt)->first <<  " second : "<< mExprs.find(stmt)->second << "\n";
		llvm::errs() << "[*] getstmtval : " << stmt->getStmtClassName() << " " << stmt << "\n";
	   	assert (mExprs.find(stmt) != mExprs.end());
	   	return mExprs[stmt];
   	}
   	void setPC(Stmt * stmt) {
	   	mPC = stmt;
   	}
   	Stmt * getPC() {
	   	return mPC;
   	}

	void pushStmtVal(Stmt *stmt, int64_t value)
	{
		mExprs.insert(pair<Stmt *, int64_t>(stmt, value));
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
	
	Heap mHeap;
   	FunctionDecl * mFree;				/// Declartions to the built-in functions
   	FunctionDecl * mMalloc;
   	FunctionDecl * mInput;
   	FunctionDecl * mOutput;

   	FunctionDecl * mEntry;

	bool retType = 0; // 0-> void 1 -> int
	int64_t retValue = 0;

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

	//return 

	void setReturn(bool tp, int64_t val){
		retType = tp;
		retValue = val;
	}

    bool haveReturn(){
		if(retType==0 && retValue==0){
			return false;
		}else{
			return true;
		}
	}

	int64_t getReturn(){
		if (retType){
			return retValue;
		}
		return 0;
	}

	void mStack_pushStmtVal(CallExpr *call, int64_t retvalue){
		mStack.back().pushStmtVal(call, retvalue);
	}

	void mStack_pop_back(){
		mStack.pop_back();
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
		//处理BinaryOperator节点下的Expr等其他节点,比如存在两个子节点为expr或者BinaryOperator；此时需要把这些节点做decl或者stmt的bind
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
			// 不是所有stmt都能getStmtVal，我们这里选择expr函数来进行解析
			int val;
			cout << "[*]Opcode =  " << Opcode << endl;
			switch (Opcode)
			{
			case BO_Add: // + 
				mStack.back().bindStmt(bop, expr(left) + expr(right));
				break;
			case BO_Sub: // -
				mStack.back().bindStmt(bop, expr(left) - expr(right));
				break;
			case BO_Mul: // *
				mStack.back().bindStmt(bop, expr(left) * expr(right));
				break;
			case BO_Div: //  / ; check the b can not be 0
				if (expr(right) == 0){
					llvm::errs() << "the BinaryOperator /, can not div 0 " << "\n";
					exit(0);
				}
				mStack.back().bindStmt(bop, expr(left) / expr(right));
				break;
			case BO_LT: // <
				cout << "<<<<<<" << endl;
				val = (expr(left) < expr(right)) ? 1:0;
				mStack.back().bindStmt(bop, val);
				break;
			case BO_GT: // >
				val = (expr(left) > expr(right)) ? 1:0;
				mStack.back().bindStmt(bop, val);
				break;
			case BO_EQ: // ==
				val = (expr(left) == expr(right)) ? 1:0;
				mStack.back().bindStmt(bop, val);
				break;
			case BO_GE:  //>=
				if( expr(left) >= expr(right) )
					mStack.back().bindStmt(bop, 1);
				else
					mStack.back().bindStmt(bop, 0);
				break;
			case BO_LE:  //>=
				if( expr(left) <= expr(right) )
					mStack.back().bindStmt(bop, 1);
				else
					mStack.back().bindStmt(bop, 0);
				break;
			case BO_NE: // !=
				if( expr(left) != expr(right) )
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
		cout << "[*] decl !!!" << endl;
	   	for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end(); it != ie; ++ it) {
			//in ast, the sub-node is usually VarDecl
			Decl * decl = *it;
			//VarDecl 表示变量声明或定义
		   	if (VarDecl * vardecl = dyn_cast<VarDecl>(decl)) {
				// global var
				llvm::errs() << "decl: " << (vardecl->getType()).getAsString() << "\n";
				llvm::errs() << "global var decl: " << vardecl << "\n";
				if (vardecl->getType().getTypePtr()->isIntegerType() || vardecl->getType().getTypePtr()->isPointerType() 
					|| vardecl->getType().getTypePtr()->isCharType() )
				{
					int val = 0;
					if (vardecl->hasInit()) {
						val = expr(vardecl->getInit());
					}
					mStack.back().bindDecl(vardecl, val);
				}else{
					//array
					if (auto array = dyn_cast<ConstantArrayType>(vardecl->getType().getTypePtr()))
					{ // int/char A[100];
						int64_t length = array->getSize().getSExtValue();
						if (array->getElementType().getTypePtr()->isIntegerType())
						{ // IntegerArray
							int *a = new int[length];
							for (int i = 0; i < length; i++)
							{
								a[i] = 0;
							}
							mStack.back().bindDecl(vardecl, (int64_t)a);
						}
						else if (array->getElementType().getTypePtr()->isCharType())
						{ // Clang/AST/Type.h line 1652
							char *a = new char[length];
							for (int i = 0; i < length; i++)
							{
								a[i] = 0;
							}
							mStack.back().bindDecl(vardecl, (int64_t)a);
						}
						else
						{ // int* c[2];
							int64_t **a = new int64_t *[length];
							for (int i = 0; i < length; i++)
							{
								a[i] = 0;
							}
							mStack.back().bindDecl(vardecl, (int64_t)a);
						}
						/*
						if(vardecl->hasInit()){
							// todo , guess Stmt **VarDecl::getInitAddress 
						}*/
					}
				}
		   	}
	   	}
   	}

    // 对已声明的变量，函数，枚举等的引用
   	void declref(DeclRefExpr * declref) {
		llvm::errs() << "declref : " << declref->getFoundDecl()->getNameAsString() << "\n";
	   	mStack.back().setPC(declref);
		if (declref->getType()->isCharType()){
			Decl *decl = declref->getFoundDecl();
			int val = mStack.back().getDeclVal(decl);
        	llvm::errs() << " declref's char: " << val << "\n";
			mStack.back().bindStmt(declref, val);
		}else if (declref->getType()->isIntegerType()) {
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

	//process ArraySubscriptExpr, e.g. int [n]
   	void array(ArraySubscriptExpr *arrayexpr)
   	{
   		//get the base and the offset index of the array
   		Expr *leftexpr=arrayexpr->getLHS();
   		//cout<<leftexpr->getStmtClassName()<<endl;
   		int base=mStack.back().getStmtVal(leftexpr);
   		Expr *rightexpr=arrayexpr->getRHS();
   		//cout<<rightexpr->getStmtClassName()<<endl;
   		int offset=mStack.back().getStmtVal(rightexpr);
   		//cout<<valRight<<endl;

   		//by mHeap,we can get the value of addr in buf,we bind the value to ArraySubscriptExpr
   		mStack.back().bindStmt(arrayexpr,mHeap.Get(base + offset*sizeof(int)));
   }

	//get the condition value of IfStmt and WhileStmt
   	bool getcond(/*BinaryOperator *bop*/Expr *expr)
   	{
   		return mStack.back().getStmtVal(expr);
   	}

	void returnstmt(ReturnStmt *returnStmt)
	{
		int64_t value = expr(returnStmt->getRetValue());
		setReturn(true, value);
	}

   //process UnaryExprOrTypeTraitExpr, e.g. sizeof
   void unarysizeof(UnaryExprOrTypeTraitExpr *uop)
   {
   		// auto *expr=uop->getArgumentExpr();
   		// int val =sizeof(expr);
   	  	int val;
   	  	//if UnaryExprOrTypeTraitExpr is sizeof,
      	if(uop->getKind() == UETT_SizeOf )
      	{
      	 	//if the arg type is integer type, we bind sizeof(long) to UnaryExprOrTypeTraitExpr
         	if(uop->getArgumentType()->isIntegerType())
         	{
            	val = sizeof(long);
         	}
         	//if the arg type is pointer type, we bind sizeof(int *) to UnaryExprOrTypeTraitExpr
         	else if(uop->getArgumentType()->isPointerType())
         	{
            	val = sizeof(int *);
         	}
      	}    
   	  	mStack.back().bindStmt(uop,val);
   }
	// 这表示一元表达式（sizeof和alignof除外），postfix-expression中的postinc / postdec运算符以及各种扩展名
	void unaryop(UnaryOperator *unaryExpr) 
	{ // - +
		// Clang/AST/Expr.h/ line 1714
		auto op = unaryExpr->getOpcode();
		auto exp = unaryExpr->getSubExpr();
		switch (op)
		{
		case UO_Minus: //'-'
			mStack.back().pushStmtVal(unaryExpr, -1 * expr(exp));
			break;
		case UO_Plus: //'+'
			mStack.back().pushStmtVal(unaryExpr, expr(exp));
			break;
		case UO_Deref: // '*'
			mStack.back().pushStmtVal(unaryExpr, *(int64_t *)expr(unaryExpr->getSubExpr()));
			break;
		case UO_AddrOf: // '&',deref,bind the address of expr to UnaryOperator
			mStack.back().pushStmtVal(unaryExpr,(int64_t)exp);
			llvm::errs() << long(exp) << "\n";
			//mStack.back().bindStmt(uop, mHeap.Get(val));
			break;
		default:
			llvm::errs() << "process unaryOp error" << "\n";
			exit(0);
			break;
		}
	}

	int64_t expr(Expr *exp)
	{
		//跳过可能围绕此表达式的所有隐式强制转换，直到达到固定点为止
		exp = exp->IgnoreImpCasts();
		if (auto decl = dyn_cast<DeclRefExpr>(exp)){
			declref(decl);
			int64_t result = mStack.back().getStmtVal(decl);
			return result;
		}
		else if (auto intLiteral = dyn_cast<IntegerLiteral>(exp)){ 
		//a = 12
			llvm::APInt result = intLiteral->getValue();
			return result.getLimitedValue(); // intliteral->getValue().getSExtValue()
		}
		else if (auto unaryExpr = dyn_cast<UnaryOperator>(exp)){ 
		// a = -13 and a = +12;
			unaryop(unaryExpr);
			int64_t result = mStack.back().getStmtVal(unaryExpr);
			return result;
		}
		else if (auto charLiteral = dyn_cast<CharacterLiteral>(exp)){
		// a = 'a'
			llvm::errs() << "char :" << charLiteral->getValue() <<"\n";
			return charLiteral->getValue(); // Clang/AST/Expr.h/ line 1369
		}
		else if (auto binaryExpr = dyn_cast<BinaryOperator>(exp)){ 
		//+ - * / < > ==
			binop(binaryExpr); // 这个是为了在for语句的时候直接解析`a < 10`语句而不调用visit->binop
			int64_t result = mStack.back().getStmtVal(binaryExpr);
			return result;
		}
		else if (auto parenExpr = dyn_cast<ParenExpr>(exp)){
		// (E)
			return expr(parenExpr->getSubExpr());
		}
		else if (auto callexpr = dyn_cast<CallExpr>(exp)){
			return mStack.back().getStmtVal(callexpr);
		}
		else if (auto sizeofexpr = dyn_cast<UnaryExprOrTypeTraitExpr>(exp)){
			if (sizeofexpr->getKind() == UETT_SizeOf){ 
			//sizeof
				if (sizeofexpr->getArgumentType()->isIntegerType()){
					return sizeof(int64_t); // 8 byte
				}
				else if (sizeofexpr->getArgumentType()->isPointerType()){
					return sizeof(int64_t *); // 8 byte
				}
			}
		}
		else if (auto castexpr = dyn_cast<CStyleCastExpr>(exp)){
			return expr(castexpr->getSubExpr());
		}
		llvm::errs() << "have not handle this situation" << "\n";
		return 0;
	}
   	/// !TODO Support Function Call
   	void call(CallExpr * callexpr) {
	   	mStack.back().setPC(callexpr);
	   	int64_t val = 0;
	   	FunctionDecl * callee = callexpr->getDirectCallee();
	   	if (callee == mInput) {
		  	llvm::errs() << "Please Input an Integer Value : ";
			cin >> val;

			mStack.back().bindStmt(callexpr, val);
	   	} else if (callee == mOutput) {
			// Todo: cout the char value.
			Expr *decl = callexpr->getArg(0);
			Expr *exp = decl->IgnoreImpCasts();
			if (auto array = dyn_cast<ArraySubscriptExpr>(exp))
			{
				val = expr(decl);
				std::cout << "output : " << val << endl;
			}
			else
			{
				val = expr(decl);
				std::cout << "output : " << val << endl;
			}
		}else if (callee == mMalloc){
			int64_t malloc_size = expr(callexpr->getArg(0));
			int64_t *p = (int64_t *)std::malloc(malloc_size);
			mStack.back().bindStmt(callexpr, (int64_t)p);
		}else if (callee == mFree){
			int64_t *p = (int64_t *)expr(callexpr->getArg(0));
			std::free(p);
		}else{  // other callee
			llvm::errs() << "other callee : ???\n";
			StackFrame stack;
			auto pit=callee->param_begin();
			for(auto it=callexpr->arg_begin(), ie=callexpr->arg_end();it!=ie;++it,++pit)
			{
				int val=mStack.back().getStmtVal(*it);
				stack.bindDecl(*pit,val);
			}
			mStack.push_back(stack);
	   	}
   	}
};


