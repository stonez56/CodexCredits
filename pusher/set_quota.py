#!/usr/bin/env python3
"""
Human-friendly CLI helper to initialize or update pusher/real_quota.json
for Codex reset schedules and Gemini Pro #1 / #2 pool quotas.

Examples:
  python set_quota.py --show
  python set_quota.py --codex-resets "12/01 04:11, 12/02 04:51, 12/13 05:42"
  python set_quota.py --account 1 --pool "Gemini" --weekly "82% 6d20h" --five-hour "38% 1h7m" --push
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import time
from pathlib import Path

PUSHER_DIR = Path(__file__).resolve().parent
ROOT_DIR = PUSHER_DIR.parent
REAL_JSON = PUSHER_DIR / "real_quota.json"
DEFAULT_JSON = ROOT_DIR / "mock_server" / "mock_quota.json"
LOCAL_TZ = dt.timezone(dt.timedelta(hours=8))

DURATION_RE = re.compile(r"(?:(\d+)\s*d)?\s*(?:(\d+)\s*h)?\s*(?:(\d+)\s*m)?$", re.IGNORECASE)


def load_or_init_data() -> dict:
    src = REAL_JSON if REAL_JSON.exists() else DEFAULT_JSON
    return json.loads(src.read_text(encoding="utf-8"))


def save_data(data: dict) -> None:
    REAL_JSON.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def parse_datetime_epoch(text: str, now_dt: dt.datetime) -> int:
    s = text.strip()
    for fmt in ("%Y/%m/%d %H:%M", "%Y-%m-%d %H:%M"):
        try:
            parsed = dt.datetime.strptime(s, fmt).replace(tzinfo=LOCAL_TZ)
            return int(parsed.timestamp())
        except ValueError:
            pass

    for fmt in ("%m/%d %H:%M", "%m-%d %H:%M"):
        try:
            parsed = dt.datetime.strptime(s, fmt).replace(year=now_dt.year, tzinfo=LOCAL_TZ)
            if (now_dt - parsed).total_seconds() > 30 * 86400:
                parsed = parsed.replace(year=now_dt.year + 1)
            return int(parsed.timestamp())
        except ValueError:
            pass

    raise ValueError(f"Unrecognized date/time format: {text!r} (expected e.g. '12/01 04:11')")


def parse_duration_seconds(text: str) -> int:
    s = text.strip()
    if not s:
        raise ValueError("Empty duration")
    m = DURATION_RE.match(s)
    if not m or not any(m.groups()):
        raise ValueError(f"Unrecognized duration: {text!r} (expected e.g. '6d20h' or '1h7m')")
    days = int(m.group(1) or 0)
    hours = int(m.group(2) or 0)
    mins = int(m.group(3) or 0)
    return days * 86400 + hours * 3600 + mins * 60


def parse_quota_spec(spec: str, now: int) -> dict:
    parts = spec.strip().split(None, 1)
    if len(parts) != 2:
        raise ValueError(f"Expected '<pct>% <duration|date>', got: {spec!r}")
    pct_str, time_str = parts
    pct = int(pct_str.rstrip("%"))
    if not (0 <= pct <= 100):
        raise ValueError(f"Percentage out of range 0..100: {pct}")

    now_dt = dt.datetime.fromtimestamp(now, tz=LOCAL_TZ)
    if "/" in time_str or "-" in time_str or ":" in time_str:
        expires_at = parse_datetime_epoch(time_str, now_dt)
    else:
        expires_at = now + parse_duration_seconds(time_str)

    return {"remaining_pct": pct, "expires_at_epoch": expires_at}


def format_remaining(epoch: int, now: int) -> str:
    diff = max(0, epoch - now)
    days, rem = divmod(diff, 86400)
    hours, rem = divmod(rem, 3600)
    mins = rem // 60
    if days > 0:
        return f"{days}d{hours}h"
    if hours > 0:
        return f"{hours}h{mins}m"
    return f"{mins}m"


def print_summary(data: dict) -> None:
    now = int(time.time())
    codex = data.get("codex_plus", {})
    fh = codex.get("five_hour", {})
    wk = codex.get("weekly", {})
    resets = codex.get("resets", {})
    print(f"File: {REAL_JSON if REAL_JSON.exists() else str(DEFAULT_JSON) + ' (template)'}")
    print(
        f"Codex Plus: 5h={fh.get('remaining_pct', '?')}% ({format_remaining(fh.get('expires_at_epoch', now), now)}), "
        f"weekly={wk.get('remaining_pct', '?')}% ({format_remaining(wk.get('expires_at_epoch', now), now)}), "
        f"resets={resets.get('available_count', 0)}"
    )
    for ep in resets.get("schedule_epochs", []):
        t_str = dt.datetime.fromtimestamp(ep, tz=LOCAL_TZ).strftime("%m/%d %H:%M")
        print(f"  - Reset: {t_str} ({format_remaining(ep, now)})")

    for idx, acc in enumerate(data.get("gemini_pro_accounts", []), start=1):
        print(f"Account #{idx} [{acc.get('label', '?')}] ({acc.get('status', '?')}):")
        for pool in acc.get("pools", []):
            w = pool.get("weekly")
            f = pool.get("five_hour")
            w_txt = f"weekly={w['remaining_pct']}% ({format_remaining(w['expires_at_epoch'], now)})" if w else ""
            f_txt = f"5h={f['remaining_pct']}% ({format_remaining(f['expires_at_epoch'], now)})" if f else ""
            joined = ", ".join(x for x in (w_txt, f_txt) if x)
            print(f"  - {pool.get('name', '?')}: {joined}")


def find_account(accounts: list[dict], selector: str) -> dict:
    if selector.isdigit():
        idx = int(selector) - 1
        if 0 <= idx < len(accounts):
            return accounts[idx]
    low = selector.lower()
    for acc in accounts:
        if low in str(acc.get("label", "")).lower():
            return acc
    raise ValueError(f"Gemini Pro account not found: {selector!r}")


def find_pool(pools: list[dict], selector: str) -> dict:
    low = selector.lower()
    for pool in pools:
        if low in str(pool.get("name", "")).lower():
            return pool
    raise ValueError(f"Pool {selector!r} not found in {[p.get('name') for p in pools]}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Update pusher/real_quota.json")
    parser.add_argument("--show", action="store_true", help="Show current quota summary")
    parser.add_argument("--codex-resets", help="Comma-separated reset datetimes (UTC+8), e.g. '12/01 04:11, 12/02 04:51'")
    parser.add_argument("--codex-reset-count", type=int, help="Override codex_plus.resets.available_count")
    parser.add_argument("--account", help="Gemini Pro account index (1, 2) or label substring")
    parser.add_argument("--pool", help="Pool name or substring (e.g. 'Gemini', 'Claude', 'G3', 'G2')")
    parser.add_argument("--weekly", help="Weekly quota, e.g. '82%% 6d20h' or '82%% 11/21 18:00'")
    parser.add_argument("--five-hour", help="5-hour quota, e.g. '38%% 1h7m'")
    parser.add_argument("--push", action="store_true", help="Push immediately after updating")
    args = parser.parse_args()

    data = load_or_init_data()
    now = int(time.time())
    changed = False

    if args.codex_resets is not None:
        now_dt = dt.datetime.fromtimestamp(now, tz=LOCAL_TZ)
        items = [x.strip() for x in args.codex_resets.split(",") if x.strip()]
        epochs = [parse_datetime_epoch(item, now_dt) for item in items]
        resets = data.setdefault("codex_plus", {}).setdefault("resets", {})
        resets["schedule_epochs"] = epochs
        resets["available_count"] = args.codex_reset_count if args.codex_reset_count is not None else len(epochs)
        changed = True
    elif args.codex_reset_count is not None:
        resets = data.setdefault("codex_plus", {}).setdefault("resets", {})
        resets["available_count"] = args.codex_reset_count
        changed = True

    if args.weekly is not None or args.five_hour is not None:
        if not args.account or not args.pool:
            parser.error("--weekly and --five-hour require both --account and --pool")
        acc = find_account(data.get("gemini_pro_accounts", []), args.account)
        pool = find_pool(acc.get("pools", []), args.pool)
        if args.weekly is not None:
            pool["weekly"] = parse_quota_spec(args.weekly, now)
        if args.five_hour is not None:
            pool["five_hour"] = parse_quota_spec(args.five_hour, now)
        changed = True

    if changed:
        save_data(data)
        print(f"Updated {REAL_JSON}")

    if args.show or not changed:
        print_summary(data)

    if args.push:
        import push_quota
        return push_quota.main()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

