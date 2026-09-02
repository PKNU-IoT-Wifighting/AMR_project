#!/usr/bin/env bash
set -eo pipefail

TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SERVER_DIR="$TEST_DIR/../Server"
PYTHON="$TEST_DIR/.venv/bin/python"

if [[ ! -x "$PYTHON" ]]; then
    echo "가상환경이 없습니다. 먼저 $TEST_DIR/setup.sh 를 실행하세요." >&2
    exit 1
fi

source /opt/ros/jazzy/setup.bash
set -u

exec "$PYTHON" "$SERVER_DIR/server.py" \
    --config "$TEST_DIR/config.yaml"
