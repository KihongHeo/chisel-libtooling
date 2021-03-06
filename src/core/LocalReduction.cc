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
#include "Report.h"
#include "RewriteUtils.h"
#include "StringUtils.h"
#include "TransformationManager.h"
#include "VectorUtils.h"

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

bool LocalReduction::test(std::vector<clang::Stmt *> &toBeRemoved) {
  const SourceManager *SM = &Context->getSourceManager();
  SourceLocation totalStart, totalEnd;
  totalStart = toBeRemoved.front()->getSourceRange().getBegin();
  Stmt *last = toBeRemoved.back();

  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(last)) {
    totalEnd = CS->getRBracLoc().getLocWithOffset(1);
  } else if (IfStmt *IS = dyn_cast<IfStmt>(last)) {
    totalEnd = last->getSourceRange().getEnd().getLocWithOffset(1);
  } else if (WhileStmt *WS = dyn_cast<WhileStmt>(last)) {
    totalEnd = last->getSourceRange().getEnd().getLocWithOffset(1);
  } else if (LabelStmt *LS = dyn_cast<LabelStmt>(last)) {
    auto subStmt = LS->getSubStmt();
    if (CompoundStmt *LS_CS = dyn_cast<CompoundStmt>(subStmt)) {
      totalEnd = LS_CS->getRBracLoc().getLocWithOffset(1);
    } else {
      totalEnd =
          RewriteHelper->getEndLocationUntil(subStmt->getSourceRange(), ';')
              .getLocWithOffset(1);
    }
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(last)) {
    totalEnd = RewriteHelper->getEndLocationAfter(last->getSourceRange(), ';');
  } else if (ReturnStmt *RS = dyn_cast<ReturnStmt>(last)) {
    totalEnd = RewriteHelper->getEndLocationAfter(RS->getSourceRange(), ';');
  } else if (GotoStmt *GS = dyn_cast<GotoStmt>(last)) {
    totalEnd = RewriteHelper->getEndLocationUntil(last->getSourceRange(), ';')
                   .getLocWithOffset(1);
  } else if (BreakStmt *BS = dyn_cast<BreakStmt>(last)) {
    totalEnd = RewriteHelper->getEndLocationUntil(last->getSourceRange(), ';')
                   .getLocWithOffset(1);
  } else if (ContinueStmt *CS = dyn_cast<ContinueStmt>(last)) {
    totalEnd = RewriteHelper->getEndLocationUntil(last->getSourceRange(), ';')
                   .getLocWithOffset(1);
  } else if (DeclStmt *DS = dyn_cast<DeclStmt>(last)) {
    totalEnd = RewriteHelper->getEndLocationAfter(last->getSourceRange(), ';');
  } else if (CallExpr *CE = dyn_cast<CallExpr>(last)) {
    totalEnd = RewriteHelper->getEndLocationUntil(last->getSourceRange(), ';')
                   .getLocWithOffset(1);
  } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(last)) {
    totalEnd = RewriteHelper->getEndLocationUntil(last->getSourceRange(), ';')
                   .getLocWithOffset(1);
  } else {
    return false;
  }

  if (totalEnd.isInvalid() || totalStart.isInvalid())
    return false;

  std::string revert =
      Transformation::getSourceText(SourceRange(totalStart, totalEnd));
  TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd),
                          StringUtils::placeholder(revert));
  Transformation::writeToFile(Option::inputFile);

  if (Transformation::callOracle("local")) {
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

std::vector<Stmt *> LocalReduction::getImmediateChildren(clang::Stmt *s) {
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

void LocalReduction::reduceIf(IfStmt *IS) {
  // remove else branch
  SourceLocation beginIf = IS->getSourceRange().getBegin();
  SourceLocation endIf = IS->getSourceRange().getEnd().getLocWithOffset(1);
  SourceLocation endCond =
      IS->getThen()->getSourceRange().getBegin().getLocWithOffset(-1);
  SourceLocation endThen = IS->getThen()->getSourceRange().getEnd();
  SourceLocation elseLoc;

  Stmt *Then = IS->getThen();
  Stmt *Else = IS->getElse();
  Stmt *ElseAsCompoundStmt;
  if (Else) {
    elseLoc = IS->getElseLoc();
    ElseAsCompoundStmt = getImmediateChildren(IS).back();
    if (elseLoc.isInvalid())
      return;
  }

  if (beginIf.isInvalid() || endIf.isInvalid() || endCond.isInvalid() ||
      endThen.isInvalid())
    return;

  std::string revertIf =
      Transformation::getSourceText(SourceRange(beginIf, endIf));

  if (Else) { // then, else
    // remove else branch
    std::string ifAndCond =
        Transformation::getSourceText(SourceRange(beginIf, endCond));
    TheRewriter.ReplaceText(SourceRange(beginIf, endCond),
                            StringUtils::placeholder(ifAndCond));
    std::string elsePart =
        Transformation::getSourceText(SourceRange(elseLoc, endIf));
    TheRewriter.ReplaceText(SourceRange(elseLoc, endIf),
                            StringUtils::placeholder(elsePart));
    Transformation::writeToFile(Option::inputFile);
    if (Transformation::callOracle("if")) { // successfully remove else branch
      q.push(Then);
    } else { // revert else branch removal
      TheRewriter.ReplaceText(SourceRange(beginIf, endIf), revertIf);
      Transformation::writeToFile(Option::inputFile);
      // remove then branch
      std::string ifAndThenAndElseWord = Transformation::getSourceText(
          SourceRange(beginIf, elseLoc.getLocWithOffset(4)));
      TheRewriter.ReplaceText(SourceRange(beginIf, elseLoc.getLocWithOffset(4)),
                              StringUtils::placeholder(ifAndThenAndElseWord));
      Transformation::writeToFile(Option::inputFile);
      if (Transformation::callOracle("if")) { // successfully remove then branch
        q.push(ElseAsCompoundStmt);
      } else { // revert then branch removal
        TheRewriter.ReplaceText(SourceRange(beginIf, endIf), revertIf);
        Transformation::writeToFile(Option::inputFile);
        q.push(Then);
        q.push(ElseAsCompoundStmt);
      }
    }
  } else { // then
    // remove condition
    std::string ifAndCond =
        Transformation::getSourceText(SourceRange(beginIf, endCond));
    TheRewriter.ReplaceText(SourceRange(beginIf, endCond),
                            StringUtils::placeholder(ifAndCond));
    Transformation::writeToFile(Option::inputFile);
    if (Transformation::callOracle("if")) {
      q.push(Then);
    } else {
      TheRewriter.ReplaceText(SourceRange(beginIf, endIf), revertIf);
      Transformation::writeToFile(Option::inputFile);
      q.push(Then);
    }
  }
}

void LocalReduction::reduceWhile(WhileStmt *WS) {
  auto body = WS->getBody();
  SourceLocation beginWhile = WS->getSourceRange().getBegin();
  SourceLocation endWhile = WS->getSourceRange().getEnd().getLocWithOffset(1);
  SourceLocation endCond =
      body->getSourceRange().getBegin().getLocWithOffset(-1);

  std::string revertWhile =
      Transformation::getSourceText(SourceRange(beginWhile, endWhile));

  std::string whileAndCond =
      Transformation::getSourceText(SourceRange(beginWhile, endCond));
  TheRewriter.ReplaceText(SourceRange(beginWhile, endCond),
                          StringUtils::placeholder(whileAndCond));
  Transformation::writeToFile(Option::inputFile);
  if (Transformation::callOracle("loop")) {
    q.push(body);
  } else {
    // revert
    TheRewriter.ReplaceText(SourceRange(beginWhile, endWhile), revertWhile);
    Transformation::writeToFile(Option::inputFile);
    q.push(body);
  }
}

void LocalReduction::reduceCompound(CompoundStmt *CS) {
  auto stmts = getBodyStatements(CS);
  for (auto stmt : stmts)
    q.push(stmt);
  ddmin(stmts);
}

void LocalReduction::reduceLabel(LabelStmt *LS) {
  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(LS->getSubStmt())) {
    q.push(CS);
  }
}

void LocalReduction::hdd(Stmt *s) {
  if (s == NULL)
    return;

  if (IfStmt *IS = dyn_cast<IfStmt>(s)) {
    if (Option::verbose)
      llvm::outs() << "hhd: if\n";
    reduceIf(IS);
  } else if (WhileStmt *WS = dyn_cast<WhileStmt>(s)) {
    if (Option::verbose)
      llvm::outs() << "hdd: while\n";
    reduceWhile(WS);
  } else if (CompoundStmt *CS = dyn_cast<CompoundStmt>(s)) {
    if (Option::verbose)
      llvm::outs() << "hdd: compound\n";
    reduceCompound(CS);
  } else if (LabelStmt *LS = dyn_cast<LabelStmt>(s)) {
    if (Option::verbose)
      llvm::outs() << "hdd: label\n";
    reduceLabel(LS);
  } else {
    return;
    // TODO add other cases: For, Do, Switch...
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
