#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include <sqlite3.h>

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

using NavigateToPose =
    nav2_msgs::action::NavigateToPose;

using GoalHandleNav =
    rclcpp_action::ClientGoalHandle<NavigateToPose>;

namespace
{

constexpr const char * kNavigateActionName =
    "navigate_to_pose";

constexpr const char * kManualModeTopic =
    "/manual_mode";

constexpr const char * kDefaultDatabasePath =
    "/home/ubuntu/webfolder/AMR_project/Webserver/hyunbeen/data/server.db";

}  // namespace


class GuideRobotServerNode : public rclcpp::Node
{
public:

    GuideRobotServerNode()
        : Node("guiderobot_server_node"),
        destinations_(this)
    {
        // ============================================================
        // 기본 파라미터
        // ============================================================

        declare_parameter<std::string>(
            "static_web_root",
            "./web");

        declare_parameter<int>(
            "http_port",
            8080);

        declare_parameter<std::string>(
            "database_path",
            kDefaultDatabasePath);


        // ============================================================
        // SQLite DB 초기화
        // ============================================================

        std::string database_path;

        get_parameter(
            "database_path",
            database_path);

        if (!open_database(database_path))
        {
            throw std::runtime_error(
                "SQLite database open failed");
        }

        if (!initialize_database())
        {
            throw std::runtime_error(
                "SQLite database initialization failed");
        }


        // ============================================================
        // ROS2 /manual_mode 토픽 구독
        // ============================================================

        manual_mode_sub_ =
            create_subscription<std_msgs::msg::Bool>(
                kManualModeTopic,
                rclcpp::QoS(10),
                std::bind(
                    &GuideRobotServerNode::on_manual_mode,
                    this,
                    std::placeholders::_1));


        // ============================================================
        // Nav2 NavigateToPose 액션 클라이언트
        // ============================================================

        nav_client_ =
            rclcpp_action::create_client<NavigateToPose>(
                this,
                kNavigateActionName);


        RCLCPP_INFO(
            get_logger(),
            "GuideRobot server node started.");

        RCLCPP_INFO(
            get_logger(),
            "SQLite database: %s",
            database_path.c_str());
    }


    ~GuideRobotServerNode()
    {
        if (db_)
        {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }


    // ================================================================
    // RobotState 접근
    // ================================================================

    RobotState & state()
    {
        return state_;
    }


    // ================================================================
    // DestinationMap 접근
    // ================================================================

    DestinationMap & destinations()
    {
        return destinations_;
    }


    // ================================================================
    // Nav2 목표 전송 에러
    // ================================================================

    enum class SendGoalError
    {
        kNone,

        // 목적지 ID 자체가 없음
        kInvalidDestination,

        // Nav2 action server가 없음
        kNav2Unavailable,
    };


    // ================================================================
    // SQLite DB 열기
    // ================================================================

    bool open_database(
        const std::string & database_path)
    {
        const int rc =
            sqlite3_open(
                database_path.c_str(),
                &db_);

        if (rc != SQLITE_OK)
        {
            RCLCPP_ERROR(
                get_logger(),
                "SQLite DB 열기 실패: %s",
                db_
                    ? sqlite3_errmsg(db_)
                    : "unknown error");

            return false;
        }


        RCLCPP_INFO(
            get_logger(),
            "SQLite DB 연결 성공: %s",
            database_path.c_str());

        return true;
    }


    // ================================================================
    // SQLite 테이블 생성
    // ================================================================

    bool initialize_database()
    {
        const char * sql = R"SQL(

            CREATE TABLE IF NOT EXISTS navigation_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,

                destination TEXT NOT NULL,

                status TEXT NOT NULL
                    DEFAULT 'STARTED',

                started_at DATETIME NOT NULL
                    DEFAULT (
                        datetime(
                            'now',
                            'localtime'
                        )
                    ),

                finished_at DATETIME
            );

        )SQL";


        char * error_message = nullptr;


        const int rc =
            sqlite3_exec(
                db_,
                sql,
                nullptr,
                nullptr,
                &error_message);


        if (rc != SQLITE_OK)
        {
            RCLCPP_ERROR(
                get_logger(),
                "navigation_history 테이블 생성 실패: %s",
                error_message
                    ? error_message
                    : "unknown error");

            if (error_message)
            {
                sqlite3_free(error_message);
            }

            return false;
        }


