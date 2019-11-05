/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "DexClass.h"

namespace type {

DexType* _void();

DexType* _byte();

DexType* _char();

DexType* _short();

DexType* _int();

DexType* _long();

DexType* _boolean();

DexType* _float();

DexType* _double();

DexType* java_lang_String();

DexType* java_lang_Class();

DexType* java_lang_Enum();

DexType* java_lang_Object();

DexType* java_lang_Throwable();

DexType* java_lang_Boolean();

DexType* java_lang_Byte();

DexType* java_lang_Short();

DexType* java_lang_Character();

DexType* java_lang_Integer();

DexType* java_lang_Long();

DexType* java_lang_Float();

DexType* java_lang_Double();

/**
 * Return true if the type is a primitive.
 */
bool is_primitive(const DexType* type);

/**
 * Return true if the type is either a long or a double
 */
bool is_wide_type(const DexType* type);

/**
 * Return true if the type is an array type.
 */
bool is_array(const DexType* type);

/**
 * Return true if the type is an object type (array types included).
 */
bool is_object(const DexType* type);

/**
 * Return true if the type is a primitive type that fits within a 32-bit
 * register, i.e., boolean, byte, char, short or int.
 */
bool is_integer(const DexType* type);

bool is_boolean(const DexType* type);

bool is_long(const DexType* type);

bool is_float(const DexType* type);

bool is_double(const DexType* type);

bool is_void(const DexType* type);

/*
 * Return the shorty char for this type.
 * int -> I
 * bool -> Z
 * ... primitive etc.
 * any reference -> L
 */
char type_shorty(const DexType* type);

/**
 * Check whether a type can be cast to another type.
 * That is, if 'base_type' is an ancestor or an interface implemented by 'type'.
 * However the check is only within classes known to the app. So
 * you may effectively get false for a check_cast that would succeed at
 * runtime. Otherwise 'true' implies the type can cast.
 */
bool check_cast(const DexType* type, const DexType* base_type);

/**
 * Return the package for a valid DexType.
 */
std::string get_package_name(const DexType* type);

/**
 * Return the simple name w/o the package name and the ending ';' for a valid
 * DexType. E.g., 'Lcom/facebook/Simple;' -> 'Simple'.
 */
std::string get_simple_name(const DexType* type);

/**
 * Return the level of the array type, that is the number of '[' in the array.
 * int[] => [I
 * int[][] => [[I
 * etc.
 */
uint32_t get_array_level(const DexType* type);

/**
 * The component type of an array is the type of the values contained in the
 * array. E.g.:
 *
 * [LFoo; -> LFoo;
 * [[LFoo; -> [LFoo;
 */
DexType* get_array_component_type(const DexType* type);

/**
 * An array's component type may also be an array. Recursively unwrapping these
 * array types will give us the element type. E.g.:
 *
 * [LFoo; -> LFoo;
 * [[LFoo; -> LFoo;
 *
 * If the input argument is not an array type, this returns null.
 *
 * The terms "component type" and "element type" are defined in the JLS:
 * https://docs.oracle.com/javase/specs/jls/se7/html/jls-10.html
 */
DexType* get_array_element_type(const DexType* type);

/**
 * Return the element type of a given array type or the type itself if it's not
 * an array.
 *
 * Examples:
 *   [java.lang.String -> java.lang.String
 *   java.lang.Integer -> java.lang.Integer
 */
const DexType* get_element_type_if_array(const DexType* type);

/**
 * Return the (level 1) array type of a given type.
 */
DexType* make_array_type(const DexType* type);

/**
 * Return the array type of a given type in specified level.
 */
DexType* make_array_type(const DexType* type, uint32_t level);

}; // namespace type