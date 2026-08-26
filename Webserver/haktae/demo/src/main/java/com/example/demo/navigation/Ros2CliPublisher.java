package com.example.demo.navigation;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

import org.springframework.stereotype.Service;

@Service
public class Ros2CliPublisher implements Ros2Publisher {
    private static final String POSE_STAMPED_TYPE = "geometry_msgs/msg/PoseStamped";

    private final Ros2Properties properties;

    public Ros2CliPublisher(Ros2Properties properties) {
        this.properties = properties;
    }

    @Override
    public void publishGoal(Destination destination) {
        Instant now = Instant.now();
        String message = String.format(
            java.util.Locale.ROOT,
            "{header: {stamp: {sec: %d, nanosec: %d}, frame_id: map}, "
                + "pose: {position: {x: %.15f, y: %.15f, z: %.15f}, "
                + "orientation: {x: 0.0, y: 0.0, z: 0.0, w: 1.0}}}",
            now.getEpochSecond(), now.getNano(),
            destination.x(), destination.y(), destination.z());

        publish(properties.goalTopic(), POSE_STAMPED_TYPE, message);
    }

    private void publish(String topic, String messageType, String message) {
        List<String> command = new ArrayList<>(List.of(
            "/bin/bash", "-c",
            "source \"$1\" && shift && exec \"$@\"",
            "ros2-jazzy", properties.setupFile(), properties.executable(),
            "topic", "pub", "--once", topic, messageType, message));
        Process process = null;
        try {
            process = new ProcessBuilder(command)
                .redirectErrorStream(true)
                .start();
            boolean completed = process.waitFor(
                properties.publishTimeout().toMillis(), TimeUnit.MILLISECONDS);
            if (!completed) {
                process.destroyForcibly();
                throw new Ros2PublishException(
                    "ROS 2 토픽 발행 시간이 초과되었습니다: " + topic);
            }

            String output = new String(
                process.getInputStream().readAllBytes(), StandardCharsets.UTF_8).trim();
            if (process.exitValue() != 0) {
                throw new Ros2PublishException(
                    "ROS 2 토픽 발행 실패(" + topic + "): " + output);
            }
        } catch (IOException e) {
            throw new Ros2PublishException(
                "ROS 2 실행 파일을 시작할 수 없습니다: " + properties.executable(), e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new Ros2PublishException("ROS 2 토픽 발행이 중단되었습니다", e);
        } finally {
            if (process != null && process.isAlive()) {
                process.destroyForcibly();
            }
        }
    }
}
