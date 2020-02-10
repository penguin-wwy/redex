/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "DexClass.h"

namespace method {
/**
 * True if the method is a constructor (matches the "<init>" name)
 */
bool is_init(const DexMethodRef* method);

/**
 * True if the method is a static constructor (matches the "<clinit>" name)
 */
bool is_clinit(const DexMethodRef* method);

/**
 * Whether the method is a ctor or static ctor.
 */
inline bool is_any_init(const DexMethodRef* method) {
  return is_init(method) || is_clinit(method);
}

/**
 * Return true if the clinit is Trivial.
 * A trivial clinit should only contain a return-void instruction.
 */
bool is_trivial_clinit(const DexMethod* method);

/**
 * Check that the method contains no invoke-super instruction; this is a
 * requirement to relocate a method outside of its original inheritance
 * hierarchy.
 */
bool no_invoke_super(const DexMethod* method);

/**
 * Determine if the method is a constructor.
 *
 * Notes:
 * - Does NOT distinguish between <init> and <clinit>, will return true
 *   for static class initializers
 */

inline bool is_constructor(const DexMethod* meth) {
  return meth->get_access() & ACC_CONSTRUCTOR;
}

inline bool is_constructor(const DexMethodRef* meth) {
  return meth->is_def() &&
         method::is_constructor(static_cast<const DexMethod*>(meth));
}

/** Determine if the method takes no arguments. */
inline bool has_no_args(const DexMethodRef* meth) {
  return meth->get_proto()->get_args()->get_type_list().empty();
}

/** Determine if the method takes exactly n arguments. */
inline bool has_n_args(const DexMethodRef* meth, size_t n) {
  return meth->get_proto()->get_args()->get_type_list().size() == n;
}

/**
 * Determine if the method has code.
 *
 * Notes:
 * - Native methods are not considered to "have code"
 */
inline bool has_code(const DexMethodRef* meth) {
  return meth->is_def() &&
         static_cast<const DexMethod*>(meth)->get_code() != nullptr;
}

/**
 * Return true if method signatures (name and proto) match.
 */
inline bool signatures_match(const DexMethodRef* a, const DexMethodRef* b) {
  return a->get_name() == b->get_name() && a->get_proto() == b->get_proto();
}
}; // namespace method
