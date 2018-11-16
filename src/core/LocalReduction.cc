#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "CommonStatementVisitor.h"
#include "LocalReduction.h"
#include "Options.h"
#include "RewriteUtils.h"
#include "TransformationManager.h"
#include "VectorUtils.h"
#include "StringUtils.h"

using namespace clang;

static const char *DescriptionMsg = "Perform local-level reduction";

class LocalReductionCollectionVisitor
    : public RecursiveASTVisitor<LocalReductionCollectionVisitor> {
public:
  explicit LocalReductionCollectionVisitor(LocalReduction *Instance)
      : ConsumerInstance(Instance) {}

  bool VisitFunctionDecl(FunctionDecl *FD);

private:
  LocalReduction *ConsumerInstance;
};

static RegisterTransformation<LocalReduction> Trans("local-reduction",
                                                    DescriptionMsg);

bool LocalReductionCollectionVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  if (Option::verbose)
    llvm::outs() << "function decl " << FD->getNameInfo().getAsString() << "\n";
  if (FD->isThisDeclarationADefinition()) {
    ConsumerInstance->functionBodies.emplace_back(FD->getBody());
  }
  return true;
}

void LocalReduction::Initialize(ASTContext &context) {
  Transformation::Initialize(context);
  CollectionVisitor = new LocalReductionCollectionVisitor(this);
}

bool LocalReduction::HandleTopLevelDecl(DeclGroupRef D) {
  for (DeclGroupRef::iterator I = D.begin(), E = D.end(); I != E; ++I) {
    CollectionVisitor->TraverseDecl(*I);
  }
  return true;
}

void LocalReduction::HandleTranslationUnit(ASTContext &Ctx) {
  localReduction();
}

bool LocalReduction::testEmpty() {
  if (system(Option::oracleFile.c_str()) == 0)
    return true;
  return false;
}

bool LocalReduction::test(std::vector<clang::Stmt *> &toBeRemoved) {
  const SourceManager *SM = &Context->getSourceManager();
  std::string revert = "";
  SourceLocation totalStart, totalEnd;
  totalStart = toBeRemoved.front()->getSourceRange().getBegin();
  
  Stmt* last = toBeRemoved.back();
    if (CompoundStmt *CS = dyn_cast<CompoundStmt>(last)) {
      totalEnd = CS->getSourceRange().getEnd().getLocWithOffset(1);
    } else if (IfStmt *IS = dyn_cast<IfStmt>(last)) {
      totalEnd = IS->getSourceRange().getEnd().getLocWithOffset(1);
    } else if (WhileStmt *WS = dyn_cast<WhileStmt>(last)) {
      totalEnd = WS->getSourceRange().getEnd().getLocWithOffset(1);
    } else if (DeclStmt *DS = dyn_cast<DeclStmt>(last)) {
      totalEnd = DS->getSourceRange().getEnd().getLocWithOffset(1);
    } else {
      totalEnd = RewriteHelper->getEndLocationUntil(last->getSourceRange(), ';')
                .getLocWithOffset(1);
    }
  revert = Transformation::getSourceText(SourceRange(totalStart, totalEnd));
  TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd), StringUtils::placeholder(revert));
   //auto buffer = TheRewriter.getRewriteBufferFor(
   // Context->getSourceManager().getMainFileID());
   //if (buffer != nullptr)
   //  buffer->write(llvm::outs());
   //llvm::outs() << "===========================\n";
   Transformation::writeToFile(Option::inputFile);
  if (system(Option::oracleFile.c_str()) == 0) {
    return true;
  } else {
    TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd), revert);
    Transformation::writeToFile(Option::inputFile);
    return false;
  }
}

void LocalReduction::ddmin(std::vector<clang::Stmt *> stmts) {
  std::vector<Stmt *> stmts_;
  stmts_ = std::move(stmts);
  int n = 2;
  while (stmts_.size() >= 1) {
    std::vector<std::vector<clang::Stmt *>> subsets =
        VectorUtils::split<clang::Stmt *>(stmts_, n);
    bool complementSucceeding = false;

    for (std::vector<Stmt *> &subset : subsets) {
      std::vector<Stmt *> complement =
          VectorUtils::difference<clang::Stmt *>(stmts_, subset);
      bool status = test(subset);
      if (status) {
        stmts_ = std::move(complement);
        n = std::max(n - 1, 2);
        complementSucceeding = true;
        break;
      }
    }

    if (!complementSucceeding) {
      if (n == stmts_.size()) {
        break;
      }

      n = std::min(n * 2, static_cast<int>(stmts_.size()));
    }
  }
}

