"use strict";

const destinations = new Map([
    ["toilet", { id: "toilet", name: "화장실 앞", icon: "🚻" }],
    ["301", { id: "301", name: "강의실 301호", icon: "🏫" }],
    ["302", { id: "302", name: "강의실 302호", icon: "🏫" }],
    ["elevator", { id: "elevator", name: "엘리베이터 앞", icon: "🛗" }]
]);

const state = {
    selectedDestination: null,
    isStatusReady: false,
    isGuiding: false,
    hasArrived: false,
    isManualMode: false,
    isSendingCommand: false,
    isConfirmationOpen: false,
    commandMessage: "",
    commandSucceeded: false
};

const elements = {
    heroTitle: document.querySelector("#hero-title"),
    heroDescription: document.querySelector("#hero-description"),
    destinationSection: document.querySelector("#destination-section"),
    destinationButtons: [...document.querySelectorAll(".destination-button")],
    selectedDestination: document.querySelector("#selected-destination"),
    startButton: document.querySelector("#start-button"),
    commandResult: document.querySelector("#command-result"),
    journeyCard: document.querySelector("#journey-card"),
    journeyLabel: document.querySelector("#journey-label"),
    journeyDestination: document.querySelector("#journey-destination"),
    guidanceState: document.querySelector("#guidance-state"),
    journeyMessage: document.querySelector("#journey-message"),
    journeyPulse: document.querySelector("#journey-pulse"),
    journeyMessageText: document.querySelector("#journey-message-text"),
    secondaryButton: document.querySelector("#secondary-button"),
    confirmationBackdrop: document.querySelector("#confirmation-backdrop"),
    confirmationDialog: document.querySelector(".confirmation-dialog"),
    confirmationIcon: document.querySelector("#confirmation-icon"),
    confirmationDestination: document.querySelector("#confirmation-destination"),
    confirmationCancel: document.querySelector("#confirmation-cancel"),
    confirmationStart: document.querySelector("#confirmation-start"),
    manualOverlay: document.querySelector("#manual-control-overlay")
};

function render() {
    const destination = state.selectedDestination;

    elements.heroTitle.textContent = state.hasArrived
        ? "목적지에 도착했습니다"
        : state.isGuiding
            ? "로봇이 안내 중입니다"
            : "어디로 안내할까요?";

    elements.heroDescription.textContent = state.hasArrived
        ? `${destination.name}에 안전하게 도착했어요.`
        : state.isGuiding
            ? `${destination.name}까지 안전하게 안내하고 있어요.`
            : "GuideRobot이 길을 안내해 드립니다.";

    elements.destinationSection.hidden = state.isGuiding;
    elements.startButton.hidden = state.isGuiding;
    elements.journeyCard.hidden = !state.isGuiding;

    elements.selectedDestination.hidden = destination === null || state.isGuiding;
    elements.selectedDestination.textContent = destination ? `선택: ${destination.name}` : "";

    for (const button of elements.destinationButtons) {
        const selected = destination?.id === button.dataset.destination;
        button.classList.toggle("selected", selected);
        button.setAttribute("aria-pressed", String(selected));
        button.disabled = !state.isStatusReady || state.isManualMode;
    }

    elements.startButton.disabled = !state.isStatusReady || state.isManualMode || destination === null || state.isSendingCommand;
    elements.commandResult.hidden = !state.commandMessage || state.isGuiding;
    elements.commandResult.textContent = state.commandMessage;
    elements.commandResult.className = `command-result ${state.commandSucceeded ? "success" : "failure"}`;

    if (state.isGuiding) {
        elements.journeyDestination.textContent = destination.name;
        elements.journeyLabel.textContent = state.hasArrived ? "안내 완료" : "안내 진행 상황";
        elements.guidanceState.textContent = state.hasArrived ? "도착 완료" : "안내 중";
        elements.guidanceState.classList.toggle("arrived", state.hasArrived);
        elements.journeyMessage.classList.toggle("arrived", state.hasArrived);
        elements.journeyPulse.hidden = state.hasArrived;
        elements.journeyMessageText.textContent = state.hasArrived
            ? "목적지에 도착했습니다."
            : "안내 중입니다. 로봇을 따라와 주세요.";
        elements.secondaryButton.hidden = !state.hasArrived;
        elements.secondaryButton.disabled = state.isManualMode;
    }

    elements.confirmationBackdrop.hidden = !state.isConfirmationOpen || destination === null;
    if (state.isConfirmationOpen && destination) {
        elements.confirmationIcon.textContent = destination.icon;
        elements.confirmationDestination.textContent = destination.name;
    }
    elements.confirmationCancel.disabled = state.isManualMode;
    elements.confirmationStart.disabled = state.isManualMode || state.isSendingCommand;
    elements.confirmationStart.textContent = state.isSendingCommand ? "서버에 전달 중..." : "안내 시작";

    elements.manualOverlay.hidden = !state.isManualMode;
}

