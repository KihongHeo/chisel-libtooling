#ifndef TRANSFORMER_HPP
#define TRANSFORMER_HPP

#include <clang/ASTMatchers/ASTMatchFinder.h>
namespace clang
{
    class ASTContext;
    class raw_ostream;
    class Rewriter;
}

class Transformer
{
    protected:

        clang::ASTContext &context;
        clang::Rewriter &rewriter;

    public:

        explicit Transformer(clang::ASTContext &context, clang::Rewriter &rewriter);
};

#endif
