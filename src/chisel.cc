#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>

#include "Options.h"
#include "Report.h"
#include "Stats.h"
#include "TransformationManager.h"
#include "llvm/Support/raw_ostream.h"

static TransformationManager *TransMgr;
int ErrorCode = -1;
std::string ErrorMsg = "error";
void stat() {
  // auto stats = Stats::getStatementCount(Option::inputFile.c_str());
  auto stats = Stats::getWordCount(Option::inputFile.c_str());
  std::cout << "# Words : " << stats << std::endl;
  // std::cout << "# Functions  : " << stats[1] << std::endl;
  // std::cout << "# Statements : " << stats[0] << std::endl;
  exit(0);
}

int main(int argc, char **argv) {
  Option::handleOptions(argc, argv);

  if (Option::stat)
    stat();
  mkdir(Option::outputDir.c_str(), ACCESSPERMS);

  if (Option::profile)
    Report::totalProfiler.startTimer();

  TransMgr = TransformationManager::GetInstance();
  TransMgr->setSrcFileName(Option::inputFile);

  int wc = 0, wc0 = 0;
  while (true) {
    wc0 = Stats::getWordCount(Option::inputFile.c_str());

    if (!Option::skipGlobal) {
      TransMgr->setTransformation("global-reduction");
      TransMgr->initializeCompilerInstance(ErrorMsg);
      TransMgr->doTransformation(ErrorMsg, ErrorCode);
    }
    if (!Option::skipLocal) {
      TransMgr->setTransformation("local-reduction");
      TransMgr->initializeCompilerInstance(ErrorMsg);
      TransMgr->doTransformation(ErrorMsg, ErrorCode);
    }

    wc = Stats::getWordCount(Option::inputFile.c_str());
    if (wc == wc0)
      break;
  }

  if (Option::profile)
    Report::totalProfiler.stopTimer();

  TransformationManager::Finalize();
  if (Option::profile)
    Report::print();
  return 0;
}
