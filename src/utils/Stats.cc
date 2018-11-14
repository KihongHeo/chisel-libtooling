#include <clang-c/Index.h>
#include <string.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "Stats.h"

int Stats::getWordCount(const char *srcPath) {
  std::ifstream ifs(srcPath);
  std::string word;
  int count = 0;
  while (ifs >> word) {
    count++;
  }
  return count;
}
