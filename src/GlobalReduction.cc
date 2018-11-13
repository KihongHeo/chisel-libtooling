#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "CommonStatementVisitor.h"
#include "GlobalReduction.h"
#include "RewriteUtils.h"
#include "TransformationManager.h"

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

private:
  GlobalReduction *ConsumerInstance;
};

static RegisterTransformation<GlobalReduction> Trans("global-reduction",
                                                     DescriptionMsg);

bool GlobalReductionCollectionVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  if (FD->isThisDeclarationADefinition()) {
    ConsumerInstance->decls.emplace_back(FD);
  }
  return true;
}

bool GlobalReductionCollectionVisitor::VisitVarDecl(VarDecl *VD) {
  if (VD->hasGlobalStorage()) {
    ConsumerInstance->decls.emplace_back(VD);
  }
  return true;
}

bool GlobalReductionCollectionVisitor::VisitRecordDecl(RecordDecl *RD) {
  ConsumerInstance->decls.emplace_back(RD);
  return true;
}

bool GlobalReductionCollectionVisitor::VisitTypedefDecl(TypedefDecl *TD) {
  ConsumerInstance->decls.emplace_back(TD);
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
  if (QueryInstanceOnly)
    return;

  Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

  globalReduction();

  if (Ctx.getDiagnostics().hasErrorOccurred() ||
      Ctx.getDiagnostics().hasFatalErrorOccurred())
    TransError = TransInternalError;
}

std::vector<std::vector<clang::Decl *>>
GlobalReduction::split(std::vector<clang::Decl *> &vec, int n) {
  std::vector<std::vector<clang::Decl *>> result;
  int length = static_cast<int>(vec.size()) / n;
  int remain = static_cast<int>(vec.size()) % n;

  int begin = 0, end = 0;
  for (int i = 0; i < std::min(n, static_cast<int>(vec.size())); ++i) {
    end += (remain > 0) ? (length + !!(remain--)) : length;
    result.emplace_back(
        std::vector<Decl *>(vec.begin() + begin, vec.begin() + end));
    begin = end;
  }
  return result;
}

bool GlobalReduction::contains(std::vector<clang::Decl *> vec, clang::Decl *d) {
  for (auto v : vec) {
    if (v->getSourceRange() == d->getSourceRange()) {
      return true;
    }
  }
  return false;
}

std::vector<clang::Decl *>
GlobalReduction::difference(std::vector<clang::Decl *> &a,
                            std::vector<clang::Decl *> &b) {
  // a - b
  std::vector<clang::Decl *> minus;
  if (a.size() == 0)
    return minus;
  if (b.size() == 0)
    return a;
  for (clang::Decl *d : a) {
    if (!contains(b, d))
      minus.emplace_back(d);
  }
  return minus;
}

void GlobalReduction::prettyPrintSubset(std::vector<clang::Decl *> vec) {
  for (auto d : vec) {
    d->dump();
    llvm::outs() << "===============\n";
  }
  llvm::outs() << "\n";
}

int i = 0;
bool GlobalReduction::test(std::vector<clang::Decl *> &toBeRemoved) {
  const SourceManager *SM = &Context->getSourceManager();
  std::string revert = "";
  SourceLocation totalStart, totalEnd;
  totalStart = toBeRemoved.front()->getSourceRange().getBegin();
  int index = 0;
  for (auto d : toBeRemoved) {
    SourceLocation start = d->getSourceRange().getBegin();
    SourceLocation end;

    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(d)) {
      end = FD->getSourceRange().getEnd().getLocWithOffset(1);
    } else {
      end = RewriteHelper->getEndLocationUntil(d->getSourceRange(), ';')
                .getLocWithOffset(1);
    }
    totalEnd = end;
    llvm::StringRef ref = Lexer::getSourceText(
        CharSourceRange::getCharRange(SourceRange(start, end)), *SM,
        LangOptions());
    revert += std::string(ref.str()) +
              ((index == toBeRemoved.size() - 1) ? "" : "\n");
    index++;
  }
  std::string replacement = "";
  for (auto chr : revert) {
    if (chr == '\n')
      replacement += '\n';
    else
      replacement += " ";
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
  if (system("./test.sh") == 0) {
    llvm::outs() << "ORACLE = YES!\n";
    return true;
  } else {
    llvm::outs() << "ORACLE = NO!\n";
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
    std::vector<std::vector<clang::Decl *>> subsets = split(decls_, n);
    bool complementSucceeding = false;

    for (std::vector<Decl *> subset : subsets) {
      std::vector<Decl *> complement = difference(decls_, subset);
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

      // increase set granularity
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
