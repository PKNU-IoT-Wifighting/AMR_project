package com.example.demo.navigation;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

import jakarta.annotation.PreDestroy;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.core.io.ClassPathResource;
import org.springframework.stereotype.Component;

@Component
public class Ros2StatusBridgeLifecycle {
    private static final Logger logger = LoggerFactory.getLogger(Ros2StatusBridgeLifecycle.class);
    private static final String BRIDGE_RESOURCE = "ros/navigation_status_bridge.py";

    private final Ros2Properties properties;

    private volatile boolean shuttingDown;
    private Process bridgeProcess;
    private Path bridgeScript;

    public Ros2StatusBridgeLifecycle(Ros2Properties properties) {
        this.properties = properties;
    }

    @EventListener(ApplicationReadyEvent.class)
    public synchronized void start() {
        if (!properties.statusBridgeEnabled()) {
            logger.info("ROS 2 navigation status bridge is disabled");
            return;
        }

        if (!isLinux()) {
            logger.warn("ROS 2 navigation status bridge starts only on Linux; current OS: {}",
                System.getProperty("os.name"));
            return;
        }

        if (bridgeProcess != null && bridgeProcess.isAlive()) {
            return;
        }

        shuttingDown = false;
        try {
            bridgeScript = extractBridgeScript();
            ProcessBuilder processBuilder = new ProcessBuilder(buildCommand(bridgeScript));
            processBuilder.redirectErrorStream(true);
            processBuilder.redirectOutput(ProcessBuilder.Redirect.INHERIT);
            processBuilder.environment().put("WEB_SERVER_URL", properties.serverUrl());
            processBuilder.environment().put(
                "NAV_ACTION_STATUS_TOPIC", properties.actionStatusTopic());

            bridgeProcess = processBuilder.start();
            long processId = bridgeProcess.pid();

            if (bridgeProcess.waitFor(500, TimeUnit.MILLISECONDS)) {
                int exitCode = bridgeProcess.exitValue();
                bridgeProcess = null;
                throw new IOException(
                    "ROS 2 navigation status bridge exited during startup (code=" + exitCode + ")");
            }

            logger.info("ROS 2 navigation status bridge started (pid={}, topic={})",
                processId, properties.actionStatusTopic());

            bridgeProcess.onExit().thenAccept(exitedProcess -> {
                if (!shuttingDown) {
                    logger.error("ROS 2 navigation status bridge exited unexpectedly (pid={}, code={})",
                        processId, exitedProcess.exitValue());
                }
            });
        } catch (IOException error) {
            stop();
            throw new IllegalStateException("ROS 2 navigation status bridge could not start", error);
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            stop();
            throw new IllegalStateException("ROS 2 navigation status bridge startup was interrupted", error);
        }
    }

    List<String> buildCommand(Path scriptPath) {
        return List.of(
            "/bin/bash", "-c",
            "source \"$1\" && shift && exec \"$@\"",
            "ros2-status-bridge", properties.setupFile(),
            properties.pythonExecutable(), scriptPath.toString());
    }

    private Path extractBridgeScript() throws IOException {
        ClassPathResource resource = new ClassPathResource(BRIDGE_RESOURCE);
        Path temporaryScript = Files.createTempFile("navigation-status-bridge-", ".py");
        try (InputStream input = resource.getInputStream()) {
            Files.copy(input, temporaryScript, StandardCopyOption.REPLACE_EXISTING);
        }
        return temporaryScript;
    }

    private static boolean isLinux() {
        return System.getProperty("os.name", "")
            .toLowerCase(Locale.ROOT)
            .contains("linux");
    }

    @PreDestroy
    public synchronized void stop() {
        shuttingDown = true;

        if (bridgeProcess != null && bridgeProcess.isAlive()) {
            bridgeProcess.destroy();
            try {
                if (!bridgeProcess.waitFor(2, TimeUnit.SECONDS)) {
                    bridgeProcess.destroyForcibly();
                    bridgeProcess.waitFor(2, TimeUnit.SECONDS);
                }
            } catch (InterruptedException error) {
                Thread.currentThread().interrupt();
                bridgeProcess.destroyForcibly();
            }
        }
        bridgeProcess = null;

        if (bridgeScript != null) {
            try {
                Files.deleteIfExists(bridgeScript);
            } catch (IOException error) {
                logger.warn("Temporary ROS 2 bridge script could not be deleted: {}", bridgeScript);
            }
            bridgeScript = null;
        }
    }
}
