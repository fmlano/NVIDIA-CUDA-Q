/*******************************************************************************
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include <cudaq.h>

// RUN: nvq++ %cpp_std %s --target iqm --emulate --iqm-machine Apollo -o %t.x && %t.x | FileCheck %s

// CHECK: { 0:{{[0-9]+}} 1:{{[0-9]+}} }

template <std::size_t N>
struct ghz {
  auto operator()() __qpu__ {
    cudaq::qarray<N> q;
    h(q[0]);
    for (int i = 0; i < N - 1; i++) {
#if CUDAQ_USE_STD20
      x<cudaq::ctrl>(q[i], q[i + 1]);
#else
      cx(q[i], q[i + 1]);
#endif
    }
    auto result = mz(q[0]);
  }
};

int main() {

  auto kernel = ghz<2>{};
  auto counts = cudaq::sample(kernel);
  counts.dump();
  return 0;
}
