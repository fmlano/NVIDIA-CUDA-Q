# ============================================================================ #
# Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

# RUN: PYTHONPATH=../../ pytest -rP  %s | FileCheck %s

import os

import pytest
import numpy as np

import cudaq

def test_kernel_apply_call_no_args():
    """
    Tests that we can call a non-parameterized kernel (`other_kernel`), 
    from a :class:`Kernel`.
    """
    other_kernel = cudaq.make_kernel()
    other_qubit = other_kernel.qalloc()
    other_kernel.x(other_qubit)

    kernel = cudaq.make_kernel()
    kernel.apply_call(other_kernel)
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}() {
# CHECK:           call @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}() : () -> ()
# CHECK:           return
# CHECK:         }

# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}() {
# CHECK:           %[[VAL_0:.*]] = quake.alloca : !quake.qref
# CHECK:           quake.x (%[[VAL_0]])
# CHECK:           return
# CHECK:         }


def test_kernel_apply_call_qubit_args():
    """
    Tests that we can call another kernel that's parameterized 
    by a qubit (`other_kernel`), from a :class:`Kernel`.
    """
    other_kernel, other_qubit = cudaq.make_kernel(cudaq.qubit)
    other_kernel.h(other_qubit)

    kernel = cudaq.make_kernel()
    qubit = kernel.qalloc()
    kernel.apply_call(other_kernel, qubit)
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}() {
# CHECK:           %[[VAL_0:.*]] = quake.alloca : !quake.qref
# CHECK:           call @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(%[[VAL_0]]) : (!quake.qref) -> ()
# CHECK:           return
# CHECK:         }

# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:                                                                   %[[VAL_0:.*]]: !quake.qref) {
# CHECK:           quake.h (%[[VAL_0]])
# CHECK:           return
# CHECK:         }


def test_kernel_apply_call_qreg_args():
    """
    Tests that we can call another kernel that's parameterized 
    by a qubit (`other_kernel`), from a :class:`Kernel`.
    """
    other_kernel, other_qreg = cudaq.make_kernel(cudaq.qreg)
    other_kernel.h(other_qreg)

    kernel = cudaq.make_kernel()
    qreg = kernel.qalloc(5)
    kernel.apply_call(other_kernel, qreg)
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}() {
# CHECK:           %[[VAL_0:.*]] = quake.alloca : !quake.qvec<5>
# CHECK:           %[[VAL_1:.*]] = quake.relax_size %[[VAL_0]] : (!quake.qvec<5>) -> !quake.qvec<?>
# CHECK:           call @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(%[[VAL_1]]) : (!quake.qvec<?>) -> ()
# CHECK:           return
# CHECK:         }

# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:                                                                   %[[VAL_0:.*]]: !quake.qvec<?>) {
# CHECK:           %[[VAL_1:.*]] = arith.constant 1 : index
# CHECK:           %[[VAL_2:.*]] = arith.constant 0 : index
# CHECK:           %[[VAL_3:.*]] = quake.qvec_size %[[VAL_0]] : (!quake.qvec<?>) -> i64
# CHECK:           %[[VAL_4:.*]] = arith.index_cast %[[VAL_3]] : i64 to index
# CHECK:           %[[VAL_5:.*]] = cc.loop while ((%[[VAL_6:.*]] = %[[VAL_2]]) -> (index)) {
# CHECK:             %[[VAL_7:.*]] = arith.cmpi slt, %[[VAL_6]], %[[VAL_4]] : index
# CHECK:             cc.condition %[[VAL_7]](%[[VAL_6]] : index)
# CHECK:           } do {
# CHECK:           ^bb0(%[[VAL_8:.*]]: index):
# CHECK:             %[[VAL_9:.*]] = quake.qextract %[[VAL_0]]{{\[}}%[[VAL_8]]] : !quake.qvec<?>[index] -> !quake.qref
# CHECK:             quake.h (%[[VAL_9]])
# CHECK:             cc.continue %[[VAL_8]] : index
# CHECK:           } step {
# CHECK:           ^bb0(%[[VAL_10:.*]]: index):
# CHECK:             %[[VAL_11:.*]] = arith.addi %[[VAL_10]], %[[VAL_1]] : index
# CHECK:             cc.continue %[[VAL_11]] : index
# CHECK:           } {counted}
# CHECK:           return
# CHECK:         }


def test_kernel_apply_call_float_args():
    """
    Tests that we can call another kernel that's parameterized 
    by a float (`other_kernel`), from a :class:`Kernel`.
    """
    other_kernel, other_float = cudaq.make_kernel(float)
    other_qubit = other_kernel.qalloc()
    other_kernel.rx(other_float, other_qubit)

    kernel, _float = cudaq.make_kernel(float)
    kernel.apply_call(other_kernel, _float)
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:      %[[VAL_0:.*]]: f64) {
# CHECK:           call @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(%[[VAL_0]]) : (f64) -> ()
# CHECK:           return
# CHECK:         }

# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:      %[[VAL_0:.*]]: f64) {
# CHECK:           %[[VAL_1:.*]] = quake.alloca : !quake.qref
# CHECK:           quake.rx |%[[VAL_0]] : f64|(%[[VAL_1]])
# CHECK:           return
# CHECK:         }


def test_kernel_apply_call_int_args():
    """
    Tests that we can call another kernel that's parameterized 
    by an int (`other_kernel`), from a :class:`Kernel`.
    """
    other_kernel, other_int = cudaq.make_kernel(int)
    other_qubit = other_kernel.qalloc()
    # TODO:
    # Would like to be able to test kernel operations that
    # can accept an int.

    kernel, _int = cudaq.make_kernel(int)
    kernel.apply_call(other_kernel, _int)
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:      %[[VAL_0:.*]]: i32) {
# CHECK:           call @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(%[[VAL_0]]) : (i32) -> ()
# CHECK:           return
# CHECK:         }

# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:      %[[VAL_0:.*]]: i32) {
# CHECK:           %[[VAL_1:.*]] = quake.alloca : !quake.qref
# CHECK:           return
# CHECK:         }


def test_kernel_apply_call_list_args():
    """
    Tests that we can call another kernel that's parameterized 
    by a list (`other_kernel`), from a :class:`Kernel`.
    """
    other_kernel, other_list = cudaq.make_kernel(list)
    other_qubit = other_kernel.qalloc()
    other_kernel.rx(other_list[0], other_qubit)

    kernel, _list = cudaq.make_kernel(list)
    kernel.apply_call(other_kernel, _list)
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:      %[[VAL_0:.*]]: !cc.stdvec<f64>) {
# CHECK:           call @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(%[[VAL_0]]) : (!cc.stdvec<f64>) -> ()
# CHECK:           return
# CHECK:         }

# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}(
# CHECK-SAME:      %[[VAL_0:.*]]: !cc.stdvec<f64>) {
# CHECK:           %[[VAL_1:.*]] = quake.alloca : !quake.qref
# CHECK:           %[[VAL_2:.*]] = cc.stdvec_data %[[VAL_0]] : (!cc.stdvec<f64>) -> !llvm.ptr<f64>
# CHECK:           %[[VAL_3:.*]] = llvm.load %[[VAL_2]] : !llvm.ptr<f64>
# CHECK:           quake.rx |%[[VAL_3]] : f64|(%[[VAL_1]])
# CHECK:           return
# CHECK:         }


# leave for gdb debugging
if __name__ == "__main__":
    loc = os.path.abspath(__file__)
    pytest.main([loc, "-rP"])