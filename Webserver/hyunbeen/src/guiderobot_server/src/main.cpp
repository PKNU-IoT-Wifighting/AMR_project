// // GuideRobot C++ Robot API 서버
// //
// // 역할
// //  1) Qt 관리자 화면 / 웹 사용자 HMI를 위한 REST API 제공
// //       GET  /api/status   -> {"status": "idle|moving|arrived", "manual_mode": bool}
// //       POST /api/command  -> {"destination": "room_301"} | {"command": "cancel"}
// //  2) 정적 웹 파일 서빙 (GuideRobot.StaticHmi: index.html, app.css, app.js, robot-mark.svg)
// //  3) ROS2 노드로 동작
// //       - /manual_mode (std_msgs/Bool) 구독 : Qt 관리자 화면이 직접 발행함
// //       - Nav2 NavigateToPose 액션 클라이언트 : 목적지 이동 / 취소 / 도착 결과 수신
// //
// // 스레드 모델
// //  - main 스레드   : rclcpp::spin() (ROS2 콜백: /manual_mode 구독, Nav2 액션 결과)
// //  - 별도 스레드   : httplib 서버 (svr.listen)
// //  - 공유 상태     : RobotState (mutex로 보호), HTTP 핸들러 <-> ROS2 콜백이 함께 사용

// #include <chrono>
// #include <functional>
// #include <memory>
// #include <mutex>
// #include <string>
// #include <thread>

// #include "httplib.h"

// #include "rclcpp/rclcpp.hpp"
// #include "rclcpp_action/rclcpp_action.hpp"
// #include "std_msgs/msg/bool.hpp"
// #include "nav2_msgs/action/navigate_to_pose.hpp"

// #include "guiderobot_server/robot_state.hpp"
// #include "guiderobot_server/destinations.hpp"

// using guiderobot_server::DriveStatus;
// using guiderobot_server::RobotState;
// using guiderobot_server::DestinationMap;
// using NavigateToPose = nav2_msgs::action::NavigateToPose;
// using GoalHandleNav = rclcpp_action::ClientGoalHandle<NavigateToPose>;

// namespace
// {
// constexpr const char * kNavigateActionName = "navigate_to_pose";
// constexpr const char * kManualModeTopic = "/manual_mode";
// }  // namespace

// class GuideRobotServerNode : public rclcpp::Node
// {
// public:
//   GuideRobotServerNode()
//   : Node("guiderobot_server_node"),
//     destinations_(this)
//   {
//     // 정적 웹 파일이 있는 디렉터리. 배포 시 ros2 param 또는 launch에서 덮어쓴다.
//     declare_parameter<std::string>("static_web_root", "./web");
//     declare_parameter<int>("http_port", 8080);

//     manual_mode_sub_ = create_subscription<std_msgs::msg::Bool>(
//       kManualModeTopic, rclcpp::QoS(10),
//       std::bind(&GuideRobotServerNode::on_manual_mode, this, std::placeholders::_1));

//     nav_client_ = rclcpp_action::create_client<NavigateToPose>(this, kNavigateActionName);

//     RCLCPP_INFO(get_logger(), "GuideRobot server node started.");
//   }

//   RobotState & state() { return state_; }
//   DestinationMap & destinations() { return destinations_; }

//   enum class SendGoalError
//   {
//     kNone,
//     kInvalidDestination,   // 목적지 ID 자체가 목록에 없음
//     kNav2Unavailable,      // ID는 맞지만 Nav2 action server를 못 찾음
//   };

//   /// 목적지 ID를 받아 Nav2 목표를 전송한다. 실패 사유를 out_error에 채운다.
//   bool send_navigation_goal(const std::string & destination_id, SendGoalError & out_error)
//   {
//     out_error = SendGoalError::kNone;

//     if (!destinations_.contains(destination_id)) {
//       out_error = SendGoalError::kInvalidDestination;
//       return false;
//     }

//     if (!nav_client_->wait_for_action_server(std::chrono::seconds(2))) {
//       RCLCPP_ERROR(get_logger(), "Nav2 action server(%s)를 찾을 수 없습니다.", kNavigateActionName);
//       out_error = SendGoalError::kNav2Unavailable;
//       return false;
//     }

