"""HTTP OTA uploader with pre-upload version check (skips if already on target version)."""

import http.client
import json
import re
import sys
import urllib.request
import uuid
from pathlib import Path


def read_target_version(project_dir: Path) -> str:
    cfg = project_dir / "include" / "config.h"
    if not cfg.is_file():
        return ""
    m = re.search(r'APP_VERSION\[\]\s*=\s*"([^"]+)"', cfg.read_text(encoding="utf-8"))
    return m.group(1) if m else ""


def check_remote_version(host: str) -> str:
    url = f"http://{host}/version"
    try:
        with urllib.request.urlopen(url, timeout=3.0) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            return str(data.get("version", "")).strip()
    except Exception as exc:
        print(f"[OTA] Version endpoint unavailable ({exc}); proceeding with upload...")
        return ""


def upload_http_ota(bin_path: Path, host: str) -> int:
    boundary = f"----CodexOTA{uuid.uuid4().hex}"
    head = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="firmware"; filename="{bin_path.name}"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode("utf-8")
    tail = f"\r\n--{boundary}--\r\n".encode("utf-8")
    file_size = bin_path.stat().st_size
    total_len = len(head) + file_size + len(tail)

    print(f"[OTA] Uploading {bin_path.name} ({file_size} bytes) to http://{host}/update ...")
    conn = http.client.HTTPConnection(host, 80, timeout=60)
    try:
        conn.putrequest("POST", "/update")
        conn.putheader("Content-Type", f"multipart/form-data; boundary={boundary}")
        conn.putheader("Content-Length", str(total_len))
        conn.endheaders()
        conn.send(head)

        sent = 0
        last_pct = -1
        with bin_path.open("rb") as f:
            while True:
                chunk = f.read(32768)
                if not chunk:
                    break
                conn.send(chunk)
                sent += len(chunk)
                pct = (sent * 100) // file_size
                if pct >= last_pct + 10 or pct == 100:
                    print(f"[OTA] Progress: {pct}% ({sent}/{file_size})")
                    last_pct = pct

        conn.send(tail)
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", errors="replace")
        print(f"[OTA] Response {resp.status}: {body}")
        return 0 if resp.status == 200 else 1
    finally:
        conn.close()


def main() -> int:
    if len(sys.argv) < 3:
        print("Usage: ota_upload.py <firmware.bin> <host_or_ip>")
        return 1

    bin_path = Path(sys.argv[1]).resolve()
    host = sys.argv[2].strip()
    project_dir = Path(__file__).resolve().parent.parent

    target_ver = read_target_version(project_dir)
    if target_ver:
        current_ver = check_remote_version(host)
        if current_ver == target_ver:
            print(f"[OTA] Device at {host} already runs {current_ver} — skipping upload.")
            return 0
        if current_ver:
            print(f"[OTA] Upgrading {host}: {current_ver} -> {target_ver}")

    return upload_http_ota(bin_path, host)


if __name__ == "__main__":
    raise SystemExit(main())

