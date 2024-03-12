# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

# [Begin Documentation]
import sys
import cudaq

print(f"Running on target {cudaq.get_target().name}")
n_qubits = int(sys.argv[1]) if 1 < len(sys.argv) else 2

kernel = cudaq.make_kernel()
qubits = kernel.qalloc(n_qubits)
kernel.h(qubits[0])
for i in range(1, nr_qubits):
    cnot(qubits[0], qubits[i])
kernel.mz(qubits)

result = cudaq.sample(kernel)
print(result)  # Example: { 111:500 000:500 }
# [End Documentation]
