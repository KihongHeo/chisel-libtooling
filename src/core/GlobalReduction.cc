#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

#include "CommonStatementVisitor.h"
#include "GlobalReduction.h"
#include "Options.h"
#include "Report.h"
#include "RewriteUtils.h"
#include "StringUtils.h"
#include "TransformationManager.h"
#include "VectorUtils.h"

using namespace clang;
using namespace clang::ast_matchers;

static const char *DescriptionMsg = "Perform global-level reduction";

std::map<FunctionDecl *, SourceRange> functionRanges;
std::map<Decl *, std::vector<DeclRefExpr *>> refList;
std::map<DeclRefExpr *, Decl *>
    whereUsed; // where does this call/access happen?

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

  bool VisitDeclRefExpr(DeclRefExpr *DRE);

private:
  GlobalReduction *ConsumerInstance;
};

static RegisterTransformation<GlobalReduction> Trans("global-reduction",
                                                     DescriptionMsg);

bool GlobalReductionCollectionVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  if (Option::verbose)
    llvm::outs() << "function decl " << FD->getNameInfo().getAsString() << "\n";
  if (!FD->isMain()) {
    ConsumerInstance->decls.emplace_back(FD);
  }
  return true;
}

bool GlobalReductionCollectionVisitor::VisitDeclRefExpr(DeclRefExpr *DRE) {
  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(DRE->getDecl())) {
    if (FD->isThisDeclarationADefinition()) {
      whereUsed.insert(std::make_pair(DRE, FD));
      if (refList.find(FD) != refList.end()) { // found
        std::vector<DeclRefExpr *> &v_ref = refList[FD];
        v_ref.emplace_back(DRE);
      } else { // not found
        std::vector<DeclRefExpr *> v;
        v.emplace_back(DRE);
        refList.insert(std::make_pair(FD, v));
      }
    }
  } else if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
    whereUsed.insert(std::make_pair(DRE, VD));
    if (refList.find(VD) != refList.end()) { // found
      std::vector<DeclRefExpr *> &v_ref = refList[VD];
      v_ref.emplace_back(DRE);
    } else { // not found
      std::vector<DeclRefExpr *> v;
      v.emplace_back(DRE);
      refList.insert(std::make_pair(VD, v));
    }
  }
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

bool GlobalReduction::test(std::vector<clang::Decl *> &toBeRemoved) {
  const SourceManager *SM = &Context->getSourceManager();
  std::string revert = "";
  SourceLocation totalStart, totalEnd;
  totalStart = toBeRemoved.front()->getSourceRange().getBegin();
  for (auto const &d : toBeRemoved) {
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
  revert = getSourceText(SourceRange(totalStart, totalEnd));

  TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd),
                          StringUtils::placeholder(revert));
  Transformation::writeToFile(Option::inputFile);

  if (Transformation::callOracle("global")) {
    /*// remove from refList and whereUsed
    for (auto const &d : toBeRemoved) {
      for (auto entry : refList) {
        llvm::outs() << " in ref list ... \n";
        if (entry.first == d)
          continue;
        llvm::outs() << "==== D:";
        d->dump();
        std::vector<DeclRefExpr*> tempRefs = std::move(entry.second);
        llvm::outs() << entry.second.size() << "\n";
        for (auto ref : entry.second) {
          llvm::outs() << "***** compare against: ";
          whereUsed[ref]->dump();
          if (whereUsed[ref] == d) {
            tempRefs.erase(std::remove(tempRefs.begin(), tempRefs.end(), ref), tempRefs.end()); 
            llvm::outs() << "erased\n";
          }
        }
        refList.insert(std::make_pair(d, tempRefs));
      }
    }*/
    return true;
  } else {
    // revert
    TheRewriter.ReplaceText(SourceRange(totalStart, totalEnd), revert);
    Transformation::writeToFile(Option::inputFile);
    return false;
  }
}

std::vector<std::vector<clang::Decl *>> GlobalReduction::refineSubsets(
    std::vector<std::vector<clang::Decl *>> &subsets) {
  std::vector<std::vector<clang::Decl *>> result;
  for (auto const &subset : subsets) {
    bool flag = true;
    for (auto const &i : subset) {
      if (refList[i].size() != 0) {
        flag = false;
        break;
      }
    }
    if (flag)
      result.emplace_back(subset);
  }
  return result;
}

void GlobalReduction::ddmin(std::vector<clang::Decl *> &decls) {
  std::vector<Decl *> decls_;
  decls_ = std::move(decls);
  int n = 2;
  while (decls_.size() >= 1) {
    std::vector<std::vector<clang::Decl *>> subsets =
        VectorUtils::split<clang::Decl *>(decls_, n);
    bool complementSucceeding = false;

    auto refinedSubsets = refineSubsets(subsets);

    for (auto subset : refinedSubsets) {
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

void GlobalReduction::globalReduction(void) { ddmin(decls); }

GlobalReduction::~GlobalReduction(void) { delete CollectionVisitor; }
