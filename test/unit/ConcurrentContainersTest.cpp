/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "ConcurrentContainers.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/thread/thread.hpp>

constexpr size_t kThreads = 50;
constexpr size_t kSampleSize = 1000;

class ConcurrentContainersTest : public ::testing::Test {
 protected:
  ConcurrentContainersTest()
      : m_rd_device(),
        m_generator(m_rd_device()),
        m_size(kSampleSize),
        m_elem_dist(0, 1000000000),
        m_data(generate_random_data()),
        m_subset_data(generate_random_subset(m_data)),
        m_data_set(m_data.begin(), m_data.end()) {
    for (size_t t = 0; t < kThreads; ++t) {
      for (size_t i = t; i < m_data.size(); i += kThreads) {
        m_samples[t].push_back(m_data[i]);
      }
      for (size_t i = t; i < m_data.size(); i += kThreads) {
        m_subset_samples[t].push_back(m_subset_data[i]);
      }
    }
  }

  std::vector<uint32_t> generate_random_data() {
    std::vector<uint32_t> s;
    for (size_t i = 0; i < m_size; ++i) {
      s.push_back(m_elem_dist(m_generator));
    }
    return s;
  }

  std::vector<uint32_t> generate_random_subset(
      const std::vector<uint32_t>& data) {
    auto new_data = data;
    std::random_shuffle(new_data.begin(), new_data.end());
    new_data.erase(new_data.begin(), new_data.begin() + m_size / 2);
    return new_data;
  }

  void run_on_samples(
      const std::vector<uint32_t> samples[],
      const std::function<void(const std::vector<uint32_t>&)>& operation) {
    std::vector<boost::thread> threads;
    for (size_t t = 0; t < kThreads; ++t) {
      const auto& sample = samples[t];
      threads.emplace_back([&sample, operation]() { operation(sample); });
    }
    for (auto& thread : threads) {
      thread.join();
    }
  }

  void run_on_samples(
      const std::function<void(const std::vector<uint32_t>&)>& operation) {
    run_on_samples(m_samples, operation);
  }

  void run_on_subset_samples(
      const std::function<void(const std::vector<uint32_t>&)>& operation) {
    run_on_samples(m_subset_samples, operation);
  }

  std::random_device m_rd_device;
  std::mt19937 m_generator;
  uint32_t m_size;
  std::uniform_int_distribution<uint32_t> m_elem_dist;
  std::vector<uint32_t> m_data;
  std::vector<uint32_t> m_subset_data;
  std::unordered_set<uint32_t> m_data_set;
  std::vector<uint32_t> m_samples[kThreads];
  std::vector<uint32_t> m_subset_samples[kThreads];
};

TEST_F(ConcurrentContainersTest, concurrentSetTest) {
  ConcurrentSet<uint32_t> set;

  run_on_samples([&set](const std::vector<uint32_t>& sample) {
    for (size_t i = 0; i < sample.size(); ++i) {
      set.insert(sample[i]);
      EXPECT_EQ(1, set.count(sample[i]));
    }
  });
  EXPECT_EQ(m_data_set.size(), set.size());
  auto check_initial_values = [&](const ConcurrentSet<uint32_t>& set) {
    for (uint32_t x : m_data) {
      EXPECT_EQ(1, set.count(x));
      EXPECT_NE(set.end(), set.find(x));
    }
  };
  check_initial_values(set);

  auto copy = set;

  run_on_subset_samples([&set](const std::vector<uint32_t>& sample) {
    for (size_t i = 0; i < sample.size(); ++i) {
      set.erase(sample[i]);
    }
  });

  for (uint32_t x : m_subset_data) {
    EXPECT_EQ(0, set.count(x));
    EXPECT_EQ(set.end(), set.find(x));
  }

  run_on_samples([&set](const std::vector<uint32_t>& sample) {
    for (size_t i = 0; i < sample.size(); ++i) {
      set.erase(sample[i]);
    }
  });
  EXPECT_EQ(0, set.size());
  for (uint32_t x : m_data) {
    EXPECT_EQ(0, set.count(x));
    EXPECT_EQ(set.end(), set.find(x));
  }

  // Check that copy is unchanged.
  check_initial_values(copy);

  auto moved = std::move(copy);
  check_initial_values(moved);

  set.insert({1, 2, 3});
  EXPECT_EQ(3, set.size());
  set.clear();
  EXPECT_EQ(0, set.size());
}

TEST_F(ConcurrentContainersTest, concurrentMapTest) {
  ConcurrentMap<std::string, uint32_t> map;

  run_on_samples([&map](const std::vector<uint32_t>& sample) {
    for (size_t i = 0; i < sample.size(); ++i) {
      std::string s = std::to_string(sample[i]);
      map.insert({s, sample[i]});
      EXPECT_EQ(1, map.count(s));
    }
  });
  EXPECT_EQ(m_data_set.size(), map.size());
  for (uint32_t x : m_data) {
    std::string s = std::to_string(x);
    EXPECT_EQ(1, map.count(s));
    auto it = map.find(s);
    EXPECT_NE(map.end(), it);
    EXPECT_EQ(s, it->first);
    EXPECT_EQ(x, it->second);
  }

  std::unordered_map<uint32_t, size_t> occurrences;
  for (uint32_t x : m_data) {
    ++occurrences[x];
  }
  run_on_samples([&map](const std::vector<uint32_t>& sample) {
    for (size_t i = 0; i < sample.size(); ++i) {
      std::string s = std::to_string(sample[i]);
      map.update(
          s, [&s, i](const std::string& key, uint32_t& value, bool key_exists) {
            EXPECT_EQ(s, key);
            EXPECT_TRUE(key_exists);
            ++value;
          });
    }
  });
  EXPECT_EQ(m_data_set.size(), map.size());
  auto check_initial_values =
      [&](const ConcurrentMap<std::string, uint32_t>& map) {
        for (uint32_t x : m_data) {
          std::string s = std::to_string(x);
          EXPECT_EQ(1, map.count(s));
          auto it = map.find(s);
          EXPECT_NE(map.end(), it);
          EXPECT_EQ(s, it->first);
          EXPECT_EQ(x + occurrences[x], it->second);
        }
      };
  check_initial_values(map);

  auto copy = map;

  run_on_subset_samples([&map](const std::vector<uint32_t>& sample) {
    for (size_t i = 0; i < sample.size(); ++i) {
      map.erase(std::to_string(sample[i]));
    }
  });

  for (uint32_t x : m_subset_data) {
    std::string s = std::to_string(x);
    EXPECT_EQ(0, map.count(s));
    EXPECT_EQ(map.end(), map.find(s));
  }

  run_on_samples([&map](const std::vector<uint32_t>& sample) {
    for (size_t i = 0; i < sample.size(); ++i) {
      map.erase(std::to_string(sample[i]));
    }
  });
  EXPECT_EQ(0, map.size());
  for (uint32_t x : m_data) {
    std::string s = std::to_string(x);
    EXPECT_EQ(0, map.count(s));
    EXPECT_EQ(map.end(), map.find(s));
  }

  // Check that copy is unchanged.
  check_initial_values(copy);

  auto moved = std::move(copy);
  check_initial_values(moved);

  map.insert({{"a", 1}, {"b", 2}, {"c", 3}});
  EXPECT_EQ(3, map.size());
  map.clear();
  EXPECT_EQ(0, map.size());
}
