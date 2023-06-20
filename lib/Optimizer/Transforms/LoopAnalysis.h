/****************************************************************-*- C++ -*-****
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#pragma once

#include "cudaq/Optimizer/Dialect/CC/CCOps.h"

namespace cudaq::opt {

// We expect the loop control value to have the following form.
//
//   %final = cc.loop while ((%iter = %initial) -> (iN)) {
//     ...
//     %cond = arith.cmpi {<.<=,!=,>=,>}, %iter, %bound : iN
//     cc.condition %cond (%iter : iN)
//   } do {
//    ^bb1(%iter : iN):
//     ...
//     cc.continue %iter : iN
//   } step {
//    ^bb2(%iter : iN):
//     ...
//     %next = arith.{addi,subi} %iter, %step : iN
//     cc.continue %next : iN
//   }
//
// with the additional requirement that none of the `...` sections can modify
// the value of `%bound` or `%step`. Those values are invariant if there are
// no side-effects in the loop Op (no store or call operations) and these values
// do not depend on a block argument.
bool hasMonotonicControlInduction(cc::LoopOp loop);

/// A monotonic loop is defined to be a loop that will execute some bounded
/// number of iterations that can be predetermined before the loop, in fact,
/// executes. A semi-open iterval loop such as
/// ```
///   for(i = start; i < stop; i += step)
/// ```
/// is a monotonic loop that must execute a number of iterations as given
/// by the following equation. Early exits (break statements) are not permitted.
/// ```
///   let iterations = (stop - 1 - start + step) / step
///      iterations : if iterations > 0
///      0 : otherwise
/// ```
bool isaMonotonicLoop(mlir::Operation *op);

/// A counted loop is defined to be a loop that will execute some compile-time
/// constant number of iterations. We recognize a normalized, semi-open iterval
/// loop such as
/// ```
///   for(i = 0; i < number_of_iterations; ++i)
/// ```
/// as a canonical counted loop.
bool isaCountedLoop(cc::LoopOp op, bool allowClosedInterval = true);

struct LoopComponents {
  LoopComponents() = default;

  bool stepIsAnAddOp();
  bool shouldCommuteStepOp();
  bool isClosedIntervalForm();

  unsigned induction = 0;
  mlir::Value initialValue;
  mlir::Operation *compareOp = nullptr;
  mlir::Value compareValue;
  mlir::Region *stepRegion = nullptr;
  mlir::Operation *stepOp = nullptr;
  mlir::Value stepValue;
};

/// Recover the different subexpressions from the loop if it conforms to the
/// pattern. Given a LoopOp where induction is in a register:
/// ```
///   for (int induction = initialValue;
///        induction compareOp compareValue;
///        induction = induction stepOp stepValue) ...
/// ```
///
/// Get references to each of: induction, initialValue, compareOp, compareValue,
/// stepOp, and stepValue regardless of the loop structure. Otherwise return
/// `std::nullopt`.
std::optional<LoopComponents> getLoopComponents(cc::LoopOp loop);

} // namespace cudaq::opt
