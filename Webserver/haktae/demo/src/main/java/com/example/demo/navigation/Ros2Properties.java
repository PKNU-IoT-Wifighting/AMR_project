package com.example.demo.navigation;

import java.time.Duration;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "manager.ros2")
public record Ros2Properties(
    String executable,
    String setupFile,
    String goalTopic,
    Duration publishTimeout
) {
}
