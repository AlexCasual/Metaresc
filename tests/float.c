#define __USE_GNU
#include <math.h>
#include <values.h>
#include <check.h>
#include <reslib.h>
#include <regression.h>

#define ASSERT_SAVE_LOAD_FLOAT(METHOD, VALUE) ({			\
      ASSERT_SAVE_LOAD_TYPE (METHOD, float, VALUE, CMP_SCALAR);		\
      ASSERT_SAVE_LOAD_TYPE (METHOD, struct_float, { VALUE }, CMP_STRUCT_X); \
    })

TYPEDEF_STRUCT (struct_float, float x);

RL_START_TEST(zero_float, "zero float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, 0); } END_TEST
RL_START_TEST(nan_float, "NAN float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, NAN); } END_TEST
RL_START_TEST(inf_float, "INFINITY float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, INFINITY); } END_TEST
RL_START_TEST(huge_valf_float, "HUGE_VALF float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, HUGE_VALF); } END_TEST
RL_START_TEST(flt_max_float, "FLT_MAX float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, FLT_MAX); } END_TEST
RL_START_TEST(flt_min_float, "FLT_MIN float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, FLT_MIN); } END_TEST
RL_START_TEST(flt_epsilon_float, "FLT_EPSILON float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, FLT_EPSILON); } END_TEST
RL_START_TEST(random_float, "random float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, 1.23456789012345678909L); } END_TEST
RL_START_TEST(pi_float, "pi long_float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, M_PI); } END_TEST
RL_START_TEST(e_float, "e long_float") { ALL_METHODS (ASSERT_SAVE_LOAD_FLOAT, M_E); } END_TEST
