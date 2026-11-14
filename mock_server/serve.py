"""
輕量本地 Mock JSON 伺服器（僅供 Phase 2~4 開發與測試）。
啟動方式：python serve.py --port 8080
動態將 expires_at_epoch 調整為相對「現在時間」的未來時間，方便實機測試倒數顯示。
"""

import argparse
import json
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

MOCK_FILE = Path(__file__).parent / "mock_quota.json"


def build_dynamic_payload() -> bytes:
    now = int(time.time())
    data = json.loads(MOCK_FILE.read_text(encoding="utf-8"))
    data["generated_at_epoch"] = now

    # 讓倒數永遠相對於現在時間，便於實機測試
    data["codex_plus"]["five_hour"]["expires_at_epoch"] = now + (3 * 3600 + 42 * 60)
    data["codex_plus"]["weekly"]["expires_at_epoch"] = now + (4 * 86400 + 18 * 3600 + 10 * 60)
    data["codex_plus"]["resets"]["schedule_epochs"] = [
        now + (1 * 86400 + 6 * 3600),
        now + (4 * 86400 + 12 * 3600),
    ]

    accounts = data.get("gemini_pro_accounts", [])
    if len(accounts) >= 1:
        accounts[0]["weekly"]["expires_at_epoch"] = now + (6 * 86400 + 22 * 3600)
        accounts[0]["five_hour"]["expires_at_epoch"] = now + (3 * 3600 + 43 * 60)
    if len(accounts) >= 2:
        accounts[1]["weekly"]["expires_at_epoch"] = now + (4 * 86400 + 11 * 3600)
        accounts[1]["five_hour"]["expires_at_epoch"] = now + (2 * 3600 + 15 * 60)

    return json.dumps(data, ensure_ascii=False, indent=2).encode("utf-8")


class MockQuotaHandler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        if self.path.split("?")[0] not in ("/quota.json", "/api/quota", "/"):
            self.send_response(404)
            self.end_headers()
            return
        payload = build_dynamic_payload()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


def main() -> None:
    parser = argparse.ArgumentParser(description="CodexCredits Mock Quota Server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    server = HTTPServer((args.host, args.port), MockQuotaHandler)
    print(f"Mock Quota Server running on http://{args.host}:{args.port}/quota.json")
    server.serve_forever()


if __name__ == "__main__":
    main()

