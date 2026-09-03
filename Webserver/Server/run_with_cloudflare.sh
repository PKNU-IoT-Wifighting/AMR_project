#!/usr/bin/env bash
set -eo pipefail

SERVER_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SERVER_PID=""
TUNNEL_PID=""
TUNNEL_LOG="$(mktemp /tmp/guiderobot-cloudflared.XXXXXX.log)"

cleanup() {
    if [[ -n "$TUNNEL_PID" ]] && kill -0 "$TUNNEL_PID" 2>/dev/null; then
        kill -INT "$TUNNEL_PID" 2>/dev/null || true
        wait "$TUNNEL_PID" 2>/dev/null || true
    fi
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -INT "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f -- "$TUNNEL_LOG"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

if ! command -v cloudflared >/dev/null 2>&1; then
    echo "오류: cloudflared가 설치되어 있지 않습니다." >&2
    echo "먼저 Server/README.md의 Cloudflare 설치 명령을 실행하세요." >&2
    exit 1
fi

if [[ -x "$SERVER_DIR/.venv/bin/python" ]]; then
    PYTHON="$SERVER_DIR/.venv/bin/python"
else
    PYTHON="$(command -v python3)"
fi

source /opt/ros/jazzy/setup.bash
set -u

if ! "$PYTHON" -c 'import fastapi, uvicorn, yaml, rclpy, nav2_msgs' 2>/dev/null; then
    echo "오류: 서버 Python 패키지를 찾을 수 없습니다." >&2
    echo "Server/README.md의 가상환경 설치 과정을 먼저 실행하세요." >&2
    exit 1
fi

echo "GuideRobot 서버를 시작합니다: http://127.0.0.1:8080"
"$PYTHON" "$SERVER_DIR/server.py" "$@" &
SERVER_PID=$!

server_ready=false
for _ in $(seq 1 30); do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "오류: GuideRobot 서버가 실행 중 종료되었습니다." >&2
        wait "$SERVER_PID" || true
        exit 1
    fi
    if curl --fail --silent --max-time 1 \
        http://127.0.0.1:8080/api/status >/dev/null; then
        server_ready=true
        break
    fi
    sleep 1
done

if [[ "$server_ready" != true ]]; then
    echo "오류: 30초 안에 8080 포트의 서버가 준비되지 않았습니다." >&2
    exit 1
fi

echo "Cloudflare Quick Tunnel 주소를 생성하는 중..."
# Some networks block QUIC on UDP 7844. HTTP/2 uses the TCP path that is
# available in those environments.
cloudflared tunnel --protocol http2 --url http://127.0.0.1:8080 \
    >"$TUNNEL_LOG" 2>&1 &
TUNNEL_PID=$!

public_url=""
connected=false
for _ in $(seq 1 45); do
    public_url="$(grep -Eo 'https://[-a-z0-9]+\.trycloudflare\.com' \
        "$TUNNEL_LOG" | head -n 1 || true)"
    if grep -q 'Registered tunnel connection' "$TUNNEL_LOG"; then
        connected=true
    fi
    if [[ -n "$public_url" && "$connected" == true ]]; then
        break
    fi
    if ! kill -0 "$TUNNEL_PID" 2>/dev/null; then
        echo "오류: Cloudflare Tunnel 실행에 실패했습니다." >&2
        cat "$TUNNEL_LOG" >&2
        wait "$TUNNEL_PID" || true
        exit 1
    fi
    sleep 1
done

if [[ -z "$public_url" || "$connected" != true ]]; then
    echo "오류: 45초 안에 Cloudflare Tunnel이 연결되지 않았습니다." >&2
    cat "$TUNNEL_LOG" >&2
    exit 1
fi

echo
echo "============================================================"
echo "외부 접속 주소: $public_url"
echo "로컬 접속 주소: http://localhost:8080/"
echo "종료: Ctrl+C"
echo "주의: 외부 접속 주소를 아는 사람은 로봇 API에 접근할 수 있습니다."
echo "============================================================"
echo

wait "$TUNNEL_PID"
