/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "CommandProfiling.h"

#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <cstring>
#include <iostream>
#include <sstream>

#include "Debug.h"

namespace {

pid_t spawn(const std::string& cmd) {
#ifdef _POSIX_VERSION
  auto child = fork();
  if (child == -1) {
    always_assert_log(false, "Failed to fork");
    not_reached();
  } else if (child != 0) {
    return child;
  } else {
    execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);
    always_assert_log(false, "exec of command failed");
    not_reached();
  }
#else
  std::cerr << "spawn() is a no-op on non-POSIX systems" << std::endl;
  return 0;
#endif
}

/*
 * Appends the PID of the current process to :cmd and invokes it.
 */
pid_t spawn_profiler(const std::string& cmd) {
#ifdef _POSIX_VERSION
  auto parent = getpid();
  std::ostringstream ss;
  ss << cmd << " " << parent;
  auto full_cmd = ss.str();
  return spawn(full_cmd);
#else
  std::cerr << "spawn_profiler() is a no-op on non-POSIX systems" << std::endl;
  return 0;
#endif
}

pid_t kill_and_wait(pid_t pid, int sig) {
#ifdef _POSIX_VERSION
  kill(pid, sig);
  return waitpid(pid, nullptr, 0);
#else
  std::cerr << "kill_and_wait() is a no-op on non-POSIX systems" << std::endl;
  return 0;
#endif
}

void run_post_cmd(const std::string& cmd) {
#ifdef _POSIX_VERSION
  auto child = spawn(cmd + " perf.data");
  if (child == 0) {
    return;
  }

  int status;
  pid_t wpid = waitpid(child, &status, 0);
  always_assert_log(wpid != -1, "Failed to waitpid: %s", strerror(errno));
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::cerr << "Failed post-cmd " << cmd << std::endl;
  }
#else
  std::cerr << "run_post_cmd() is a no-op on non-POSIX systems" << std::endl;
#endif
}

} // namespace

ScopedCommandProfiling::ScopedCommandProfiling(
    boost::optional<std::string> cmd, boost::optional<std::string> post_cmd) {
  if (cmd) {
    m_post_cmd = std::move(post_cmd);
    fprintf(stderr, "Running profiler...\n");
    m_profiler = spawn_profiler(*cmd);
  }
}

ScopedCommandProfiling::~ScopedCommandProfiling() {
  if (m_profiler != -1) {
    std::cerr << "Waiting for profiler to finish..." << std::endl;
    kill_and_wait(m_profiler, SIGINT);
    if (m_post_cmd) {
      run_post_cmd(*m_post_cmd);
    }
  }
}
