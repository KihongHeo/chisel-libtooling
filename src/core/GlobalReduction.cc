#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "CommonStatementVisitor.h"
#include "GlobalReduction.h"
#include "Options.h"
#include "RewriteUtils.h"
#include "TransformationManager.h"
#include "VectorUtils.h"

using namespace clang;

static const char *DescriptionMsg = "Perform global-level reduction";

class GlobalReductionCollectionVisitor
    : public RecursiveASTVisitor<GlobalReductionCollectionVisitor> {
public:
  explicit GlobalReductionCollectionVisitor(GlobalReduction *Instance)
      : ConsumerInstance(Instance) {}

  bool VisitFunctionDecl(FunctionDecl *FD);
  bool VisitVarDecl(VarDecl *VD);
  bool VisitRecordDecl(RecordDecl *RD);
  bool VisitTypedefDecl(TypedefDecl *TD);
  bool VisitEnumDecl(EnumDecl *ED);

private:
  GlobalReduction *ConsumerInstance;
};

static RegisterTransformation<GlobalReduction> Trans("global-reduction",
                                                     DescriptionMsg);

bool GlobalReductionCollectionVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  if (Option::verbose)
    llvm::outs() << "function decl " << FD->getNameInfo().getAsString() << "\n";
  ConsumerInstance->decls.emplace_back(FD);
  return true;
}

bool GlobalReductionCollectionVisitor::VisitVarDecl(VarDecl *VD) {
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
}

void GlobalReduction::Initialize(ASTContext &context) {
  Transformation::Initialize(context);
  CollectionVisitor = new GlobalReductionCollectionVisitor(this);
}

bool GlobalReduction::HandleTopLevelDecl(DeclGroupRef D) {
  for (DeclGroupRef::iterator I = D.begin(), E = D.end(); I != E; ++I) {
    CollectionVisitor->TraverseDecl(*I);
  }
  return true;
}

void GlobalReduction::HandleTranslationUnit(ASTContext &Ctx) {
  globalReduction();
}

void GlobalReduction::prettyPrintSubset(std::vector<clang::Decl *> vec) {
  for (auto d : vec) {
    d->dump();
    llvm::outs() << "===============\n";
  }
  llvm::outs() << "\n";
}

bool GlobalReduction::test(std::vector<clang::Decl *> &toBeRemoved) {
  const SourceManager *SM = &Context->getSourceManager();
  std::string revert = "";
  SourceLocation totalStart, totalEnd;
  totalStart = toBeRemoved.front()->getSourceRange().getBegin();
  for (auto d : toBeRemoved) {
    SourceLocation start = d->getSourceRange().getBegin();
    SourceLocation end;

    FunctionDecl *FD = dyn_cast<FunctionDecl>(d);
    if (FD && FD->isThisDeclarationADefinition()) {
      end = FD->getSourceRange().getEnd().getLocWithOffset(1);
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
  for (auto chr : revert) {
    if (chr == '\n')
      replacement += '\n';
    else if (isprint(chr))
      replacement += " ";
    else
      replacement += chr;
  }

  TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd), replacement);
  // auto buffer = TheRewriter.getRewriteBufferFor(
  //    Context->getSourceManager().getMainFileID());
  // if (buffer != nullptr)
  //  buffer->write(llvm::outs());
  std::error_code error_code;
  llvm::raw_fd_ostream outFile("conditional.c", error_code,
                               llvm::sys::fs::F_None);
  TheRewriter.getEditBuffer(Context->getSourceManager().getMainFileID())
      .write(outFile);
  outFile.close();
  if (system(Option::oracleFile.c_str()) == 0) {
    return true;
  } else {
    TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd), revert);
    std::error_code error_code2;
    llvm::raw_fd_ostream outFile2("conditional.c", error_code2,
                                  llvm::sys::fs::F_None);
    TheRewriter.getEditBuffer(Context->getSourceManager().getMainFileID())
        .write(outFile2);
    outFile2.close();
    return false;
  }
}

void GlobalReduction::ddmin(std::vector<clang::Decl *> &decls) {
  std::vector<Decl *> decls_;
  decls_ = std::move(decls);
  int n = 2;
  while (decls_.size() >= 1) {
    std::vector<std::vector<clang::Decl *>> subsets =
        VectorUtils::split<clang::Decl *>(decls_, n);
    bool complementSucceeding = false;

    for (std::vector<Decl *> subset : subsets) {
      std::vector<Decl *> complement =
          VectorUtils::difference<clang::Decl *>(decls_, subset);
      bool status = test(subset);
      if (status) {
        decls_ = std::move(complement);
        n = std::max(n - 1, 2);
        complementSucceeding = true;
        break;
      }
    }

    if (!complementSucceeding) {
      if (n == decls_.size()) {
        break;
      }

      n = std::min(n * 2, static_cast<int>(decls_.size()));
    }
  }
}

void GlobalReduction::globalReduction(void) {
  ddmin(decls);
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

GlobalReduction::~GlobalReduction(void) { delete CollectionVisitor; }