//     const auto goal_pose = destinations_.lookup(destination_id, now());
//     if (!goal_pose) {
//       out_error = SendGoalError::kInvalidDestination;
//       return false;
//     }

//     NavigateToPose::Goal goal_msg;
//     goal_msg.pose = *goal_pose;

//     rclcpp_action::Client<NavigateToPose>::SendGoalOptions options;

//     options.result_callback =
//       [this](const GoalHandleNav::WrappedResult & result) {
//         switch (result.code) {
//           case rclcpp_action::ResultCode::SUCCEEDED:
//             RCLCPP_INFO(get_logger(), "목적지 도착. status -> arrived");
//             state_.set_status(DriveStatus::kArrived);
//             break;
//           case rclcpp_action::ResultCode::CANCELED:
//             RCLCPP_INFO(get_logger(), "안내 취소됨. status -> idle");
//             state_.clear_destination();
//             break;
//           case rclcpp_action::ResultCode::ABORTED:
//           default:
//             RCLCPP_WARN(get_logger(), "Nav2 목표 실패. status -> idle");
//             state_.clear_destination();
//             break;
//         }
//         std::lock_guard<std::mutex> lock(goal_handle_mutex_);
//         current_goal_handle_.reset();
//       };

//     auto goal_handle_future = nav_client_->async_send_goal(goal_msg, options);

//     // 액션 클라이언트 핸들을 저장해서 이후 cancel 요청에 사용한다.
//     // (goal_handle_future의 완료는 executor 콜백 스레드에서 처리되므로 여기서 블로킹하지 않는다)
//     std::thread([this, goal_handle_future = std::move(goal_handle_future)]() mutable {
//       auto handle = goal_handle_future.get();
//       std::lock_guard<std::mutex> lock(goal_handle_mutex_);
//       current_goal_handle_ = handle;
//     }).detach();

//     state_.set_destination(destination_id);
//     return true;
//   }

//   /// 진행 중인 안내를 취소한다. 취소할 목표가 없으면 상태만 idle로 되돌린다.
//   void cancel_navigation()
//   {
//     std::shared_ptr<GoalHandleNav> handle;
//     {
//       std::lock_guard<std::mutex> lock(goal_handle_mutex_);
//       handle = current_goal_handle_;
//     }
//     if (handle) {
//       nav_client_->async_cancel_goal(handle);
//       // 실제 상태 전환은 result_callback(CANCELED)에서 처리한다.
//     } else {
//       state_.clear_destination();
//     }
//   }

// private:
//   void on_manual_mode(const std_msgs::msg::Bool::SharedPtr msg)
//   {
//     RCLCPP_INFO(get_logger(), "manual_mode <- %s", msg->data ? "true" : "false");
//     state_.set_manual_mode(msg->data);
//     if (msg->data) {
//       // 수동 모드 진입 시 자율 안내는 즉시 중단한다.
//       cancel_navigation();
//     }
//   }

//   RobotState state_;
//   DestinationMap destinations_;
//   rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr manual_mode_sub_;
//   rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;

//   std::mutex goal_handle_mutex_;
//   std::shared_ptr<GoalHandleNav> current_goal_handle_;
// };

// namespace
// {

// void register_routes(httplib::Server & svr, GuideRobotServerNode & node)
// {
//   svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}});

//   svr.Get("/api/status", [&node](const httplib::Request &, httplib::Response & res) {
//     const auto snap = node.state().snapshot();
//     std::string body = "{";
//     body += "\"status\":\"" + std::string(guiderobot_server::to_string(snap.status)) + "\",";
//     body += "\"manual_mode\":" + std::string(snap.manual_mode ? "true" : "false");
//     if (snap.current_destination) {
//       body += ",\"destination\":\"" + *snap.current_destination + "\"";
//     }
//     body += "}";
//     res.set_content(body, "application/json; charset=utf-8");
//   });

