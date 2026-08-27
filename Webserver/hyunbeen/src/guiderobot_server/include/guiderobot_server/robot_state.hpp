#ifndef GUIDEROBOT_SERVER__ROBOT_STATE_HPP_
#define GUIDEROBOT_SERVER__ROBOT_STATE_HPP_

#include <mutex>
#include <optional>
#include <string>

namespace guiderobot_server
{

/// 로봇 상태 머신. GET /api/status가 그대로 직렬화해서 내려주는 값들이다.
enum class DriveStatus
{
  kIdle,     // 대기 중
  kMoving,   // 목적지 이동 중
  kArrived,  // 실제 목적지 도착
};

inline const char * to_string(DriveStatus status)
{
  switch (status) {
    case DriveStatus::kIdle: return "idle";
    case DriveStatus::kMoving: return "moving";
    case DriveStatus::kArrived: return "arrived";
  }
  return "idle";
}

/// HTTP 핸들러 스레드와 ROS2 콜백(구독/액션 결과)이 동시에 접근하는 공유 상태.
/// 모든 접근은 반드시 mutex를 통해서만 이루어진다.
class RobotState
{
public:
  struct Snapshot
  {
    DriveStatus status;
    bool manual_mode;
    std::optional<std::string> current_destination;
  };

  Snapshot snapshot() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return {status_, manual_mode_, current_destination_};
  }

  void set_status(DriveStatus status)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
  }

  // Qt 관리자 화면이 /manual_mode 토픽으로 발행한 값을 반영한다.
  void set_manual_mode(bool enabled)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    manual_mode_ = enabled;
  }

  void set_destination(const std::string & destination_id)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    current_destination_ = destination_id;
    status_ = DriveStatus::kMoving;
  }

  void clear_destination()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    current_destination_.reset();
    status_ = DriveStatus::kIdle;
  }

private:
  mutable std::mutex mutex_;
  DriveStatus status_{DriveStatus::kIdle};
  bool manual_mode_{false};
  std::optional<std::string> current_destination_;
};

}  // namespace guiderobot_server

#endif  // GUIDEROBOT_SERVER__ROBOT_STATE_HPP_
