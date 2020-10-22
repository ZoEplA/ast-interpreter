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
   	std::map<Decl*, int64_t> mVars;
   	std::map<Stmt*, int64_t> mExprs;
   	/// The current stmt
   	Stmt * mPC;
	
public:

   	StackFrame() : mVars(), mExprs(), mPC() {
   	}


  	void bindDecl(Decl* decl, int64_t val) {
      	mVars[decl] = val;
   	}    
   	int64_t getDeclVal(Decl * decl) {
      	assert (mVars.find(decl) != mVars.end());
		// llvm::errs() << "[*] getDeclVal first  : "<< mVars.find(decl)->first <<  " second : "<< mVars.find(decl)->second << "\n";
      	// [*] getDeclVal first  : 0x55b2fdd04f48 second : 3
		// 这里的first和second就是(vardecl, val)
		return mVars.find(decl)->second;
   	}
   	void bindStmt(Stmt * stmt, int64_t val) {
		// llvm::errs() << "		[*] bindStmt : " << stmt->getStmtClassName() << " " << stmt << " " << val << "\n";
	   	mExprs[stmt] = val;
   	}
   	int64_t getStmtVal(Stmt * stmt) {
		// llvm::errs() << "		[*] getstmtval first  : "<< mExprs.find(stmt)->first <<  " second : "<< mExprs.find(stmt)->second << "\n";
		// llvm::errs() << "		[*] getstmtval : " << stmt->getStmtClassName() << " " << stmt << "\n";
	   	assert (mExprs.find(stmt) != mExprs.end());
	   	return mExprs[stmt];
   	}
   	void setPC(Stmt * stmt) {
	   	mPC = stmt;
   	}
   	Stmt * getPC() {
	   	return mPC;
   	}

	bool exprExits(Stmt *stmt)
	{
		return mExprs.find(stmt) != mExprs.end();
	}

	// void pushStmtVal(Stmt *stmt, int64_t value)
	// {
	// 	mExprs.insert(pair<Stmt *, int64_t>(stmt, value));
	// }
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
   int64_t Get(int64_t addr) {
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
                // llvm::errs() << "global var decl: " << vdecl << "\n";
                if (vdecl->getType().getTypePtr()->isIntegerType() || vdecl->getType().getTypePtr()->isCharType() ||
					vdecl->getType().getTypePtr()->isPointerType())
				{
					if (vdecl->hasInit())
						mStack.back().bindDecl(vdecl, Expr_GetVal(vdecl->getInit()));
					else
						mStack.back().bindDecl(vdecl, 0);
				}
				else
				{ // todo array
					cout<<"		couldn't find the type when init." <<endl;
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

	void setReturn(bool type, int64_t ret_val){
		retType = type;
		retValue = ret_val;
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

	void mStack_bindStmt(CallExpr *call, int64_t retvalue){
		// cout << "		push_func_stack_stmt = " << call << endl;
		mStack.back().bindStmt(call, retvalue);
	}

	void mStack_pop_back(){
		mStack.pop_back();
		setReturn(false, 0);
	}
   	/// !TODO Support comparison operation
	// 二进制运算符
   	void binop(BinaryOperator *bop) {
	   	Expr * left = bop->getLHS();
	   	Expr * right = bop->getRHS();
        BinaryOperatorKind Opcode = bop->getOpcode();
		
		// llvm::errs() << "		binop left : " << left->getStmtClassName() << " " << left << "\n";	
		// llvm::errs() << "		binop right : " << right->getStmtClassName() << " " << right << "\n";
		// isAssignmentOp : 判断是赋值语句还是一个 +-*/的语句
	   	if (bop->isAssignmentOp()) { 
			//if left expr is a refered expr, bind the right value to it
		   	if (DeclRefExpr * declexpr = dyn_cast<DeclRefExpr>(left)) {
				// llvm::errs() << "binop left : " << left->getStmtClassName() << "\n";
				// llvm::errs() << "binop left : "  << dyn_cast<DeclRefExpr>(left)->getFoundDecl()->getNameAsString() << "\n";
				//获取发生此引用的NamedDecl,绑定右节点的值到左节点
				int64_t val = Expr_GetVal(right);
				mStack.back().bindStmt(left, val);
			   	Decl * decl = declexpr->getFoundDecl();
			   	mStack.back().bindDecl(decl, val);
		   	}else if (auto array = dyn_cast<ArraySubscriptExpr>(left))
			{
				// cout << "handle array - ArraySubscriptExpr." << endl;
				if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(array->getLHS()->IgnoreImpCasts()))
				{
					// cout << "handle array - DeclRefExpr." << endl;
					Decl *decl = declexpr->getFoundDecl();
					int64_t val = Expr_GetVal(right);
					int index = Expr_GetVal(array->getRHS());
					if (VarDecl *vardecl = dyn_cast<VarDecl>(decl)){
						if (auto array = dyn_cast<ConstantArrayType>(vardecl->getType().getTypePtr())){	
						// set the array value.
							if (array->getElementType().getTypePtr()->isIntegerType()){ 
							// int64_t a[3];
								int64_t tmp = mStack.back().getDeclVal(vardecl);
								int64_t *p = (int64_t *)tmp;
								// cout << "[-] array tmp : " << tmp << endl;
								// cout << "[-] array index : " << index << endl;
								// cout << "[-] array val : " << val << endl;
								*(p + index) = val;
							}else if (array->getElementType().getTypePtr()->isCharType()){ 
							// char a[3];
								int64_t tmp = mStack.back().getDeclVal(vardecl);
								char *p = (char *)tmp;
								*(p + index) = (char)val;
							}else if(array->getElementType().getTypePtr()->isPointerType()){ 
							// int* a[3];
								int64_t tmp = mStack.back().getDeclVal(vardecl);
								int64_t **p = (int64_t **)tmp;
								*(p + index) = (int64_t *)val;
							}
						}
					}
				}
			}
			else if (auto unaryExpr = dyn_cast<UnaryOperator>(left))
			{ // *(p+1)
				if( (unaryExpr->getOpcode()) == UO_Deref)
				{
					int64_t val = Expr_GetVal(right);
					int64_t addr = Expr_GetVal(unaryExpr->getSubExpr());
					int64_t *p = (int64_t *)addr;
					*p = val;
				}
			}
	   	}
		else{
			// 不是所有stmt都能getStmtVal，我们这里选择expr函数来进行解析
			int64_t result;
			// cout << "[*]Opcode =  " << Opcode << endl;
			switch (Opcode)
			{
			case BO_Add: // + 
				// cout << "[*] BO_Add = " <<BO_Add  << endl;
				result = Expr_GetVal(left) + Expr_GetVal(right);
				break;
			case BO_Sub: // -
				result = Expr_GetVal(left) - Expr_GetVal(right);
				break;
			case BO_Mul: // *
				// cout << "[*] BO_Mul = " <<BO_Mul  <<endl;
				result = Expr_GetVal(left) * Expr_GetVal(right);
				break;
			case BO_Div: //  / ; check the b can not be 0
				if (Expr_GetVal(right) == 0){
					llvm::errs() << "		the BinaryOperator /, can not div 0 " << "\n";
					exit(0);
				}
				result = Expr_GetVal(left) / Expr_GetVal(right);
				break;
			case BO_LT: // < BO_LT = 10
				// cout << "[*] BO_LT = " <<BO_LT  <<endl;
				result = (Expr_GetVal(left) < Expr_GetVal(right)) ? 1:0;
				// cout << "[*] BO_LT bindstmt" << result  <<endl;
				break;
			case BO_GT: // >
				result = (Expr_GetVal(left) > Expr_GetVal(right)) ? 1:0;
				break;
			case BO_EQ: // ==
				result = (Expr_GetVal(left) == Expr_GetVal(right)) ? 1:0;
				break;
			case BO_GE:  //>=
				if( Expr_GetVal(left) >= Expr_GetVal(right) )
					result = 1;
				else
					result = 0;
				break;
			case BO_LE:  //>=
				if( Expr_GetVal(left) <= Expr_GetVal(right) )
					result = 1;
				else
					result = 0;
				break;
			case BO_NE: // !=
				if( Expr_GetVal(left) != Expr_GetVal(right) )
					result = 1;
				else
					result = 0;
				break;
			default:
				llvm::errs() << "		process binaryOp error" << "\n";
				exit(0);
				break;
			}

			mStack.back().bindStmt(bop, result);
		}
	}

	//CFG: 表示源级别的过程内CFG，它表示Stmt的控制流。
	//DeclStmt-用于将声明与语句和表达式混合的适配器类
	// 声明的变量，函数，枚举
   	void decl(DeclStmt * declstmt) {
		// cout << "[*] decl !!!" << endl;
	   	for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end(); it != ie; ++ it) {
			//in ast, the sub-node is usually VarDecl
			Decl * decl = *it;
			//VarDecl 表示变量声明或定义
		   	if (VarDecl * vardecl = dyn_cast<VarDecl>(decl)) {
				// global var
				// llvm::errs() << "decl: " << (vardecl->getType()).getAsString() << "\n";
				// llvm::errs() << "global var decl: " << vardecl << "\n";
				if (vardecl->getType().getTypePtr()->isIntegerType() || vardecl->getType().getTypePtr()->isPointerType() 
					|| vardecl->getType().getTypePtr()->isCharType() )
				{
					int val = 0;
					if (vardecl->hasInit()) {
						val = Expr_GetVal(vardecl->getInit());
					}
					mStack.back().bindDecl(vardecl, val);
				}else{
					//array
					if (auto array = dyn_cast<ConstantArrayType>(vardecl->getType().getTypePtr())){ 
					// array declstmt, bind a's addr to the vardecl.
						int64_t len = array->getSize().getLimitedValue();
						if (array->getElementType().getTypePtr()->isIntegerType()){ 
						// int a[3]; 
							int *a = new int[len];
							for (int i = 0; i < len; i++)
							{
								a[i] = 0x61;
							}
							// cout<<"[-] array init = "<<a<<endl;
							mStack.back().bindDecl(vardecl, (int64_t)a);
						}else if (array->getElementType().getTypePtr()->isCharType()){
						// char a[3];
							char *a = new char[len];
							for (int i = 0; i < len; i++)
							{
								a[i] = 0;
							}
							mStack.back().bindDecl(vardecl, (int64_t)a);
						}else if(array->getElementType().getTypePtr()->isPointerType()){ 
						// int* a[3];
							int64_t **a = new int64_t *[len];
							for (int i = 0; i < len; i++){
								a[i] = 0;
							}
							mStack.back().bindDecl(vardecl, (int64_t)a);
						}
					}
				}
		   	}
	   	}
   	}

    // 对已声明的变量，函数，枚举等的引用
   	void declref(DeclRefExpr * declref) {
		// llvm::errs() << "declref : " << declref->getFoundDecl()->getNameAsString() << "\n";
	   	mStack.back().setPC(declref);
		if (declref->getType()->isCharType()){
			Decl *decl = declref->getFoundDecl();
			int64_t val = mStack.back().getDeclVal(decl);
        	// llvm::errs() << " declref's char: " << val << "\n";
			mStack.back().bindStmt(declref, val);
		}else if (declref->getType()->isIntegerType()) {
		   	Decl* decl = declref->getFoundDecl();
		   	int64_t val = mStack.back().getDeclVal(decl);
        	// llvm::errs() << " declref's int: " << val << "\n";
		   	mStack.back().bindStmt(declref, val);
	   	}else if (declref->getType()->isPointerType()){
			Decl *decl = declref->getFoundDecl();
			int64_t val = mStack.back().getDeclVal(decl);
        	// llvm::errs() << " declref's Pointer: " << val << "\n";
			mStack.back().bindStmt(declref, val);
	   	} 
   	}

	//get the condition value of IfStmt and WhileStmt
   	bool getcond(/*BinaryOperator *bop*/Expr *expr)
   	{
   		return mStack.back().getStmtVal(expr);
   	}

	void returnstmt(ReturnStmt *returnStmt)
	{
		// cout << "		get returnstmt !!!" << endl;
		int64_t value = Expr_GetVal(returnStmt->getRetValue());
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
            	val = sizeof(int64_t);
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
			mStack.back().bindStmt(unaryExpr, -1 * Expr_GetVal(exp));
			break;
		case UO_Plus: //'+'
			mStack.back().bindStmt(unaryExpr, Expr_GetVal(exp));
			break;
		case UO_Deref: // '*'
			mStack.back().bindStmt(unaryExpr, *(int64_t *)Expr_GetVal(unaryExpr->getSubExpr()));
			break;
		case UO_AddrOf: // '&',deref,bind the address of expr to UnaryOperator
			mStack.back().bindStmt(unaryExpr,(int64_t)exp);
			llvm::errs() << long(exp) << "\n";
			//mStack.back().bindStmt(uop, mHeap.Get(val));
			break;
		default:
			llvm::errs() << "		process unaryOp error" << "\n";
			exit(0);
			break;
		}
	}

	int64_t Expr_GetVal(Expr *exp)
	{
		//跳过可能围绕此表达式的所有隐式强制转换，直到达到固定点为止
		exp = exp->IgnoreImpCasts();
		if (auto decl = dyn_cast<DeclRefExpr>(exp)){//还没搞清楚的
      		// llvm::errs() << "[*] begin test decl\n";
			// cout<<"		DeclRefExpr expr"<<endl;
			declref(decl);
			int64_t result = mStack.back().getStmtVal(decl);
			return result;
		}else if (auto intLiteral = dyn_cast<IntegerLiteral>(exp)){ 
		//a = 12
			// cout<<"		IntegerLiteral expr"<<endl;
			llvm::APInt result = intLiteral->getValue();
			return result.getSExtValue(); // intliteral->getValue().getSExtValue()
		}
		else if (auto unaryExpr = dyn_cast<UnaryOperator>(exp)){ 
		// a = -13 and a = +12;
			unaryop(unaryExpr);
			int64_t result = mStack.back().getStmtVal(unaryExpr);
			return result;
		}
		else if (auto charLiteral = dyn_cast<CharacterLiteral>(exp)){
		// a = 'a'
			// llvm::errs() << "char :" << charLiteral->getValue() <<"\n";
			return charLiteral->getValue(); // Clang/AST/Expr.h/ line 1369
		}
		else if (auto binaryExpr = dyn_cast<BinaryOperator>(exp)){ 
		//+ - * / < > ==
			// cout<<"		BinaryOperator expr"<<endl;
			binop(binaryExpr); // 这个是为了在for语句的时候直接解析`a < 10`语句而不调用visit->binop
			int64_t result = mStack.back().getStmtVal(binaryExpr);
			return result;
		}
		else if (auto parenExpr = dyn_cast<ParenExpr>(exp)){
		// (E)
			return Expr_GetVal(parenExpr->getSubExpr());
		}
		else if (auto callexpr = dyn_cast<CallExpr>(exp)){
			// cout<<"		callexpr expr"<<endl;
			return mStack.back().getStmtVal(callexpr);
		}
		else if (auto array = dyn_cast<ArraySubscriptExpr>(exp)){ 
			// cout<<"		ArraySubscriptExpr expr"<<endl;
		// a[12]
			if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(array->getLHS()->IgnoreImpCasts()))
			{
				Decl *decl = declexpr->getFoundDecl();
				int64_t index = Expr_GetVal(array->getRHS());
				if (VarDecl *vardecl = dyn_cast<VarDecl>(decl))
				{
					if (auto array = dyn_cast<ConstantArrayType>(vardecl->getType().getTypePtr()))
					{
						if (array->getElementType().getTypePtr()->isIntegerType()){ 
						// int a[3];
							int64_t tmp = mStack.back().getDeclVal(vardecl);
							int64_t *p = (int64_t *)tmp;
							return *(p + index);
						}else if (array->getElementType().getTypePtr()->isIntegerType()){ 
						// char a[3];
							int64_t tmp = mStack.back().getDeclVal(vardecl);
							char *p = (char *)tmp;
							return *(p + index);
						}else if(array->getElementType().getTypePtr()->isPointerType()){ 
							// int* a[3];
							int64_t tmp = mStack.back().getDeclVal(vardecl);
							int64_t** p = (int64_t**)tmp;
							return (int64_t)(*(p+index));
						}
					}
				}
			}
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
		else if (auto castexpr = dyn_cast<CStyleCastExpr>(exp)){ //还没搞清楚的
			// cout<<"		CStyleCastExpr expr"<<endl;
      		// llvm::errs() << "[*] begin test castexpr\n";
			return Expr_GetVal(castexpr->getSubExpr());
		}
		llvm::errs() << "		have not handle this situation" << "\n";
		return 0;
	}
   	/// !TODO Support Function Call
   	void call(CallExpr * callexpr) {
	   	mStack.back().setPC(callexpr);
	   	int64_t val = 0;
	   	FunctionDecl * callee = callexpr->getDirectCallee();
	   	if (callee == mInput) {
		  	llvm::errs() << "		Please Input an Integer Value : ";
			cin >> val;

			mStack.back().bindStmt(callexpr, val);
	   	} else if (callee == mOutput) {
			// Todo: cout the char value.
			Expr *decl = callexpr->getArg(0);
			Expr *exp = decl->IgnoreImpCasts();
			val = Expr_GetVal(decl);
			std::cout << "	output : " << val << endl;
		}else if (callee == mMalloc){
			int64_t malloc_size = Expr_GetVal(callexpr->getArg(0));
			int64_t *p = (int64_t *)std::malloc(malloc_size);
			mStack.back().bindStmt(callexpr, (int64_t)p);
		}else if (callee == mFree){
			int64_t *p = (int64_t *)Expr_GetVal(callexpr->getArg(0));
			std::free(p);
		}else{  // other callee
			// cout<<"		other callee"<<endl;
			StackFrame stack;
			auto pit=callee->param_begin();
			for(auto it=callexpr->arg_begin(), ie=callexpr->arg_end();it!=ie;++it,++pit)
			{
				// int64_t val=mStack.back().getStmtVal(*it);
				stack.bindDecl(*pit,Expr_GetVal(*it));
			}
			mStack.push_back(stack);
	   	}
   	}
};


