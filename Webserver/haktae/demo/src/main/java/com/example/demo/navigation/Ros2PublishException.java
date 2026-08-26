package com.example.demo.navigation;

public class Ros2PublishException extends RuntimeException {
    public Ros2PublishException(String message) {
        super(message);
    }

    public Ros2PublishException(String message, Throwable cause) {
        super(message, cause);
    }
}
