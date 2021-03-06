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

#include "../mapping/submaps.h"

#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>

#include "Eigen/Geometry"
#include "src/common/make_unique.h"
#include "src/common/port.h"
#include "glog/logging.h"

namespace cartographer
{
namespace mapping
{

ProbabilityGrid ComputeCroppedProbabilityGrid(
    const ProbabilityGrid &probability_grid)
{
  Eigen::Array2i offset;
  CellLimits limits;
  probability_grid.ComputeCroppedLimits(&offset, &limits);
  const double resolution = probability_grid.limits().resolution();
  const Eigen::Vector2d max =
      probability_grid.limits().max() -
      resolution * Eigen::Vector2d(offset.y(), offset.x());
  ProbabilityGrid cropped_grid(MapLimits(resolution, max, limits));
  for (const Eigen::Array2i &xy_index : XYIndexRangeIterator(limits))
  {
    if (probability_grid.IsKnown(xy_index + offset))
    {
      cropped_grid.SetProbability(
          xy_index, probability_grid.GetProbability(xy_index + offset));
    }
  }
  return cropped_grid;
}

proto::SubmapsOptions CreateSubmapsOptions(
    common::LuaParameterDictionary *const parameter_dictionary)
{
  proto::SubmapsOptions options;
  //options.set_resolution(0.05);
  //options.set_num_range_data(30);
  //*options.mutable_range_data_inserter_options() =
  //    CreateRangeDataInserterOptions(
  //      parameter_dictionary);

  options.set_resolution(parameter_dictionary->GetDouble("resolution"));
  options.set_num_range_data(parameter_dictionary->GetNonNegativeInt("num_range_data"));
  *options.mutable_range_data_inserter_options() =
      CreateRangeDataInserterOptions(
          parameter_dictionary->GetDictionary("range_data_inserter").get());
  CHECK_GT(options.num_range_data(), 0);
  return options;
}


Submap::Submap(const MapLimits &limits, const Eigen::Vector2f &origin)
    : local_pose_(transform::Rigid3d::Translation(Eigen::Vector3d(origin.x(), origin.y(), 0.))),
      probability_grid_(limits) {}

void Submap::InsertRangeData(const sensor::RangeData &range_data,
                             const RangeDataInserter &range_data_inserter)
{
  CHECK(!finished_);
  range_data_inserter.Insert(range_data, &probability_grid_);
  SetNumRangeData(num_range_data() + 1);
}

void Submap::Finish()
{
  CHECK(!finished_);
  probability_grid_ = ComputeCroppedProbabilityGrid(probability_grid_);
  finished_ = true;
}

ActiveSubmaps::ActiveSubmaps(const proto::SubmapsOptions &options)
: options_(options),
  range_data_inserter_(options.range_data_inserter_options())
{
// We always want to have at least one likelihood field which we can return,
// and will create it at the origin in absence of a better choice.
AddSubmap(Eigen::Vector2f::Zero());
}

void ActiveSubmaps::InsertRangeData(const sensor::RangeData &range_data)
{
  for (auto &submap : submaps_)
  {
    submap->InsertRangeData(range_data, range_data_inserter_);
  }
  if (submaps_.back()->num_range_data() == options_.num_range_data())
  {
    AddSubmap(range_data.origin.head<2>());
  }
}

std::vector<std::shared_ptr<Submap>> ActiveSubmaps::submaps() const
{
  return submaps_;
}

int ActiveSubmaps::matching_index() const { return matching_submap_index_; }

void ActiveSubmaps::FinishSubmap()
{
  Submap *submap = submaps_.front().get();
  submap->Finish();
  ++matching_submap_index_;
  submaps_.erase(submaps_.begin());
}

void ActiveSubmaps::AddSubmap(const Eigen::Vector2f &origin)
{
  if (submaps_.size() > 1)
  {
    // This will crop the finished Submap before inserting a new Submap to
    // reduce peak memory usage a bit.
    FinishSubmap();
  }
  constexpr int kInitialSubmapSize = 100;
  submaps_.push_back(common::make_unique<Submap>(
      MapLimits(options_.resolution(),
                origin.cast<double>() + 0.5 * kInitialSubmapSize * options_.resolution() * Eigen::Vector2d::Ones(),
                CellLimits(kInitialSubmapSize, kInitialSubmapSize)),
      origin));
  LOG(INFO) << "Added submap " << matching_submap_index_ + submaps_.size();
}

} // namespace mapping
} // namespace cartographer
