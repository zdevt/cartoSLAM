/*
 * Copyright 2016 The Cartographer Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/mapping/connected_components.h"

#include <algorithm>
#include <unordered_set>

#include "src/mapping/proto/connected_components.pb.h"
#include "glog/logging.h"

namespace cartographer {
namespace mapping {

ConnectedComponents::ConnectedComponents()
    : lock_(), forest_(), connection_map_() {}

void ConnectedComponents::Add(const int trajectory_id) {
  common::MutexLocker locker(&lock_);
  forest_.emplace(trajectory_id, trajectory_id);
}

void ConnectedComponents::Connect(const int trajectory_id_a,
                                     const int trajectory_id_b) {
  common::MutexLocker locker(&lock_);
  Union(trajectory_id_a, trajectory_id_b);
  auto sorted_pair = std::minmax(trajectory_id_a, trajectory_id_b);
  ++connection_map_[sorted_pair];
}

void ConnectedComponents::Union(const int trajectory_id_a,
                                   const int trajectory_id_b) {
  forest_.emplace(trajectory_id_a, trajectory_id_a);
  forest_.emplace(trajectory_id_b, trajectory_id_b);
  const int representative_a = FindSet(trajectory_id_a);
  const int representative_b = FindSet(trajectory_id_b);
  forest_[representative_a] = representative_b;
}

int ConnectedComponents::FindSet(const int trajectory_id) {
  auto it = forest_.find(trajectory_id);
  CHECK(it != forest_.end());
  if (it->first != it->second) {
    it->second = FindSet(it->second);
  }
  return it->second;
}

bool ConnectedComponents::TransitivelyConnected(const int trajectory_id_a,
                                                   const int trajectory_id_b) {
  if (trajectory_id_a == trajectory_id_b) {
    return true;
  }

  common::MutexLocker locker(&lock_);

  if (forest_.count(trajectory_id_a) == 0 ||
      forest_.count(trajectory_id_b) == 0) {
    return false;
  }
  return FindSet(trajectory_id_a) == FindSet(trajectory_id_b);
}

std::vector<std::vector<int>> ConnectedComponents::Components() {
  // Map from cluster exemplar -> growing cluster.
  std::unordered_map<int, std::vector<int>> map;
  common::MutexLocker locker(&lock_);
  for (const auto& trajectory_id_entry : forest_) {
    map[FindSet(trajectory_id_entry.first)].push_back(
        trajectory_id_entry.first);
  }

  std::vector<std::vector<int>> result;
  result.reserve(map.size());
  for (auto& pair : map) {
    result.emplace_back(std::move(pair.second));
  }
  return result;
}

int ConnectedComponents::ConnectionCount(const int trajectory_id_a,
                                            const int trajectory_id_b) {
  common::MutexLocker locker(&lock_);
  const auto it =
      connection_map_.find(std::minmax(trajectory_id_a, trajectory_id_b));
  return it != connection_map_.end() ? it->second : 0;
}

proto::ConnectedComponents ToProto(
    std::vector<std::vector<int>> connected_components) {
  proto::ConnectedComponents proto;
  for (auto& connected_component : connected_components) {
    std::sort(connected_component.begin(), connected_component.end());
  }
  std::sort(connected_components.begin(), connected_components.end());
  for (const auto& connected_component : connected_components) {
    auto* proto_connected_component = proto.add_connected_component();
    for (const int trajectory_id : connected_component) {
      proto_connected_component->add_trajectory_id(trajectory_id);
    }
  }
  return proto;
}

proto::ConnectedComponents::ConnectedComponent FindConnectedComponent(
    const proto::ConnectedComponents& connected_components,
    const int trajectory_id) {
  for (const auto& connected_component :
       connected_components.connected_component()) {
    if (std::find(connected_component.trajectory_id().begin(),
                  connected_component.trajectory_id().end(),
                  trajectory_id) != connected_component.trajectory_id().end()) {
      return connected_component;
    }
  }

  proto::ConnectedComponents::ConnectedComponent connected_component;
  connected_component.add_trajectory_id(trajectory_id);
  return connected_component;
}

}  // namespace mapping
}  // namespace cartographer
