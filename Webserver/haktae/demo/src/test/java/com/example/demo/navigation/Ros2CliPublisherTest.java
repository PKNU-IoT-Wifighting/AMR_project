package com.example.demo.navigation;

import static org.assertj.core.api.Assertions.assertThat;

import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class Ros2CliPublisherTest {
    @TempDir
    Path tempDirectory;

    @Test
    void buildsPoseStampedMessageWithConfiguredTopic() throws Exception {
        Path output = tempDirectory.resolve("arguments.txt");
        Path fakeRos2 = tempDirectory.resolve("ros2");
        Files.writeString(fakeRos2, "#!/bin/sh\nprintf '%s\\n' \"$@\" > '"
            + output + "'\n");
        fakeRos2.toFile().setExecutable(true);

        Ros2CliPublisher publisher = new Ros2CliPublisher(new Ros2Properties(
            fakeRos2.toString(), "/opt/ros/jazzy/setup.bash",
            "/goal_pose", Duration.ofSeconds(2)));

        publisher.publishGoal(Destination.ELEVATOR);

        String arguments = Files.readString(output);
        assertThat(arguments)
            .contains("topic\npub\n--once\n/goal_pose\ngeometry_msgs/msg/PoseStamped")
            .contains("frame_id: map")
            .contains("x: -21.816679000854492")
            .contains("y: 2.625699043273926")
            .contains("w: 1.0");
    }
}
