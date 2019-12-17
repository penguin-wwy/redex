/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "ControlFlow.h"
#include "WeakTopologicalOrdering.h"

#include <queue>

namespace loop_impl {

template <class NodeId>
using ComponentWrapper =
    std::reference_wrapper<const sparta::WtoComponent<NodeId>>;

/**
 * Visits a weak topological ordering component depth first search, applying
 * f to each basic block within the ordering
 */
template <class NodeId>
void visit_depth_first(const sparta::WtoComponent<NodeId>& comp,
                       std::function<void(NodeId)> f) {
  std::function<void(const sparta::WtoComponent<NodeId>&)> visit_component;
  visit_component = [&visit_component,
                     &f](const sparta::WtoComponent<NodeId>& v) {
    f(v.head_node());
    if (v.is_scc()) {
      for (const auto& inner : v) {
        visit_component(inner);
      }
    }
  };
  visit_component(comp);
}

/**
 * Constructs a level order traversal of the component heads of the provided
 * weak topological ordering and stores it in level_order.
 */
template <class NodeId>
void construct_level_order_traversal(
    std::vector<ComponentWrapper<NodeId>>& level_order,
    const sparta::WeakTopologicalOrdering<NodeId>& wto) {

  std::queue<ComponentWrapper<NodeId>> bfs_queue;

  // Get all of the outer SCCs by traversing the weak topological ordering.
  // For example, if the WTO looks like the following
  //
  //   1 2 (3 4 5 (6 7) 8) (9 10)
  //
  // bfs_queue will look like [3, 9].
  for (auto& vertex : wto) {
    if (vertex.is_scc()) {
      bfs_queue.push(vertex);
    }
  }

  // Run BFS on bfs_queue. We store the components in level order in the
  // level_order vector.
  while (!bfs_queue.empty()) {
    auto& vertex = bfs_queue.front().get();
    level_order.emplace_back(vertex);
    bfs_queue.pop();

    for (auto it = vertex.begin(); it != vertex.end(); ++it) {
      if (it->is_scc()) {
        bfs_queue.push(*it);
      }
    }
  }
}

class Loop {
 public:
  Loop(const std::vector<cfg::Block*>& blocks,
       const std::unordered_set<Loop*>& subloops,
       cfg::Block* loop_preheader)
      : m_blocks(blocks),
        m_block_set(blocks.begin(), blocks.end()),
        m_subloops(subloops),
        m_loop_preheader(loop_preheader),
        m_parent_loop(nullptr) {}

  Loop(const std::vector<cfg::Block*>& blocks,
       const std::unordered_set<Loop*>& subloops,
       cfg::Block* loop_preheader,
       Loop* parent_loop)
      : m_blocks(blocks),
        m_block_set(blocks.begin(), blocks.end()),
        m_subloops(subloops),
        m_loop_preheader(loop_preheader),
        m_parent_loop(parent_loop) {}

  cfg::Block* get_header();

  cfg::Block* get_preheader();

  Loop* get_parent_loop();

  void set_preheader(cfg::Block* ph);

  bool contains(Loop* l) const;

  bool contains(cfg::Block* block) const;

  int get_loop_depth() const;

  void get_exit_blocks(std::unordered_set<cfg::Block*>& result);

  void update_parent_loop_fields();

 private:
  Loop(const Loop&) = delete;
  const Loop& operator=(const Loop&) = delete;
  std::vector<cfg::Block*> m_blocks;
  std::unordered_set<cfg::Block*> m_block_set;
  std::unordered_set<Loop*> m_subloops;
  cfg::Block* m_loop_preheader;
  Loop* m_parent_loop;
};

class LoopInfo {
 public:
  explicit LoopInfo(cfg::ControlFlowGraph& cfg);
  size_t num_loops();

 private:
  std::vector<Loop*> m_loops;
  std::unordered_map<cfg::Block*, int> m_loop_depth;
};

} // namespace loop_impl
