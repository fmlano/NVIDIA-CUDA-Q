#!/bin/bash

# ============================================================================ #
# Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

# Usage: 
# This script builds and installs a minimal set of dependencies needed to build 
# CUDA Quantum from source. 
#
# Usage: 
# bash install_prerequisites.sh
#
# The necessary LLVM components will be installed in the location defined by the
# LLVM_INSTALL_PREFIX if they do not already exist in that location.
# If OpenBLAS is not found, it will be built from source and installed the location
# defined by the OPENBLAS_INSTALL_PREFIX.
# If OpenSSL is not found, it will be built from source and installed the location
# defined by the OPENSSL_INSTALL_PREFIX.

LLVM_INSTALL_PREFIX=${LLVM_INSTALL_PREFIX:-/opt/llvm}
OPENBLAS_INSTALL_PREFIX=${OPENBLAS_INSTALL_PREFIX:-/usr/local}
OPENSSL_INSTALL_PREFIX=${OPENSSL_INSTALL_PREFIX:-/usr/local}

function temp_install_if_command_unknown {
    if [ ! -x "$(command -v $1)" ]; then
        apt-get install -y --no-install-recommends $2
        APT_UNINSTALL="$APT_UNINSTALL $2"
    fi
}

function remove_temp_installs {
  if [ "$APT_UNINSTALL" != "" ]; then
      echo "Uninstalling packages used for bootstrapping: $APT_UNINSTALL"
      apt-get remove -y $APT_UNINSTALL && apt-get autoremove -y --purge
      unset APT_UNINSTALL
  fi
}

trap remove_temp_installs EXIT
this_file_dir=`dirname "$(readlink -f "${BASH_SOURCE[0]}")"`

if [ ! -x "$(command -v cmake)" ]; then
    apt-get update && apt-get install -y --no-install-recommends cmake
    APT_UNINSTALL="$APT_UNINSTALL $2"
fi
if [ "$CC" == "" ] && [ "$CXX" == "" ]; then
  source "$this_file_dir/install_toolchain.sh" -t gcc12
fi

llvm_dir="$LLVM_INSTALL_PREFIX/lib/cmake/llvm"
if [ ! -d "$llvm_dir" ]; then
  echo "Could not find llvm libraries."

  # Build llvm libraries from source and install them in the install directory
  source "$this_file_dir/build_llvm.sh"
  (return 0 2>/dev/null) && is_sourced=true || is_sourced=false

  if [ ! -d "$llvm_dir" ]; then
    echo "Failed to find directory $llvm_dir."
    if $is_sourced; then return 1; else exit 1; fi
  fi
else 
  echo "Configured C compiler: $CC"
  echo "Configured C++ compiler: $CXX"
fi

if [ ! -x "$(command -v ar)" ] && [ -x "$(command -v "$LLVM_INSTALL_PREFIX/bin/llvm-ar")" ]; then
    ln -s "$LLVM_INSTALL_PREFIX/bin/llvm-ar" /usr/bin/ar
    created_ld_sym_link=$?
    if [ "$created_ld_sym_link" = "" ] || [ ! "$created_ld_sym_link" -eq "0" ]; then
        echo "Failed to find ar or llvm-ar."
    else 
        echo "Setting llvm-ar as the default ar."
    fi
fi

if [ ! -f "$OPENBLAS_INSTALL_PREFIX/libopenblas.a" ] && [ ! -f "$OPENBLAS_INSTALL_PREFIX/lib/libopenblas.a" ]; then
  apt-get update && apt-get install -y --no-install-recommends libopenblas-dev
  if [ "$OPENBLAS_INSTALL_PREFIX" != "/usr/lib/x86_64-linux-gnu" ]; then
    ln -s "/usr/lib/x86_64-linux-gnu/libopenblas.a" "$OPENBLAS_INSTALL_PREFIX/libopenblas.a"
  fi

  # We could build this from source, but we need a fortran compiler to do so.
  # Building from source, we end up with slight incompatibilities if we are not careful.
  # Additionally, if we build from source it would be nice to build with OpenMP support
  # if OpenMP is available. Note that clang and gcc work with different OpenMP libraries
  # (libgomp vs libomp), and clang cannot work with libgomp.
  # Download location: https://github.com/xianyi/OpenBLAS/releases/download/v0.3.23/OpenBLAS-0.3.23.tar.gz
  # Build with OpenMP: make USE_OPENMP=0 && make install PREFIX="$OPENBLAS_INSTALL_PREFIX"
fi

if [ ! -d "$OPENSSL_INSTALL_PREFIX" ] || [ -z "$(ls -A "$OPENSSL_INSTALL_PREFIX")" ]; then
  apt-get update
  temp_install_if_command_unknown wget wget
  temp_install_if_command_unknown make make
  temp_install_if_command_unknown perl perl

  wget https://www.openssl.org/source/openssl-3.1.1.tar.gz
  tar -xf openssl-3.1.1.tar.gz && cd openssl-3.1.1
  ./config no-zlib --prefix="$OPENSSL_INSTALL_PREFIX" --openssldir="$OPENSSL_INSTALL_PREFIX"
  make install && cd .. && rm -rf openssl-3.1.1*
fi
