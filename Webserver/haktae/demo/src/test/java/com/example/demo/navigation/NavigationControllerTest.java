package com.example.demo.navigation;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.BeforeEach;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.webmvc.test.autoconfigure.WebMvcTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.context.annotation.Import;
import org.springframework.test.web.servlet.MockMvc;

@WebMvcTest(NavigationController.class)
@Import({ManualControlState.class, NavigationState.class})
class NavigationControllerTest {
    @Autowired
    private MockMvc mockMvc;

    @MockitoBean
    private Ros2Publisher publisher;

    @MockitoBean
    private Ros2Properties properties;

    @Autowired
    private ManualControlState manualControlState;

    @Autowired
    private NavigationState navigationState;

    @BeforeEach
    void configureProperties() {
        when(properties.goalTopic()).thenReturn("/goal_pose");
        manualControlState.setActive(false);
        navigationState.markIdle();
    }

    @Test
    void exposesStatusForQtConnectionCheck() throws Exception {
        mockMvc.perform(get("/api/status"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.status").value("ok"))
            .andExpect(jsonPath("$.navigationStatus").value("idle"));
    }

    @Test
    void publishesSelectedDestination() throws Exception {
        mockMvc.perform(post("/api/navigation")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"destination\":\"301\"}"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.destination").value("301"))
            .andExpect(jsonPath("$.x").value(-9.861356735229492));

        verify(publisher).publishGoal(Destination.ROOM_301);

        mockMvc.perform(get("/api/status"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.navigationStatus").value("moving"));
    }

    @Test
    void rejectsUnknownDestination() throws Exception {
        mockMvc.perform(post("/api/navigation")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"destination\":\"unknown\"}"))
            .andExpect(status().isBadRequest());
    }

    @Test
    void acceptsQtStopCommand() throws Exception {
        mockMvc.perform(post("/api/command")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"command\":\"stop\"}"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.command").value("stop"))
            .andExpect(jsonPath("$.manualMode").value(true));

        // The attached Qt client sends the same payload when toggled off.
        mockMvc.perform(post("/api/command")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"command\":\"stop\"}"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.manualMode").value(false));

        verifyNoInteractions(publisher);
    }

    @Test
    void blocksDestinationWhileManualControlIsActive() throws Exception {
        manualControlState.setActive(true);

        mockMvc.perform(post("/api/navigation")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"destination\":\"301\"}"))
            .andExpect(status().isConflict())
            .andExpect(jsonPath("$.manualMode").value(true));

        verifyNoInteractions(publisher);
    }

    @Test
    void acceptsExplicitManualModeStateFromNewQtClient() throws Exception {
        mockMvc.perform(post("/api/command")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"command\":\"stop\",\"manualMode\":true}"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.manualMode").value(true));

        mockMvc.perform(post("/api/command")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"command\":\"stop\",\"manualMode\":false}"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.manualMode").value(false));
    }

    @Test
    void publishesZeroVelocityWhenGuidanceIsCancelled() throws Exception {
        navigationState.markMoving();

        mockMvc.perform(post("/api/command")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"command\":\"cancel\"}"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.command").value("cancel"))
            .andExpect(jsonPath("$.navigationStatus").value("idle"));

        verify(publisher).publishStop();
    }

    @Test
    void acceptsArrivalStatusReportedByRosBridge() throws Exception {
        navigationState.markMoving();

        mockMvc.perform(post("/api/navigation/status")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"status\":\"arrived\"}"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.navigationStatus").value("arrived"));

        mockMvc.perform(get("/api/status"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.navigationStatus").value("arrived"));
    }
}
