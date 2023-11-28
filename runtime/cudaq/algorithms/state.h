/****************************************************************-*- C++ -*-****
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#pragma once

#include "common/ExecutionContext.h"
#include "cudaq/concepts.h"
#include "cudaq/platform.h"
#include "cudaq/platform/QuantumExecutionQueue.h"
#include "host_config.h"
#include <complex>
#include <vector>

namespace cudaq {

/// @brief The cudaq::state encapsulate backend simulation state
/// vector or density matrix data.
class state {

private:
  /// @brief Reference to the simulation data
  State _data;

public:
  /// @brief The constructor, takes the simulation data
  state(State d) : _data(d) {}

  /// @brief  Default constructor (empty state)
  state() : _data({0}, {}){};

  /// @brief Return the data element at the given indices
  std::complex<double> operator[](std::size_t idx);
  std::complex<double> operator()(std::size_t idx, std::size_t jdx);

  /// @brief Dump the state to standard out
  void dump();
  void dump(std::ostream &os);

  /// @brief Return the dimensions of the state vector or density
  /// matrix.
  std::vector<std::size_t> get_shape();

  /// @brief Return the raw quantum state data.
  std::complex<double> *get_data();

  /// @brief Compute the overlap of this state
  /// with the other one.
  double overlap(state &other);
};

#if CUDAQ_USE_STD20
/// @brief Define a valid kernel concept
template <typename QuantumKernel, typename... Args>
concept KernelCallValid =
    ValidArgumentsPassed<QuantumKernel, Args...> &&
    HasVoidReturnType<std::invoke_result_t<QuantumKernel, Args...>>;
#endif

namespace details {

/// @brief Execute the given kernel functor and extract the
/// state representation.
template <typename KernelFunctor>
state extractState(KernelFunctor &&kernel) {
  // Get the platform.
  auto &platform = cudaq::get_platform();

  // This can only be done in simulation
  if (!platform.is_simulator())
    throw std::runtime_error("Cannot use get_state on a physical QPU.");

  // Create an execution context, indicate this is for
  // extracting the state representation
  ExecutionContext context("extract-state");

  // Perform the usual pattern set the context,
  // execute and then reset
  platform.set_exec_ctx(&context);
  kernel();
  platform.reset_exec_ctx();

  // Return the state data.
  return state(context.simulationData);
}

template <typename KernelFunctor>
auto runGetStateAsync(KernelFunctor &&wrappedKernel,
                      cudaq::quantum_platform &platform, std::size_t qpu_id) {
  // This can only be done in simulation
  if (!platform.is_simulator())
    throw std::runtime_error("Cannot use get_state_async on a physical QPU.");

  if (qpu_id >= platform.num_qpus())
    throw std::invalid_argument(
        "Provided qpu_id is invalid (must be <=to platform.num_qpus()).");

  std::promise<state> promise;
  auto f = promise.get_future();
  // Wrapped it as a generic (returning void) function
  QuantumTask wrapped = detail::make_copyable_function(
      [p = std::move(promise), qpu_id, &platform,
       func = std::forward<KernelFunctor>(wrappedKernel)]() mutable {
        ExecutionContext context("extract-state");
        // Indicate that this is an async exec
        context.asyncExec = true;
        // Set the platform and the qpu id.
        platform.set_exec_ctx(&context, qpu_id);
        platform.set_current_qpu(qpu_id);
        func();
        platform.reset_exec_ctx(qpu_id);
        // Extract state data
        p.set_value(state(context.simulationData));
      });

  platform.enqueueAsyncTask(qpu_id, wrapped);
  return f;
}
} // namespace details

/// @brief Return the state representation generated by
/// the kernel at the given runtime arguments.
template <typename QuantumKernel, typename... Args>
auto get_state(QuantumKernel &&kernel, Args &&...args) {
  return details::extractState(
      [&kernel, ... args = std::forward<Args>(args)]() mutable {
        kernel(std::forward<Args>(args)...);
      });
}

/// @brief Return type for asynchronous `get_state`.
using async_state_result = std::future<state>;

/// \brief Return the state representation generated by
/// the kernel at the given runtime arguments asynchronously.
///
/// \param qpu_id the id of the QPU to run asynchronously on
/// \param kernel the kernel expression, must contain final measurements
/// \param args the variadic concrete arguments for evaluation of the kernel.
/// \returns state future, A std::future containing the resultant state vector
///
#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires KernelCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
async_state_result get_state_async(std::size_t qpu_id, QuantumKernel &&kernel,
                                   Args &&...args) {
  auto &platform = cudaq::get_platform();
  return details::runGetStateAsync(
      [&kernel, ... args = std::forward<Args>(args)]() mutable {
        kernel(std::forward<Args>(args)...);
      },
      platform, qpu_id);
}

/// \brief Return the state representation generated by
/// the kernel at the given runtime arguments asynchronously on the default QPU
/// (id = 0).
///
/// \param kernel the kernel expression, must contain final measurements
/// \param args the variadic concrete arguments for evaluation of the kernel.
/// \returns state future, A std::future containing the resultant state vector
///
#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires KernelCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
async_state_result get_state_async(QuantumKernel &&kernel, Args &&...args) {
  return get_state_async(0, std::forward<QuantumKernel>(kernel),
                         std::forward<Args>(args)...);
}
} // namespace cudaq
