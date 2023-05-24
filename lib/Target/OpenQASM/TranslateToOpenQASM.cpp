/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#include "cudaq/Frontend/nvqpp/AttributeNames.h"
#include "cudaq/Optimizer/Dialect/CC/CCTypes.h"
#include "cudaq/Optimizer/Dialect/Quake/QuakeOps.h"
#include "cudaq/Target/Emitter.h"
#include "cudaq/Target/OpenQASM/OpenQASMEmitter.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace cudaq;

//===----------------------------------------------------------------------===//
// Helper functions
//===----------------------------------------------------------------------===//

/// Translates operation names into OpenQASM gate names
static LogicalResult translateOperatorName(quake::OperatorInterface optor,
                                           StringRef &name) {
  StringRef qkeName = optor->getName().stripDialect();
  if (optor.getControls().size() == 0) {
    name = StringSwitch<StringRef>(qkeName).Case("r1", "cu1").Default(qkeName);
  } else if (optor.getControls().size() == 1) {
    name = StringSwitch<StringRef>(qkeName)
               .Case("h", "ch")
               .Case("x", "cx")
               .Case("y", "cy")
               .Case("z", "cz")
               .Case("r1", "cu1")
               .Case("rx", "crx")
               .Case("ry", "cry")
               .Case("rz", "crz")
               .Default("");
  } else if (optor.getControls().size() == 2) {
    name = StringSwitch<StringRef>(qkeName).Case("x", "ccx").Default("");
  }
  if (name.empty())
    return failure();
  return success();
}

static LogicalResult printParameters(Emitter &emitter, ValueRange parameters) {
  if (parameters.empty())
    return success();
  emitter.os << '(';
  auto isFailure = false;
  llvm::interleaveComma(parameters, emitter.os, [&](Value value) {
    auto parameter = getParameterValueAsDouble(value);
    if (!parameter.has_value()) {
      isFailure = true;
      return;
    }
    emitter.os << *parameter;
  });
  emitter.os << ')';
  // TODO: emit error here?
  return failure(isFailure);
}

static StringRef printClassicalAllocation(Emitter &emitter, Value bitOrVector,
                                          size_t size) {
  auto name = emitter.createName();
  emitter.os << llvm::formatv("creg {0}[{1}];\n", name, size);
  if (size == 1)
    name.append("[0]");
  return emitter.getOrAssignName(bitOrVector, name);
}

//===----------------------------------------------------------------------===//
// Emitters functions
//===----------------------------------------------------------------------===//

static LogicalResult emitOperation(Emitter &emitter, Operation &op);

static LogicalResult emitEntryPoint(Emitter &emitter, func::FuncOp kernel) {
  Emitter::Scope scope(emitter, /*isEntryPoint=*/true);
  for (Operation &op : kernel.getOps()) {
    if (failed(emitOperation(emitter, op)))
      return failure();
  }
  return success();
}

static LogicalResult emitOperation(Emitter &emitter, ModuleOp moduleOp) {
  func::FuncOp entryPoint = nullptr;
  emitter.os << "// Code generated by NVIDIA's nvq++ compiler\n";
  emitter.os << "OPENQASM 2.0;\n\n";
  emitter.os << "include \"qelib1.inc\";\n\n";
  for (Operation &op : moduleOp) {
    if (op.hasAttr(cudaq::entryPointAttrName)) {
      if (entryPoint)
        return moduleOp.emitError("has multiple entrypoints");
      entryPoint = dyn_cast_or_null<func::FuncOp>(op);
      continue;
    }
    if (failed(emitOperation(emitter, op)))
      return failure();
    emitter.os << '\n';
  }
  if (!entryPoint)
    return moduleOp.emitError("does not contain an entrypoint");
  return emitEntryPoint(emitter, entryPoint);
  return success();
}

static LogicalResult emitOperation(Emitter &emitter, quake::AllocaOp allocaOp) {
  Value refOrVeq = allocaOp.getRefOrVec();
  auto name = emitter.createName();
  auto size = 1;
  if (auto veq = dyn_cast<quake::VeqType>(refOrVeq.getType())) {
    if (!veq.hasSpecifiedSize())
      return allocaOp.emitError("allocates unbounded veq");
    size = veq.getSize();
  }
  emitter.os << llvm::formatv("qreg {0}[{1}];\n", name, size);
  if (isa<quake::RefType>(refOrVeq.getType()))
    name.append("[0]");
  emitter.getOrAssignName(refOrVeq, name);
  return success();
}

static LogicalResult emitOperation(Emitter &emitter, quake::ApplyOp op) {
  // In Quake's reference semantics form, kernels only return classical types.
  // Thus, we check whether the numbers of results is zero or not.
  if (op.getNumResults() > 0)
    return op.emitError("cannot return classical results");
  if (!op.getControls().empty())
    return op.emitError("cannot add controls to a gate call");
  emitter.os << op.getCallee();

  // Separate classical and quantum arguments.
  SmallVector<Value> parameters;
  SmallVector<Value> targets;
  for (auto arg : op.getArgs()) {
    if (isa<quake::RefType, quake::VeqType>(arg.getType()))
      targets.push_back(arg);
    else
      parameters.push_back(arg);
  }
  if (!parameters.empty()) {
    emitter.os << '(';
    llvm::interleaveComma(parameters, emitter.os, [&](auto param) {
      emitter.os << emitter.getOrAssignName(param);
    });
    emitter.os << ')';
  }
  emitter.os << ' ';
  llvm::interleaveComma(targets, emitter.os, [&](auto target) {
    emitter.os << emitter.getOrAssignName(target);
  });
  emitter.os << ";\n";
  return success();
}

