#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"
#include "tf2_ros/transform_broadcaster.h"

using namespace std::chrono_literals;

class WheelOdomNode : public rclcpp::Node
{
public:
  WheelOdomNode()
  : Node("wheel_odom_node")
  {
    // 반드시 실제 차량 치수로 수정해야 함.
    wheel_radius_ = this->declare_parameter<double>(
      "wheel_radius", 0.0335);

    wheel_separation_ = this->declare_parameter<double>(
      "wheel_separation", 0.2026);

    publish_rate_ = this->declare_parameter<double>(
      "publish_rate", 50.0);

    left_topic_ = this->declare_parameter<std::string>(
      "left_rps_topic", "/encoder/left_rps");

    right_topic_ = this->declare_parameter<std::string>(
      "right_rps_topic", "/encoder/right_rps");

    odom_topic_ = this->declare_parameter<std::string>(
      "odom_topic", "/wheel/odom_raw");

    odom_frame_ = this->declare_parameter<std::string>(
      "odom_frame", "odom");

    base_frame_ = this->declare_parameter<std::string>(
      "base_frame", "base_link");

    if (wheel_radius_ <= 0.0) {
      throw std::runtime_error("wheel_radius must be greater than zero");
    }

    if (wheel_separation_ <= 0.0) {
      throw std::runtime_error("wheel_separation must be greater than zero");
    }

    if (publish_rate_ <= 0.0) {
      throw std::runtime_error("publish_rate must be greater than zero");
    }

    odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>(
      odom_topic_, rclcpp::QoS(10));

    tf_broadcaster_ =
      std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    left_sub_ = this->create_subscription<std_msgs::msg::Float32>(
      left_topic_,
      rclcpp::SensorDataQoS(),
      std::bind(
        &WheelOdomNode::leftRpsCallback,
        this,
        std::placeholders::_1));

    right_sub_ = this->create_subscription<std_msgs::msg::Float32>(
      right_topic_,
      rclcpp::SensorDataQoS(),
      std::bind(
        &WheelOdomNode::rightRpsCallback,
        this,
        std::placeholders::_1));

    const auto period = std::chrono::duration<double>(
      1.0 / publish_rate_);

    timer_ = this->create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period),
      std::bind(&WheelOdomNode::updateOdometry, this));

    last_time_ = this->now();

    RCLCPP_INFO(
      this->get_logger(),
      "wheel_radius=%.4f m, wheel_separation=%.4f m, rate=%.1f Hz",
      wheel_radius_,
      wheel_separation_,
      publish_rate_);

    RCLCPP_INFO(
      this->get_logger(),
      "Subscribing: %s, %s",
      left_topic_.c_str(),
      right_topic_.c_str());

    RCLCPP_INFO(
      this->get_logger(),
      "Publishing: %s and TF %s -> %s",
      odom_topic_.c_str(),
      odom_frame_.c_str(),
      base_frame_.c_str());
  }

