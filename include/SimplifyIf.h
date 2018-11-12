#ifndef SIMPLIFY_IF_H
#define SIMPLIFY_IF_H

#include "Transformation.h"
#include <string>

    namespace clang {
  class DeclGroupRef;
  class ASTContext;
  class Stmt;
  class IfStmt;
}

class SimplifyIfCollectionVisitor;
class SimplifyIfStatementVisitor;

class SimplifyIf : public Transformation {
  friend class SimplifyIfCollectionVisitor;
  friend class SimplifyIfStatementVisitor;

public:
  SimplifyIf(const char *TransName, const char *Desc)
      : Transformation(TransName, Desc), CollectionVisitor(NULL),
        StmtVisitor(NULL), TheIfStmt(NULL), NeedParen(false) {}

  ~SimplifyIf(void);

private:
  virtual void Initialize(clang::ASTContext &context);

  virtual bool HandleTopLevelDecl(clang::DeclGroupRef D);

  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);

  void simplifyIfStmt(void);

  SimplifyIfCollectionVisitor *CollectionVisitor;

  SimplifyIfStatementVisitor *StmtVisitor;

  clang::IfStmt *TheIfStmt;

  bool NeedParen;

  // Unimplemented
  SimplifyIf(void);

  SimplifyIf(const SimplifyIf &);

  void operator=(const SimplifyIf &);
};
#endif
