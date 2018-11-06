#include "global_element_finder.h"

#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchers.h>

GlobalElementFinder::GlobalElementFinder(clang::ASTContext &context)
    : Finder(context)
{}

void GlobalElementFinder::start()
{
using namespace clang::ast_matchers;
MatchFinder globalElementFinder;
auto globalMatcher = decl(hasParent(translationUnitDecl())).bind("globalElement");
globalElementFinder.addMatcher(globalMatcher, this);
globalElementFinder.matchAST(context);
}

void GlobalElementFinder::run(const clang::ast_matchers::MatchFinder::MatchResult &result)
{
    using namespace clang;
    
    if(const Decl *decl = result.Nodes.getNodeAs<Decl>("globalElement"))
    {
        if(result.SourceManager->isInSystemHeader(decl->getSourceRange().getBegin()))
            return;
        globalElements.emplace_back(decl);
        llvm::outs() << "ADD!\n";
    }
}
