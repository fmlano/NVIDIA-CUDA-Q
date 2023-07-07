/*******************************************************************************
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

// This code is from Issue 251.

// RUN: nvq++ -v %s --target quantinuum --emulate -o %t.x && %t.x | FileCheck %s

#include <cudaq.h>
#include <iostream>

struct ak2 {
  void operator()() __qpu__ {
    cudaq::qubit q;
    h(q);
    mz(q);
  }
};

int main() {
  auto counts = cudaq::sample(ak2{});
  for (auto& [k,v] : counts) 
    std::cout << k << " : " << v << "\n";
  return 0;
}

// CHECK-DAG: 0 : {{[0-9]+}}
// CHECK-DAG: 1 : {{[0-9]+}}