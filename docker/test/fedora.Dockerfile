# ============================================================================ #
# Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

ARG os_version=38
FROM fedora:$os_version

ARG python_version=3.10
ARG pip_install_flags="--user"

ARG DEBIAN_FRONTEND=noninteractive
RUN dnf install -y --nobest --setopt=install_weak_deps=False \
        python$(echo $python_version | tr -d .)
RUN python${python_version} -m ensurepip --upgrade \
    && python${python_version} -m pip install ${pip_install_flags} numpy pytest

ARG cuda_quantum_wheel=cuda_quantum-0.0.0-cp310-cp310-manylinux_2_28_x86_64.whl
COPY $cuda_quantum_wheel /tmp/$cuda_quantum_wheel
COPY docs/sphinx/examples/python /tmp/examples/
COPY python/tests /tmp/tests/

RUN python${python_version} -m pip install ${pip_install_flags} /tmp/$cuda_quantum_wheel