/****************************************************************-*- C++ -*-****
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#pragma once
#include "GPUInfo.h"
#include "common/ExecutionContext.h"
#include "common/FmtCore.h"
#include "cudaq/Support/Version.h"
#include "cudaq/simulators.h"
#include "nlohmann/json.hpp"
/*! \file
    \brief Utility to support JSON serialization between the client and server.
*/

using json = nlohmann::json;

namespace std {
// Complex data serialization.
template <class T>
void to_json(json &j, const std::complex<T> &p) {
  j = json{p.real(), p.imag()};
}

template <class T>
void from_json(const json &j, std::complex<T> &p) {
  p.real(j.at(0));
  p.imag(j.at(1));
}
} // namespace std

namespace cudaq {

// `ExecutionResult` serialization.
// Here, we capture full data (not just bit string statistics) since the remote
// platform can populate simulator-only data, such as `expectationValue`.
inline void to_json(json &j, const ExecutionResult &result) {
  j = json{{"counts", result.counts},
           {"registerName", result.registerName},
           {"sequentialData", result.sequentialData}};
  if (result.expectationValue.has_value())
    j["expectationValue"] = result.expectationValue.value();
}

inline void from_json(const json &j, ExecutionResult &result) {
  j.at("counts").get_to(result.counts);
  j.at("registerName").get_to(result.registerName);
  j.at("sequentialData").get_to(result.sequentialData);
  double expVal = 0.0;
  if (j.contains("expectationValue")) {
    j.at("expectationValue").get_to(expVal);
    result.expectationValue = expVal;
  }
}

// `ExecutionContext` serialization.
inline void to_json(json &j, const ExecutionContext &context) {
  j = json{{"name", context.name},
           {"shots", context.shots},
           {"hasConditionalsOnMeasureResults",
            context.hasConditionalsOnMeasureResults}};

  const auto &regNames = context.result.register_names();
  // Here, we serialize the full lists of ExecutionResult records so that
  // expectation values are captured.
  std::vector<ExecutionResult> results;
  for (const auto &regName : regNames) {
    ExecutionResult result;
    result.registerName = regName;
    result.counts = context.result.to_map(regName);
    result.sequentialData = context.result.sequential_data(regName);
    if (context.result.has_expectation(regName))
      result.expectationValue = context.result.expectation(regName);
    results.emplace_back(std::move(result));
  }
  j["result"] = results;

  if (context.expectationValue.has_value()) {
    j["expectationValue"] = context.expectationValue.value();
  }

  if (context.simulationState) {
    j["simulationData"] = json();
    j["simulationData"]["dim"] = context.simulationState->getTensor().extents;
    if (context.simulationState->getPrecision() ==
        cudaq::SimulationState::precision::fp32) {
      std::vector<std::complex<float>> hostData(
          context.simulationState->getNumElements());
      if (context.simulationState->isDeviceData()) {
        context.simulationState->toHost(hostData.data(), hostData.size());
        j["simulationData"]["data"] = hostData;
      } else {
        auto *ptr = reinterpret_cast<std::complex<float> *>(
            context.simulationState->getTensor().data);
        j["simulationData"]["data"] = std::vector<std::complex<float>>(
            ptr, ptr + context.simulationState->getNumElements());
      }
    } else {
      std::vector<std::complex<double>> hostData(
          context.simulationState->getNumElements());
      if (context.simulationState->isDeviceData()) {
        context.simulationState->toHost(hostData.data(), hostData.size());
        j["simulationData"]["data"] = hostData;
      } else {
        auto *ptr = reinterpret_cast<std::complex<double> *>(
            context.simulationState->getTensor().data);
        j["simulationData"]["data"] = std::vector<std::complex<double>>(
            ptr, ptr + context.simulationState->getNumElements());
      }
    }
  }

  if (context.spin.has_value() && context.spin.value() != nullptr) {
    const std::vector<double> spinOpRepr =
        context.spin.value()->getDataRepresentation();
    const auto spinOpN = context.spin.value()->num_qubits();
    j["spin"] = json();
    j["spin"]["num_qubits"] = spinOpN;
    j["spin"]["data"] = spinOpRepr;
  }
  j["registerNames"] = context.registerNames;
  if (context.overlapResult.has_value())
    j["overlapResult"] = context.overlapResult.value();
  if (!context.amplitudeMaps.empty())
    j["amplitudeMaps"] = context.amplitudeMaps;
}

inline void from_json(const json &j, ExecutionContext &context) {
  j.at("shots").get_to(context.shots);
  j.at("hasConditionalsOnMeasureResults")
      .get_to(context.hasConditionalsOnMeasureResults);

  if (j.contains("result")) {
    std::vector<ExecutionResult> results;
    j.at("result").get_to(results);
    context.result = sample_result(results);
  }

  if (j.contains("expectationValue")) {
    double expectationValue;
    j["expectationValue"].get_to(expectationValue);
    context.expectationValue = expectationValue;
  }

  if (j.contains("spin")) {
    std::vector<double> spinData;
    j["spin"]["data"].get_to(spinData);
    const std::size_t nQubits = j["spin"]["num_qubits"];
    auto serializedSpinOps = std::make_unique<spin_op>(spinData, nQubits);
    context.spin = serializedSpinOps.release();
  }

  if (j.contains("simulationData")) {
    std::vector<std::size_t> stateDim;
    auto *simulator = cudaq::get_simulator();
    if (simulator->name().find("fp32") != std::string::npos) {
      std::vector<std::complex<float>> stateData;
      j["simulationData"]["dim"].get_to(stateDim);
      j["simulationData"]["data"].get_to(stateData);
      context.simulationState = simulator->createStateFromData(
          std::make_pair(stateData.data(), stateDim[0]));
    } else {
      std::vector<std::complex<double>> stateData;
      j["simulationData"]["dim"].get_to(stateDim);
      j["simulationData"]["data"].get_to(stateData);
      context.simulationState = simulator->createStateFromData(
          std::make_pair(stateData.data(), stateDim[0]));
    }
  }

  if (j.contains("overlapResult")) {
    std::complex<double> overlapResult;
    j.at("overlapResult").get_to(overlapResult);
    context.overlapResult = overlapResult;
  }

  if (j.contains("amplitudeMaps")) {
    j.at("amplitudeMaps").get_to(context.amplitudeMaps);
  }

  if (j.contains("registerNames"))
    j["registerNames"].get_to(context.registerNames);
}

// Enum data to denote the payload format.
enum class CodeFormat { MLIR, LLVM };

NLOHMANN_JSON_SERIALIZE_ENUM(CodeFormat, {
                                             {CodeFormat::MLIR, "MLIR"},
                                             {CodeFormat::LLVM, "LLVM"},
                                         });
// Encapsulate the IR payload
struct IRPayLoad {
  // Underlying code (IR) payload as a Base64 string.
  std::string ir;

