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

#include "point_cloud.h"

#include "src/sensor/proto/sensor.pb.h"
#include "src/transform/transform.h"

namespace cartographer
{
namespace sensor
{

PointCloud TransformPointCloud(const PointCloud &point_cloud, const transform::Rigid3f &transform)
{
  PointCloud result;
  result.reserve(point_cloud.size());
  for (const Eigen::Vector3f &point : point_cloud)
  {
    result.emplace_back(transform * point);
  }
  return result;
}

PointCloud Crop(const PointCloud &point_cloud, const float min_z, const float max_z)
{
  PointCloud cropped_point_cloud;
  for (const auto &point : point_cloud)
  {
    if (min_z <= point.z() && point.z() <= max_z)
    {
      cropped_point_cloud.push_back(point);
    }
  }
  return cropped_point_cloud;
}

} // namespace sensor
} // namespace cartographer
