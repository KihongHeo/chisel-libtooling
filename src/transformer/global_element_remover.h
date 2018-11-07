#ifndef GLOBAL_ELEMENT_REMOVER_H_
#define GLOBAL_ELEMENT_REMOVER_H_

#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <string>

using namespace clang;
using namespace clang::ast_matchers;

class GlobalElementRemover {
private:
  ASTContext &context;
  Rewriter &rewriter;

public:
  GlobalElementRemover(ASTContext &context, Rewriter &rewriter);
  void remove(const Decl *decl);
};

#endif
