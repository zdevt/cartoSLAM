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

#include "msg_conversion.h"

#include "../common/port.h"

#include "src/transform/proto/transform.pb.h"
#include "../transform/transform.h"
#include "geometry_msgs/Pose.h"
#include "geometry_msgs/Quaternion.h"
#include "geometry_msgs/Transform.h"
#include "geometry_msgs/TransformStamped.h"
#include "geometry_msgs/Vector3.h"
#include "glog/logging.h"
#include "ros/ros.h"
#include "sensor_msgs/Imu.h"
#include "sensor_msgs/LaserScan.h"
#include "sensor_msgs/MultiEchoLaserScan.h"
#include "sensor_msgs/PointCloud2.h"

namespace cartographer_ros {

  
using ::cartographer::transform::Rigid3d;
using ::cartographer::sensor::PointCloudWithIntensities;
constexpr float kPointCloudComponentFourMagic = 1.;


// For sensor_msgs::LaserScan.
bool HasEcho(float) { return true; }

float GetFirstEcho(float range) { return range; }

// For sensor_msgs::MultiEchoLaserScan.
bool HasEcho(const sensor_msgs::LaserEcho& echo) {
  return !echo.echoes.empty();
}

float GetFirstEcho(const sensor_msgs::LaserEcho& echo) {
  return echo.echoes[0];
}


template <typename LaserMessageType>
PointCloudWithIntensities LaserScanToPointCloudWithIntensities(const LaserMessageType &msg)
{
  PointCloudWithIntensities point_cloud;
  float angle = msg.angle_min;
  for (size_t i = 0; i < msg.ranges.size(); ++i)
  {
    const auto &echoes = msg.ranges[i];
    if (HasEcho(echoes))
    {
      const float first_echo = GetFirstEcho(echoes);
      if (msg.range_min <= first_echo && first_echo <= msg.range_max)
      {
        const Eigen::AngleAxisf rotation(angle, Eigen::Vector3f::UnitZ());
        point_cloud.points.push_back(rotation *
                                     (first_echo * Eigen::Vector3f::UnitX()));
        if (msg.intensities.size() > 0)
        {
          const auto &echo_intensities = msg.intensities[i];
          point_cloud.intensities.push_back(GetFirstEcho(echo_intensities));
        }
        else
        {
          point_cloud.intensities.push_back(0.f);
        }
      }
    }
    angle += msg.angle_increment;
  }
  return point_cloud;
}

sensor_msgs::PointCloud2 PreparePointCloud2Message(const double timestamp,
  const string& frame_id,
  const int num_points) {
sensor_msgs::PointCloud2 msg;
msg.header.stamp = ::ros::Time(timestamp);
msg.header.frame_id = frame_id;
msg.height = 1;
msg.width = num_points;
msg.fields.resize(3);
msg.fields[0].name = "x";
msg.fields[0].offset = 0;
msg.fields[0].datatype = sensor_msgs::PointField::FLOAT32;
msg.fields[0].count = 1;
msg.fields[1].name = "y";
msg.fields[1].offset = 4;
msg.fields[1].datatype = sensor_msgs::PointField::FLOAT32;
msg.fields[1].count = 1;
msg.fields[2].name = "z";
msg.fields[2].offset = 8;
msg.fields[2].datatype = sensor_msgs::PointField::FLOAT32;
msg.fields[2].count = 1;
msg.is_bigendian = false;
msg.point_step = 16;
msg.row_step = 16 * msg.width;
msg.is_dense = true;
msg.data.resize(16 * num_points);
return msg;
}


sensor_msgs::PointCloud2 ToPointCloud2Message(
  const double timestamp, const string& frame_id,
  const ::cartographer::sensor::PointCloud& point_cloud) {
auto msg = PreparePointCloud2Message(timestamp, frame_id, point_cloud.size());
::ros::serialization::OStream stream(msg.data.data(), msg.data.size());
for (const auto& point : point_cloud) {
  stream.next(point.x());
  stream.next(point.y());
  stream.next(point.z());
  stream.next(kPointCloudComponentFourMagic);
}
return msg;
}

PointCloudWithIntensities ToPointCloudWithIntensities(
  const sensor_msgs::LaserScan& msg) {
return LaserScanToPointCloudWithIntensities(msg);
}

Rigid3d ToRigid3d(const geometry_msgs::TransformStamped& transform) {
  return Rigid3d(ToEigen(transform.transform.translation),
                 ToEigen(transform.transform.rotation));
}

Rigid3d ToRigid3d(const geometry_msgs::Pose& pose) {
  return Rigid3d({pose.position.x, pose.position.y, pose.position.z},
                 ToEigen(pose.orientation));
}

Eigen::Vector3d ToEigen(const geometry_msgs::Vector3& vector3) {
  return Eigen::Vector3d(vector3.x, vector3.y, vector3.z);
}

Eigen::Quaterniond ToEigen(const geometry_msgs::Quaternion& quaternion) {
  return Eigen::Quaterniond(quaternion.w, quaternion.x, quaternion.y,
                            quaternion.z);
}

/*

geometry_msgs::Transform ToGeometryMsgTransform(const Rigid3d& rigid3d) {
  geometry_msgs::Transform transform;
  transform.translation.x = rigid3d.translation().x();
  transform.translation.y = rigid3d.translation().y();
  transform.translation.z = rigid3d.translation().z();
  transform.rotation.w = rigid3d.rotation().w();
  transform.rotation.x = rigid3d.rotation().x();
  transform.rotation.y = rigid3d.rotation().y();
  transform.rotation.z = rigid3d.rotation().z();
  return transform;
}

geometry_msgs::Pose ToGeometryMsgPose(const Rigid3d& rigid3d) {
  geometry_msgs::Pose pose;
  pose.position = ToGeometryMsgPoint(rigid3d.translation());
  pose.orientation.w = rigid3d.rotation().w();
  pose.orientation.x = rigid3d.rotation().x();
  pose.orientation.y = rigid3d.rotation().y();
  pose.orientation.z = rigid3d.rotation().z();
  return pose;
}

geometry_msgs::Point ToGeometryMsgPoint(const Eigen::Vector3d& vector3d) {
  geometry_msgs::Point point;
  point.x = vector3d.x();
  point.y = vector3d.y();
  point.z = vector3d.z();
  return point;
}*/

}  // namespace cartographer_ros
