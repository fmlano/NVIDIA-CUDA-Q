# ============================================================================ #
# Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #
import os

import pytest
import numpy as np

import cudaq


def overlap(self, other, vector=False):
    self_ = np.array(self)
    other_ = np.array(other)
    fidelity = 0.0
    if vector:
        for index in range(len(np.array(self_))):
            # fidelity = |<self | other>| ^ 2
            fidelity = np.abs(other_.conj().dot(self_))**2
    else:

        def svd_routine(density_matrix):
            unitary_1, singular_values, unitary_2 = np.linalg.svd(
                density_matrix)
            diagonal = np.diag(np.sqrt(singular_values))
            return unitary_1.dot(diagonal).dot(unitary_2)

        term_1 = svd_routine(self_)
        term_2 = svd_routine(other_)
        fidelity = np.linalg.norm(term_1.dot(term_2), ord="nuc")**2

    return fidelity


def test_simple():
    one_state = np.array([0.0, 1.0], dtype=np.complex128)
    got_state = cudaq.State(one_state)

    print(overlap(got_state, got_state, True))
    print(got_state.overlap(got_state))


def test_bug():
    # Basis vectors.
    one_state = np.array([0.0, 1.0], dtype=np.complex128)
    zero_state = np.array([1.0, 0.0], dtype=np.complex128)

    # |psi> = |1> <0|
    psi = np.outer(one_state, np.conjugate(zero_state))

    # Call `State` constructor twice.
    got_state_a = cudaq.State(psi)
    got_state_b = cudaq.State(psi)
    assert np.allclose(np.asarray(got_state_a), np.asarray(got_state_b))

    # They should have perfect overlap, but the overlap is
    # returned as 0.0
    # assert got_state_a.overlap(got_state_b) == 1.0
    print(overlap(got_state_a, got_state_b, False))
    # assert got_state_a.overlap(got_state_b) == 1.0

    # # Check numpy functions between the numpy created object
    # # and the vector returned from the state buffer.
    # want_outer = np.outer(psi, psi)
    # got_outer = np.outer(got_state_a, got_state_b)
    # assert np.allclose(want_outer, got_outer)

    # print(overlap(want_outer, got_outer, False))
    # # assert want_outer.overlap(got_outer) == 1.0

    # # Note: this test passes fine with `psi = |1> <1|` and
    # # `psi = |0> <0|`, only fails when 1 0 or 0 1 -- mixed
    # # states.


@pytest.mark.parametrize("want_state", [
    np.array([0.0, 1.0], dtype=np.complex128),
    np.array([[0.0, 1.0]], dtype=np.complex128),
])
def test_state_buffer_vector(want_state):
    """
    Tests writing to and returning from the :class:`State` buffer
    on different state vectors.
    """
    got_state = cudaq.State(want_state)

    other_state = cudaq.State(want_state)
    print("overlap of itself = ", got_state.overlap(other_state), "\n\n")

    print("want_state = ", want_state)
    print("\n\ngot_state = ", got_state)

    # Should have full overlap.
    assert got_state.overlap(want_state) == 1.0


@pytest.mark.parametrize("want_state", [
    np.array([[0.0, 0.0], [1.0, 0.0]], dtype=np.complex128),
    np.array(
        [[.5, 0, 0, .5], [0., 0., 0., 0.], [0., 0., 0., 0.], [.5, 0., 0., .5]],
        dtype=np.complex128),
    np.array([[0.0, 0.0], [0.0, 0.0], [0.0, 0.0], [1.0, 0.0]],
             dtype=np.complex128)
])
def test_state_buffer_density_matrix(want_state):
    """
    Tests writing to and returning from the :class:`State` buffer
    on different density matrices.
    """
    got_state = cudaq.State(want_state)

    other_state = cudaq.State(want_state)
    print("overlap of itself = ", got_state.overlap(other_state), "\n\n")

    print("want_state = ", want_state)
    print("\n\ngot_state = ", got_state)

    # Should have full overlap.
    assert got_state.overlap(want_state) == 1.0


def test_state_integration_vector():
    """Integration test of the `State` class on state vector backend."""
    kernel = cudaq.make_kernel()
    qubits = kernel.qalloc(2)
    kernel.h(qubits[0])
    kernel.cx(qubits[0], qubits[1])

    # Get the state, this will be a state vector
    got_state = cudaq.get_state(kernel)
    got_state.dump()

    want_state = np.array([1. / np.sqrt(2.), 0., 0., 1. / np.sqrt(2.)],
                          dtype=np.complex128)

    assert np.allclose(want_state, np.array(got_state))

    print('overlap = ', got_state.overlap(want_state))
    np.isclose(1., got_state.overlap(want_state), 1e-3)

    # Make a general 2 qubit SO4 rotation
    so4, parameters = cudaq.make_kernel(list)
    q = so4.qalloc(2)
    so4.ry(parameters[0], q[0])
    so4.ry(parameters[1], q[1])
    so4.cz(q[0], q[1])
    so4.ry(parameters[2], q[0])
    so4.ry(parameters[3], q[1])
    so4.cz(q[0], q[1])
    so4.ry(parameters[4], q[0])
    so4.ry(parameters[5], q[1])
    so4.cz(q[0], q[1])

    def objective(x):
        testState = cudaq.get_state(so4, x)
        return 1. - got_state.overlap(testState)

    # Compute the parameters that make this kernel == bell state
    optimizer = cudaq.optimizers.COBYLA()
    optimizer.max_iterations = 50
    opt_val, opt_params = optimizer.optimize(6, objective)

    print(opt_val)
    np.isclose(0.0, opt_val, 1e-3)


def test_state_integration_density_matrix():
    """Integration test of the `State` class on density matrix backend."""
    cudaq.set_qpu('dm')

    # Create the bell state
    circuit = cudaq.make_kernel()
    q = circuit.qalloc(2)
    circuit.h(q[0])
    circuit.cx(q[0], q[1])

    # Get the state, this will be a density matrix
    state = cudaq.get_state(circuit)
    state.dump()
    np.isclose(.5, state[0, 0].real)
    np.isclose(.5, state[0, 3].real)
    np.isclose(.5, state[3, 0].real)
    np.isclose(.5, state[3, 3].real)

    # Make a general 2 qubit SO4 rotation
    so4, parameters = cudaq.make_kernel(list)
    q = so4.qalloc(2)
    so4.ry(parameters[0], q[0])
    so4.ry(parameters[1], q[1])
    so4.cz(q[0], q[1])
    so4.ry(parameters[2], q[0])
    so4.ry(parameters[3], q[1])
    so4.cz(q[0], q[1])
    so4.ry(parameters[4], q[0])
    so4.ry(parameters[5], q[1])
    so4.cz(q[0], q[1])

    def objective(x):
        testState = cudaq.get_state(so4, x)
        return 1. - state.overlap(testState)

    # Compute the parameters that make this circuit == bell state
    optimizer = cudaq.optimizers.COBYLA()
    optimizer.max_iterations = 50
    opt_val, opt_params = optimizer.optimize(6, objective)
    np.isclose(0.0, opt_val, 1e-3)

    # Can test overlap with numpy arrau
    test = np.array(
        [[.5, 0, 0, .5], [0., 0., 0., 0.], [0., 0., 0., 0.], [.5, 0., 0., .5]],
        dtype=np.complex128)
    np.isclose(1., state.overlap(test))
    cudaq.set_qpu('qpp')


# leave for gdb debugging
if __name__ == "__main__":
    loc = os.path.abspath(__file__)
    pytest.main([loc, "-rP"])
