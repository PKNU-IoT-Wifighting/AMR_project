package com.example.demo.navigation;

import java.util.Locale;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

import org.springframework.stereotype.Component;

@Component
public class NavigationState {
    private static final Set<String> ALLOWED_STATUSES = Set.of("idle", "moving", "arrived");

    private final AtomicReference<String> status = new AtomicReference<>("idle");

    public String getStatus() {
        return status.get();
    }

    public void markIdle() {
        status.set("idle");
    }

    public void markMoving() {
        status.set("moving");
    }

    public void setStatus(String newStatus) {
        String normalized = newStatus == null ? "" : newStatus.trim().toLowerCase(Locale.ROOT);
        if (!ALLOWED_STATUSES.contains(normalized)) {
            throw new IllegalArgumentException("지원하지 않는 주행 상태입니다: " + newStatus);
        }
        status.set(normalized);
    }
}
