# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

## [SKIP_TEST]: FileCheck input needs to be updated.
##              Once fixed, append '| FileCheck %s' to the following command
# RUN: PYTHONPATH=../../.. pytest -rP  %s

import os

import pytest
import numpy as np

import cudaq


def test_make_kernel_multiple_floats():
    """
    Test `cudaq.make_kernel` with multiple parameters.
    """
    kernel, parameter_1, parameter_2 = cudaq.make_kernel(float, float)
    # Kernel should have 2 arguments and parameters.
    got_arguments = kernel.arguments
    got_argument_count = kernel.argument_count
    assert len(got_arguments) == 2
    assert got_argument_count == 2
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:    %[[VAL_0:.*]]: f64,
# CHECK:         %[[VAL_1:.*]]: f64) attributes {"cudaq-entrypoint"} {
# CHECK:           return
# CHECK:         }

# leave for gdb debugging
if __name__ == "__main__":
    loc = os.path.abspath(__file__)
    pytest.main([loc, "-rP"])
