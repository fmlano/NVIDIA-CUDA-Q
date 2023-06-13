#!/bin/bash

# ============================================================================ #
# Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

# Usage: 

LLVM_DIR=${LLVM_DIR:-/opt/llvm/lib/cmake/llvm}
CPR_DIR=${CPR_DIR:-/cpr/install}
BLAS_PATH=${BLAS_PATH:-/usr/lib64/libblas.a}
# Get the CC and CXX directories and export so scikit-build can find them.
# CC=${CC:-/opt/llvm/clang-16/bin/clang}
# CXX=${CXX:-/opt/llvm/clang-16/bin/clang++}
# export CC=$CC
# export CXX=$CXX

# Run the script from the top-level of the repo
working_dir=`pwd`
this_file_dir=`dirname "$(readlink -f "${BASH_SOURCE[0]}")"`
repo_root=$(cd "$this_file_dir" && git rev-parse --show-toplevel)
cd "$repo_root"

# Clone the submodules (skipping llvm)
echo "Cloning submodules..."
git -c submodule.tpls/llvm.update=none submodule update --init --recursive

if [ ! -d "$LLVM_DIR" ]; then
  echo "Could not find llvm libraries."
  # Build llvm libraries from source and install them in the install directory
  llvm_build_script=`pwd`/scripts/build_llvm.sh
  cd "$working_dir" && source "$llvm_build_script" -c $build_configuration && cd "$repo_root"
  (return 0 2>/dev/null) && is_sourced=true || is_sourced=false
else 
  echo "Configured C compiler: $CC"
  echo "Configured C++ compiler: $CXX"
  echo "LLVM directory: $LLVM_DIR"
fi
export LLVM_DIR=$LLVM_DIR

# # Look for required apt packages and install any not found.
# apt-get update
# declare -a REQUIRED_PKGS=("build-essential" "wget" "gfortran" "python3.10-venv")
# for REQUIRED_PKG in "${REQUIRED_PKGS[@]}"; do
#   PKG_OK=$(dpkg-query -W --showformat='${Status}\n' $REQUIRED_PKG|grep "install ok installed")
#   echo Checking for $REQUIRED_PKG: $PKG_OK
#   if [ "" = "$PKG_OK" ]; then
#     echo "Could not find $REQUIRED_PKG. Installing $REQUIRED_PKG."
#     apt-get --yes install $REQUIRED_PKG
#   fi
# done

if [ ! -d "$CPR_DIR" ]; then
  echo "Could not find libcpr install dir"
  cd / && git clone https://github.com/libcpr/cpr
  cd cpr && mkdir build && cd build
  cmake .. -G Ninja -DCPR_FORCE_USE_SYSTEM_CURL=FALSE 
                    -DCMAKE_INSTALL_LIBDIR=lib 
                    -DOPENSSL_USE_STATIC_LIBS=TRUE 
                    -DBUILD_SHARED_LIBS=FALSE 
                    -DOPENSSL_ROOT_DIR=/usr/local/ssl 
                    -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE 
                    -DCMAKE_INSTALL_PREFIX=$CPR_DIR
  ninja install
else 
  echo "libcpr directory: $CPR_DIR"
fi
export CPR_DIR=$CPR_DIR

if [ ! -f "$BLAS_PATH" ]; then
  echo "Could not find libblas.a"
  # Find or configure BLAS 
  cd "$CPR_DIR" 
  wget http://www.netlib.org/blas/blas-3.11.0.tgz
  tar -xzvf blas-3.11.0.tgz && cd BLAS-3.11.0
  make && mv blas_LINUX.a /usr/lib64/libblas.a
else 
  echo "Found libblas.a"
fi

# Check for pip and install
# wget https://bootstrap.pypa.io/get-pip.py && python3.10 ./get-pip.py

# FIXME: Hard-coded on python3.10
# Install the python build package via pip
python3.10 -m pip install build