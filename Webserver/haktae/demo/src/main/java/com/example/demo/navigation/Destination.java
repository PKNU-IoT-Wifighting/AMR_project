package com.example.demo.navigation;

import java.util.Arrays;

public enum Destination {
    TOILET("toilet", "화장실", -6.62200403213501, -0.46175462007522583, 0.002532958984375),
    ROOM_301("301", "301", -9.861356735229492, 2.575411319732666, 0.002471923828125),
    ROOM_302("302", "302", -15.290921211242676, 3.2860002517700195, 0.002471923828125),
    ELEVATOR("elevator", "엘리베이터", -21.816679000854492, 2.625699043273926, 0.195281982421875);

    private final String id;
    private final String name;
    private final double x;
    private final double y;
    private final double z;

    Destination(String id, String name, double x, double y, double z) {
        this.id = id;
        this.name = name;
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public String id() {
        return id;
    }

    public String displayName() {
        return name;
    }

    public double x() {
        return x;
    }

    public double y() {
        return y;
    }

    public double z() {
        return z;
    }

    public static Destination fromId(String id) {
        return Arrays.stream(values())
            .filter(destination -> destination.id.equalsIgnoreCase(id)
                || destination.name.equals(id))
            .findFirst()
            .orElseThrow(() -> new IllegalArgumentException("알 수 없는 목적지입니다: " + id));
    }
}
