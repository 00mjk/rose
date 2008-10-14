#!/bin/bash

# Stub file to test the head of  the external ROSE repository
# 9/24/2008

export JAVA_HOME=/usr/apps/java/jdk1.5.0_11
export BOOST_ROOT="/home/liao6/opt/boost_1_35_0"
export LD_LIBRARY_PATH="/usr/apps/java/jdk1.5.0_11/lib:${JAVA_HOME}/jre/lib/i386/server:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH="${BOOST_ROOT}/lib:${LD_LIBRARY_PATH}"

source /usr/apps/gcc/4.2.2/setup.sh
export PATH="/usr/apps/automake/1.9.6/bin:$JAVA_HOME/bin:$PATH"

# using a time stamp to avoid overwriting previous tests
TIMESTAMP=$(date +%Y%m%d_%H%M%S) 
ROSE_TOP=/home/liao6/daily-test-rose/${TIMESTAMP}

# for regular build and tests
CONFIGURE_FLAGS=( --with-boost=${BOOST_ROOT} --with-bddbddb --enable-dq-developer-tests --with-ROSE_LONG_MAKE_CHECK_RULE=yes --with-CXX_DEBUG=-g --with-C_DEBUG=-g --with-CXX_WARNINGS=-Wall --prefix=${ROSE_TOP}/install "$@")

#ROSE_SVNROOT=file:///usr/casc/overture/ROSE/svn/ROSE/trunk/ROSE
ROSE_SVNROOT=https://outreach.scidac.gov/svn/rose/trunk
MAILADDRS="liaoch@llnl.gov willcock2@llnl.gov"

# the right version of subversion is essential!!
SVN=/nfs/apps/subversion/1.5.1/bin/svn
SVNOP=checkout

# check out the revision of the past 11:00pm PDT (06:00am UTC)
# using data as revision number needs {}
# default is head
#SVNVERSIONOPTION="{`date -d '06:00Z' -u '+%FT%H:%M:%SZ'`}"
#SVNVERSIONOPTION=1303

# bddbddb needs python 
source /home/liao6/opt/python-2.4.5/setup.sh

# How many processes for make
MAKEFLAGS="-j16"

# my additional stuff, not suitable for the external repository
#ENABLE_UPLOAD_SVN=yes
#ENABLE_UPLOAD_WEB=yes

#KEEP_TEST_DIR=yes
NORMAL_INSTALL_DIR=yes # no chmod 000 for 'install'
# skip make dist, make distcheck, 
#SKIP_DIST_TEST=yes
# end-users are not supposed to run ./build in source tree
#SKIP_BUILD_SOURCE=yes
#make source_with_binary_edg_dist etc
#ENABLE_BUILD_BINARY_EDG=1
set -e
set -o pipefail
