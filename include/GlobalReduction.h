#ifndef GLOBAL_REDUCTION_H
#define GLOBAL_REDUCTION_H

#include "Transformation.h"
#include <string>

namespace clang {
  class DeclGroupRef;
  class ASTContext;
  class Stmt;
}

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

  std::vector<std::vector<clang::Decl *>> split(std::vector<clang::Decl *> vec,
    int n);

  std::vector<clang::Decl *> difference(std::vector<clang::Decl *> a,
    std::vector<clang::Decl *> b);

  void prettyPrintSubset(std::vector<clang::Decl *> vec);

  void ddmin(std::vector<clang::Decl *> decls);
  
  void test(std::vector<clang::Decl*> toBeRemoved);
  
  GlobalReductionCollectionVisitor *CollectionVisitor;

  std::vector<clang::Decl*> decls;
  
  // Unimplemented
  GlobalReduction(void);

  GlobalReduction(const GlobalReduction &);

  void operator=(const GlobalReduction &);
};
#endif