//   svr.Post("/api/command", [&node](const httplib::Request & req, httplib::Response & res) {
//     // NOTE: httplib이 Content-Length만큼 본문을 모두 읽은 뒤 콜백을 호출하므로
//     // README2.md에서 요구한 "본문이 나뉘어 도착해도 전부 받은 뒤 파싱" 조건을 만족한다.
//     std::string destination;
//     std::string command;

//     // 아주 가벼운 JSON 파싱만 필요하므로 정식 JSON 라이브러리 없이 처리한다.
//     // 팀 컨벤션이나 요구사항이 늘어나면 nlohmann::json 도입을 권장한다.
//     auto extract_field = [](const std::string & json, const std::string & key) -> std::string {
//       const std::string needle = "\"" + key + "\"";
//       auto pos = json.find(needle);
//       if (pos == std::string::npos) return "";
//       pos = json.find(':', pos);
//       if (pos == std::string::npos) return "";
//       pos = json.find('"', pos);
//       if (pos == std::string::npos) return "";
//       const auto end = json.find('"', pos + 1);
//       if (end == std::string::npos) return "";
//       return json.substr(pos + 1, end - pos - 1);
//     };

//     destination = extract_field(req.body, "destination");
//     command = extract_field(req.body, "command");

//     if (!destination.empty()) {
//       GuideRobotServerNode::SendGoalError error;
//       const bool ok = node.send_navigation_goal(destination, error);
//       if (!ok) {
//         switch (error) {
//           case GuideRobotServerNode::SendGoalError::kNav2Unavailable:
//             // 목적지 ID는 유효하지만 Nav2가 아직 안 떠 있는 경우.
//             // 503(Service Unavailable)로 구분해서 응답한다.
//             res.status = 503;
//             res.set_content(R"({"error":"nav2_unavailable"})", "application/json");
//             return;
//           case GuideRobotServerNode::SendGoalError::kInvalidDestination:
//           default:
//             res.status = 400;
//             res.set_content(R"({"error":"invalid_destination"})", "application/json");
//             return;
//         }
//       }
//       res.set_content(R"({"success":true})", "application/json");
//       return;
//     }

//     if (command == "cancel" || command == "stop") {
//       node.cancel_navigation();
//       res.set_content(R"({"success":true})", "application/json");
//       return;
//     }

//     res.status = 400;
//     res.set_content(R"({"error":"destination_or_command_required"})", "application/json");
//   });
// }

// }  // namespace

// int main(int argc, char ** argv)
// {
//   rclcpp::init(argc, argv);
//   auto node = std::make_shared<GuideRobotServerNode>();

//   std::string static_root;
//   int http_port = 8080;
//   node->get_parameter("static_web_root", static_root);
//   node->get_parameter("http_port", http_port);

//   httplib::Server svr;
//   // 정적 파일: GuideRobot.StaticHmi (index.html, app.css, app.js, robot-mark.svg)
//   svr.set_mount_point("/", static_root);
//   register_routes(svr, *node);

//   std::thread http_thread([&svr, http_port, &static_root, &node]() {
//     RCLCPP_INFO(node->get_logger(), "HTTP 서버 시작: 0.0.0.0:%d (정적 파일 루트: %s)",
//       http_port, static_root.c_str());
//     if (!svr.listen("0.0.0.0", http_port)) {
//       RCLCPP_ERROR(node->get_logger(), "HTTP 서버 시작 실패 (포트 %d 사용 중일 수 있음)", http_port);
//     }
//   });

//   rclcpp::spin(node);

//   svr.stop();
//   if (http_thread.joinable()) {
//     http_thread.join();
//   }
//   rclcpp::shutdown();
//   return 0;
// }