        RCLCPP_INFO(
            get_logger(),
            "SQLite navigation_history 테이블 준비 완료.");

        return true;
    }


    // ================================================================
    // 안내 시작 기록 저장
    //
    // destination:
    //   restroom
    //   room_301
    //   room_302
    //   elevator
    //
    // status:
    //   STARTED
    //
    // started_at:
    //   SQLite가 현재 시간 자동 저장
    // ================================================================

    bool save_navigation_start(
        const std::string & destination)
    {
        if (!db_)
        {
            RCLCPP_ERROR(
                get_logger(),
                "SQLite DB가 연결되어 있지 않습니다.");

            return false;
        }


        const char * sql = R"SQL(

            INSERT INTO navigation_history
            (
                destination,
                status,
                started_at
            )
            VALUES
            (
                ?,
                'STARTED',
                datetime('now', 'localtime')
            );

        )SQL";


        sqlite3_stmt * stmt = nullptr;


        const int prepare_rc =
            sqlite3_prepare_v2(
                db_,
                sql,
                -1,
                &stmt,
                nullptr);


        if (prepare_rc != SQLITE_OK)
        {
            RCLCPP_ERROR(
                get_logger(),
                "SQLite INSERT 준비 실패: %s",
                sqlite3_errmsg(db_));

            return false;
        }


        // destination 바인딩
        sqlite3_bind_text(
            stmt,
            1,
            destination.c_str(),
            -1,
            SQLITE_TRANSIENT);


        const int step_rc =
            sqlite3_step(stmt);


        sqlite3_finalize(stmt);


        if (step_rc != SQLITE_DONE)
        {
            RCLCPP_ERROR(
                get_logger(),
                "안내 시작 기록 저장 실패: %s",
                sqlite3_errmsg(db_));

            return false;
        }


        RCLCPP_INFO(
            get_logger(),
            "DB 저장 완료: destination=%s",
            destination.c_str());

        return true;
    }


    // ================================================================
    // Nav2 목표 전송
    // ================================================================

    bool send_navigation_goal(
        const std::string & destination_id,
        SendGoalError & out_error)
    {
        out_error =
            SendGoalError::kNone;


        // ------------------------------------------------------------
        // 목적지 존재 여부 확인
        // ------------------------------------------------------------

        if (!destinations_.contains(destination_id))
        {
            out_error =
                SendGoalError::kInvalidDestination;

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

            out_error =
                SendGoalError::kNav2Unavailable;

            return false;
        }


        // ------------------------------------------------------------
        // 목적지 좌표 가져오기
        // ------------------------------------------------------------

        const auto goal_pose =
            destinations_.lookup(
                destination_id,
                now());


        if (!goal_pose)
        {
            out_error =
                SendGoalError::kInvalidDestination;

            return false;
        }


        // ------------------------------------------------------------
        // NavigateToPose Goal 생성
        // ------------------------------------------------------------

        NavigateToPose::Goal goal_msg;

        goal_msg.pose =
            *goal_pose;


        rclcpp_action::Client<
            NavigateToPose
            >::SendGoalOptions options;


        // ------------------------------------------------------------
        // Nav2 결과 콜백
        // ------------------------------------------------------------

        options.result_callback =
            [this](
                const GoalHandleNav::WrappedResult & result)
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


            // 현재 goal handle 제거
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
             std::move(goal_handle_future)]()
            mutable
            {
                auto handle =
                    goal_handle_future.get();


                std::lock_guard<std::mutex> lock(
                    goal_handle_mutex_);


                current_goal_handle_ =
                    handle;

            }).detach();


        // ------------------------------------------------------------
        // 현재 목적지 상태 저장
        // ------------------------------------------------------------

        state_.set_destination(
            destination_id);


        return true;
    }


    // ================================================================
    // 진행 중인 Nav2 안내 취소
    // ================================================================

    void cancel_navigation()
    {
        std::shared_ptr<GoalHandleNav> handle;


        {
            std::lock_guard<std::mutex> lock(
                goal_handle_mutex_);

            handle =
                current_goal_handle_;
        }


        if (handle)
        {
            nav_client_->async_cancel_goal(
                handle);

            // 실제 상태 변경은
            // result_callback(CANCELED)에서 처리
        }
        else
        {
            state_.clear_destination();
        }
    }


