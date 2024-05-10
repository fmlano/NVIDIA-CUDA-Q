/*******************************************************************************
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include "mps_simulation_state.h"
#include "simulator_cutensornet.h"


namespace nvqir {

class SimulatorMPS : public SimulatorTensorNetBase {
  MPSSettings m_settings;
  std::vector<MPSTensor> m_mpsTensors_d;
  // List of auxiliary qubits that were used for controlled-gate decomposition.
  std::vector<std::size_t> m_auxQubitsForGateDecomp;

public:
  SimulatorMPS() : SimulatorTensorNetBase() {}

  virtual void prepareQubitTensorState() override {
    LOG_API_TIME();
    // Clean up previously factorized MPS tensors
    for (auto &tensor : m_mpsTensors_d) {
      HANDLE_CUDA_ERROR(cudaFree(tensor.deviceData));
    }
    m_mpsTensors_d.clear();
    // Factorize the state:
    if (m_state->getNumQubits() > 1)
      m_mpsTensors_d =
          m_state->factorizeMPS(m_settings.maxBond, m_settings.absCutoff, m_settings.relCutoff);
  }

  virtual void applyGate(const GateApplicationTask &task) override {
    // Check that we don't apply gates on 3+ qubits (not supported in MPS)
    if (task.controls.size() + task.targets.size() > 2) {
      const std::string gateDesc = task.operationName +
                                   containerToString(task.controls) +
                                   containerToString(task.targets);
      throw std::runtime_error("MPS simulator: Gates on 3 or more qubits are "
                               "unsupported. Encountered: " +
                               gateDesc);
    }
    SimulatorTensorNetBase::applyGate(task);
  }

  virtual std::size_t calculateStateDim(const std::size_t numQubits) override {
    return numQubits;
  }

  virtual void
  addQubitsToState(const cudaq::SimulationState &in_state) override {
    LOG_API_TIME();
    const MPSSimulationState *const casted =
        dynamic_cast<const MPSSimulationState *>(&in_state);
    if (!casted)
      throw std::invalid_argument(
          "[SimulatorMPS simulator] Incompatible state input");
    if (!m_state) {
      m_state = casted->reconstructBackendState();
    } else {
      // Expand an existing state:
      // Append MPS tensors
      throw std::runtime_error(
          "[SimulatorMPS simulator] Expanding state is not supported");
    }
  }

  virtual std::string name() const override { return "tensornet-mps"; }

  CircuitSimulator *clone() override {
    thread_local static auto simulator = std::make_unique<SimulatorMPS>();
    return simulator.get();
  }

  void addQubitsToState(std::size_t numQubits, const void *ptr) override {
    LOG_API_TIME();
    if (!m_state) {
      if (!ptr) {
        m_state = std::make_unique<TensorNetState>(numQubits, m_cutnHandle);
      } else {
        auto [state, mpsTensors] = MPSSimulationState::createFromStateVec(
            m_cutnHandle, 1ULL << numQubits,
            reinterpret_cast<std::complex<double> *>(const_cast<void *>(ptr)), m_settings.maxBond);
        m_state = std::move(state);
      }
    } else {
      // FIXME: expand the MPS tensors to the max extent
      if (!ptr) {
        auto tensors =
            m_state->factorizeMPS(m_settings.maxBond, m_settings.absCutoff, m_settings.relCutoff);
        // The right most MPS tensor needs to have one more extra leg (no longer
        // the boundary tensor).
        tensors.back().extents.emplace_back(1);
        // The newly added MPS tensors are in zero state
        constexpr std::complex<double> tensorBody[2]{1.0, 0.0};
        constexpr auto tensorSizeBytes = 2 * sizeof(std::complex<double>);
        for (std::size_t i = 0; i < numQubits; ++i) {
          const std::vector<int64_t> extents =
              (i != numQubits - 1) ? std::vector<int64_t>{1, 2, 1}
                                   : std::vector<int64_t>{1, 2};
          void *mpsTensor{nullptr};
          HANDLE_CUDA_ERROR(cudaMalloc(&mpsTensor, tensorSizeBytes));
          HANDLE_CUDA_ERROR(cudaMemcpy(mpsTensor, tensorBody, tensorSizeBytes,
                                       cudaMemcpyHostToDevice));
          tensors.emplace_back(MPSTensor(mpsTensor, extents));
        }
        m_state = TensorNetState::createFromMpsTensors(tensors, m_cutnHandle);
      } else {
        // Non-zero state needs to be factorized and appended.
        auto [state, mpsTensors] = MPSSimulationState::createFromStateVec(
            m_cutnHandle, 1ULL << numQubits,
            reinterpret_cast<std::complex<double> *>(const_cast<void *>(ptr)), m_settings.maxBond);
        auto tensors =
            m_state->factorizeMPS(m_settings.maxBond, m_settings.absCutoff, m_settings.relCutoff);
        // Adjust the extents of the last tensor in the original state
        tensors.back().extents.emplace_back(1);

        // Adjust the extents of the first tensor in the state to be appended.
        auto extents = mpsTensors.front().extents;
        extents.insert(extents.begin(), 1);
        mpsTensors.front().extents = extents;
        // Combine the list
        tensors.insert(tensors.end(), mpsTensors.begin(), mpsTensors.end());
        m_state = TensorNetState::createFromMpsTensors(tensors, m_cutnHandle);
      }
    }
  }

  std::unique_ptr<cudaq::SimulationState> getSimulationState() override {
    LOG_API_TIME();

    if (!m_state || m_state->getNumQubits() == 0)
      return std::make_unique<MPSSimulationState>(
          std::move(m_state), std::vector<MPSTensor>{},
          std::vector<std::size_t>{}, m_cutnHandle);

    if (m_state->getNumQubits() > 1) {
      std::vector<MPSTensor> tensors =
          m_state->factorizeMPS(m_settings.maxBond, m_settings.absCutoff, m_settings.relCutoff);
      return std::make_unique<MPSSimulationState>(
          std::move(m_state), tensors, m_auxQubitsForGateDecomp, m_cutnHandle);
    }

    auto [d_tensor, numElements] = m_state->contractStateVectorInternal({});
    assert(numElements == 2);
    MPSTensor stateTensor;
    stateTensor.deviceData = d_tensor;
    stateTensor.extents = {static_cast<int64_t>(numElements)};

    return std::make_unique<MPSSimulationState>(
        std::move(m_state), std::vector<MPSTensor>{stateTensor},
        m_auxQubitsForGateDecomp, m_cutnHandle);
  }

  virtual ~SimulatorMPS() noexcept {
    for (auto &tensor : m_mpsTensors_d) {
      HANDLE_CUDA_ERROR(cudaFree(tensor.deviceData));
    }
    m_mpsTensors_d.clear();
  }

  void deallocateStateImpl() override {
    m_auxQubitsForGateDecomp.clear();
    SimulatorTensorNetBase::deallocateStateImpl();
  }

  std::vector<size_t> addAuxQubits(std::size_t n) {
    if (m_state->isDirty())
      throw std::runtime_error(
          "[MPS Simulator] Unable to perform multi-control gate decomposition "
          "due to dynamical circuits.");
    std::vector<size_t> aux(n);
    std::iota(aux.begin(), aux.end(), m_state->getNumQubits());
    m_state = std::make_unique<TensorNetState>(m_state->getNumQubits() + n,
                                               m_cutnHandle);
    return aux;
  }

  template <typename QuantumOperation>
  void
  decomposeMultiControlledInstruction(const std::vector<double> &params,
                                      const std::vector<std::size_t> &controls,
                                      const std::vector<std::size_t> &targets) {
    if (controls.size() <= 1) {
      enqueueQuantumOperation<QuantumOperation>(params, controls, targets);
      return;
    }

    // CCNOT decomposition
    const auto ccnot = [&](std::size_t a, std::size_t b, std::size_t c) {
      enqueueQuantumOperation<nvqir::h<double>>({}, {}, {c});
      enqueueQuantumOperation<nvqir::x<double>>({}, {b}, {c});
      enqueueQuantumOperation<nvqir::tdg<double>>({}, {}, {c});
      enqueueQuantumOperation<nvqir::x<double>>({}, {a}, {c});
      enqueueQuantumOperation<nvqir::t<double>>({}, {}, {c});
      enqueueQuantumOperation<nvqir::x<double>>({}, {b}, {c});
      enqueueQuantumOperation<nvqir::tdg<double>>({}, {}, {c});
      enqueueQuantumOperation<nvqir::x<double>>({}, {a}, {c});
      enqueueQuantumOperation<nvqir::t<double>>({}, {}, {b});
      enqueueQuantumOperation<nvqir::t<double>>({}, {}, {c});
      enqueueQuantumOperation<nvqir::h<double>>({}, {}, {c});
      enqueueQuantumOperation<nvqir::x<double>>({}, {a}, {b});
      enqueueQuantumOperation<nvqir::t<double>>({}, {}, {a});
      enqueueQuantumOperation<nvqir::tdg<double>>({}, {}, {b});
      enqueueQuantumOperation<nvqir::x<double>>({}, {a}, {b});
    };

    // Collects the given list of control qubits into the given auxiliary
    // qubits, using all but the last qubits in the auxiliary list as scratch
    // qubits.
    //
    // For example, if the controls list is 6 qubits, the auxiliary list must be
    // 5 qubits, and the state from the 6 control qubits will be collected into
    // the last qubit of the auxiliary array.
    const auto collectControls = [&](const std::vector<std::size_t> &ctls,
                                     const std::vector<std::size_t> &aux,
                                     bool reverse = false) {
      std::vector<std::tuple<std::size_t, std::size_t, std::size_t>> ccnotList;
      for (int i = 0; i < static_cast<int>(ctls.size()) - 1; i += 2)
        ccnotList.emplace_back(
            std::make_tuple(ctls[i], ctls[i + 1], aux[i / 2]));

      for (int i = 0; i < static_cast<int>(ctls.size()) / 2 - 1; ++i)
        ccnotList.emplace_back(std::make_tuple(aux[i * 2], aux[(i * 2) + 1],
                                               aux[i + ctls.size() / 2]));

      if (ctls.size() % 2 != 0)
        ccnotList.emplace_back(std::make_tuple(
            ctls[ctls.size() - 1], aux[ctls.size() - 3], aux[ctls.size() - 2]));

      if (reverse)
        std::reverse(ccnotList.begin(), ccnotList.end());

      for (const auto &[a, b, c] : ccnotList)
        ccnot(a, b, c);
    };

    if (m_auxQubitsForGateDecomp.size() < controls.size() - 1) {
      const auto aux =
          addAuxQubits(controls.size() - 1 - m_auxQubitsForGateDecomp.size());
      m_auxQubitsForGateDecomp.insert(m_auxQubitsForGateDecomp.end(),
                                      aux.begin(), aux.end());
    }

    collectControls(controls, m_auxQubitsForGateDecomp);

    // Add to the singly-controlled instruction queue
    enqueueQuantumOperation<QuantumOperation>(
        params, {m_auxQubitsForGateDecomp[controls.size() - 2]}, targets);

    collectControls(controls, m_auxQubitsForGateDecomp, true);
  };

// Gate implementations:
// Here, we forward all the call to the multi-control decomposition helper.
// Decomposed gates are added to the queue.
#define CIRCUIT_SIMULATOR_ONE_QUBIT(NAME)                                      \
  using CircuitSimulator::NAME;                                                \
  void NAME(const std::vector<std::size_t> &controls,                          \
            const std::size_t qubitIdx) override {                             \
    decomposeMultiControlledInstruction<nvqir::NAME<double>>(                  \
        {}, controls, std::vector<std::size_t>{qubitIdx});                     \
  }

#define CIRCUIT_SIMULATOR_ONE_QUBIT_ONE_PARAM(NAME)                            \
  using CircuitSimulator::NAME;                                                \
  void NAME(const double angle, const std::vector<std::size_t> &controls,      \
            const std::size_t qubitIdx) override {                             \
    decomposeMultiControlledInstruction<nvqir::NAME<double>>(                  \
        {angle}, controls, std::vector<std::size_t>{qubitIdx});                \
  }

  /// @brief The X gate
  CIRCUIT_SIMULATOR_ONE_QUBIT(x)
  /// @brief The Y gate
  CIRCUIT_SIMULATOR_ONE_QUBIT(y)
  /// @brief The Z gate
  CIRCUIT_SIMULATOR_ONE_QUBIT(z)
  /// @brief The H gate
  CIRCUIT_SIMULATOR_ONE_QUBIT(h)
  /// @brief The S gate
  CIRCUIT_SIMULATOR_ONE_QUBIT(s)
  /// @brief The T gate
  CIRCUIT_SIMULATOR_ONE_QUBIT(t)
  /// @brief The Sdg gate
  CIRCUIT_SIMULATOR_ONE_QUBIT(sdg)
  /// @brief The Tdg gate
  CIRCUIT_SIMULATOR_ONE_QUBIT(tdg)
  /// @brief The RX gate
  CIRCUIT_SIMULATOR_ONE_QUBIT_ONE_PARAM(rx)
  /// @brief The RY gate
  CIRCUIT_SIMULATOR_ONE_QUBIT_ONE_PARAM(ry)
  /// @brief The RZ gate
  CIRCUIT_SIMULATOR_ONE_QUBIT_ONE_PARAM(rz)
  /// @brief The Phase gate
  CIRCUIT_SIMULATOR_ONE_QUBIT_ONE_PARAM(r1)
// Undef those preprocessor defines.
#undef CIRCUIT_SIMULATOR_ONE_QUBIT
#undef CIRCUIT_SIMULATOR_ONE_QUBIT_ONE_PARAM

  // Swap gate implementation
  using CircuitSimulator::swap;
  void swap(const std::vector<std::size_t> &ctrlBits, const std::size_t srcIdx,
            const std::size_t tgtIdx) override {
    if (ctrlBits.empty())
      return SimulatorTensorNetBase::swap(ctrlBits, srcIdx, tgtIdx);
    // Controlled swap gate: using cnot decomposition of swap gate to perform
    // decomposition.
    const auto size = ctrlBits.size();
    std::vector<std::size_t> ctls(size + 1);
    std::copy(ctrlBits.begin(), ctrlBits.end(), ctls.begin());
    {
      ctls[size] = tgtIdx;
      decomposeMultiControlledInstruction<nvqir::x<double>>({}, ctls, {srcIdx});
    }
    {
      ctls[size] = srcIdx;
      decomposeMultiControlledInstruction<nvqir::x<double>>({}, ctls, {tgtIdx});
    }
    {
      ctls[size] = tgtIdx;
      decomposeMultiControlledInstruction<nvqir::x<double>>({}, ctls, {srcIdx});
    }
  }

  // `exp-pauli` gate implementation: forward the middle-controlled Rz to the
  // decomposition helper.
  void applyExpPauli(double theta, const std::vector<std::size_t> &controls,
                     const std::vector<std::size_t> &qubitIds,
                     const cudaq::spin_op &op) override {
    if (op.is_identity()) {
      if (controls.empty()) {
        // exp(i*theta*Id) is noop if this is not a controlled gate.
        return;
      } else {
        // Throw an error if this exp_pauli(i*theta*Id) becomes a non-trivial
        // gate due to control qubits.
        // FIXME: revisit this once
        // https://github.com/NVIDIA/cuda-quantum/issues/483 is implemented.
        throw std::logic_error("Applying controlled global phase via exp_pauli "
                               "of identity operator is not supported");
      }
    }
    std::vector<std::size_t> qubitSupport;
    std::vector<std::function<void(bool)>> basisChange;
    op.for_each_pauli([&](cudaq::pauli type, std::size_t qubitIdx) {
      if (type != cudaq::pauli::I)
        qubitSupport.push_back(qubitIds[qubitIdx]);

      if (type == cudaq::pauli::Y)
        basisChange.emplace_back([&, qubitIdx](bool reverse) {
          rx(!reverse ? M_PI_2 : -M_PI_2, qubitIds[qubitIdx]);
        });
      else if (type == cudaq::pauli::X)
        basisChange.emplace_back(
            [&, qubitIdx](bool) { h(qubitIds[qubitIdx]); });
    });

    if (!basisChange.empty())
      for (auto &basis : basisChange)
        basis(false);

    std::vector<std::pair<std::size_t, std::size_t>> toReverse;
    for (std::size_t i = 0; i < qubitSupport.size() - 1; i++) {
      x({qubitSupport[i]}, qubitSupport[i + 1]);
      toReverse.emplace_back(qubitSupport[i], qubitSupport[i + 1]);
    }

    // Perform multi-control decomposition.
    decomposeMultiControlledInstruction<nvqir::rz<double>>(
        {-2.0 * theta}, controls, {qubitSupport.back()});

    std::reverse(toReverse.begin(), toReverse.end());
    for (auto &[i, j] : toReverse)
      x({i}, j);

    if (!basisChange.empty()) {
      std::reverse(basisChange.begin(), basisChange.end());
      for (auto &basis : basisChange)
        basis(true);
    }
  }

  int getBondDim() const { return m_settings.maxBond; }
};

} // end namespace nvqir

NVQIR_REGISTER_SIMULATOR(nvqir::SimulatorMPS, tensornet_mps)