// GuideRobot C++ Robot API 서버
//
// 역할
//  1) Qt 관리자 화면 / 웹 사용자 HMI를 위한 REST API 제공
//       GET  /api/status
//         -> {"status": "idle|moving|arrived", "manual_mode": bool}
//
//       POST /api/command
//         -> {"destination": "room_301"}
//         -> {"command": "cancel"}
//         -> {"command": "stop", "manual_mode": true}
//         -> {"manual_mode": true}
//         -> {"manual_mode": false}
//
//  2) 정적 웹 파일 서빙
//       (GuideRobot.StaticHmi: index.html, app.css, app.js, robot-mark.svg)
//
//  3) ROS2 노드로 동작
//       - /manual_mode (std_msgs/Bool) 구독
//       - Nav2 NavigateToPose 액션 클라이언트
//         : 목적지 이동 / 취소 / 도착 결과 수신
//
// 스레드 모델
//  - main 스레드   : rclcpp::spin()
//  - 별도 스레드   : httplib 서버 (svr.listen)
//  - 공유 상태     : RobotState
//                    mutex로 보호
//
// 중요
//  - Qt가 HTTP POST로 manual_mode를 보내는 경우
//    이 서버가 직접 RobotState의 manual_mode를 변경한다.
//  - Qt가 ROS2 /manual_mode 토픽을 발행하는 것과는 별개다.

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "httplib.h"

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_msgs/msg/bool.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"

#include "guiderobot_server/robot_state.hpp"
#include "guiderobot_server/destinations.hpp"

using guiderobot_server::DriveStatus;
using guiderobot_server::RobotState;
using guiderobot_server::DestinationMap;
using NavigateToPose = nav2_msgs::action::NavigateToPose;
using GoalHandleNav = rclcpp_action::ClientGoalHandle<NavigateToPose>;

namespace
{

constexpr const char * kNavigateActionName = "navigate_to_pose";
constexpr const char * kManualModeTopic = "/manual_mode";

}  // namespace


class GuideRobotServerNode : public rclcpp::Node
{
public:

    GuideRobotServerNode()
        : Node("guiderobot_server_node"),
        destinations_(this)
    {
        // 정적 웹 파일이 있는 디렉터리.
        // 배포 시 ros2 parameter 또는 launch에서 덮어쓴다.
        declare_parameter<std::string>("static_web_root", "./web");
        declare_parameter<int>("http_port", 8080);

        // ------------------------------------------------------------
        // ROS2 /manual_mode 토픽 구독
        // ------------------------------------------------------------
        manual_mode_sub_ = create_subscription<std_msgs::msg::Bool>(
            kManualModeTopic,
            rclcpp::QoS(10),
            std::bind(
                &GuideRobotServerNode::on_manual_mode,
                this,
                std::placeholders::_1));

        // ------------------------------------------------------------
        // Nav2 NavigateToPose 액션 클라이언트
        // ------------------------------------------------------------
        nav_client_ =
            rclcpp_action::create_client<NavigateToPose>(
                this,
                kNavigateActionName);

        RCLCPP_INFO(
            get_logger(),
            "GuideRobot server node started.");
    }


    RobotState & state()
    {
        return state_;
    }


    DestinationMap & destinations()
    {
        return destinations_;
    }


    enum class SendGoalError
    {
        kNone,

        // 목적지 ID 자체가 목록에 없음
        kInvalidDestination,

        // 목적지는 맞지만 Nav2 action server가 없음
        kNav2Unavailable,
    };


