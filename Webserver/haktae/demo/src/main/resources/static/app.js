const buttons = [...document.querySelectorAll(".destination")];
const message = document.querySelector("#message");
const serverStatus = document.querySelector("#serverStatus");
const manualOverlay = document.querySelector("#manualOverlay");
let requestBusy = false;
let manualMode = false;

function updateButtons() {
    buttons.forEach(button => { button.disabled = requestBusy || manualMode; });
    manualOverlay.hidden = !manualMode;
}

function setBusy(busy) {
    requestBusy = busy;
    updateButtons();
}

async function checkServer() {
    try {
        const response = await fetch("/api/status");
        if (!response.ok) throw new Error();
        const status = await response.json();
        const wasManual = manualMode;
        manualMode = status.manualMode === true;
        updateButtons();
        if (manualMode) {
            serverStatus.className = "badge pending";
            serverStatus.innerHTML = "<span></span>관리자 수동 운행 중";
        } else {
            serverStatus.className = "badge";
            serverStatus.innerHTML = "<span></span>서버 연결됨";
            if (wasManual) {
                message.className = "message";
                message.textContent = "수동 운행이 종료되었습니다. 목적지를 선택해 주세요.";
            }
        }
    } catch {
        serverStatus.className = "badge error";
        serverStatus.innerHTML = "<span></span>서버 연결 실패";
    }
}

async function navigate(destination, label) {
    if (manualMode) return;
    setBusy(true);
    message.className = "message";
    message.textContent = `${label}(으)로 목적지를 전송하는 중입니다...`;
    try {
        const response = await fetch("/api/navigation", {
            method: "POST",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify({destination})
        });
        const result = await response.json();
        if (!response.ok) {
            if (result.manualMode === true) {
                manualMode = true;
                updateButtons();
            }
            throw new Error(result.error || "전송에 실패했습니다.");
        }
        message.className = "message success";
        message.textContent = `${label} 좌표를 ${result.topic} 토픽으로 전송했습니다.`;
    } catch (error) {
        message.className = "message error";
        message.textContent = error.message;
    } finally {
        setBusy(false);
    }
}

buttons.forEach(button => {
    button.addEventListener("click", () => {
        navigate(button.dataset.destination, button.querySelector("strong").textContent);
    });
});

checkServer();
setInterval(checkServer, 2000);
