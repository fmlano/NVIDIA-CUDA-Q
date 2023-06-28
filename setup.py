# ******************************************************************************
# Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the MIT License which accompanies this distribution.
# ******************************************************************************
import os
import sys
try:
    import skbuild
except ImportError:
    print("Trying to install required module: skbuild")
    os.system(f"{sys.executable} -m pip install scikit-build")
import skbuild
import setuptools

__version__ = "0.0.3"

# The setup.py script gets called twice when installing from source 
# with `pip install .` . Once for the `egg_info` subcommand and another
# for `install`. We will only install the missing dependencies once, for 
# the `egg_info` subcommand (arbitrary choice).
if (sys.argv[1] == 'egg_info'):
    script_path = os.getcwd() + "/scripts/install_wheel_dependencies.sh"
    os.system(f"bash {script_path}")

# FIXME: Linux machines default to dist-packages unless the `--user` flag is provided to
# the pip install. We hard-code everything to site-packages in the meantime and require the
# user to install with `--user`.
# cmake_install_dir=f"lib/python{sys.version_info[0]}.{sys.version_info[1]}/site-packages/cudaq"
cmake_install_dir="cudaq"

skbuild.setup(
    name="cuda-quantum",
    version=__version__,
    package_dir={"cudaq": "python/cudaq",
                 "domains": "python/cudaq/domains",
                 "chemistry": "python/cudaq/domains/chemistry",
    },
    packages=setuptools.find_packages(where="python/cudaq", include=["*"]),
    zip_safe=False,
    python_requires=">=3.8",
    cmake_install_dir=cmake_install_dir,
    # FIXME: Remove hard-coding on zlib and libcpr path.
    cmake_args=[
        "-DCUDAQ_ENABLE_PYTHON=TRUE", "-DBLAS_LIBRARIES=/usr/lib64/libblas.a",
        "-DCMAKE_INSTALL_LIBDIR=lib", "-DCUDAQ_DISABLE_CPP_FRONTEND=ON",
        "-DCUDAQ_BUILD_TESTS=FALSE", "-DCMAKE_COMPILE_WARNING_AS_ERROR=OFF",
        "-DCUSTATEVEC_ROOT={}".format(os.environ["CUSTATEVEC_ROOT"])
        if "CUSTATEVEC_ROOT" in os.environ else "",
        "-DLLVM_DIR=/opt/llvm",
        # "-DLLVM_DIR={}".format(os.environ["LLVM_DIR"])
        # if "LLVM_DIR" in os.environ else "/opt/llvm",
         "-DOPENSSL_USE_STATIC_LIBS=TRUE",
        "-DCMAKE_EXE_LINKER_FLAGS='-static-libgcc -static-libstdc++'",
        "-DCMAKE_SHARED_LINKER_FLAGS='-static-libgcc -static-libstdc++'",
        "-DOPENSSL_ROOT_DIR=/usr/local/ssl", 
        "-DCUDAQ_CPR_INSTALL=/cpr/install",
        # "-DZLIB_ROOT=/cpr/install", 
        # "-DCUDAQ_CPR_INSTALL={}".format(os.environ["CUDAQ_CPR_INSTALL"]) if "CUDAQ_CPR_INSTALL" in os.environ else "/cpr/install",
        # "-DZLIB_ROOT={}".format(os.environ["CUDAQ_CPR_INSTALL"]) if "CUDAQ_CPR_INSTALL" in os.environ else "/cpr/install", 
        # "-DZLIB_USE_STATIC_LIBS=TRUE",
        "-DCUDAQ_BUILD_RELOCATABLE_PACKAGE=TRUE"
    ],
    setup_requires=["numpy", "pytest", "scikit-build", "setuptools"])
