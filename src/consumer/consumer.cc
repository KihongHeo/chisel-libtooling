#include "consumer.h"

#include "../finder/global_element_finder.h"
#include "../transformer/global_element_remover.h"
#include <clang/ASTMatchers/ASTMatchers.h>

#include <fstream>

XConsumer::XConsumer(clang::ASTContext &context) {
}

void XConsumer::HandleTranslationUnit(clang::ASTContext &context) {
  rewriter.setSourceMgr(context.getSourceManager(), context.getLangOpts());

  GlobalElementFinder globalElementFinder(context);
  globalElementFinder.start();

  GlobalElementRemover globalElementRemover(context, rewriter);

  // remove the second global element
  globalElementRemover.remove(globalElementFinder.globalElements[1]);

  auto buffer = rewriter.getRewriteBufferFor(context.getSourceManager().getMainFileID());

  // write the new program to the terminal 
  if (buffer != nullptr)
    buffer->write(llvm::outs());

  std::error_code error_code;
  llvm::raw_fd_ostream outFile("output.c", error_code, llvm::sys::fs::F_None);
  if (buffer != nullptr) {
    rewriter.getEditBuffer(context.getSourceManager().getMainFileID()).write(outFile); // --> this will write the result to outFile
  }
  outFile.close();
}