    // ============================================================
    // Nav2 목표 전송
    // ============================================================
    bool send_navigation_goal(
        const std::string & destination_id,
        SendGoalError & out_error)
    {
        out_error = SendGoalError::kNone;

        // ------------------------------------------------------------
        // 목적지 존재 여부 확인
        // ------------------------------------------------------------
        if (!destinations_.contains(destination_id)) {
            out_error = SendGoalError::kInvalidDestination;
            return false;
        }

        // ------------------------------------------------------------
        // Nav2 action server 확인
        // ------------------------------------------------------------
        if (!nav_client_->wait_for_action_server(
                std::chrono::seconds(2)))
        {
            RCLCPP_ERROR(
                get_logger(),
                "Nav2 action server(%s)를 찾을 수 없습니다.",
                kNavigateActionName);

            out_error = SendGoalError::kNav2Unavailable;
            return false;
        }

        // ------------------------------------------------------------
        // 목적지 좌표 가져오기
        // ------------------------------------------------------------
        const auto goal_pose =
            destinations_.lookup(destination_id, now());

        if (!goal_pose) {
            out_error = SendGoalError::kInvalidDestination;
            return false;
        }

        // ------------------------------------------------------------
        // NavigateToPose Goal 생성
        // ------------------------------------------------------------
        NavigateToPose::Goal goal_msg;
        goal_msg.pose = *goal_pose;

        rclcpp_action::Client<NavigateToPose>::SendGoalOptions options;

        // ------------------------------------------------------------
        // Nav2 결과 콜백
        // ------------------------------------------------------------
        options.result_callback =
            [this](const GoalHandleNav::WrappedResult & result)
        {
            switch (result.code)
            {
            case rclcpp_action::ResultCode::SUCCEEDED:

                RCLCPP_INFO(
                    get_logger(),
                    "목적지 도착. status -> arrived");

                state_.set_status(
                    DriveStatus::kArrived);

                break;


            case rclcpp_action::ResultCode::CANCELED:

                RCLCPP_INFO(
                    get_logger(),
                    "안내 취소됨. status -> idle");

                state_.clear_destination();

                break;


            case rclcpp_action::ResultCode::ABORTED:

            default:

                RCLCPP_WARN(
                    get_logger(),
                    "Nav2 목표 실패. status -> idle");

                state_.clear_destination();

                break;
            }

            std::lock_guard<std::mutex> lock(
                goal_handle_mutex_);

            current_goal_handle_.reset();
        };


        // ------------------------------------------------------------
        // Nav2 Goal 전송
        // ------------------------------------------------------------
        auto goal_handle_future =
            nav_client_->async_send_goal(
                goal_msg,
                options);


        // ------------------------------------------------------------
        // Goal Handle 저장
        // ------------------------------------------------------------
        std::thread(
            [this,
             goal_handle_future =
             std::move(goal_handle_future)]() mutable
            {
                auto handle =
                    goal_handle_future.get();

                std::lock_guard<std::mutex> lock(
                    goal_handle_mutex_);

                current_goal_handle_ = handle;
            }).detach();


        // ------------------------------------------------------------
        // 현재 목적지 상태 저장
        // ------------------------------------------------------------
        state_.set_destination(
            destination_id);

        return true;
    }


    // ============================================================
    // 진행 중인 Nav2 안내 취소
    // ============================================================
    void cancel_navigation()
    {
        std::shared_ptr<GoalHandleNav> handle;

        {
            std::lock_guard<std::mutex> lock(
                goal_handle_mutex_);

            handle = current_goal_handle_;
        }

        if (handle)
        {
            nav_client_->async_cancel_goal(handle);

            // 실제 상태 변경은
            // result_callback(CANCELED)에서 처리한다.
        }
        else
        {
            state_.clear_destination();
        }
    }


private:

    // ============================================================
    // ROS2 /manual_mode 토픽 콜백
    // ============================================================
    void on_manual_mode(
        const std_msgs::msg::Bool::SharedPtr msg)
    {
        RCLCPP_INFO(
            get_logger(),
            "ROS /manual_mode <- %s",
            msg->data ? "true" : "false");

        state_.set_manual_mode(msg->data);

        if (msg->data)
        {
            // 수동 모드 진입 시
            // 자율 안내를 즉시 중단한다.
            cancel_navigation();
        }
    }


    RobotState state_;

    DestinationMap destinations_;

    // ROS2 /manual_mode subscriber
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr
        manual_mode_sub_;

    // Nav2 NavigateToPose action client
    rclcpp_action::Client<NavigateToPose>::SharedPtr
        nav_client_;

    // 현재 Nav2 goal handle
    std::mutex goal_handle_mutex_;

    std::shared_ptr<GoalHandleNav>
        current_goal_handle_;
};


