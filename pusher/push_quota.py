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
PUSHER_DIR = Path(__file__).resolve().parent
ENV_FILE = PUSHER_DIR / ".env"
REAL_JSON = PUSHER_DIR / "real_quota.json"
DEFAULT_JSON = ROOT_DIR / "mock_server" / "mock_quota.json"
CODEX_AUTH_FILE = Path.home() / ".codex" / "auth.json"
CODEX_USAGE_URL = "https://chatgpt.com/backend-api/wham/usage"


def fetch_codex_usage(timeout: int = 10) -> dict | None:
    """Fetch live Codex Plus usage from ChatGPT using ~/.codex/auth.json."""
    if not CODEX_AUTH_FILE.exists():
        print("[Codex] ~/.codex/auth.json not found; using cached/base values.")
        return None
    try:
        auth = json.loads(CODEX_AUTH_FILE.read_text(encoding="utf-8"))
        access_token = (auth.get("tokens") or {}).get("access_token", "")
        if not access_token:
            print("[Codex] access_token missing in ~/.codex/auth.json; using cached/base values.")
            return None

        req = urllib.request.Request(
            CODEX_USAGE_URL,
            headers={
                "Authorization": f"Bearer {access_token}",
                "Accept": "application/json",
                "User-Agent": "CodexCreditsPusher/1.1",
            },
            method="GET",
        )
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            if resp.status != 200:
                print(f"[Codex] HTTP {resp.status}; using cached/base values.")
                return None
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        print(f"[Codex] HTTP error {e.code}; using cached/base values.")
    except Exception as e:
        print(f"[Codex] Fetch failed ({type(e).__name__}); using cached/base values.")
    return None


def apply_codex_usage(data: dict, usage: dict) -> None:
    """Overlay live wham/usage metrics onto schema v1.1 codex_plus object."""
    codex = data.setdefault("codex_plus", {})
    codex["status"] = "ok"

    rate_limit = usage.get("rate_limit") or {}
    primary = rate_limit.get("primary_window") or rate_limit.get("primary") or usage.get("primary")
    secondary = rate_limit.get("secondary_window") or rate_limit.get("secondary") or usage.get("secondary")

    if isinstance(primary, dict) and "used_percent" in primary and "reset_at" in primary:
        codex["five_hour"] = {
            "remaining_pct": max(0, min(100, 100 - int(round(primary["used_percent"])))),
            "expires_at_epoch": int(primary["reset_at"]),
        }

    if isinstance(secondary, dict) and "used_percent" in secondary and "reset_at" in secondary:
        codex["weekly"] = {
            "remaining_pct": max(0, min(100, 100 - int(round(secondary["used_percent"])))),
            "expires_at_epoch": int(secondary["reset_at"]),
        }

    credits = usage.get("credits") or {}
    try:
        balance = float(credits.get("balance") or 0)
    except (TypeError, ValueError):
        balance = 0.0
    has_credits = bool(credits.get("has_credits")) and balance > 0
    codex["purchased_credits"] = {
        "has_credits": has_credits,
        "display_text": f"${balance:.2f}" if has_credits else "$0.00",
    }


def load_env() -> None:
    if not ENV_FILE.exists():
        return
    for line in ENV_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        os.environ.setdefault(k.strip(), v.strip())


def build_payload(src_path: Path, live_merge: bool = False) -> bytes:
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

    if live_merge:
        usage = fetch_codex_usage()
        if usage is not None:
            apply_codex_usage(data, usage)
            five_h = data.get("codex_plus", {}).get("five_hour", {}).get("remaining_pct", "?")
            weekly = data.get("codex_plus", {}).get("weekly", {}).get("remaining_pct", "?")
            print(f"[Codex] Live usage merged: 5h={five_h}%, weekly={weekly}%")
            if REAL_JSON.exists():
                REAL_JSON.write_text(
                    json.dumps(data, ensure_ascii=False, indent=2) + "\n",
                    encoding="utf-8",
                )

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

    if len(sys.argv) > 1:
        src_path = Path(sys.argv[1])
        live_merge = False
    else:
        src_path = REAL_JSON if REAL_JSON.exists() else DEFAULT_JSON
        live_merge = True

    print(f"[Pusher] Base file: {src_path.name}")
    payload = build_payload(src_path, live_merge=live_merge)

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

