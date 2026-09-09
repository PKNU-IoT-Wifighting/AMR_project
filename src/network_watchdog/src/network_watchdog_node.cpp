#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "std_msgs/msg/empty.hpp"

using namespace std::chrono_literals;

class NetworkWatchdog : public rclcpp::Node
{
public:
  NetworkWatchdog()
  : Node("network_watchdog"),
    last_heartbeat_(std::chrono::steady_clock::now()),
    heartbeat_received_(false),
    timeout_active_(true)
  {
    heartbeat_sub_ = create_subscription<std_msgs::msg::Empty>(
      "/pc_heartbeat",
      10,
      [this](const std_msgs::msg::Empty::SharedPtr) {
        last_heartbeat_ = std::chrono::steady_clock::now();
        heartbeat_received_ = true;

        if (timeout_active_) {
          RCLCPP_INFO(get_logger(), "PC heartbeat connected");
          timeout_active_ = false;
        }
      });

    cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel_watchdog_input",
      10,
      [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
        if (heartbeat_is_alive()) {
          cmd_vel_pub_->publish(*msg);
        }
      });

    cmd_vel_pub_ = create_publisher<geometry_msgs::msg::Twist>(
      "/cmd_vel",
      10);

    timer_ = create_wall_timer(
      50ms,
      std::bind(&NetworkWatchdog::timer_callback, this));

    RCLCPP_INFO(get_logger(), "Network watchdog started");
  }

private:
  bool heartbeat_is_alive() const
  {
    if (!heartbeat_received_) {
      return false;
    }

    const auto elapsed =
      std::chrono::steady_clock::now() - last_heartbeat_;

    return elapsed < 500ms;
  }

  void timer_callback()
  {
    if (heartbeat_is_alive()) {
      return;
    }

    geometry_msgs::msg::Twist stop_msg;
    cmd_vel_pub_->publish(stop_msg);

    if (!timeout_active_) {
      RCLCPP_ERROR(
        get_logger(),
        "PC heartbeat timeout - robot stopped");

      timeout_active_ = true;
    }
  }

  rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr heartbeat_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  std::chrono::steady_clock::time_point last_heartbeat_;

  bool heartbeat_received_;
  bool timeout_active_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<NetworkWatchdog>());
  rclcpp::shutdown();
  return 0;
}