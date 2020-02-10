/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "DexStore.h"
#include "Pass.h"

/// Looks for mentions of classes that have no constructors and use the fact
/// they can't be instantiated to simplify those mentions:
///
///  - If an instance method belongs to an uninstantiable class, its body can be
///    replaced with `throw null;`.
///  - `instance-of` with an uninstantiable type parameter always returns false.
///  - `invoke-virtual` and `invoke-direct` on methods whose class is
///    uninstantiable can be replaced by a `throw null;`, because they can only
///    be called on a `null` instance.
///  - `check-cast` with an uninstantiable type parameter is equivalent to a
///    a test which throws a `ClassCastException` if the value is not null.
///  - Field accesses on an uninstantiable class can be replaced by a `throw
///    null;` for the same reason as above.
///  - Field accesses returning an uninstantiable class will always return
///    `null`.
///
/// NOTE: This pass should not be run between invocations of RemoveUnreachable
/// and TypeErasure as the latter can effectively re-introduce constructors
/// removed by the former.
class RemoveUninstantiablesPass : public Pass {
 public:
  RemoveUninstantiablesPass() : Pass("RemoveUninstantiablesPass") {}

  /// Counts of references to uninstantiable classes removed.
  struct Stats {
    int instance_ofs = 0;
    int invokes = 0;
    int field_accesses_on_uninstantiable = 0;
    int instance_methods_of_uninstantiable = 0;
    int get_uninstantiables = 0;
    int check_casts = 0;

    Stats& operator+=(const Stats&);
    Stats operator+(const Stats&) const;

    /// Updates metrics tracked by \p mgr corresponding to these statistics.
    /// Simultaneously prints the statistics via TRACE.
    void report(PassManager& mgr) const;
  };

  /// Look for mentions of uninstantiable classes in \p cfg and modify them
  /// in-place.
  static Stats replace_uninstantiable_refs(cfg::ControlFlowGraph& cfg);

  /// Replace the instructions in \p cfg with `throw null;`.  Preserves the
  /// initial run of load-param instructions in the ControlFlowGraph.
  ///
  /// \pre Assumes that \p cfg is a non-empty instance method body.
  static Stats replace_all_with_throw(cfg::ControlFlowGraph& cfg);

  void run_pass(DexStoresVector&, ConfigFiles&, PassManager&) override;
};
