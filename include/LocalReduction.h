#ifndef LOCAL_REDUCTION_H
#define LOCAL_REDUCTION_H

#include "Transformation.h"
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
  void ddmin(std::vector<clang::Stmt *> &decls);
  bool test(std::vector<clang::Stmt *> &toBeRemoved);
  LocalReductionCollectionVisitor *CollectionVisitor;

  LocalReduction(void);
  LocalReduction(const LocalReduction &);
  void operator=(const LocalReduction &);

  std::vector<clang::Stmt *> functionBodies;
};
#endif