  // Name of the entry-point kernel.
  std::string entryPoint;

  // Serialized kernel arguments.
  std::vector<uint8_t> args;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(IRPayLoad, ir, entryPoint, args);
};

// Payload from client to server for a kernel execution.
class RestRequest {
private:
  /// Holder of the reconstructed execution context.
  std::unique_ptr<ExecutionContext> m_deserializedContext;
  /// Holder of the reconstructed `spin_op`.
  std::unique_ptr<spin_op> m_deserializedSpinOp;
  // Version string identifying the client version.
  static inline const std::string CUDA_QUANTUM_VERSION = []() {
    std::stringstream ss;
    ss << "CUDA-Q Version " << cudaq::getVersion() << " ("
       << cudaq::getFullRepositoryVersion() << ")";
    return ss.str();
  }();

public:
  // Version number of this payload.
  // This needs to be bumped whenever a breaking change is introduced, which
  // causes incompatibility.
  //
  // For example,
  //
  // (1) Breaking Json schema changes,
  // e.g., adding/removing non-optional fields, changing field names, etc.,
  //     which introduce parsing incompatibility.
  // (2) Breaking changes in the runtime, which make JIT execution incompatible,
  //     e.g., changing the simulator names (.so files), changing signatures of
  //     QIR functions, etc.
  // IMPORTANT: When a new version is defined, a new NVQC deployment will be
  // needed.
  // Version history:
  // 1. First NVQC release (CUDA-Q v0.7)
  // 2. CUDA-Q v0.8
  //   - Support CUDA-Q state handling: overlap and amplitude data; multiple
  //   kernel IR payloads.
  static constexpr std::size_t REST_PAYLOAD_VERSION = 2;
  RestRequest(ExecutionContext &context, int versionNumber)
      : executionContext(context), version(versionNumber),
        clientVersion(CUDA_QUANTUM_VERSION) {}
  RestRequest(const json &j)
      : m_deserializedContext(
            std::make_unique<ExecutionContext>(j["executionContext"]["name"])),
        executionContext(*m_deserializedContext) {
    from_json(j, *this);
    // Take the ownership of the spin_op pointer for proper cleanup.
    if (executionContext.spin.has_value() && executionContext.spin.value())
      m_deserializedSpinOp.reset(executionContext.spin.value());
  }