namespace
{


// ================================================================
// HTTP REST API 등록
// ================================================================
void register_routes(
    httplib::Server & svr,
    GuideRobotServerNode & node)
{

    // --------------------------------------------------------------
    // CORS
    // --------------------------------------------------------------
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"}
    });


    // ==============================================================
    // GET /api/status
    //
    // 현재 서버 상태 반환
    //
    // 예:
    // {"status":"idle","manual_mode":false}
    //
    // 또는
    //
    // {"status":"moving","manual_mode":true,"destination":"room_301"}
    // ==============================================================
    svr.Get(
        "/api/status",
        [&node](
            const httplib::Request &,
            httplib::Response & res)
        {
            const auto snap =
                node.state().snapshot();

            std::string body = "{";

            body +=
                "\"status\":\"" +
                std::string(
                    guiderobot_server::to_string(
                        snap.status)) +
                "\",";

            body +=
                "\"manual_mode\":" +
                std::string(
                    snap.manual_mode
                        ? "true"
                        : "false");

            if (snap.current_destination)
            {
                body +=
                    ",\"destination\":\"" +
                    *snap.current_destination +
                    "\"";
            }

            body += "}";

            res.set_content(
                body,
                "application/json; charset=utf-8");
        });


    // ==============================================================
    // POST /api/command
    //
    // 지원:
    //
    // 1. 목적지 이동
    //    {"destination":"room_301"}
    //
    // 2. 이동 취소
    //    {"command":"cancel"}
    //
    // 3. 정지
    //    {"command":"stop"}
    //
    // 4. 수동 모드 ON
    //    {"manual_mode":true}
    //
    // 5. 수동 모드 OFF
    //    {"manual_mode":false}
    //
    // 6. Qt에서 현재처럼 같이 보내는 경우
    //    {"command":"stop","manual_mode":true}
    // ==============================================================
    svr.Post(
        "/api/command",
        [&node](
            const httplib::Request & req,
            httplib::Response & res)
        {

            std::string destination;
            std::string command;


            // ==========================================================
            // 문자열 JSON field 추출
            // ==========================================================
            auto extract_field =
                [](
                    const std::string & json,
                    const std::string & key) -> std::string
            {
                const std::string needle =
                    "\"" + key + "\"";

                auto pos =
                    json.find(needle);

                if (pos == std::string::npos)
                {
                    return "";
                }

                pos =
                    json.find(':', pos);

                if (pos == std::string::npos)
                {
                    return "";
                }

                pos =
                    json.find('"', pos);

                if (pos == std::string::npos)
                {
                    return "";
                }

                const auto end =
                    json.find('"', pos + 1);

                if (end == std::string::npos)
                {
                    return "";
                }

                return json.substr(
                    pos + 1,
                    end - pos - 1);
            };


            // ==========================================================
            // boolean JSON field 추출
            //
            // 예:
            //
            // {"manual_mode":true}
            //
            // -> true
            //
            // {"manual_mode":false}
            //
            // -> false
            //
            // 반환값:
            //   0 = field 없음
            //   1 = true
            //   2 = false
            // ==========================================================
            auto extract_bool =
                [](
                    const std::string & json,
                    const std::string & key) -> int
            {
                const std::string needle =
                    "\"" + key + "\"";

                auto pos =
                    json.find(needle);

                if (pos == std::string::npos)
                {
                    return 0;
                }

                pos =
                    json.find(':', pos);

                if (pos == std::string::npos)
                {
                    return 0;
                }

                // ':' 다음의 공백 건너뛰기
                ++pos;

                while (
                    pos < json.size() &&
                    (
                        json[pos] == ' ' ||
                        json[pos] == '\t' ||
                        json[pos] == '\n' ||
                        json[pos] == '\r'
                        ))
                {
                    ++pos;
                }

                // true
                if (
                    json.compare(
                        pos,
                        4,
                        "true") == 0)
                {
                    return 1;
                }

                // false
                if (
                    json.compare(
                        pos,
                        5,
                        "false") == 0)
                {
                    return 2;
                }

                return 0;
            };


            // ==========================================================
            // JSON parsing
            // ==========================================================
            destination =
                extract_field(
                    req.body,
                    "destination");

            command =
                extract_field(
                    req.body,
                    "command");


            // ==========================================================
            // ★★★ manual_mode 처리 ★★★
            //
            // 기존 코드에는 이 부분이 없었음.
            //
            // Qt가 HTTP로
            //
            // {"command":"stop","manual_mode":true}
            //
            // 를 보내면 여기서 직접 RobotState를 변경한다.
            // ==========================================================
            const int manual_mode_value =
                extract_bool(
                    req.body,
                    "manual_mode");


            if (manual_mode_value == 1)
            {
                // --------------------------------------------------------
                // manual_mode = true
                // --------------------------------------------------------
                RCLCPP_INFO(
                    node.get_logger(),
                    "HTTP manual_mode <- true");

                node.state().set_manual_mode(true);

                // 수동 모드로 전환하면
                // 현재 Nav2 자율주행이 있다면 취소한다.
                node.cancel_navigation();
            }
            else if (manual_mode_value == 2)
            {
                // --------------------------------------------------------
                // manual_mode = false
                // --------------------------------------------------------
                RCLCPP_INFO(
                    node.get_logger(),
                    "HTTP manual_mode <- false");

                node.state().set_manual_mode(false);
            }


            // ==========================================================
            // 목적지 이동
            // ==========================================================
            if (!destination.empty())
            {
                GuideRobotServerNode::SendGoalError error;

                const bool ok =
                    node.send_navigation_goal(
                        destination,
                        error);

                if (!ok)
                {
                    switch (error)
                    {
                    case GuideRobotServerNode::SendGoalError::kNav2Unavailable:

                        // 목적지 ID는 유효하지만
                        // Nav2 action server가 아직 없는 경우
                        res.status = 503;

                        res.set_content(
                            R"({"error":"nav2_unavailable"})",
                            "application/json");

                        return;


                    case GuideRobotServerNode::SendGoalError::kInvalidDestination:

                    default:

                        res.status = 400;

                        res.set_content(
                            R"({"error":"invalid_destination"})",
                            "application/json");

                        return;
                    }
                }

                res.set_content(
                    R"({"success":true})",
                    "application/json");

                return;
            }


            // ==========================================================
            // cancel / stop
            // ==========================================================
            if (
                command == "cancel" ||
                command == "stop")
            {
                node.cancel_navigation();

                res.set_content(
                    R"({"success":true})",
                    "application/json");

                return;
            }


            // ==========================================================
            // manual_mode만 보낸 경우
            //
            // 예:
            // {"manual_mode":true}
            //
            // 위에서 이미 처리했으므로 성공 반환
            // ==========================================================
            if (manual_mode_value != 0)
            {
                res.set_content(
                    R"({"success":true})",
                    "application/json");

                return;
            }


            // ==========================================================
            // 알 수 없는 command
            // ==========================================================
            res.status = 400;

            res.set_content(
                R"({"error":"destination_or_command_required"})",
                "application/json");
        });
}

}  // namespace


