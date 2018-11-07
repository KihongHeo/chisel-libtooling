#include "global_element_remover.h"

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Rewrite/Core/Rewriter.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

GlobalElementRemover::GlobalElementRemover(ASTContext &context, Rewriter &rewriter) : context(context), rewriter(rewriter) {
}

void GlobalElementRemover::remove(const Decl *decl) {
  FullSourceLoc start = context.getFullLoc(decl->getSourceRange().getBegin());
  FullSourceLoc end = context.getFullLoc(decl->getSourceRange().getEnd().getLocWithOffset(1));

  /*if (start.isValid() && end.isValid())
    llvm::outs() << "Found at "
                 << start.getSpellingLineNumber() << ","
                 << start.getSpellingColumnNumber() << "-"
                 << end.getSpellingLineNumber() << "," << end.getSpellingColumnNumber() <<"\n";*/
  if (const FunctionDecl *d = dyn_cast<const FunctionDecl>(decl)) {
    // rewriter.InsertTextBefore(start, "/*");
    // rewriter.InsertTextAfter(end, "*/");
    rewriter.RemoveText(SourceRange(start, end));
    errs() << "** Rewrote funct decl\n";
  }
  if (const VarDecl *d = dyn_cast<const VarDecl>(decl)) {
    // rewriter.InsertTextBefore(start, "/*");
    // rewriter.InsertTextAfter(end, "*/");
    rewriter.RemoveText(SourceRange(start, end));
    errs() << "** Rewrote var del\n";
  }
}