private:


    // ================================================================
    // ROS2 /manual_mode 토픽 콜백
    // ================================================================

    void on_manual_mode(
        const std_msgs::msg::Bool::SharedPtr msg)
    {
        RCLCPP_INFO(
            get_logger(),
            "ROS /manual_mode <- %s",
            msg->data
                ? "true"
                : "false");


        state_.set_manual_mode(
            msg->data);


        if (msg->data)
        {
            // 수동 모드 진입 시
            // 자율 안내 즉시 취소
            cancel_navigation();
        }
    }


    // ================================================================
    // 멤버 변수
    // ================================================================

    RobotState state_;


    DestinationMap destinations_;


    // ------------------------------------------------
    // SQLite
    // ------------------------------------------------

    sqlite3 * db_{nullptr};


    // ------------------------------------------------
    // ROS2 /manual_mode subscriber
    // ------------------------------------------------

    rclcpp::Subscription<
        std_msgs::msg::Bool
        >::SharedPtr manual_mode_sub_;


    // ------------------------------------------------
    // Nav2 NavigateToPose action client
    // ------------------------------------------------

    rclcpp_action::Client<
        NavigateToPose
        >::SharedPtr nav_client_;


    // ------------------------------------------------
    // 현재 Nav2 goal handle
    // ------------------------------------------------

    std::mutex goal_handle_mutex_;


    std::shared_ptr<
        GoalHandleNav
        > current_goal_handle_;
};


