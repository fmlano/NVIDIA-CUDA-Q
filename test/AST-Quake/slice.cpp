/*******************************************************************************
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

// RUN: cudaq-quake %s | FileCheck %s

#include <cudaq.h>

__qpu__ void other(cudaq::qspan<>);

struct SliceTest {
   void operator()(int i1, int i2) __qpu__ {
      cudaq::qreg reg(10);
      auto s = reg.slice(i1, i2);
      other(s);
   }
};

// CHECK-LABEL:   func.func @__nvqpp__mlirgen__SliceTest
// CHECK-SAME:      (%[[VAL_0:.*]]: i32, %[[VAL_1:.*]]: i32) attributes {
// CHECK:           %[[VAL_4:.*]] = arith.constant 10 : i32
// CHECK:           %[[VAL_5:.*]] = arith.extsi %[[VAL_4]] : i32 to i64
// CHECK:           %[[VAL_6:.*]] = quake.alloca[%[[VAL_5]] : i64] !quake.qvec<?>
// CHECK:           %[[VAL_11:.*]] = arith.constant 1 : i64
// CHECK:           %[[VAL_12:.*]] = arith.addi %{{.*}}, %{{.*}} : i64
// CHECK:           %[[VAL_13:.*]] = arith.subi %[[VAL_12]], %[[VAL_11]] : i64
// CHECK:           %[[VAL_14:.*]] = quake.subvec %[[VAL_6]], %{{.*}}, %[[VAL_13]] : (!quake.qvec<?>, i64, i64) -> !quake.qvec<?>
// CHECK:           call @{{.*}}other{{.*}}(%[[VAL_14]]) : (!quake.qvec<?>) -> ()
// CHECK:           return
// CHECK:         }
