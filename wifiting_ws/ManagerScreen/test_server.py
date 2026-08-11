#!/usr/bin/env python3
"""ManagerScreen UI 통신 확인용 임시 REST API 서버."""

import json
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


HOST = "0.0.0.0"
PORT = 8080
STATE = {
    "connected": True,
    "manual_mode": False,
    "destination": None,
    "updated_at": None,
}


class ApiHandler(BaseHTTPRequestHandler):
    server_version = "ManagerScreenTestServer/1.0"

    def _send_json(self, status, body):
        encoded = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(encoded)

    def do_GET(self):
        if self.path == "/api/status":
            self._send_json(200, STATE)
            return
        self._send_json(404, {"error": "not_found"})

    def do_POST(self):
        if self.path != "/api/command":
            self._send_json(404, {"error": "not_found"})
            return

        try:
            content_length = int(self.headers.get("Content-Length", "0"))
            body = json.loads(self.rfile.read(content_length) or b"{}")
        except (ValueError, json.JSONDecodeError):
            self._send_json(400, {"error": "invalid_json"})
            return

        if "manual_mode" in body:
            if not isinstance(body["manual_mode"], bool):
                self._send_json(
                    400, {"error": "manual_mode_must_be_boolean"}
                )
                return
            STATE["manual_mode"] = body["manual_mode"]
        elif "command" in body:
            allowed_commands = {"forward", "backward", "left", "right", "stop"}
            if body["command"] not in allowed_commands:
                self._send_json(400, {"error": "invalid_command"})
                return
            STATE["command"] = body["command"]
        elif "destination" in body:
            STATE["destination"] = str(body["destination"])
        else:
            self._send_json(
                400, {"error": "manual_mode_or_destination_required"}
            )
            return

        STATE["updated_at"] = datetime.now(timezone.utc).isoformat()
        print(
            f"Command received: {json.dumps(body, ensure_ascii=False)}",
            flush=True,
        )
        self._send_json(200, {"success": True, "status": STATE})

    def log_message(self, fmt, *args):
        print(f"{self.client_address[0]} - {fmt % args}", flush=True)


if __name__ == "__main__":
    server = ThreadingHTTPServer((HOST, PORT), ApiHandler)
    print(f"HTTP REST API Server started on port {PORT}", flush=True)
    print(f"Status URL: http://127.0.0.1:{PORT}/api/status", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nTest server stopped", flush=True)
    finally:
        server.server_close()
