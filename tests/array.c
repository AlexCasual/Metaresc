#include <check.h>
#include <reslib.h>
#include <regression.h>

TYPEDEF_ENUM (packed_enum_t, ATTRIBUTES (__attribute__ ((packed, aligned (sizeof (uint16_t))))), ZERO, ONE, TWO)
TYPEDEF_STRUCT (char_array_t, (char, x, [2]))
TYPEDEF_STRUCT (packed_enum_array_t, (packed_enum_t, x, [2]))
TYPEDEF_STRUCT (int8_array_t, (int8_t, x, [2]))
TYPEDEF_STRUCT (int16_array_t, (int16_t, x, [2]))
TYPEDEF_STRUCT (int32_array_t, (int32_t, x, [2]))
TYPEDEF_STRUCT (int64_array_t, (int64_t, x, [2]))
TYPEDEF_STRUCT (float_array_t, (float, x, [2]))
TYPEDEF_STRUCT (double_array_t, (double, x, [2]))
TYPEDEF_STRUCT (ld_array_t, (long double, x, [2]))
TYPEDEF_STRUCT (string_array_t, (string_t, x, [2]))

#define ASSERT_SAVE_LOAD_ARRAY(METHOD, TYPE, ...) ({	\
      TYPE x = { { 1, 2 } };				\
      ASSERT_SAVE_LOAD (METHOD, TYPE, &x, __VA_ARGS__);	\
    })

RL_START_TEST (char_array, "array of chars") {
  char_array_t orig = { { 'a', 'b' }, };
  ALL_METHODS (ASSERT_SAVE_LOAD, char_array_t, &orig);
} END_TEST

RL_START_TEST (enum_array, "array of enums") {
  packed_enum_array_t orig = { { ONE, TWO }, };
  ALL_METHODS (ASSERT_SAVE_LOAD, packed_enum_array_t, &orig);
} END_TEST

RL_START_TEST (string_array, "array of strings") {
  string_array_t orig = { { "ONE", "TWO" }, };
  ALL_METHODS (ASSERT_SAVE_LOAD, string_array_t, &orig);
} END_TEST

RL_START_TEST (numeric_array, "array of numerics") {
  ALL_METHODS (ASSERT_SAVE_LOAD_ARRAY, int8_array_t);
  ALL_METHODS (ASSERT_SAVE_LOAD_ARRAY, int16_array_t);
  ALL_METHODS (ASSERT_SAVE_LOAD_ARRAY, int32_array_t);
  ALL_METHODS (ASSERT_SAVE_LOAD_ARRAY, int64_array_t);
  ALL_METHODS (ASSERT_SAVE_LOAD_ARRAY, float_array_t);
  ALL_METHODS (ASSERT_SAVE_LOAD_ARRAY, double_array_t);
  ALL_METHODS (ASSERT_SAVE_LOAD_ARRAY, ld_array_t);
} END_TEST

MAIN ();
