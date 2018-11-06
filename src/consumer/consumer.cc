#include "consumer.h"

#include "../finder/global_element_finder.h"
#include "../transformer/global_element_remover.h"
#include <clang/ASTMatchers/ASTMatchers.h>

XConsumer::XConsumer(clang::ASTContext &context)
{}

void XConsumer::HandleTranslationUnit(clang::ASTContext &context)
{
    rewriter.setSourceMgr(context.getSourceManager(), context.getLangOpts());
    
    GlobalElementFinder globalElementFinder(context);
    globalElementFinder.start();

    GlobalElementRemover globalElementRemover(context, rewriter);
for (auto g : globalElementFinder.globalElements)
    globalElementRemover.remove(g);

    auto buffer = rewriter.getRewriteBufferFor(context.getSourceManager().getMainFileID());

    if(buffer != nullptr)
        buffer->write(llvm::outs());
}


