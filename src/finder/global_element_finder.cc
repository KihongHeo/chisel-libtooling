#include "global_element_finder.h"

#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchers.h>

GlobalElementFinder::GlobalElementFinder(clang::ASTContext &context) : Finder(context) {
}

void GlobalElementFinder::start() {
  using namespace clang::ast_matchers;
  MatchFinder globalElementFinder;
  auto globalVarMatcher = varDecl(hasParent(translationUnitDecl())).bind("globalVarElement");
  auto globalFuncMatcher = functionDecl(isDefinition()).bind("globalFuncElement");
  globalElementFinder.addMatcher(globalVarMatcher, this);
  globalElementFinder.addMatcher(globalFuncMatcher, this);
  globalElementFinder.matchAST(context);
}

void GlobalElementFinder::run(const clang::ast_matchers::MatchFinder::MatchResult &result) {
  using namespace clang;

  if (const Decl *decl = result.Nodes.getNodeAs<Decl>("globalVarElement")) {
    if (result.SourceManager->isInSystemHeader(decl->getSourceRange().getBegin()))
      return;
    globalElements.emplace_back(decl);
  }
  if (const Decl *decl = result.Nodes.getNodeAs<Decl>("globalFuncElement")) {
    if (result.SourceManager->isInSystemHeader(decl->getSourceRange().getBegin()))
      return;
    globalElements.emplace_back(decl);
  }
}