// ================================================================
// main
// ================================================================
int main(
    int argc,
    char ** argv)
{

    rclcpp::init(
        argc,
        argv);


    auto node =
        std::make_shared<
            GuideRobotServerNode>();


    std::string static_root;
    int http_port = 8080;


    node->get_parameter(
        "static_web_root",
        static_root);


    node->get_parameter(
        "http_port",
        http_port);


    httplib::Server svr;


    // --------------------------------------------------------------
    // 정적 웹 파일
    // --------------------------------------------------------------
    svr.set_mount_point(
        "/",
        static_root);


    // --------------------------------------------------------------
    // REST API
    // --------------------------------------------------------------
    register_routes(
        svr,
        *node);


    // --------------------------------------------------------------
    // HTTP 서버 스레드
    // --------------------------------------------------------------
    std::thread http_thread(
        [&svr,
         http_port,
         &static_root,
         &node]()
        {
            RCLCPP_INFO(
                node->get_logger(),
                "HTTP 서버 시작: 0.0.0.0:%d (정적 파일 루트: %s)",
                http_port,
                static_root.c_str());


            if (!svr.listen(
                    "0.0.0.0",
                    http_port))
            {
                RCLCPP_ERROR(
                    node->get_logger(),
                    "HTTP 서버 시작 실패 "
                    "(포트 %d 사용 중일 수 있음)",
                    http_port);
            }
        });


    // --------------------------------------------------------------
    // ROS2 spin
    // --------------------------------------------------------------
    rclcpp::spin(node);


    // --------------------------------------------------------------
    // 종료
    // --------------------------------------------------------------
    svr.stop();


    if (http_thread.joinable())
    {
        http_thread.join();
    }


    rclcpp::shutdown();


    return 0;
}