function selectDestination(destinationId) {
    if (!state.isStatusReady || state.isManualMode) {
        return;
    }

    state.selectedDestination = destinations.get(destinationId);
    state.commandMessage = "";
    render();
}

function openConfirmation() {
    if (!state.isStatusReady || state.isManualMode || !state.selectedDestination || state.isSendingCommand) {
        return;
    }

    state.isConfirmationOpen = true;
    render();
    elements.confirmationStart.focus();
}

function closeConfirmation() {
    if (state.isManualMode) {
        return;
    }

    state.isConfirmationOpen = false;
    render();
    elements.startButton.focus();
}

async function postNavigation(destination) {
    const response = await fetch("/api/navigation", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ destination })
    });

    if (!response.ok) {
        throw new Error(response.status >= 500
            ? "서버와의 접속을 확인해 주세요."
            : "서버가 요청을 처리하지 못했습니다. 잠시 후 다시 시도해 주세요.");
    }
}

async function startGuidance() {
    if (state.isManualMode || !state.selectedDestination || state.isSendingCommand) {
        return;
    }

    state.isSendingCommand = true;
    render();

    try {
        await postNavigation(state.selectedDestination.id);
        state.isConfirmationOpen = false;
        state.isGuiding = true;
        state.hasArrived = false;
        state.commandSucceeded = true;
        state.commandMessage = "안내 명령을 서버에 전달했습니다.";
    } catch (error) {
        state.commandSucceeded = false;
        state.commandMessage = error instanceof Error ? error.message : "서버와의 접속을 확인해 주세요.";
    } finally {
        state.isSendingCommand = false;
        render();
    }
}

function confirmArrival() {
    if (!state.isManualMode && state.hasArrived) {
        resetGuidanceUi();
    }
}

function resetGuidanceUi() {
    state.selectedDestination = null;
    state.isGuiding = false;
    state.hasArrived = false;
    state.isSendingCommand = false;
    state.isConfirmationOpen = false;
    state.commandMessage = "";
    render();
}

async function refreshStatus() {
    try {
        const response = await fetch("/api/status", { cache: "no-store" });
        if (!response.ok) {
            return;
        }

        const status = await response.json();
        const manualMode = status.manualMode === true;
        let changed = !state.isStatusReady || manualMode !== state.isManualMode;

        state.isStatusReady = true;
        state.isManualMode = manualMode;
        if (manualMode && state.isConfirmationOpen) {
            state.isConfirmationOpen = false;
            changed = true;
        }

        if (state.isGuiding
            && !state.hasArrived
            && typeof status.navigationStatus === "string"
            && status.navigationStatus.toLowerCase() === "arrived") {
            state.hasArrived = true;
            changed = true;
        }

        if (changed) {
            render();
        }
    } catch {
        // 일시적인 통신 실패 시 마지막 정상 화면 상태를 유지합니다.
    }
}

for (const button of elements.destinationButtons) {
    button.addEventListener("click", () => selectDestination(button.dataset.destination));
}

elements.startButton.addEventListener("click", openConfirmation);
elements.confirmationCancel.addEventListener("click", closeConfirmation);
elements.confirmationStart.addEventListener("click", startGuidance);
elements.secondaryButton.addEventListener("click", confirmArrival);
elements.confirmationBackdrop.addEventListener("click", closeConfirmation);
elements.confirmationDialog.addEventListener("click", event => event.stopPropagation());
document.addEventListener("keydown", event => {
    if (event.key === "Escape" && state.isConfirmationOpen && !state.isManualMode) {
        closeConfirmation();
    }
});

render();
refreshStatus();
window.setInterval(refreshStatus, 1000);
