#ifndef GUIDEROBOT_SERVER__DESTINATIONS_HPP_
#define GUIDEROBOT_SERVER__DESTINATIONS_HPP_

#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

namespace guiderobot_server
{

struct DestinationPose
{
  double x{0.0};
  double y{0.0};
  double yaw{0.0};       // radians
  std::string frame_id{"map"};
};

/// SERVER_HANDOFF.md / README2.md에 정의된 4개 목적지 ID.
/// 실제 좌표는 config/destinations.yaml에서 ROS2 파라미터로 덮어쓴다.
/// (config 파일을 만들지 않으면 여기 기본값 0,0으로 동작하니 반드시 실측 좌표로 채울 것)
class DestinationMap
{
public:
  explicit DestinationMap(rclcpp::Node * node)
  {
    static const std::unordered_map<std::string, DestinationPose> kDefaults = {
      {"restroom",  {0.0, 0.0, 0.0, "map"}},
      {"room_301",  {0.0, 0.0, 0.0, "map"}},
      {"room_302",  {0.0, 0.0, 0.0, "map"}},
      {"elevator",  {0.0, 0.0, 0.0, "map"}},
    };

    for (const auto & [id, default_pose] : kDefaults) {
      const std::string prefix = "destinations." + id + ".";
      node->declare_parameter<double>(prefix + "x", default_pose.x);
      node->declare_parameter<double>(prefix + "y", default_pose.y);
      node->declare_parameter<double>(prefix + "yaw", default_pose.yaw);
      node->declare_parameter<std::string>(prefix + "frame_id", default_pose.frame_id);

      DestinationPose pose;
      node->get_parameter(prefix + "x", pose.x);
      node->get_parameter(prefix + "y", pose.y);
      node->get_parameter(prefix + "yaw", pose.yaw);
      node->get_parameter(prefix + "frame_id", pose.frame_id);
      poses_[id] = pose;
    }
  }

  std::optional<geometry_msgs::msg::PoseStamped> lookup(
    const std::string & destination_id, const rclcpp::Time & stamp) const
  {
    const auto it = poses_.find(destination_id);
    if (it == poses_.end()) {
      return std::nullopt;
    }
    const DestinationPose & p = it->second;

    geometry_msgs::msg::PoseStamped goal;
    goal.header.frame_id = p.frame_id;
    goal.header.stamp = stamp;
    goal.pose.position.x = p.x;
    goal.pose.position.y = p.y;
    goal.pose.position.z = 0.0;
    goal.pose.orientation.z = std::sin(p.yaw / 2.0);
    goal.pose.orientation.w = std::cos(p.yaw / 2.0);
    return goal;
  }

  bool contains(const std::string & destination_id) const
  {
    return poses_.count(destination_id) > 0;
  }

private:
  std::unordered_map<std::string, DestinationPose> poses_;
};

}  // namespace guiderobot_server

#endif  // GUIDEROBOT_SERVER__DESTINATIONS_HPP_