static LogicalResult emitOperation(Emitter &emitter, func::FuncOp op) {
  // In Quake's reference semantics form, kernels only return classical types.
  // Thus, we check whether the numbers of results is zero or not.
  if (op.getNumResults() > 0)
    return op.emitError("cannot return classical results");

  // Separate classical and quantum arguments.
  SmallVector<Value> parameters;
  SmallVector<Value> targets;
  for (auto arg : op.getArguments()) {
    if (isa<quake::RefType, quake::VeqType>(arg.getType()))
      targets.push_back(arg);
    else
      parameters.push_back(arg);
  }

  if (targets.empty())
    return op.emitError("cannot translated classical functions");

  Emitter::Scope scope(emitter);
  emitter.os << "gate " << op.getName();
  if (!parameters.empty()) {
    emitter.os << '(';
    llvm::interleaveComma(parameters, emitter.os, [&](auto param) {
      auto name = emitter.createName("param");
      emitter.getOrAssignName(param, name);
      emitter.os << name;
    });
    emitter.os << ')';
  }
  emitter.os << ' ';
  llvm::interleaveComma(targets, emitter.os, [&](auto target) {
    auto name = emitter.createName("q");
    emitter.getOrAssignName(target, name);
    emitter.os << name;
  });
  emitter.os << " {\n";
  emitter.os.indent();
  for (Operation &op : op.getOps()) {
    if (failed(emitOperation(emitter, op)))
      return failure();
  }
  emitter.os.unindent();
  emitter.os << "}\n";
  return success();
}

static LogicalResult emitOperation(Emitter &emitter, quake::ExtractRefOp op) {
  auto veqName = emitter.getOrAssignName(op.getVeq());
  auto index = getIndexValueAsInt(op.getIndex());
  auto qrefName = llvm::formatv("{0}[{1}]", veqName, *index);
  emitter.getOrAssignName(op.getRef(), qrefName);
  return success();
}

static LogicalResult emitOperation(Emitter &emitter,
                                   quake::OperatorInterface optor) {
  // TODO: Handle adjoint for T and S
  if (optor.isAdj())
    return optor.emitError("cannot convert adjoint operations to OpenQASM 2.0");

  StringRef name;
  if (failed(translateOperatorName(optor, name)))
    return optor.emitError("cannot convert operation to OpenQASM 2.0");
  emitter.os << name;

  if (failed(printParameters(emitter, optor.getParameters())))
    return optor.emitError("failed to emit parameters");

  if (!optor.getControls().empty()) {
    emitter.os << ' ';
    llvm::interleaveComma(optor.getControls(), emitter.os, [&](auto control) {
      emitter.os << emitter.getOrAssignName(control);
    });
    emitter.os << ',';
  }
  emitter.os << ' ';
  llvm::interleaveComma(optor.getTargets(), emitter.os, [&](auto target) {
    emitter.os << emitter.getOrAssignName(target);
  });
  emitter.os << ";\n";
  return success();
}

static LogicalResult emitOperation(Emitter &emitter, quake::MzOp op) {
  if (op.getTargets().size() > 1)
    return op.emitError(
        "cannot translate measurements with more than one target");
  auto qrefOrVeq = op.getTargets()[0];
  auto size = 1;
  if (auto veq = dyn_cast<quake::VeqType>(qrefOrVeq.getType())) {
    if (!veq.hasSpecifiedSize())
      return op.emitError("cannot emmit measure on an unbounded veq");
    size = veq.getSize();
  }
  auto bitsName = printClassicalAllocation(emitter, op.getBits(), size);
  emitter.os << "measure " << emitter.getOrAssignName(qrefOrVeq) << " -> "
             << bitsName << ";\n";
  return success();
}

static LogicalResult emitOperation(Emitter &emitter, quake::ResetOp op) {
  emitter.os << "reset " << emitter.getOrAssignName(op.getTargets()) << ";";
  return success();
}

static LogicalResult emitOperation(Emitter &emitter, Operation &op) {
  using namespace quake;
  return llvm::TypeSwitch<Operation *, LogicalResult>(&op)
      // MLIR
      .Case<ModuleOp>([&](auto op) { return emitOperation(emitter, op); })
      .Case<func::FuncOp>([&](auto op) { return emitOperation(emitter, op); })
      // Quake
      .Case<ApplyOp>([&](auto op) { return emitOperation(emitter, op); })
      .Case<AllocaOp>([&](auto op) { return emitOperation(emitter, op); })
      .Case<ExtractRefOp>([&](auto op) { return emitOperation(emitter, op); })
      .Case<OperatorInterface>(
          [&](auto optor) { return emitOperation(emitter, optor); })
      .Case<MzOp>([&](auto op) { return emitOperation(emitter, op); })
      .Case<ResetOp>([&](auto op) { return emitOperation(emitter, op); })
      // Ignore
      .Case<DeallocOp>([&](auto op) { return success(); })
      .Case<func::ReturnOp>([&](auto op) { return success(); })
      .Case<arith::ConstantOp>([&](auto op) { return success(); })
      .Default([&](Operation *) -> LogicalResult {
        if (op.getName().getDialectNamespace().equals("llvm"))
          return success();
        return op.emitOpError("unable to translate op to OpenQASM 2.0");
      });
}

LogicalResult cudaq::translateToOpenQASM(Operation *op, raw_ostream &os) {
  Emitter emitter(os);
  return emitOperation(emitter, *op);
}
