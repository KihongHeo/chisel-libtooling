#ifndef LOCAL_REDUCTION_H
#define LOCAL_REDUCTION_H

#include "Transformation.h"
#include <iterator>
#include <queue>
#include <string>

namespace clang {
class DeclGroupRef;
class ASTContext;
class Stmt;
} // namespace clang

class LocalReductionCollectionVisitor;

class LocalReduction : public Transformation {
  friend class LocalReductionCollectionVisitor;

public:
  LocalReduction(const char *TransName, const char *Desc)
      : Transformation(TransName, Desc), CollectionVisitor(NULL) {}

  ~LocalReduction(void);

private:
  virtual void Initialize(clang::ASTContext &context);
  virtual bool HandleTopLevelDecl(clang::DeclGroupRef D);
  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);
  void localReduction(void);
  std::vector<clang::Stmt *> &getImmediateChildren(clang::Stmt *s);
  std::vector<clang::Stmt *> getBodyStatements(clang::CompoundStmt *s);
  void hdd(clang::Stmt *s);
  void ddmin(std::vector<clang::Stmt *> stmts);
  bool test(std::vector<clang::Stmt *> &toBeRemoved);
  bool testEmpty();
  LocalReductionCollectionVisitor *CollectionVisitor;

  void reduceIf(clang::IfStmt *IS);
  void reduceWhile(clang::WhileStmt* WS);
  void reduceCompound(clang::CompoundStmt* CS);

  LocalReduction(void);
  LocalReduction(const LocalReduction &);
  void operator=(const LocalReduction &);

  std::vector<clang::Stmt *> functionBodies;
  std::queue<clang::Stmt *> q;
  std::vector<clang::SourceRange*> removedRanges;
};
#endif
