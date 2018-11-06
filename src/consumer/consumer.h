#ifndef CONSUMER_HPP
#define CONSUMER_HPP

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Rewrite/Core/Rewriter.h>

using namespace clang;

class XConsumer : public clang::ASTConsumer 
{
    private:
    
        Rewriter rewriter;

    public:

        explicit XConsumer(ASTContext &context);
        virtual void HandleTranslationUnit(ASTContext &context) override;
};

#endif
