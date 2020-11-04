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
		llvm::errs() << "		[*] bindStmt : " << stmt->getStmtClassName() << " " << stmt << " " << val << "\n";
	   	mExprs[stmt] = val;
   	}
   	int64_t getStmtVal(Stmt * stmt) {
		// llvm::errs() << "		[*] getstmtval first  : "<< mExprs.find(stmt)->first <<  " second : "<< mExprs.find(stmt)->second << "\n";
		llvm::errs() << "		[*] getstmtval : " << stmt->getStmtClassName() << " " << stmt << "\n";
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
	bool DeclExits(Decl * decl)
	{
		return mVars.find(decl) != mVars.end();
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
   	std::vector<StackFrame> mGlobal;
	
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
   	Environment() : mStack(), mGlobal(), mFree(NULL), mMalloc(NULL), mInput(NULL), mOutput(NULL), mEntry(NULL) {
   	}
	
   int64_t getDeclVal_GM(Decl * decl) {
	   //mstack找不到的时候去mGlobal找,实现子函数中使用全局变量
	   StackFrame my_Gstack = mGlobal.back();
	   StackFrame my_mStack = mStack.back();
	   if(my_mStack.DeclExits(decl))
		   return my_mStack.getDeclVal(decl);
	   else
		   return my_Gstack.getDeclVal(decl);
   }


   	/// Initialize the Environment
   	void init(TranslationUnitDecl * unit) {
		// put it in first ,otherwise th process global will segmentfault because no StackFrame.
		mStack.push_back(StackFrame());
		mGlobal.push_back(StackFrame());
	   	for (TranslationUnitDecl::decl_iterator i =unit->decls_begin(), e = unit->decls_end(); i != e; ++ i) {
		   	// bind global vardecl to stack
            if (VarDecl * vdecl = dyn_cast<VarDecl>(*i)) {
                llvm::errs() << "		global var decl: " << vdecl << "\n";
                if (vdecl->getType().getTypePtr()->isIntegerType() || vdecl->getType().getTypePtr()->isCharType() ||
					vdecl->getType().getTypePtr()->isPointerType())
				{
					if (vdecl->hasInit()){
						int64_t val = Expr_GetVal(vdecl->getInit());
						mStack.back().bindDecl(vdecl, val);
						mGlobal.back().bindDecl(vdecl, val); //
					}
					else{
						mStack.back().bindDecl(vdecl, 0);
						mGlobal.back().bindDecl(vdecl, 0);
					}
						
				}
				else
				{ // todo global array
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

    void intliteral(IntegerLiteral * intliteral) {         
		int64_t val = (int64_t)intliteral->getValue().getLimitedValue();       
		mStack.back().bindStmt(dyn_cast<Expr>(intliteral), val);   
	}

    void Character(CharacterLiteral * Character) {         
		int64_t val = (int64_t)Character->getValue();         
		mStack.back().bindStmt(dyn_cast<Expr>(Character), val);   
	}

	void parenexpr(ParenExpr * pexpr) {    
		llvm::errs() << "		parenexpr" << "\n";    
		Expr * expr = pexpr->getSubExpr();         
		int64_t val = mStack.back().getStmtVal(expr);         
		mStack.back().bindStmt(pexpr, val);     
	}

	void cast(CastExpr * castexpr) {
	   mStack.back().setPC(castexpr);
	   if (castexpr->getType()->isIntegerType()) {
		   int64_t val = mStack.back().getStmtVal(castexpr->getSubExpr());
		   mStack.back().bindStmt(castexpr, val);
	   } 
	   else if (castexpr->getType()->isPointerType()) {
		   if ( castexpr->getCastKind() == CK_LValueToRValue || castexpr->getCastKind() == CK_ArrayToPointerDecay || 
				castexpr->getCastKind() == CK_PointerToIntegral || castexpr->getCastKind() == CK_BitCast){
			   int64_t val = mStack.back().getStmtVal(castexpr->getSubExpr());
			   mStack.back().bindStmt(castexpr, val);
		   }
	   }else { 
			llvm::errs() << "		cast nothing" << "\n"; 
	   }
   }

   void arrayexpr(ArraySubscriptExpr * asexpr) {
	   int64_t *array = (int64_t *)mStack.back().getStmtVal(asexpr->getBase());
	   int64_t idx = mStack.back().getStmtVal(asexpr->getIdx());
			llvm::errs() << "		ArraySubscriptExpr asexpr" << array[idx] << "\n"; 

	   mStack.back().bindStmt(asexpr, array[idx]);
   }

	void mStack_bindStmt(CallExpr *call, int64_t retvalue){
		cout << "		push_func_stack_stmt = " << call << endl;
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
		
		llvm::errs() << "		binop left : " << left->getStmtClassName() << " " << left << "\n";	
		llvm::errs() << "		binop right : " << right->getStmtClassName() << " " << right << "\n";
		// isAssignmentOp : 判断是赋值语句还是一个 +-*/的语句
	   	if (bop->isAssignmentOp()) { 
			//if left expr is a refered expr, bind the right value to it
		   	if (DeclRefExpr * declexpr = dyn_cast<DeclRefExpr>(left)) {
				//获取发生此引用的NamedDecl,绑定右节点的值到左节点
				int64_t val = Expr_GetVal(right);
				mStack.back().bindStmt(left, val);
			   	Decl * decl = declexpr->getFoundDecl();
			   	mStack.back().bindDecl(decl, val);
		   	}else if (auto array = dyn_cast<ArraySubscriptExpr>(left))
			{
				if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(array->getLHS()->IgnoreImpCasts()))
				{
					Decl *decl = declexpr->getFoundDecl();
					int64_t val = mStack.back().getStmtVal(right);

					std::cout << "		binop ArraySubscriptExpr : " <<  val << endl;
					int64_t index = Expr_GetVal(array->getRHS());
					if (VarDecl *vardecl = dyn_cast<VarDecl>(decl)){
						if (auto array = dyn_cast<ConstantArrayType>(vardecl->getType().getTypePtr())){	
						   void * addr = (void *)mStack.back().getDeclVal(vardecl);
							if (array->getElementType().getTypePtr()->isIntegerType() || array->getElementType().getTypePtr()->isPointerType()){ // int64_t a[3];
								*((int64_t *)addr+index) = val;
							}else if (array->getElementType().getTypePtr()->isCharType()){ // char a[3];
								*((char *)addr+index) = (char)val;
							}
						}
					}
				}
			}else if (auto unaryExpr = dyn_cast<UnaryOperator>(left))
			{ // *(p+1)
				if( (unaryExpr->getOpcode()) == UO_Deref)
				{
					int64_t val = mStack.back().getStmtVal(right);
					int64_t addr = mStack.back().getStmtVal(unaryExpr->getSubExpr());
					int64_t *p = (int64_t *)addr;
					*p = val;
				}
			}
	   	}
		else{
			// 不是所有stmt都能getStmtVal，我们这里选择expr函数来进行解析
			int64_t result;
			switch (Opcode)
			{
			case BO_Add: // + 
					if(left->getType().getTypePtr()->isPointerType()) // 指针+8*index后存取
					{
						int64_t ptr = mStack.back().getStmtVal(left);
						result = ptr + 8 * Expr_GetVal(right);
					}else{
						result = Expr_GetVal(left) + Expr_GetVal(right);	
					}
				break;
			case BO_Sub: // -
				result = Expr_GetVal(left) - Expr_GetVal(right);
				break;
			case BO_Mul: // *
				result = Expr_GetVal(left) * Expr_GetVal(right);
				break;
			case BO_Div: //  / ; check the b can not be 0
				if (Expr_GetVal(right) == 0){
					llvm::errs() << "		the BinaryOperator /, can not div 0 " << "\n";
					exit(0);
				}
				result = Expr_GetVal(left) / Expr_GetVal(right);
				break;
			case BO_LT: // <
				result = (Expr_GetVal(left) < Expr_GetVal(right)) ? 1:0;
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
		cout << "		[*] decl !!!" << endl;
	   	for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end(); it != ie; ++ it) {
			//in ast, the sub-node is usually VarDecl
			Decl * decl = *it;
			//VarDecl 表示变量声明或定义
		   	if (VarDecl * vardecl = dyn_cast<VarDecl>(decl)) {
				if (vardecl->getType().getTypePtr()->isIntegerType() || vardecl->getType().getTypePtr()->isPointerType() 
					|| vardecl->getType().getTypePtr()->isCharType() )
				{
					int64_t val = 0;
					if (vardecl->hasInit()) {
						val = Expr_GetVal(vardecl->getInit());
					}
					mStack.back().bindDecl(vardecl, val);
				}else if(vardecl->getType().getTypePtr()->isConstantArrayType()) { //array
					if (auto array = dyn_cast<ConstantArrayType>(vardecl->getType().getTypePtr())){ // array declstmt, bind a's addr to the vardecl.
						int64_t size = array->getSize().getSExtValue();
						if (array->getElementType().getTypePtr()->isIntegerType()){ // int a[3]; 
							int64_t *my_array = new int64_t[size];
							for (int i = 0; i < size; i++)
								my_array[i] = 0x61;
							mStack.back().bindDecl(vardecl, (int64_t)my_array);
							std::cout << "		mMalloc : " <<  my_array << endl;
							std::cout << "		mMalloc : " <<  my_array[0] << endl;
						}else if (array->getElementType().getTypePtr()->isCharType()){ // char a[3];
							char *my_array = new char[size];
							for (int i = 0; i < size; i++)
								my_array[i] = 0;
							mStack.back().bindDecl(vardecl, (int64_t)my_array);
							std::cout << "		mMalloc : " <<  my_array << endl;
							std::cout << "		mMalloc : " <<  my_array[0] << endl;
						}else if(array->getElementType().getTypePtr()->isPointerType()){  // int* a[3];
							int64_t **my_array = new int64_t *[size];
							for (int i = 0; i < size; i++)
								my_array[i] = 0;
							mStack.back().bindDecl(vardecl, (int64_t)my_array);
							std::cout << "		mMalloc : " <<  my_array << endl;
							std::cout << "		mMalloc : " <<  my_array[0] << endl;
						}
					}
				}
		   	}
	   	}
   	}

    // 对已声明的变量，函数，枚举等的引用
   	void declref(DeclRefExpr * declref) {
		llvm::errs() << "		declref : " << declref->getFoundDecl()->getNameAsString() << "\n";
	   	mStack.back().setPC(declref);
		if (declref->getType()->isCharType() || declref->getType()->isPointerType() || declref->getType()->isIntegerType()){
			Decl *decl = declref->getFoundDecl();
			int64_t val = getDeclVal_GM(decl);
			mStack.back().bindStmt(declref, val);
	   	} else if (declref->getType()->isArrayType()) {
		   Decl * decl = declref->getFoundDecl();
		   int64_t val = getDeclVal_GM(decl);
		   mStack.back().bindStmt(declref, val);
		}
		else{
			llvm::errs() << "		declref nothing" <<"\n";
		}

   	}

	//get the condition value of IfStmt and WhileStmt
   	bool getcond(/*BinaryOperator *bop*/Expr *expr)
   	{
		cout<<"		getcond"<<endl;
   		return mStack.back().getStmtVal(expr);
   	}

	void returnstmt(ReturnStmt *returnStmt)
	{
		cout << "		get returnstmt !!!" << endl;
		int64_t value = Expr_GetVal(returnStmt->getRetValue());
		setReturn(true, value);
	}

   //process UnaryExprOrTypeTraitExpr, e.g. sizeof
   void unarysizeof(UnaryExprOrTypeTraitExpr *uop)
   {
		cout<<"		UnaryExprOrTypeTraitExpr type call unarysizeof"<<endl;
   	  	//if UnaryExprOrTypeTraitExpr is sizeof,
	   if(auto sizeofexpr = dyn_cast<UnaryExprOrTypeTraitExpr>(uop))
	   {
			if(sizeofexpr->getKind() == UETT_SizeOf ||  sizeofexpr->getArgumentType()->isPointerType())
			{
				//if the arg type is integer type, we bind sizeof(long) to UnaryExprOrTypeTraitExpr
				if(sizeofexpr->getArgumentType()->isIntegerType()|| sizeofexpr->getArgumentType()->isPointerType())
				{
					int64_t val = sizeof(int64_t);
					mStack.back().bindStmt(uop,val);
				}else{
					cout<<"		unarysizeof nothing"<<endl;
				}  
			}
	   }
   }
	// 这表示一元表达式（sizeof和alignof除外），postfix-expression中的postinc / postdec运算符以及各种扩展名
	void unaryop(UnaryOperator *unaryExpr) 
	{ // - +
		// Clang/AST/Expr.h/ line 1714
		cout<<"		UnaryOperator type call unaryop"<<endl;
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
			mStack.back().bindStmt(unaryExpr, *(int64_t *)Expr_GetVal(exp));
			llvm::errs() << "unaryop :" << Expr_GetVal(exp) << "\n";
			// llvm::errs() << "unaryop :" << *(Expr_GetVal(exp)) << "\n";
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
		   int64_t malloc_size = mStack.back().getStmtVal(callexpr->getArg(0));
			// int64_t malloc_size = Expr_GetVal(callexpr->getArg(0));
			int64_t *p = (int64_t *)std::malloc(malloc_size);
			std::cout << "	mMalloc : " <<  (int64_t)p << endl;
			std::cout << "	mMalloc : " <<  *p << endl;
			mStack.back().bindStmt(callexpr, (int64_t)p);
		}else if (callee == mFree){
			int64_t *p = (int64_t *)Expr_GetVal(callexpr->getArg(0));
			std::free(p);
		}else{  // other callee
			cout<<"		other callee"<<endl;
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

	int64_t Expr_GetVal(Expr *exp)
	{
		//跳过可能围绕此表达式的所有隐式强制转换，直到达到固定点为止
		exp = exp->IgnoreImpCasts();
		if (auto decl = dyn_cast<DeclRefExpr>(exp)){
			cout << "		DeclRefExpr" << getDeclVal_GM(decl->getDecl()) <<"\n";
			return getDeclVal_GM(decl->getDecl());
		}else if (auto intLiteral = dyn_cast<IntegerLiteral>(exp)){     //a = 12
			cout << "		IntegerLiteral" << intLiteral->getValue().getSExtValue() <<"\n";
			return intLiteral->getValue().getSExtValue(); 
		}else if (auto unaryExpr = dyn_cast<UnaryOperator>(exp)){      // a = -13 and a = +12;
			unaryop(unaryExpr);
			cout << "		UnaryOperator" << mStack.back().getStmtVal(unaryExpr) <<"\n";
			int64_t result = mStack.back().getStmtVal(unaryExpr);
			return result;
		}else if (auto charLiteral = dyn_cast<CharacterLiteral>(exp)){  // a = 'a'
			cout << "		CharacterLiteral" << charLiteral->getValue() <<"\n";
			return charLiteral->getValue(); // Clang/AST/Expr.h/ line 1369
		}else if (auto binaryExpr = dyn_cast<BinaryOperator>(exp)){     //+ - * / < > ==
			binop(binaryExpr); // 这个是为了在for语句的时候直接解析`a < 10`语句而不调用visit->binop
			cout << "		BinaryOperator" << mStack.back().getStmtVal(binaryExpr) <<"\n";
			return mStack.back().getStmtVal(binaryExpr);
		}else if (auto callexpr = dyn_cast<CallExpr>(exp)){
			cout << "		CallExpr" << mStack.back().getStmtVal(callexpr) <<"\n";
			return mStack.back().getStmtVal(callexpr);
		}else {
			cout << "		null" << mStack.back().getStmtVal(exp) <<"\n";
		   	return mStack.back().getStmtVal(exp);
	   	}
		llvm::errs() << "		have not handle this situation" << "\n";
		return 0;
	}
	
};