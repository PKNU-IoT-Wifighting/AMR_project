#include <chrono>
#include <cmath>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "sensor_msgs/msg/imu.hpp"

#include "amr_ekf/ekf.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "tf2_ros/transform_broadcaster.h"

using namespace std::chrono_literals;


class EkfNode : public rclcpp::Node
{
public:

  EkfNode()
  : Node("amr_ekf_node"),
    gyro_z_bias_(-0.007742),
    first_predict_(true)
  {
    //
    // Wheel odometry subscriber
    //
    wheel_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "/wheel/odom_raw",
      10,
      std::bind(
        &EkfNode::wheelCallback,
        this,
        std::placeholders::_1
      )
    );

    //
    // MPU6050 IMU subscriber
    //
    imu_sub_ = this->create_subscription<sensor_msgs::msg::Imu>(
      "/imu/data_raw",
      10,
      std::bind(
        &EkfNode::imuCallback,
        this,
        std::placeholders::_1
      )
    );

    //
    // EKF 결과 publisher
    //
    filtered_odom_pub_ =
      this->create_publisher<nav_msgs::msg::Odometry>(
        "/odometry/filtered",
        10
      );

// TF broadcaster 생성
tf_broadcaster_ =
  std::make_unique<tf2_ros::TransformBroadcaster>(*this);
    //
    // Prediction + publish loop
    // 20ms = 50Hz
    //
    timer_ = this->create_wall_timer(
      20ms,
      std::bind(&EkfNode::timerCallback, this)
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Custom EKF node started"
    );

    RCLCPP_INFO(
      this->get_logger(),
      "gyro_z_bias = %.6f rad/s",
      gyro_z_bias_
    );
  }


private:

  //
  // Wheel odometry callback
  //
  void wheelCallback(
    const nav_msgs::msg::Odometry::SharedPtr msg)
  {
    const double v =
      msg->twist.twist.linear.x;

    const double omega =
      msg->twist.twist.angular.z;

    ekf_.updateWheel(
      v,
      omega
    );
  }


  //
  // IMU callback
  //
  void imuCallback(
    const sensor_msgs::msg::Imu::SharedPtr msg)
  {
    const double raw_gyro_z =
        msg->angular_velocity.x;

    //
    // 정지 상태에서 측정한 bias 제거
    //
    const double corrected_gyro_z =
      raw_gyro_z - gyro_z_bias_;

    ekf_.updateImu(
      corrected_gyro_z
    );
  }


  //
  // 50Hz Prediction
  //
  void timerCallback()
  {
    const rclcpp::Time now =
      this->now();

    //
    // 첫 실행에서는 dt가 없으므로
    // 시간만 저장
    //
    if (first_predict_) {
      last_predict_time_ = now;
      first_predict_ = false;
      return;
    }

    //
    // 실제 경과시간 계산
    //
    const double dt =
      (now - last_predict_time_).seconds();

    last_predict_time_ = now;

    //
    // 비정상적인 dt 방지
    //
    if (dt <= 0.0 || dt > 0.5) {
      return;
    }

    //
    // EKF Prediction
    //
    ekf_.predict(dt);

    //
    // 현재 EKF 상태 가져오기
    //
    const auto & state =
      ekf_.state();

    const double x =
      state(0);

    const double y =
      state(1);

    const double yaw =
      state(2);

    const double v =
      state(3);

    const double omega =
      state(4);


    //
    // EKF 결과를 ROS Odometry로 변환
    //
    nav_msgs::msg::Odometry odom;

    odom.header.stamp = now;
    odom.header.frame_id = "odom";
    odom.child_frame_id = "base_link";


    //
    // Position
    //
    odom.pose.pose.position.x = x;
    odom.pose.pose.position.y = y;
    odom.pose.pose.position.z = 0.0;


    //
    // yaw -> quaternion
    //
    odom.pose.pose.orientation.x = 0.0;
    odom.pose.pose.orientation.y = 0.0;

    odom.pose.pose.orientation.z =
      std::sin(yaw * 0.5);

    odom.pose.pose.orientation.w =
      std::cos(yaw * 0.5);


    //
    // Velocity
    //
    odom.twist.twist.linear.x = v;
    odom.twist.twist.linear.y = 0.0;

    odom.twist.twist.angular.z =
      omega;


    //
    // Publish
    //
    filtered_odom_pub_->publish(
      odom
    );
    geometry_msgs::msg::TransformStamped tf_msg;

tf_msg.header.stamp = now;
tf_msg.header.frame_id = "odom";
tf_msg.child_frame_id = "base_link";

tf_msg.transform.translation.x = x;
tf_msg.transform.translation.y = y;
tf_msg.transform.translation.z = 0.0;

tf_msg.transform.rotation.x = 0.0;
tf_msg.transform.rotation.y = 0.0;
tf_msg.transform.rotation.z = std::sin(yaw * 0.5);
tf_msg.transform.rotation.w = std::cos(yaw * 0.5);

tf_broadcaster_->sendTransform(tf_msg);
  }


  Ekf ekf_;

  double gyro_z_bias_;

  bool first_predict_;

  rclcpp::Time last_predict_time_;


  rclcpp::Subscription<
    nav_msgs::msg::Odometry
  >::SharedPtr wheel_sub_;

  rclcpp::Subscription<
    sensor_msgs::msg::Imu
  >::SharedPtr imu_sub_;

  rclcpp::Publisher<
    nav_msgs::msg::Odometry
  >::SharedPtr filtered_odom_pub_;

  rclcpp::TimerBase::SharedPtr timer_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
};


int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  rclcpp::spin(
    std::make_shared<EkfNode>()
  );

  rclcpp::shutdown();

  return 0;
}