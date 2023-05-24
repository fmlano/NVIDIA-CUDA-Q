/*******************************************************************************
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include "cudaq/Frontend/nvqpp/AttributeNames.h"
#include "cudaq/Optimizer/Dialect/Quake/QuakeOps.h"
#include "cudaq/Target/Emitter.h"
#include "cudaq/Target/IQM/IQMJsonEmitter.h"
#include "nlohmann/json.hpp"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/FormatAdapters.h"

using namespace mlir;

namespace cudaq {

static LogicalResult emitOperation(nlohmann::json &json, Emitter &emitter,
                                   Operation &op);

static LogicalResult emitEntryPoint(nlohmann::json &json, Emitter &emitter,
                                    func::FuncOp op) {
  if (op.getBody().getBlocks().size() != 1)
    op.emitError("Cannot translate kernels with more than 1 block to IQM Json. "
                 "Must be a straight-line representation.");

  Emitter::Scope scope(emitter, /*isEntryPoint=*/true);
  json["name"] = op.getName().str();
  std::vector<nlohmann::json> instructions;
  for (Operation &op : op.getOps()) {
    nlohmann::json instruction = nlohmann::json::object();
    if (failed(emitOperation(instruction, emitter, op)))
      return failure();
    if (!instruction.empty())
      instructions.push_back(instruction);
  }
  json["instructions"] = instructions;
  return success();
}

static LogicalResult emitOperation(nlohmann::json &json, Emitter &emitter,
                                   ModuleOp moduleOp) {
  func::FuncOp entryPoint = nullptr;
  for (Operation &op : moduleOp) {
    if (op.hasAttr(cudaq::entryPointAttrName)) {
      if (entryPoint)
        return moduleOp.emitError("has multiple entrypoints");
      entryPoint = dyn_cast_or_null<func::FuncOp>(op);
      continue;
    }
  }
  if (!entryPoint)
    return moduleOp.emitError("does not contain an entrypoint");
  return emitEntryPoint(json, emitter, entryPoint);
}

static LogicalResult emitOperation(nlohmann::json &json, Emitter &emitter,
                                   quake::AllocaOp op) {
  Value refOrVeq = op.getRefOrVec();
  auto name = emitter.createName();
  emitter.getOrAssignName(refOrVeq, name);
  return success();
}

static LogicalResult emitOperation(nlohmann::json &json, Emitter &emitter,
                                   quake::ExtractRefOp op) {
  auto index = getIndexValueAsInt(op.getIndex());
  if (!index.has_value())
    return op.emitError("cannot translate runtime index to IQM Json");
  auto qrefName = llvm::formatv("{0}{1}", "QB", *index);
  emitter.getOrAssignName(op.getRef(), qrefName);
  return success();
}

static LogicalResult emitOperation(nlohmann::json &json, Emitter &emitter,
                                   quake::OperatorInterface optor) {
  auto name = optor->getName().stripDialect();
  std::vector<std::string> validInstructions{"z", "phased_rx"};
  if (std::find(validInstructions.begin(), validInstructions.end(),
                name.str()) == validInstructions.end())
    optor.emitError(
        "Invalid operation, code not lowered to IQM native gate set (" + name +
        ").");

  json["name"] = name;
  std::vector<std::string> qubits;
  for (auto target : optor.getTargets())
    qubits.push_back(emitter.getOrAssignName(target).str());
  json["qubits"] = qubits;

  if (!optor.getParameters().empty()) {
    // has to be 2 parameters
    auto parameter0 = getParameterValueAsDouble(optor.getParameters()[0]);
    auto parameter1 = getParameterValueAsDouble(optor.getParameters()[1]);

    json["args"]["angle_t"] = *parameter0;
    json["args"]["phase_t"] = *parameter1;
  } else
    json["args"] = nlohmann::json::object();

  return success();
}

static LogicalResult emitOperation(nlohmann::json &json, Emitter &emitter,
                                   quake::MzOp op) {
  json["name"] = "measurement";
  std::vector<std::string> qubits;
  for (auto target : op.getTargets())
    qubits.push_back(emitter.getOrAssignName(target).str());

  json["qubits"] = qubits;
  return success();
}

static LogicalResult emitOperation(nlohmann::json &json, Emitter &emitter,
                                   Operation &op) {
  using namespace quake;
  return llvm::TypeSwitch<Operation *, LogicalResult>(&op)
      .Case<ModuleOp>([&](auto op) { return emitOperation(json, emitter, op); })
      // Quake
      .Case<AllocaOp>([&](auto op) { return emitOperation(json, emitter, op); })
      .Case<ExtractRefOp>(
          [&](auto op) { return emitOperation(json, emitter, op); })
      .Case<OperatorInterface>(
          [&](auto optor) { return emitOperation(json, emitter, optor); })
      .Case<MzOp>([&](auto op) { return emitOperation(json, emitter, op); })
      // Ignore
      .Case<DeallocOp>([&](auto op) { return success(); })
      .Case<func::ReturnOp>([&](auto op) { return success(); })
      .Case<arith::ConstantOp>([&](auto op) { return success(); })
      .Default([&](Operation *) -> LogicalResult {
        // allow LLVM dialect ops (for storing measure results)
        if (op.getName().getDialectNamespace().equals("llvm"))
          return success();
        return op.emitOpError("unable to translate op to IQM Json");
      });
}

mlir::LogicalResult translateToIQMJson(mlir::Operation *op,
                                       llvm::raw_ostream &os) {
  nlohmann::json j;
  Emitter emitter(os);
  auto ret = emitOperation(j, emitter, *op);
  os << j.dump(4);
  return ret;
}

} // namespace cudaq
