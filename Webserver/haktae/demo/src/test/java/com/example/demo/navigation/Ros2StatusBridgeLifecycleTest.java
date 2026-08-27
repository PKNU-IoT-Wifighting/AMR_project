package com.example.demo.navigation;

import static org.assertj.core.api.Assertions.assertThat;

import java.nio.file.Path;
import java.time.Duration;

import org.junit.jupiter.api.Test;

class Ros2StatusBridgeLifecycleTest {

    @Test
    void buildsCommandThatLoadsRosEnvironmentBeforeStartingBridge() {
        Ros2Properties properties = new Ros2Properties(
            "ros2", "/opt/ros/jazzy/setup.bash",
            "/goal_pose", "/cmd_vel",
            true, "python3",
            "/navigate_to_pose/_action/status", "http://127.0.0.1:8080",
            Duration.ofSeconds(5));
        Ros2StatusBridgeLifecycle lifecycle = new Ros2StatusBridgeLifecycle(properties);
        Path scriptPath = Path.of("navigation_status_bridge.py");

        assertThat(lifecycle.buildCommand(scriptPath))
            .containsExactly(
                "/bin/bash", "-c",
                "source \"$1\" && shift && exec \"$@\"",
                "ros2-status-bridge", "/opt/ros/jazzy/setup.bash",
                "python3", scriptPath.toString());
    }
}
