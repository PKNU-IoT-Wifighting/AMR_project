package com.example.demo.navigation;

import static org.assertj.core.api.Assertions.assertThat;

import org.junit.jupiter.api.Test;

class DestinationTest {
    @Test
    void mapsEveryUserChoiceToProvidedCoordinates() {
        assertCoordinates("화장실", -6.62200403213501, -0.46175462007522583, 0.002532958984375);
        assertCoordinates("301", -9.861356735229492, 2.575411319732666, 0.002471923828125);
        assertCoordinates("302", -15.290921211242676, 3.2860002517700195, 0.002471923828125);
        assertCoordinates("엘리베이터", -21.816679000854492, 2.625699043273926, 0.195281982421875);
    }

    private void assertCoordinates(String id, double x, double y, double z) {
        Destination destination = Destination.fromId(id);
        assertThat(destination.x()).isEqualTo(x);
        assertThat(destination.y()).isEqualTo(y);
        assertThat(destination.z()).isEqualTo(z);
    }
}
