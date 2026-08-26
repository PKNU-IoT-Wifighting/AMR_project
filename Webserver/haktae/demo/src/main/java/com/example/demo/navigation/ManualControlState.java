package com.example.demo.navigation;

import java.util.concurrent.atomic.AtomicBoolean;

import org.springframework.stereotype.Component;

@Component
public class ManualControlState {
    private final AtomicBoolean active = new AtomicBoolean(false);

    public boolean isActive() {
        return active.get();
    }

    public boolean setActive(boolean enabled) {
        active.set(enabled);
        return enabled;
    }

    public boolean toggle() {
        while (true) {
            boolean current = active.get();
            if (active.compareAndSet(current, !current)) {
                return !current;
            }
        }
    }
}
