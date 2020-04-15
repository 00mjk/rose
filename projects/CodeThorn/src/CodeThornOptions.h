#ifndef CODETHORN_OPTIONS_H
#define CODETHORN_OPTIONS_H

#include "Options.h"
#include <vector>
#include <string>

struct CodeThornOptions : public Options {
  // hidden options
  int maxTransitionsForcedTop1;
  int maxTransitionsForcedTop2;
  int maxTransitionsForcedTop3;
  int maxTransitionsForcedTop4;
  int maxTransitionsForcedTop5;
  int solver;

  // pass on to ROSE
  std::vector<std::string> includeDirs;
  std::vector<std::string> preProcessorDefines;
  std::string languageStandard;
  bool edgNoWarningsFlag;

  // visualization
  struct Visualization {
    bool rwClusters;
    bool rwData;
    bool rwHighlightRaces;
    std::string dotIOStg;
    std::string dotIOStgForcedTop;
    bool tg1EStateAddress;
    bool tg1EStateId;
    bool tg1EStateProperties;
    bool tg1EStatePredicate;
    bool tg1EStateMemorySubgraphs;
    bool tg2EStateAddress;
    bool tg2EStateId;
    bool tg2EStateProperties;
    bool tg2EStatePredicate;
    bool visualizeRWSets;
    bool viz;
    bool vizTg2;
    std::string icfgFileName;
  } visualization;
  
  bool colors;
};

#endif
