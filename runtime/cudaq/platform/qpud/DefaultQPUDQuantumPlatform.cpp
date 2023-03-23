/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include "common/ExecutionContext.h"
#include "nvqpp_config.h"
#include "qpud_client.h"
#include "rpc/client.h"
#include "cudaq/platform/qpu.h"
#include "cudaq/platform/quantum_platform.h"
#include "llvm/Support/Program.h"
#include <cudaq/spin_op.h>
#include <fmt/core.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

/// This file defines the qpud default platform. It is meant to be
/// used in conjuction with the --enable-mlir flag of nvq++. It takes
/// kernel invocations which invoke altLaunchKernel and forwards them to
/// the remote qpud daemon for execution.

namespace {

/// The QPUD QPU is a QPU that enables kernel invocation via
/// remote process calls to the qpud daemon. It's job is to connect to
/// the remote daemon (or start it if one is not specified), and forward
/// all calls to launchKernel to the daemon via the rpc client.
class QpudQPU : public cudaq::QPU {
protected:
  /// @brief The QPUD Client, enables kernel launches
  cudaq::qpud_client qpudClient;

  /// The number of shots
  std::optional<int> nShots;

public:
  QpudQPU() : QPU() {}
  QpudQPU(QpudQPU &&) = delete;
  virtual ~QpudQPU() = default;

  void enqueue(cudaq::QuantumTask &task) override {
    execution_queue->enqueue(task);
  }

  /// @brief Ask qpud if the current backend is a simulator
  /// @return
  bool isSimulator() override { return qpudClient.is_simulator(); }

  /// @brief Ask qpud if the current backend supports conditional feedback
  bool supportsConditionalFeedback() override {
    return qpudClient.supports_conditional_feedback();
  }

  /// Provide the number of shots
  void setShots(int _nShots) override { nShots = _nShots; }

  /// Clear the number of shots
  void clearShots() override { nShots = std::nullopt; }

  /// Store the execution context for launchKernel
  void setExecutionContext(cudaq::ExecutionContext *context) override {
    executionContext = context;
  }

  /// Reset the execution context
  void resetExecutionContext() override {
    // do nothing here
    executionContext = nullptr;
  }

  void setTargetBackend(const std::string &backend) override {
    qpudClient.set_backend(backend);
  }

  /// Launch the kernel with given name and runtime arguments.
  void launchKernel(const std::string &kernelName, void (*kernelFunc)(void *),
                    void *args, std::uint64_t voidStarSize,
                    std::uint64_t resultOffset) override {
    // Execute based on the context...
    if (executionContext &&
        executionContext->name.find("sample") != std::string::npos) {
      // Sample the state generated by the quake code
      executionContext->result = qpudClient.sample(
          kernelName, nShots.value_or(1000), args, voidStarSize);
    } else if (executionContext && executionContext->name == "observe") {
      // Observe the state with respect to the given operator
      if (!executionContext->spin.has_value())
        throw std::runtime_error(
            "Observe ExecutionContext specified without a cudaq::spin_op.");
      auto H = *executionContext->spin.value();
      auto res = qpudClient.observe(kernelName, H, args, voidStarSize,
                                    (std::size_t)nShots.value_or(0));
      executionContext->expectationValue = res.exp_val_z();
      executionContext->result = res.raw_data();
    } else {
      // Just execute the kernel
      qpudClient.execute(kernelName, args, voidStarSize, resultOffset);
    }
  }
};

class DefaultQPUDQuantumPlatform : public cudaq::quantum_platform {
public:
  DefaultQPUDQuantumPlatform() : quantum_platform() {
    // Populate the information and add the QPUs
    platformQPUs.emplace_back(std::make_unique<QpudQPU>());
    platformNumQPUs = platformQPUs.size();
  }

  /// @brief Set the target backend on the remote qpud process.
  /// @param backend
  void setTargetBackend(const std::string &backend) override {
    platformQPUs.front()->setTargetBackend(backend);
  }

  void set_shots(int numShots) override {
    cudaq::quantum_platform::set_shots(numShots);
    platformQPUs.back()->setShots(numShots);
  }
};
} // namespace

CUDAQ_REGISTER_PLATFORM(DefaultQPUDQuantumPlatform, qpud)
