package com.example.demo.navigation;

import java.time.Instant;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.ResponseStatus;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api")
public class NavigationController {
    private final Ros2Publisher publisher;
    private final Ros2Properties properties;
    private final ManualControlState manualControlState;
    private final NavigationState navigationState;

    public NavigationController(
        Ros2Publisher publisher,
        Ros2Properties properties,
        ManualControlState manualControlState,
        NavigationState navigationState
    ) {
        this.publisher = publisher;
        this.properties = properties;
        this.manualControlState = manualControlState;
        this.navigationState = navigationState;
    }

    @GetMapping("/status")
    public Map<String, Object> status() {
        return Map.of(
            "status", "ok",
            "goalTopic", properties.goalTopic(),
            "manualMode", manualControlState.isActive(),
            "navigationStatus", navigationState.getStatus(),
            "timestamp", Instant.now().toString());
    }

    @GetMapping("/destinations")
    public List<DestinationView> destinations() {
        return Arrays.stream(Destination.values())
            .map(DestinationView::from)
            .toList();
    }

    @PostMapping("/navigation")
    public DispatchResponse navigate(@Valid @RequestBody NavigationRequest request) {
        if (manualControlState.isActive()) {
            throw new ManualControlActiveException();
        }
        Destination destination = Destination.fromId(request.destination());
        publisher.publishGoal(destination);
        navigationState.markMoving();
        return DispatchResponse.from(destination, properties.goalTopic());
    }

    @PostMapping("/navigation/status")
    public Map<String, Object> updateNavigationStatus(
        @Valid @RequestBody NavigationStatusRequest request
    ) {
        navigationState.setStatus(request.status());
        return Map.of(
            "status", "ok",
            "navigationStatus", navigationState.getStatus());
    }

    // Qt mainwindow.cpp compatibility endpoint.
    @PostMapping("/command")
    public Map<String, Object> command(@Valid @RequestBody CommandRequest request) {
        if ("cancel".equalsIgnoreCase(request.command())) {
            publisher.publishStop();
            navigationState.markIdle();
            return Map.of(
                "status", "ok",
                "command", "cancel",
                "navigationStatus", navigationState.getStatus(),
                "message", "navigation cancelled and stop velocity published");
        }

        if ("stop".equalsIgnoreCase(request.command())) {
            // In the Qt protocol, "stop" means that manual control is in
            // progress. It is an acknowledgement, not a /cmd_vel command.
            // Old Qt clients omit manualMode, so toggle for compatibility.
            boolean manualMode = request.manualMode() == null
                ? manualControlState.toggle()
                : manualControlState.setActive(request.manualMode());
            return Map.of(
                "status", "ok",
                "command", "stop",
                "manualMode", manualMode,
                "message", "manual control acknowledged");
        }
        throw new IllegalArgumentException("지원하지 않는 명령입니다: " + request.command());
    }

    @ExceptionHandler(IllegalArgumentException.class)
    @ResponseStatus(HttpStatus.BAD_REQUEST)
    public Map<String, String> badRequest(IllegalArgumentException exception) {
        return Map.of("error", exception.getMessage());
    }

    @ExceptionHandler(Ros2PublishException.class)
    @ResponseStatus(HttpStatus.SERVICE_UNAVAILABLE)
    public Map<String, String> rosUnavailable(Ros2PublishException exception) {
        return Map.of("error", exception.getMessage());
    }

    @ExceptionHandler(ManualControlActiveException.class)
    @ResponseStatus(HttpStatus.CONFLICT)
    public Map<String, Object> manualControlActive(ManualControlActiveException exception) {
        return Map.of(
            "error", exception.getMessage(),
            "manualMode", true);
    }

    public record NavigationRequest(@NotBlank String destination) {
    }

    public record CommandRequest(@NotBlank String command, Boolean manualMode) {
    }

    public record NavigationStatusRequest(@NotBlank String status) {
    }

    public record DestinationView(String id, String name, double x, double y, double z) {
        static DestinationView from(Destination destination) {
            return new DestinationView(
                destination.id(), destination.displayName(),
                destination.x(), destination.y(), destination.z());
        }
    }

    public record DispatchResponse(
        String status, String destination, String name,
        double x, double y, double z, String topic
    ) {
        static DispatchResponse from(Destination destination, String topic) {
            return new DispatchResponse(
                "published", destination.id(), destination.displayName(),
                destination.x(), destination.y(), destination.z(), topic);
        }
    }
}
