//===- LoopAnalysis.cpp - Misc loop analysis routines //-------------------===//
//
// Copyright 2019 The MLIR Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================
//
// This file implements miscellaneous loop analysis routines.
//
//===----------------------------------------------------------------------===//

#include "mlir/Analysis/LoopAnalysis.h"

#include "mlir/Analysis/AffineAnalysis.h"
#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Statements.h"

using mlir::AffineExpr;

/// Returns the trip count of the loop as an affine expression if the latter is
/// expressible as an affine expression, and nullptr otherwise. The trip count
/// expression is simplified before returning.
AffineExpr *mlir::getTripCount(const ForStmt &forStmt) {
  // upper_bound - lower_bound + 1
  int64_t loopSpan;

  int64_t step = forStmt.getStep();
  auto *context = forStmt.getContext();

  if (forStmt.hasConstantBounds()) {
    int64_t lb = forStmt.getConstantLowerBound();
    int64_t ub = forStmt.getConstantUpperBound();
    loopSpan = ub - lb + 1;
  } else {
    const AffineBound lb = forStmt.getLowerBound();
    const AffineBound ub = forStmt.getUpperBound();
    auto lbMap = lb.getMap();
    auto ubMap = ub.getMap();
    // TODO(bondhugula): handle max/min of multiple expressions.
    if (lbMap->getNumResults() != 1 || ubMap->getNumResults() != 1 ||
        lbMap->getNumDims() != ubMap->getNumDims() ||
        lbMap->getNumSymbols() != ubMap->getNumSymbols()) {
      return nullptr;
    }

    // TODO(bondhugula): handle bounds with different operands.
    unsigned i, e = lb.getNumOperands();
    for (i = 0; i < e; i++) {
      if (lb.getStmtOperand(i).get() != ub.getStmtOperand(i).get())
        break;
    }
    // Bounds have different operands, unhandled for now.
    if (i != e)
      return nullptr;

    // ub_expr - lb_expr + 1
    auto *loopSpanExpr = AffineBinaryOpExpr::getAdd(
        AffineBinaryOpExpr::getSub(ubMap->getResult(0), lbMap->getResult(0),
                                   context),
        1, context);

    if (auto *expr = simplifyAffineExpr(loopSpanExpr, lbMap->getNumDims(),
                                        lbMap->getNumSymbols(), context))
      loopSpanExpr = expr;

    auto *cExpr = dyn_cast<AffineConstantExpr>(loopSpanExpr);
    if (!cExpr)
      return AffineBinaryOpExpr::getCeilDiv(loopSpanExpr, std::abs(step),
                                            context);
    loopSpan = cExpr->getValue();
  }

  // 0 iteration loops.
  if ((loopSpan < 0 && step >= 1) || (loopSpan > 0 && step <= -1))
    return 0;

  return AffineConstantExpr::get(
      static_cast<uint64_t>(loopSpan % step == 0 ? loopSpan / step
                                                 : loopSpan / step + 1),
      context);
}

/// Returns the trip count of the loop if it's a constant, None otherwise. This
/// method uses affine expression analysis (in turn using getTripCount) and is
/// able to determine constant trip count in non-trivial cases.
llvm::Optional<uint64_t> mlir::getConstantTripCount(const ForStmt &forStmt) {
  AffineExpr *tripCountExpr = getTripCount(forStmt);

  if (auto *constExpr = dyn_cast_or_null<AffineConstantExpr>(tripCountExpr))
    return constExpr->getValue();

  return None;
}

/// Returns the greatest known integral divisor of the trip count. Affine
/// expression analysis is used (indirectly through getTripCount), and
/// this method is thus able to determine non-trivial divisors.
uint64_t mlir::getLargestDivisorOfTripCount(const ForStmt &forStmt) {
  AffineExpr *tripCountExpr = getTripCount(forStmt);

  if (!tripCountExpr)
    return 1;

  if (auto *constExpr = dyn_cast<AffineConstantExpr>(tripCountExpr)) {
    uint64_t tripCount = constExpr->getValue();

    // 0 iteration loops (greatest divisor is 2^64 - 1).
    if (tripCount == 0)
      return ULONG_MAX;

    // The greatest divisor is the trip count.
    return tripCount;
  }

  // Trip count is not a known constant; return its largest known divisor.
  return tripCountExpr->getLargestKnownDivisor();
}