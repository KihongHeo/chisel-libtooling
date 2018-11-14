#ifndef INCLUDE_STATS_H_
#define INCLUDE_STATS_H_

#include <clang-c/Index.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

class Stats {
public:
  static int getWordCount(const char *srcPath);
};

#endif // INCLUDE_STATS_H_
