package com.example.demo.navigation;

import static org.assertj.core.api.Assertions.assertThat;

import java.time.Duration;
import java.time.Instant;
import java.util.List;

import org.junit.jupiter.api.Test;

class Ros2CliPublisherTest {
    @Test
    void buildsPoseStampedMessageWithConfiguredTopic() {
        Ros2CliPublisher publisher = new Ros2CliPublisher(new Ros2Properties(
            "/opt/ros/jazzy/bin/ros2", "/opt/ros/jazzy/setup.bash",
            "/goal_pose", "/cmd_vel", Duration.ofSeconds(2)));

        List<String> command = publisher.buildGoalCommand(
            Destination.ELEVATOR,
            Instant.ofEpochSecond(1_787_308_794L, 689_250_237L));

        String arguments = String.join("\n", command);
        assertThat(arguments)
            .contains("topic\npub\n--once\n/goal_pose\ngeometry_msgs/msg/PoseStamped")
            .contains("frame_id: map")
            .contains("x: -21.816679000854492")
            .contains("y: 2.625699043273926")
            .contains("w: 1.0");
    }

    @Test
    void buildsZeroTwistCommandForConfiguredCmdVelTopic() {
        Ros2CliPublisher publisher = new Ros2CliPublisher(new Ros2Properties(
            "/opt/ros/jazzy/bin/ros2", "/opt/ros/jazzy/setup.bash",
            "/goal_pose", "/cmd_vel", Duration.ofSeconds(2)));

        List<String> command = publisher.buildStopCommand();

        String arguments = String.join("\n", command);
        assertThat(arguments)
            .contains("topic\npub\n--once\n/cmd_vel\ngeometry_msgs/msg/Twist")
            .contains("linear: {x: 0.0, y: 0.0, z: 0.0}")
            .contains("angular: {x: 0.0, y: 0.0, z: 0.0}");
    }
}
