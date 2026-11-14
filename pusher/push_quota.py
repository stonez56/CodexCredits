"""
本機推播工具：將最新 AI 額度 JSON 推送至 stonez56.com/api/quota.php。
使用方式：
  1. 複製 pusher/.env.example 為 pusher/.env 並填入 QUOTA_API_URL 與 QUOTA_API_TOKEN
  2. 執行 python pusher/push_quota.py（預設讀取 mock_server/mock_quota.json 並更新為即時時間）
"""

import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
ENV_FILE = Path(__file__).resolve().parent / ".env"
DEFAULT_JSON = ROOT_DIR / "mock_server" / "mock_quota.json"


def load_env() -> None:
    if not ENV_FILE.exists():
        return
    for line in ENV_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        os.environ.setdefault(k.strip(), v.strip())


def build_payload(src_path: Path) -> bytes:
    now = int(time.time())
    data = json.loads(src_path.read_text(encoding="utf-8"))
    data["generated_at_epoch"] = now

    # 若為預設範例資料，動態刷新倒數基準方便驗證 [LIVE] 顯示
    if src_path == DEFAULT_JSON:
        data["codex_plus"]["five_hour"]["expires_at_epoch"] = now + (3 * 3600 + 42 * 60)
        data["codex_plus"]["weekly"]["expires_at_epoch"] = now + (4 * 86400 + 18 * 3600 + 10 * 60)
        data["codex_plus"]["resets"]["schedule_epochs"] = [
            now + (1 * 86400 + 6 * 3600),
            now + (4 * 86400 + 12 * 3600),
        ]
        accounts = data.get("gemini_pro_accounts", [])
        for acc in accounts:
            for pool in acc.get("pools", []):
                if "weekly" in pool:
                    pool["weekly"]["expires_at_epoch"] = now + (6 * 86400 + 20 * 3600)
                if "five_hour" in pool:
                    pool["five_hour"]["expires_at_epoch"] = now + (1 * 3600 + 7 * 60)

    return json.dumps(data, ensure_ascii=False).encode("utf-8")


def main() -> int:
    load_env()
    url = os.environ.get("QUOTA_API_URL", "")
    token = os.environ.get("QUOTA_API_TOKEN", "")

    if not url or url.startswith("REPLACE_WITH_"):
        print("ERROR: QUOTA_API_URL is not set in pusher/.env", file=sys.stderr)
        return 1
    if not token or token.startswith("REPLACE_WITH_"):
        print("ERROR: QUOTA_API_TOKEN is not set in pusher/.env", file=sys.stderr)
        return 1

    src_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_JSON
    payload = build_payload(src_path)

    req = urllib.request.Request(
        url,
        data=payload,
        method="POST",
        headers={
            "Content-Type": "application/json; charset=utf-8",
            "Authorization": f"Bearer {token}",
            "User-Agent": "CodexCredits-Pusher/1.0",
        },
    )

    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            body = resp.read().decode("utf-8", errors="replace")
            print(f"OK ({resp.status}): {body}")
            return 0
    except urllib.error.HTTPError as e:
        print(f"HTTP Error {e.code}: {e.read().decode('utf-8', errors='replace')}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Request failed: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

