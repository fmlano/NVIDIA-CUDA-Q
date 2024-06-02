# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

# This file is used to build CUDA-Q NVQC service container to be deployed to NVCF.
#
# Usage:
# Must be built from the repo root with:
#   DOCKER_BUILDKIT=1 docker build -f docker/release/cudaq.nvqc.Dockerfile . --output out

# Base image is CUDA-Q image 
ARG base_image=nvcr.io/nvidia/nightly/cuda-quantum:latest
FROM $base_image as nvcf_image
