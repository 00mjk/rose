// Copyright 2005,2006,2007 Markus Schordan, Gergo Barany
// $Id: set_pag_options.h,v 1.1 2007-09-20 09:25:32 adrian Exp $

// Author: Markus Schordan, 2006

#ifndef SETPAGOPTIONS
#define SETPAGOPTIONS

// setting PAG's options requires the use of a number of global variables
// therefore we separate out this function to bundle all those accesses in
// one function/file

#include <config.h>

// required for handling pag options
//#include "iface.h"
//#include "iterate.h"
#include "mapping.h"
#include "paggdl.h"

#include "AnalyzerOptions.h"

void setPagOptions(AnalyzerOptions opt);

#endif