std::vector<Stmt *> &LocalReduction::getImmediateChildren(clang::Stmt *s) {
  std::vector<Stmt *> children;
  for (Stmt::child_iterator i = s->child_begin(), e = s->child_end(); i != e;
       ++i) {
    Stmt *currStmt = *i;
    children.emplace_back(currStmt);
  }
  return children;
}

std::vector<Stmt *> LocalReduction::getBodyStatements(clang::CompoundStmt *s) {
  std::vector<Stmt *> stmts;
  for (clang::CompoundStmt::body_iterator i = s->body_begin(),
                                          e = s->body_end();
       i != e; ++i) {
    Stmt *currStmt = *i;
    stmts.emplace_back(currStmt);
  }
  return stmts;
}

void LocalReduction::reduceIf(IfStmt* IS) {
  // remove else branch
  SourceLocation beginIf = IS->getIfLoc();
  llvm::outs() << "BEGIN IF:";
  beginIf.dump(Context->getSourceManager());
  llvm::outs() << "\n";
  SourceLocation endIf = IS->getLocEnd();
  llvm::outs() << "END IF:";
  endIf.dump(Context->getSourceManager());
  llvm::outs() << "\n";
  llvm::outs() << "END COND: ";
  SourceLocation endCond = IS->getThen()->getLocStart().getLocWithOffset(-1);
  endCond.dump(Context->getSourceManager());
  llvm::outs() << "\n";
  SourceLocation endThen = IS->getThen()->getLocEnd();
  std::string revertIf = Transformation::getSourceText(IS->getSourceRange());
  
  std::string ifAndCond = Transformation::getSourceText(SourceRange(beginIf, endCond));
  TheRewriter.ReplaceText(SourceRange(beginIf, endCond), StringUtils::placeholder(ifAndCond));

  const Stmt *Else = IS->getElse();
  SourceLocation elseLoc;
  if (Else) {
    elseLoc = IS->getElseLoc();
    std::string elsePart = Transformation::getSourceText(SourceRange(elseLoc, endIf));
    TheRewriter.ReplaceText(SourceRange(elseLoc, endIf), StringUtils::placeholder(elsePart));
  }

  Transformation::writeToFile(Option::inputFile);
  if (testEmpty()) {
    q.push(IS->getThen());
  } else {
    // revert
    TheRewriter.ReplaceText(SourceRange(beginIf, endIf), StringUtils::placeholder(revertIf));
    Transformation::writeToFile(Option::inputFile);
    // try removing then branch
    std::string thenPart = Transformation::getSourceText(SourceRange(beginIf, endThen));
    TheRewriter.ReplaceText(SourceRange(beginIf), StringUtils::placeholder(thenPart));
    
    if (Else)
      TheRewriter.ReplaceText(elseLoc, 4, "    "); // len("else") = 4
      
    Transformation::writeToFile(Option::inputFile);
    if (testEmpty()) {
      if (Else)
        q.push(IS->getElse());
    } else {
      // revert
      TheRewriter.ReplaceText(SourceRange(beginIf, endIf), StringUtils::placeholder(revertIf));
      Transformation::writeToFile(Option::inputFile);
      q.push(IS->getThen());
      if (Else)
        q.push(IS->getElse());
    }
  }
}

void LocalReduction::reduceWhile(WhileStmt *WS) {
  q.push(WS->getBody());
}

void LocalReduction::reduceCompound(CompoundStmt *CS) {
  auto stmts = getBodyStatements(CS);
  for (auto stmt : stmts)
    q.push(stmt);
  ddmin(stmts);
}

void LocalReduction::hdd(Stmt *s) {
  if (s == NULL)
    return;
  if (IfStmt *IS = dyn_cast<IfStmt>(s)) {
    reduceIf(IS);
  } else if (WhileStmt *WS = dyn_cast<WhileStmt>(s)) {
    reduceWhile(WS);
  } else if (CompoundStmt *CS = dyn_cast<CompoundStmt>(s)) {
    reduceCompound(CS); 
  }
}

void LocalReduction::localReduction(void) {
  for (auto const &body : functionBodies) {
    q.push(body);
    while (!q.empty()) {
      Stmt *s = q.front();
      q.pop();
      hdd(s);
    }
  }
}

LocalReduction::~LocalReduction(void) { delete CollectionVisitor; }
