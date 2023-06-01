/*******************************************************************************
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

// RUN: cudaq-quake %s | cudaq-opt --pass-pipeline='builtin.module(expand-measurements,canonicalize,cc-loop-unroll,canonicalize)' | FileCheck %s

#include <cudaq.h>

struct C {
   void operator()() __qpu__ {
      cudaq::qreg r(2);
      mz(r);
   }
};

// CHECK-LABEL:   func.func @__nvqpp__mlirgen__C()
// CHECK-DAG:       %[[VAL_1:.*]] = arith.constant 1 : index
// CHECK-DAG:       %[[VAL_2:.*]] = arith.constant 0 : index
// CHECK-DAG:       %[[VAL_3:.*]] = quake.alloca !quake.veq<2>
// CHECK-DAG:       %[[VAL_4:.*]] = cc.alloca !cc.array<i1 x 2>
// CHECK:           %[[VAL_5:.*]] = quake.extract_ref %[[VAL_3]][%[[VAL_2]]] : (!quake.veq<2>, index) -> !quake.ref
// CHECK:           %[[VAL_6:.*]] = quake.mz %[[VAL_5]] : (!quake.ref) -> i1
// CHECK:           cc.store %[[VAL_6]], %{{.*}} : !cc.ptr<i1>
// CHECK:           %[[VAL_7:.*]] = quake.extract_ref %[[VAL_3]][%[[VAL_1]]] : (!quake.veq<2>, index) -> !quake.ref
// CHECK:           %[[VAL_8:.*]] = quake.mz %[[VAL_7]] : (!quake.ref) -> i1
// CHECK:           %[[VAL_9:.*]] = cc.compute_ptr %[[VAL_4]][1] : (!cc.ptr<!cc.array<i1 x 2>>) -> !cc.ptr<i1>
// CHECK:           cc.store %[[VAL_8]], %[[VAL_9]] : !cc.ptr<i1>
// CHECK:           return
// CHECK:         }