namespace
{

// ====================================================================
// HTTP REST API 등록
// ====================================================================

void register_routes(
    httplib::Server & svr,
    GuideRobotServerNode & node)
{

    // ================================================================
    // CORS
    // ================================================================

    svr.set_default_headers({
        {
            "Access-Control-Allow-Origin",
            "*"
        }
    });


    // ================================================================
    // GET /api/status
    //
    // 현재 서버 상태 반환
    // ================================================================

    svr.Get(
        "/api/status",

        [&node](
            const httplib::Request &,
            httplib::Response & res)
        {
            const auto snap =
                node.state().snapshot();


            std::string body =
                "{";


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


            body +=
                "}";


            res.set_content(
                body,
                "application/json; charset=utf-8");
        });


    // ================================================================
    // POST /api/command
    //
    // 목적지 이동:
    //
    // {"destination":"restroom"}
    //
    //
    // 이동 취소:
    //
    // {"command":"cancel"}
    //
    //
    // 정지:
    //
    // {"command":"stop"}
    //
    //
    // 수동모드:
    //
    // {"manual_mode":true}
    // ================================================================

    svr.Post(
        "/api/command",

        [&node](
            const httplib::Request & req,
            httplib::Response & res)
        {

            std::string destination;

            std::string command;


            // ========================================================
            // 문자열 JSON field 추출
            // ========================================================

            auto extract_field =
                [](
                    const std::string & json,
                    const std::string & key)
                -> std::string
            {
                const std::string needle =
                    "\"" +
                    key +
                    "\"";


                auto pos =
                    json.find(needle);


                if (pos == std::string::npos)
                {
                    return "";
                }


                pos =
                    json.find(
                        ':',
                        pos);


                if (pos == std::string::npos)
                {
                    return "";
                }


                pos =
                    json.find(
                        '"',
                        pos);


                if (pos == std::string::npos)
                {
                    return "";
                }


                const auto end =
                    json.find(
                        '"',
                        pos + 1);


                if (end == std::string::npos)
                {
                    return "";
                }


                return json.substr(
                    pos + 1,
                    end - pos - 1);
            };


            // ========================================================
            // boolean JSON field 추출
            //
            // 반환:
            //
            // 0 = 없음
            // 1 = true
            // 2 = false
            // ========================================================

            auto extract_bool =
                [](
                    const std::string & json,
                    const std::string & key)
                -> int
            {
                const std::string needle =
                    "\"" +
                    key +
                    "\"";


                auto pos =
                    json.find(needle);


                if (pos == std::string::npos)
                {
                    return 0;
                }


                pos =
                    json.find(
                        ':',
                        pos);


                if (pos == std::string::npos)
                {
                    return 0;
                }


                // ':' 다음 공백 건너뛰기
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


            // ========================================================
            // JSON parsing
            // ========================================================

            destination =
                extract_field(
                    req.body,
                    "destination");


            command =
                extract_field(
                    req.body,
                    "command");


            // ========================================================
            // manual_mode
            // ========================================================

            const int manual_mode_value =
                extract_bool(
                    req.body,
                    "manual_mode");


            if (manual_mode_value == 1)
            {
                RCLCPP_INFO(
                    node.get_logger(),
                    "HTTP manual_mode <- true");


                node.state().set_manual_mode(
                    true);


                // 수동 모드 전환 시
                // 현재 Nav2 자율주행 취소
                node.cancel_navigation();
            }


            else if (manual_mode_value == 2)
            {
                RCLCPP_INFO(
                    node.get_logger(),
                    "HTTP manual_mode <- false");


                node.state().set_manual_mode(
                    false);
            }


            // ========================================================
            // 목적지 이동
            // ========================================================

            if (!destination.empty())
            {
                GuideRobotServerNode::SendGoalError error;


                // ----------------------------------------------------
                // 먼저 Nav2에 목표 전송
                // ----------------------------------------------------

                const bool ok =
                    node.send_navigation_goal(
                        destination,
                        error);


                // ----------------------------------------------------
                // Nav2 목표 전송 실패
                // ----------------------------------------------------

                if (!ok)
                {
                    switch (error)
                    {

                    case GuideRobotServerNode::
                        SendGoalError::
                        kNav2Unavailable:

                        res.status =
                            503;


                        res.set_content(
                            R"({"error":"nav2_unavailable"})",
                            "application/json");


                        return;


                    case GuideRobotServerNode::
                        SendGoalError::
                        kInvalidDestination:

                    default:

                        res.status =
                            400;


                        res.set_content(
                            R"({"error":"invalid_destination"})",
                            "application/json");


                        return;
                    }
                }


                // ----------------------------------------------------
                // ★ 여기서 SQLite에 안내 시작 기록
                //
                // Nav2 목표 전송이 성공한 경우에만 저장
                // ----------------------------------------------------

                if (
                    !node.save_navigation_start(
                        destination))
                {
                    RCLCPP_ERROR(
                        node.get_logger(),
                        "Nav2는 시작되었지만 "
                        "SQLite DB 기록에 실패했습니다.");
                }


                // ----------------------------------------------------
                // 웹에 성공 응답
                // ----------------------------------------------------

                res.set_content(
                    R"({"success":true})",
                    "application/json");


                return;
            }


            // ========================================================
            // cancel / stop
            // ========================================================

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


            // ========================================================
            // manual_mode만 보낸 경우
            // ========================================================

            if (manual_mode_value != 0)
            {
                res.set_content(
                    R"({"success":true})",
                    "application/json");


                return;
            }


            // ========================================================
            // 알 수 없는 command
            // ========================================================

            res.status =
                400;


            res.set_content(
                R"({"error":"destination_or_command_required"})",
                "application/json");
        });
}

}  // namespace


// ====================================================================
// main
// ====================================================================

int main(
    int argc,
    char ** argv)
{

    // ================================================================
    // ROS2 초기화
    // ================================================================

    rclcpp::init(
        argc,
        argv);


    // ================================================================
    // 서버 Node 생성
    // ================================================================

    auto node =
        std::make_shared<
            GuideRobotServerNode>();


    // ================================================================
    // Parameter
    // ================================================================

    std::string static_root;

    int http_port =
        8080;


    node->get_parameter(
        "static_web_root",
        static_root);


    node->get_parameter(
        "http_port",
        http_port);


    // ================================================================
    // HTTP 서버
    // ================================================================

    httplib::Server svr;


    // ================================================================
    // 정적 웹 파일
    // ================================================================

    svr.set_mount_point(
        "/",
        static_root);


    // ================================================================
    // REST API 등록
    // ================================================================

    register_routes(
        svr,
        *node);


    // ================================================================
    // HTTP 서버 스레드
    // ================================================================

    std::thread http_thread(
        [&svr,
         http_port,
         &static_root,
         &node]()
        {

            RCLCPP_INFO(
                node->get_logger(),
                "HTTP 서버 시작: "
                "0.0.0.0:%d "
                "(정적 파일 루트: %s)",
                http_port,
                static_root.c_str());


            if (
                !svr.listen(
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


    // ================================================================
    // ROS2 spin
    // ================================================================

    rclcpp::spin(
        node);


    // ================================================================
    // 종료
    // ================================================================

    svr.stop();


    if (
        http_thread.joinable())
    {
        http_thread.join();
    }


    rclcpp::shutdown();


    return 0;
}
