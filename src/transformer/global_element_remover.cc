#include "global_element_remover.h"

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Rewrite/Core/Rewriter.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

GlobalElementRemover::GlobalElementRemover(ASTContext &context, Rewriter &rewriter): context(context), rewriter(rewriter){ }

void GlobalElementRemover::remove(const Decl *decl)
{
        if (const FunctionDecl *d = dyn_cast<const FunctionDecl>(decl)) {
            rewriter.InsertTextBefore(d->getSourceRange().getBegin(), "/*");
          //rewriter.RemoveText(d->getSourceRange().getBegin(), 10);  
          errs() << "** Rewrote funct decl\n";
        }
        if (const VarDecl *d = dyn_cast<const VarDecl>(decl)) {
            rewriter.InsertTextBefore(d->getSourceRange().getBegin(), "/*");
          //rewriter.RemoveText(d->getSourceRange().getBegin(), 10);  
          errs() << "** Rewrote var del\n";
        }
}
