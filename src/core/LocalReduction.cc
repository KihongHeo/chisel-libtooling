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

/*bool GlobalReductionCollectionVisitor::VisitVarDecl(VarDecl *VD) {
  if (VD->hasGlobalStorage()) {
    if (Option::verbose)
      llvm::outs() << "var decl " << VD->getNameAsString() << "\n";
    ConsumerInstance->decls.emplace_back(VD);
  }
  return true;
}

bool GlobalReductionCollectionVisitor::VisitRecordDecl(RecordDecl *RD) {
  ConsumerInstance->decls.emplace_back(RD);
  if (Option::verbose)
    llvm::outs() << "record decl " << RD->getNameAsString() << "\n";
  return true;
}

bool GlobalReductionCollectionVisitor::VisitTypedefDecl(TypedefDecl *TD) {
  if (Option::verbose)
    llvm::outs() << "typedef decl " << TD->getNameAsString() << "\n";
  ConsumerInstance->decls.emplace_back(TD);
  return true;
}

bool GlobalReductionCollectionVisitor::VisitEnumDecl(EnumDecl *ED) {
  if (Option::verbose)
    llvm::outs() << "enum decl " << ED->getNameAsString() << "\n";
  ConsumerInstance->decls.emplace_back(ED);
  return true;
}*/

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
  std::string revert = "";
  SourceLocation totalStart, totalEnd;
  totalStart = toBeRemoved.front()->getSourceRange().getBegin();
  for (auto const &d : toBeRemoved) {
    SourceLocation start = d->getSourceRange().getBegin();
    SourceLocation end;

    if (CompoundStmt *CS = dyn_cast<CompoundStmt>(d)) {
      end = CS->getSourceRange().getEnd().getLocWithOffset(1);
    } else if (IfStmt *IS = dyn_cast<IfStmt>(d)) {
      end = IS->getSourceRange().getEnd().getLocWithOffset(1);
    } else if (WhileStmt *WS = dyn_cast<WhileStmt>(d)) {
      end = WS->getSourceRange().getEnd().getLocWithOffset(1);
    } else {
      end = RewriteHelper->getEndLocationUntil(d->getSourceRange(), ';')
                .getLocWithOffset(1);
    }
    totalEnd = end;
  }
  llvm::StringRef ref = Lexer::getSourceText(
      CharSourceRange::getCharRange(SourceRange(totalStart, totalEnd)), *SM,
      LangOptions());
  revert = ref.str();
  std::string replacement = "";
  for (auto const &chr : revert) {
    if (chr == '\n')
      replacement += '\n';
    else if (isprint(chr))
      replacement += " ";
    else
      replacement += chr;
  }

  TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd), replacement);
  // auto buffer = TheRewriter.getRewriteBufferFor(
  //   Context->getSourceManager().getMainFileID());
  // if (buffer != nullptr)
  // buffer->write(llvm::outs());
  std::error_code error_code;
  llvm::raw_fd_ostream outFile("mkdir-5.2.1.c", error_code,
                               llvm::sys::fs::F_None);
  TheRewriter.getEditBuffer(Context->getSourceManager().getMainFileID())
      .write(outFile);
  outFile.close();
  if (system(Option::oracleFile.c_str()) == 0) {
    return true;
  } else {
    llvm::outs() << "******** REVERT *********\n";
    TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd), revert);
    std::error_code error_code2;
    llvm::raw_fd_ostream outFile2("mkdir-5.2.1.c", error_code2,
                                  llvm::sys::fs::F_None);
    TheRewriter.getEditBuffer(Context->getSourceManager().getMainFileID())
        .write(outFile2);
    outFile2.close();
    return false;
  }
}

void LocalReduction::ddmin(std::vector<clang::Stmt *> &stmts) {
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

void LocalReduction::localReduction(void) {
  for (auto const &body : functionBodies) {
    // hdd(body);
    llvm::outs() << "DDMIN ON FUNCTION!\n";
    std::vector<Stmt *> children;
    for (Stmt::child_iterator i = body->child_begin(), e = body->child_end();
         i != e; ++i) {
      Stmt *currStmt = *i;
      children.emplace_back(*i);
    }
    ddmin(children);
  }

  // const Expr *Cond = TheIfStmt->getCond();
  // TransAssert(Cond && "Bad Cond Expr!");
  // std::string CondStr;
  // RewriteHelper->getExprString(Cond, CondStr);
  // CondStr += ";";
  // RewriteHelper->addStringBeforeStmt(TheIfStmt, CondStr, NeedParen);

  // RewriteHelper->removeIfAndCond(TheIfStmt);

  // const Stmt *ElseS = TheIfStmt->getElse();
  // if (ElseS) {
  //  SourceLocation ElseLoc = TheIfStmt->getElseLoc();
  //  std::string ElseStr = "else";
  //  TheRewriter.RemoveText(ElseLoc, ElseStr.size());
  //}
}

LocalReduction::~LocalReduction(void) { delete CollectionVisitor; }
