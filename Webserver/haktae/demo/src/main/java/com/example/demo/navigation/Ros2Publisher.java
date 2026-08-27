package com.example.demo.navigation;

public interface Ros2Publisher {
    void publishGoal(Destination destination);

    void publishStop();
}
