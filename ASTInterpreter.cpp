//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

#include "Environment.h"

class InterpreterVisitor : 
   public EvaluatedExprVisitor<InterpreterVisitor> {
public:
   explicit InterpreterVisitor(const ASTContext &context, Environment * env)
   : EvaluatedExprVisitor(context), mEnv(env) {}
   virtual ~InterpreterVisitor() {}

   // virtual void VisitIntegerLiteral(IntegerLiteral * intliteral) {
   //    llvm::errs() << "[+] visit IntegerLiteral\n";
   //    mEnv->intliteral(intliteral);
   // }

   // process BinaryOperator,e.g. assignment, add and etc.
   virtual void VisitBinaryOperator (BinaryOperator * bop) {
      if(mEnv->haveReturn()){
         return;
      } 
      // llvm::errs() << "[+] visit BinaryOperator\n";
	   //VisitStmt : 分析表达式，分析该节点下所有子树节点，依次进行深度优先遍历的递归调用去获取函数的值，有些子节点比如说VisitIntegerLiteral下不会再有子树，则不需要visit
      VisitStmt(bop);
      // llvm::errs() << "[+] visitStmt BinaryOperator done\n";
	   mEnv->binop(bop);
   }

   // process DeclRefExpr, e.g. refered decl expr
   virtual void VisitDeclRefExpr(DeclRefExpr * expr) {
      if(mEnv->haveReturn()){
         return;
      }  
      // llvm::errs() << "[+] visit DeclRefExpr\n";
	   VisitStmt(expr);
      // llvm::errs() << "[+] visitStmt VisitDeclRefExpr done\n";
	   mEnv->declref(expr);
   }

   // process CastExpr
   // virtual void VisitCastExpr(CastExpr * expr) {
   //    llvm::errs() << "[+] visit CastExpr\n";
	//    VisitStmt(expr);
   //    llvm::errs() << "[+] visitStmt VisitCastExpr done\n";
	//    mEnv->cast(expr);
   // }

   // process CallExpr,e.g. function call
   virtual void VisitCallExpr(CallExpr * call) {
      if(mEnv->haveReturn()){
         return;
      }  
      // llvm::errs() << "[+] visit CallExpr\n";
	   VisitStmt(call);
	   mEnv->call(call);

      if (FunctionDecl *callee = call->getDirectCallee()){
         if ((!callee->getName().equals("GET")) && (!callee->getName().equals("PRINT")) &&
            (!callee->getName().equals("MALLOC")) && (!callee->getName().equals("FREE"))){
               Stmt *body=callee->getBody();
               if(body && isa<CompoundStmt>(body) )
               {
                  //visit the function body
                  Visit(body);
                  int64_t retvalue = mEnv->getReturn();
                  mEnv->mStack_pop_back();
                  mEnv->mStack_bindStmt(call, retvalue);
               }  
         }
      }

   }

   virtual void VisitIfStmt(IfStmt *ifstmt) {
      if(mEnv->haveReturn()){
         return;
      }  
      //get the condition expr and visit relevant node in ast
      Expr *expr=ifstmt->getCond();
      Visit(expr);
      //cout<<expr->getStmtClassName()<<endl;
      //BinaryOperator * bop = dyn_cast<BinaryOperator>(expr);
      //get the bool value of condition expr
      bool cond=mEnv->getcond(expr);
      //if condition value is true, visit then block,else visit else block
      if(cond)
      {
        Visit(ifstmt->getThen()); // must use the Visit, not the VisitStmt
      }
      else
      {
        Stmt *else_block=ifstmt->getElse();
        //if else block really exists
        if(else_block)
          Visit(else_block);
      }
   }
   
   //process WhileStmt
   virtual void VisitWhileStmt(WhileStmt *whilestmt) {
      if(mEnv->haveReturn()){
         return;
      }  
      //get the condition expr of WhileStmt in ast,and visit relevant node
      Expr *expr = whilestmt->getCond();
      Visit(expr);
      //BinaryOperator *bop = dyn_cast<BinaryOperator>(expr);
      //get the condition value of WhileStmt,if it is true, visit the body of WhileStmt
      bool cond=mEnv->getcond(expr);
      Stmt *body=whilestmt->getBody();
      while(cond)
      {
        if( body && isa<CompoundStmt>(body) )
        {
          VisitStmt(whilestmt->getBody());
        }
        //update the condition value
        Visit(expr);
        cond=mEnv->getcond(expr);
      }
   }

   //process ForStmt
   //https://clang.llvm.org/doxygen/Stmt_8h_source.html#l2451
   virtual void VisitForStmt(ForStmt *forstmt){
      // llvm::errs() << "[+] visit VisitForStmt\n";
      if(mEnv->haveReturn()){
         return;
      }  
      if(Stmt *init = forstmt->getInit()){
         Visit(init);
      }
      for(; mEnv->Expr_GetVal(forstmt->getCond()); Visit(forstmt->getInc())){ //getCond 返回值是bool，而不是void，不能使用Visit，只能直接利用expr对该语句进行解析from ‘void’ to ‘bool’
         Stmt *body=forstmt->getBody();
         if(body && isa<CompoundStmt>(body)){
            Visit(forstmt->getBody());
         }
            
      }
   }

   // process return stmt.
   virtual void VisitReturnStmt(ReturnStmt *returnStmt)
   {
      //isCurFunctionReturn
      if(mEnv->haveReturn()){
         return;
      }
      Visit(returnStmt->getRetValue());
      mEnv->returnstmt(returnStmt);
   }

   // process DeclStmt,e.g. int a; int a=c+d; and etc.
   virtual void VisitDeclStmt(DeclStmt * declstmt) {
      if(mEnv->haveReturn()){
         return;
      }  
      // llvm::errs() << "[+] visit DeclStmt\n";
	   mEnv->decl(declstmt);
   }
   //process UnaryOperator, e.g. -, * and etc.
   // virtual void VisitUnaryOperator (UnaryOperator * uop) {
   //    if(mEnv->haveReturn()){
   //       return;
   //    }  
   //    llvm::errs() << "[+] visit VisitUnaryOperator\n";
   //    VisitStmt(uop);
   //    mEnv->unaryop(uop);
   // }

   // //process UnaryExprOrTypeTraitExpr, e.g. sizeof and etc.
   // virtual void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *uop)
   // {
   //    if(mEnv->haveReturn()){
   //       return;
   //    }  
   //    llvm::errs() << "[+] visit VisitUnaryExprOrTypeTraitExpr\n";
   //    VisitStmt(uop);
   //    mEnv->unarysizeof(uop);
   // }
   // process ArraySubscriptExpr, e.g. int [2]
   // virtual void VisitArraySubscriptExpr(ArraySubscriptExpr *arrayexpr)
   // {
   //    if(mEnv->haveReturn()){
   //       return;
   //    }  
   //    // llvm::errs() << "[+] visit VisitArraySubscriptExpr\n";
   //    return ;
   //    // VisitStmt(arrayexpr);
   //    // mEnv->array(arrayexpr);
   // }

private:
   Environment * mEnv;
};

class InterpreterConsumer : public ASTConsumer {
public:
   explicit InterpreterConsumer(const ASTContext& context) : mEnv(),
   	   mVisitor(context, &mEnv) {
   }
   virtual ~InterpreterConsumer() {}

   virtual void HandleTranslationUnit(clang::ASTContext &Context) {
	   TranslationUnitDecl * decl = Context.getTranslationUnitDecl();
	   mEnv.init(decl);

	   FunctionDecl * entry = mEnv.getEntry();
	   mVisitor.VisitStmt(entry->getBody());
   }
private:
   Environment mEnv;
   InterpreterVisitor mVisitor;
};

class InterpreterClassAction : public ASTFrontendAction {
public: 
   virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
      return std::unique_ptr<clang::ASTConsumer>(
         new InterpreterConsumer(Compiler.getASTContext()));
   }
};

int main (int argc, char ** argv) {
   if (argc > 1) {
      clang::tooling::runToolOnCode(std::unique_ptr<clang::FrontendAction>(new InterpreterClassAction), argv[1]);
   }
}
