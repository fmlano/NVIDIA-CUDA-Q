import cudaq
from cudaq import spin

import numpy as np

# Here we build up a kernel for QAOA with `p` layers, with each layer
# containing the alternating set of unitaries corresponding to the problem
# and the mixer Hamiltonians. The algorithm leverages the VQE algorithm
# to compute the Max-Cut of a rectangular graph illustrated below.

#       v0  0---------------------0 v1
#           |                     |
#           |                     |
#           |                     |
#           |                     |
#       v3  0---------------------0 v2
# The Max-Cut for this problem is 0101 or 1010.

# The problem Hamiltonian
hamiltonian = 0.5 * spin.z(0) * spin.z(1) + 0.5 * spin.z(1) * spin.z(2) \
       + 0.5 * spin.z(0) * spin.z(3) + 0.5 * spin.z(2) * spin.z(3)

# Problem parameters.
qubit_count: int = 4
layer_count: int = 2
parameter_count: int = 2 * layer_count


@cudaq.kernel(jit=True)
def kernel_qaoa(qubit_count: int, layer_count: int, thetas: list):
    """QAOA ansatz for Max-Cut"""
    qreg = cudaq.qvector(qubit_count)

    # Create superposition
    h(qreg)

    # Loop over the layers
    for i in range(layer_count):
        # Loop over the qubits
        # Problem unitary
        for j in range(qubit_count):
            x.ctrl(qreg[j], qreg[(j + 1) % qubit_count])
            rz(2.0 * thetas[i], qreg[(j + 1) % qubit_count])
            x.ctrl(qreg[j], qreg[(j + 1) % qubit_count])

        # Mixer unitary
        for j in range(qubit_count):
            rx(2.0 * thetas[i + layer_count], qreg[j])


# Specify the optimizer and its initial parameters. Make it repeatable.
cudaq.set_random_seed(13)
optimizer = cudaq.optimizers.COBYLA()
np.random.seed(13)
optimizer.initial_parameters = np.random.uniform(-np.pi / 8.0, np.pi / 8.0,
                                                 parameter_count)
print("Initial parameters = ", optimizer.initial_parameters)


# Define the objective, return <state(params) | H | state(params)>
def objective(parameters):
    return cudaq.observe(kernel_qaoa, hamiltonian, qubit_count, layer_count,
                         parameters).expectation()


# Optimize!
optimal_expectation, optimal_parameters = optimizer.optimize(
    dimensions=parameter_count, function=objective)

# Print the optimized value and its parameters
print("Optimal value = ", optimal_expectation)
print("Optimal parameters = ", optimal_parameters)

# Sample the circuit using the optimized parameters
counts = cudaq.sample(kernel_qaoa, qubit_count, layer_count, optimal_parameters)
counts.dump()
