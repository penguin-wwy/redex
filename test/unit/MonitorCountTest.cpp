/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "IRAssembler.h"
#include "IRCode.h"
#include "MonitorCount.h"
#include "RedexTest.h"

using namespace monitor_count;

class MonitorCountTest : public RedexTest {};

TEST_F(MonitorCountTest, good1) {
  auto code = assembler::ircode_from_string(R"(
    (
      (load-param v0)
      (monitor-enter v0)

      (.try_start a)
      (check-cast v0 "LFoo;")
      (move-result-pseudo-object v1)
      (.try_end a)

      (.catch (a))
      (monitor-exit v0)
      (return-void)
    )
  )");
  code->build_cfg();

  EXPECT_EQ(find_synchronized_throw_outside_catch_all(*code), nullptr);
}

TEST_F(MonitorCountTest, noCatch) {
  auto code = assembler::ircode_from_string(R"(
    (
      (load-param v0)
      (monitor-enter v0)

      (.try_start a)
      (check-cast v0 "LFoo;")
      (move-result-pseudo-object v1)
      (.try_end a)
      (check-cast v0 "LBar;")
      (move-result-pseudo-object v1)

      (.catch (a))
      (monitor-exit v0)
      (return-void)
    )
  )");
  code->build_cfg();

  auto bad_insn = find_synchronized_throw_outside_catch_all(*code);
  ASSERT_NE(bad_insn, nullptr);
  EXPECT_EQ(bad_insn->opcode(), OPCODE_CHECK_CAST);
  EXPECT_EQ(bad_insn->get_type(), DexType::get_type("LBar;"));
}

TEST_F(MonitorCountTest, catchButNotCatchAll) {
  auto code = assembler::ircode_from_string(R"(
    (
      (load-param v0)
      (monitor-enter v0)

      (.try_start a)
      (check-cast v0 "LFoo;")
      (move-result-pseudo-object v1)
      (.try_end a)

      (.catch (a) "LMyThrowable;")
      (monitor-exit v0)
      (return-void)
    )
  )");
  code->build_cfg();

  auto bad_insn = find_synchronized_throw_outside_catch_all(*code);
  ASSERT_NE(bad_insn, nullptr);
  EXPECT_EQ(bad_insn->opcode(), OPCODE_CHECK_CAST);
  EXPECT_EQ(bad_insn->get_type(), DexType::get_type("LFoo;"));
}