private:
  void leftRpsCallback(const std_msgs::msg::Float32::SharedPtr msg)
  {
    std::lock_guard<std::mutex> lock(rps_mutex_);
    left_rps_ = static_cast<double>(msg->data);
    left_received_ = true;
    left_stamp_ = this->now();
  }

  void rightRpsCallback(const std_msgs::msg::Float32::SharedPtr msg)
  {
    std::lock_guard<std::mutex> lock(rps_mutex_);
    right_rps_ = static_cast<double>(msg->data);
    right_received_ = true;
    right_stamp_ = this->now();
  }

  void updateOdometry()
  {
    const rclcpp::Time now = this->now();
    const double dt = (now - last_time_).seconds();
    last_time_ = now;

    if (dt <= 0.0 || dt > 1.0) {
      return;
    }

    double left_rps;
    double right_rps;
    rclcpp::Time left_stamp;
    rclcpp::Time right_stamp;

    {
      std::lock_guard<std::mutex> lock(rps_mutex_);

      if (!left_received_ || !right_received_) {
        RCLCPP_WARN_THROTTLE(
          this->get_logger(),
          *this->get_clock(),
          3000,
          "Waiting for left and right RPS messages");
        return;
      }

      left_rps = left_rps_;
      right_rps = right_rps_;
      left_stamp = left_stamp_;
      right_stamp = right_stamp_;
    }

    // 0.5초 이상 RPS가 갱신되지 않으면 안전하게 0으로 처리
    constexpr double timeout_sec = 0.5;

    if ((now - left_stamp).seconds() > timeout_sec) {
      left_rps = 0.0;
    }

    if ((now - right_stamp).seconds() > timeout_sec) {
      right_rps = 0.0;
    }

    // RPS(회전/초) → 바퀴 선속도(m/s)
    const double circumference = 2.0 * M_PI * wheel_radius_;
    const double left_velocity = left_rps * circumference;
    const double right_velocity = right_rps * circumference;

    // 차동구동 운동학
    const double linear_velocity =
      (right_velocity + left_velocity) / 2.0;

    const double angular_velocity =
      (right_velocity - left_velocity) / wheel_separation_;

    const double delta_distance = linear_velocity * dt;
    const double delta_theta = angular_velocity * dt;

    // 회전 중 위치 오차를 줄이기 위해 중간 각도 사용
    const double middle_theta = theta_ + delta_theta / 2.0;

    x_ += delta_distance * std::cos(middle_theta);
    y_ += delta_distance * std::sin(middle_theta);
    theta_ += delta_theta;

    // 각도를 -pi ~ pi 범위로 정규화
    theta_ = std::atan2(std::sin(theta_), std::cos(theta_));

    // 평면 회전 yaw → quaternion
    const double half_theta = theta_ / 2.0;
    const double qz = std::sin(half_theta);
    const double qw = std::cos(half_theta);

    publishOdometry(
      now,
      linear_velocity,
      angular_velocity,
      qz,
      qw);

    publishTransform(now, qz, qw);
  }

  void publishOdometry(
    const rclcpp::Time & stamp,
    double linear_velocity,
    double angular_velocity,
    double qz,
    double qw)
  {
    nav_msgs::msg::Odometry odom;

    odom.header.stamp = stamp;
    odom.header.frame_id = odom_frame_;
    odom.child_frame_id = base_frame_;

    odom.pose.pose.position.x = x_;
    odom.pose.pose.position.y = y_;
    odom.pose.pose.position.z = 0.0;

    odom.pose.pose.orientation.x = 0.0;
    odom.pose.pose.orientation.y = 0.0;
    odom.pose.pose.orientation.z = qz;
    odom.pose.pose.orientation.w = qw;

    odom.twist.twist.linear.x = linear_velocity;
    odom.twist.twist.linear.y = 0.0;
    odom.twist.twist.angular.z = angular_velocity;

    // 기본 covariance 예시. 실제 데이터로 조정하는 것이 좋음.
    odom.pose.covariance[0] = 0.01;   // x
    odom.pose.covariance[7] = 0.01;   // y
    odom.pose.covariance[35] = 0.03;  // yaw

    odom.twist.covariance[0] = 0.02;
    odom.twist.covariance[35] = 0.04;

    odom_pub_->publish(odom);
  }

  void publishTransform(
    const rclcpp::Time & stamp,
    double qz,
    double qw)
  {
    geometry_msgs::msg::TransformStamped transform;

    transform.header.stamp = stamp;
    transform.header.frame_id = odom_frame_;
    transform.child_frame_id = base_frame_;

    transform.transform.translation.x = x_;
    transform.transform.translation.y = y_;
    transform.transform.translation.z = 0.0;

    transform.transform.rotation.x = 0.0;
    transform.transform.rotation.y = 0.0;
    transform.transform.rotation.z = qz;
    transform.transform.rotation.w = qw;

//    tf_broadcaster_->sendTransform(transform);
  }

  double wheel_radius_;
  double wheel_separation_;
  double publish_rate_;

  std::string left_topic_;
  std::string right_topic_;
  std::string odom_topic_;
  std::string odom_frame_;
  std::string base_frame_;

  double left_rps_{0.0};
  double right_rps_{0.0};

  bool left_received_{false};
  bool right_received_{false};

  double x_{0.0};
  double y_{0.0};
  double theta_{0.0};

  rclcpp::Time left_stamp_{0, 0, RCL_ROS_TIME};
  rclcpp::Time right_stamp_{0, 0, RCL_ROS_TIME};
  rclcpp::Time last_time_{0, 0, RCL_ROS_TIME};

  std::mutex rps_mutex_;

  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr left_sub_;
  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr right_sub_;

  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;

  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

  rclcpp::TimerBase::SharedPtr timer_;
};


int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  try {
    rclcpp::spin(std::make_shared<WheelOdomNode>());
  } catch (const std::exception & error) {
    RCLCPP_FATAL(
      rclcpp::get_logger("wheel_odom_node"),
      "Fatal error: %s",
      error.what());
  }

  rclcpp::shutdown();
  return 0;
}
