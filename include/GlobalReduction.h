#ifndef GLOBAL_REDUCTION_H
#define GLOBAL_REDUCTION_H

#include "Transformation.h"
#include <string>

namespace clang {
class DeclGroupRef;
class ASTContext;
class Stmt;
} // namespace clang

class GlobalReductionCollectionVisitor;

class GlobalReduction : public Transformation {
  friend class GlobalReductionCollectionVisitor;

public:
  GlobalReduction(const char *TransName, const char *Desc)
      : Transformation(TransName, Desc), CollectionVisitor(NULL) {}

  ~GlobalReduction(void);

private:
  virtual void Initialize(clang::ASTContext &context);
  virtual bool HandleTopLevelDecl(clang::DeclGroupRef D);
  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);
  void globalReduction(void);
  void prettyPrintSubset(std::vector<clang::Decl *> vec);
  void ddmin(std::vector<clang::Decl *> &decls);
  bool test(std::vector<clang::Decl *> &toBeRemoved);
  GlobalReductionCollectionVisitor *CollectionVisitor;
  std::vector<std::vector<clang::Decl *>>
  refineSubsets(std::vector<std::vector<clang::Decl *>> &subsets);
  GlobalReduction(void);
  GlobalReduction(const GlobalReduction &);
  void operator=(const GlobalReduction &);

  std::vector<clang::Decl *> decls;
  std::vector<clang::Stmt *> functionBodies;
};
#endif
