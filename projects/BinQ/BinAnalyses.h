#ifndef BINANALYSES_R_H
#define BINANALYSES_R_H
#include "rose.h"

#include <iostream>


class  BinAnalyses {
 public:

  BinAnalyses(){};
  virtual ~BinAnalyses(){};
  virtual void run()=0;
  virtual std::string name()=0;
  virtual std::string getDescription()=0;
  virtual bool twoFiles()=0;
};



#endif