  std::vector<IRPayLoad> code;
  // Name of the NVQIR simulator to use.
  std::string simulator;
  // The ExecutionContext to run the simulation.
  // The server will execute in this context, and populate simulation data in
  // this context, to be sending back to the client once finished.
  ExecutionContext &executionContext;
  // Format of the code buffer.
  CodeFormat format;
  // Simulation random seed.
  std::size_t seed;
  // List of MLIR passes to be applied on the code before execution.
  std::vector<std::string> passes;

  // Version of this schema for compatibility check.
  std::size_t version;
  // Version of the runtime client submitting the request.
  std::string clientVersion;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(RestRequest, version, simulator,
                                 executionContext, code, format, seed, passes,
                                 clientVersion);
};

/// NVCF function version status
enum class FunctionStatus { ACTIVE, DEPLOYING, ERROR, INACTIVE, DELETED };
NLOHMANN_JSON_SERIALIZE_ENUM(FunctionStatus,
                             {
                                 {FunctionStatus::ACTIVE, "ACTIVE"},
                                 {FunctionStatus::DEPLOYING, "DEPLOYING"},
                                 {FunctionStatus::ERROR, "ERROR"},
                                 {FunctionStatus::INACTIVE, "INACTIVE"},
                                 {FunctionStatus::DELETED, "DELETED"},
                             });

// Encapsulates a function version info
// Note: we only parse a subset of required fields (always present). There may
// be other fields, which are not required.
struct NvcfFunctionVersionInfo {
  // Function Id
  std::string id;
  // NVIDIA NGC Org Id (NCA Id)
  std::string ncaId;
  // Version Id
  std::string versionId;
  // Function name
  std::string name;
  // Status of this particular function version
  FunctionStatus status;
  // Function version creation timestamp (ISO 8601 string)
  // e.g., "2024-02-05T00:09:51.154Z"
  std::string createdAt;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(NvcfFunctionVersionInfo, id, ncaId, versionId,
                                 name, status, createdAt);
};

// NVCF execution metadata.
struct NvcfExecutionInfo {
  // Time point (milliseconds since epoch) when the request handling starts.
  std::size_t requestStart;
  // Time point (milliseconds since epoch) when the execution starts (JIT
  // completed).
  std::size_t simulationStart;
  // Time point (milliseconds since epoch) when the execution finishes.
  std::size_t simulationEnd;
  CudaDeviceProperties deviceProps;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(NvcfExecutionInfo, requestStart,
                                 simulationStart, simulationEnd, deviceProps);
};
} // namespace cudaq
