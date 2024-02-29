/*******************************************************************************
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

// Compile and run with: `nvq++ time.cpp && ./a.out`


#include <cudaq.h>
#include <chrono>
#include <iostream>

__qpu__ void kernel(int qubit_count) {
  // Allocate our qubits.
  cudaq::qvector qvector(qubit_count);
  // Place the first qubit in the superposition state.
  h(qvector[0]);
  // Loop through the allocated qubits and apply controlled-X,
  // or CNOT, operations between them.
  for (auto qubit : cudaq::range(qubit_count - 1)) {
    x<cudaq::ctrl>(qvector[qubit], qvector[qubit + 1]);
  }
  // Measure the qubits.
  mz(qvector);
}

// [Begin Time]
int main() {
  // 25 qubits.
  auto qubit_count = 25;
  // 1 million samples.
  auto shots_count = 1000000;
  auto start = std::chrono::high_resolution_clock::now();

  // Timing just the sample execution.
  auto result = cudaq::sample(shots_count, kernel, qubit_count);

  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration<double>(stop - start);
  std::cout << "It took " << duration.count() << " seconds.\n";
}
// [End Time]