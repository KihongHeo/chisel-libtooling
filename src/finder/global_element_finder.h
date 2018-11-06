#ifndef GLOBAL_ELEMENT_FINDER_H_
#define GLOBAL_ELEMENT_FINDER_H_

#include "finder.h"
#include <vector>
#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchers.h>

using namespace clang;
using namespace clang::ast_matchers;

class GlobalElementFinder : public Finder
{
    public:
        std::vector<const Decl*> globalElements;
        explicit GlobalElementFinder(clang::ASTContext &context);
        virtual void start() override;
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &result);
};


#endif
