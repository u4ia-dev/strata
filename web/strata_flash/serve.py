#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serve the Strata flasher to this computer only, at http://127.0.0.1:8000/, and make the flash backup it asks for
before any write. Web MIDI's SysEx needs a secure context, and a loopback address is one. Open that address in Chrome
or Edge; Ctrl+C stops the server.

The backup (the page's step 1, "Back up now"): this server waits for the FM-1 in backup mode (the chip's own USB disk
"WL82 UBOOT1.00", however it got there: the Flipper Zero route, the soft key, OCT- + OCT+), runs flipper-fm1-recovery's
tools/pc_backup.py on it (read-only: the chip and flash checked, the whole flash read twice, two identical files),
checks the image (an FM-1's flash layout and chip key, recovery_plan.py), lets the FM-1 go, and writes backup.json
beside the two files. The page writes nothing until a backup from the last MAX_AGE_H hours checks out (GET
/api/backup); starting and stopping one takes this run's token and an Origin of this server (POST). Reading the disk
needs Windows and an administrator PowerShell; without them the page shows why and the install stays off.

  python serve.py [PORT] [--recovery DIR] [--backups DIR]       (Windows: py serve.py, as administrator)
  --recovery: flipper-fm1-recovery with its .local (default: fm1-flipper-recovery beside the strata checkout)
  --backups: where each backup-YYYYMMDD-HHMMSS folder goes (default: %USERPROFILE%\\fm1-recovery)
"""
import argparse
import functools
import hashlib
import http.server
import importlib.util
import json
import os
import re
import secrets
import subprocess
import sys
import threading
import time
from pathlib import Path

FLASH_SIZE = 0x100000
MAX_AGE_H = 12
LOADER_SHA256 = "d41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376"   # jl-uboot-tool adb3f18
KEY_LINE = re.compile(r"KEY ([0-9A-F]{4}) FLASH \{'type': (\d+), 'id': (\d+)\}")
WAIT_S, READ_S = 600, 300
TOKEN_MARK = "__STRATA_TOKEN__"
# the backup-mode disk, and every USB device Windows could not read (Device Descriptor Request Failed) meanwhile
FINDER = r"""
$end = (Get-Date).AddSeconds(WAIT_S)
$prev = @{}
Get-CimInstance Win32_PnPEntity -Filter "DeviceID LIKE 'USB\\VID_0000%'" -ErrorAction SilentlyContinue |
    ForEach-Object { $prev[$_.DeviceID] = 1 }
while ((Get-Date) -lt $end) {
    $d = Get-CimInstance Win32_DiskDrive -Filter "Model LIKE '%WL82 UBOOT1.00%'" -ErrorAction SilentlyContinue |
        Where-Object { $_.InterfaceType -eq 'USB' } | Select-Object -First 1
    if ($d) { [Console]::WriteLine("DISK " + $d.DeviceID); [Console]::Out.Flush(); break }
    $now = @{}
    Get-CimInstance Win32_PnPEntity -Filter "DeviceID LIKE 'USB\\VID_0000%'" -ErrorAction SilentlyContinue |
        ForEach-Object { $now[$_.DeviceID] = $_.Name }
    foreach ($k in $now.Keys) {
        if (-not $prev.ContainsKey($k)) { [Console]::WriteLine("FAILED " + $now[$k]); [Console]::Out.Flush() }
    }
    $prev = $now
    Start-Sleep -Milliseconds 300
}
"""


class BackupError(Exception):
    pass


def is_admin():
    try:
        import ctypes
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except (AttributeError, OSError):
        return False


def windows_finder():
    """FINDER in PowerShell: "DISK <path>" when the disk is there, "FAILED <name>" for each device Windows could not
    read meanwhile; it ends after WAIT_S (the caller reads its lines and stops it)"""
    return subprocess.Popen(["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command",
                             FINDER.replace("WAIT_S", str(WAIT_S))], stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, text=True)


class Backup:
    """one backup at a time, in a thread; the state the page polls"""

    def __init__(self, recovery, backups, admin=is_admin, finder=windows_finder, python=sys.executable):
        self.recovery, self.backups = Path(recovery), Path(backups)
        self.admin, self.finder, self.python = admin, finder, python
        self.lock = threading.Lock()
        self.cancelled = threading.Event()
        self.proc = None
        self.status = {"state": "idle", "message": "", "progress": 0, "failed": [], "dir": ""}
        self._latest = (None, None)

    # ---------------------------------------------------------------- what the page reads
    def why_not(self):
        """'' when this server can make a backup, else why not"""
        if not self.admin():
            return "the flasher's server is not running as administrator (start serve.py in an administrator PowerShell)"
        tools = self.recovery / "tools"
        for f in ("pc_backup.py", "recovery_plan.py"):
            if not (tools / f).is_file():
                return f"no {tools / f} (flipper-fm1-recovery; serve.py --recovery DIR)"
        blob = self.recovery / ".local/jl-uboot-tool/data/loaderblobs/usb/wl82loader.bin"
        if not blob.is_file() or hashlib.sha256(blob.read_bytes()).hexdigest() != LOADER_SHA256:
            return f"missing or unexpected {blob} (flipper-fm1-recovery README, 'Back up on Windows')"
        if not all((self.recovery / ".local/python" / m).is_dir() for m in ("crcmod", "Cryptodome")):
            return f"no crcmod / pycryptodomex in {self.recovery / '.local/python'}"
        return ""

    def latest(self):
        """the newest backup-*/backup.json whose files still check out, with its age"""
        best = None
        for receipt in sorted(self.backups.glob("backup-*/backup.json")):
            try:
                r = json.loads(receipt.read_text(encoding="utf-8"))
                if best is None or r["made"] > best[1]["made"]:
                    best = (receipt.parent, r)
            except (OSError, ValueError, KeyError):
                continue
        if not best:
            return None
        folder, r = best
        files = [folder / "flash-1.bin", folder / "flash-2.bin"]
        key = tuple((str(f), f.stat().st_mtime, f.stat().st_size) for f in files if f.is_file())
        if self._latest[0] != key:
            ok = len(key) == 2 and all(f.stat().st_size == FLASH_SIZE for f in files) and \
                 hashlib.sha256(files[0].read_bytes()).hexdigest() == r["sha256"] and \
                 files[0].read_bytes() == files[1].read_bytes()
            self._latest = (key, ok)
        age_h = (time.time() - r["made"]) / 3600
        return {"dir": str(folder), "sha256": r["sha256"], "made": r["made"], "age_h": round(age_h, 2),
                "ok": self._latest[1] and age_h < MAX_AGE_H, "files_ok": self._latest[1]}

    def report(self):
        with self.lock:
            s = dict(self.status)
        return {**s, "why_not": self.why_not(), "latest": self.latest(), "max_age_h": MAX_AGE_H}

    def _set(self, **kw):
        with self.lock:
            self.status.update(kw)

    # ---------------------------------------------------------------- the job
    def start(self):
        with self.lock:
            if self.status["state"] in ("waiting", "reading"):
                return False
            self.status = {"state": "waiting", "message": "", "progress": 0, "failed": [], "dir": ""}
        self.cancelled.clear()
        threading.Thread(target=self._run, daemon=True).start()
        return True

    def cancel(self):
        self.cancelled.set()
        p = self.proc
        if p and p.poll() is None:
            p.kill()

    def _run(self):
        try:
            why = self.why_not()
            if why:
                raise BackupError(why)
            device = self._wait_disk()
            folder = self._read(device)
            self._check(folder, device)
            self._set(state="verified", message="", dir=str(folder), progress=32)
        except BackupError as e:
            self._set(state="cancelled" if self.cancelled.is_set() else "failed", message=str(e))
        except Exception as e:                         # (anything else: shown, never a write)
            self._set(state="failed", message=f"{type(e).__name__}: {e}")

    def _wait_disk(self):
        self._set(state="waiting", message="")
        self.proc = self.finder()
        try:
            while True:
                line = self.proc.stdout.readline()
                if self.cancelled.is_set():
                    raise BackupError("stopped: nothing was read")
                if not line:
                    raise BackupError(f"no WL82 UBOOT1.00 disk appeared in {WAIT_S // 60} min: nothing was read")
                line = line.strip()
                if line.startswith("FAILED "):
                    with self.lock:
                        self.status["failed"] = self.status["failed"] + [line[7:]]
                elif line.startswith("DISK "):
                    return line[5:]
        finally:
            if self.proc.poll() is None:
                self.proc.kill()
            self.proc.wait()
            if self.proc.stdout:
                self.proc.stdout.close()
            self.proc = None

    def _read(self, device):
        tools = self.recovery / "tools"
        folder = self.backups / time.strftime("backup-%Y%m%d-%H%M%S")
        folder.mkdir(parents=True, exist_ok=False)
        self._set(state="reading", dir=str(folder), progress=0)
        log = folder / "backup.log"
        self.proc = subprocess.Popen([self.python, str(tools / "pc_backup.py"), "--device", device,
                                      "--reference", str(self.recovery / ".local/jl-uboot-tool"),
                                      "--dependencies", str(self.recovery / ".local/python"),
                                      "--output", str(folder)], cwd=str(tools),
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        end = time.monotonic() + READ_S
        try:
            while True:
                text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
                self._set(progress=len(re.findall(r"^BACKUP [12]: 0x", text, re.M)))
                if "BACKUPS VERIFIED" in text:
                    return folder
                if self.proc.poll() is not None or self.cancelled.is_set() or time.monotonic() > end:
                    tail = " | ".join(text.strip().splitlines()[-3:])
                    raise BackupError(("stopped" if self.cancelled.is_set() else "the backup did not finish") +
                                      f" (log: {log}{': ' + tail if tail else ''})")
                time.sleep(0.3)
        finally:
            (folder / "stop").touch()                  # pc_backup.py lets the FM-1 go
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
            self.proc = None

    def _check(self, folder, device):
        one, two = (folder / "flash-1.bin").read_bytes(), (folder / "flash-2.bin").read_bytes()
        if len(one) != FLASH_SIZE or one != two:
            raise BackupError(f"the two reads are not identical 1 MiB files ({folder})")
        m = KEY_LINE.search((folder / "backup.log").read_text(encoding="utf-8", errors="replace"))
        if not m or m.group(1) != "980F" or (int(m.group(2)), int(m.group(3))) != (3, 0x856014):
            raise BackupError(f"not an FM-1's chip key and flash ({folder / 'backup.log'})")
        spec = importlib.util.spec_from_file_location("fm1_recovery_plan", self.recovery / "tools/recovery_plan.py")
        plan = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(plan)
        try:
            plan.validate_flash_layout(one[:plan.APP_END])
        except ValueError as e:
            raise BackupError(f"the backup's boot head does not check out ({e}): keep it, write nothing") from e
        receipt = {"made": time.time(), "device": device, "sha256": hashlib.sha256(one).hexdigest(),
                   "size": len(one), "key": m.group(1), "flash_id": "856014",
                   "boot_sha256": hashlib.sha256(one[:0x4000]).hexdigest(),
                   "app_sha256": hashlib.sha256(one[0x4000:0x93000]).hexdigest()}
        (folder / "backup.json").write_text(json.dumps(receipt, indent=1), encoding="utf-8")


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".fwsc": "application/octet-stream", ".md": "text/plain; charset=utf-8",
                      ".txt": "text/plain; charset=utf-8", "": "text/plain; charset=utf-8"}

    def __init__(self, *a, backup=None, token="", port=0, **kw):
        self.backup, self.token, self.port = backup, token, port
        super().__init__(*a, **kw)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")   # (a rebuilt package is never an old one from the cache)
        super().end_headers()

    def log_message(self, fmt, *args):
        if self.command == "GET" and self.path.startswith("/api/backup"):
            return                                      # (the page asks once a second)
        super().log_message(fmt, *args)

    def _host_ok(self):                                 # (a page of another site, by DNS rebinding: refused)
        return self.headers.get("Host", "") in (f"127.0.0.1:{self.port}", f"localhost:{self.port}")

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if not self._host_ok():
            self.send_error(403)
            return
        path = self.path.split("?")[0]
        if path == "/api/backup":
            self._json(200, self.backup.report())
        elif path in ("/", "/index.html"):
            body = (Path(self.directory) / "index.html").read_text(encoding="utf-8").replace(TOKEN_MARK, self.token)
            body = body.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            super().do_GET()

    def do_POST(self):
        origin = self.headers.get("Origin", "")
        if not self._host_ok() or origin not in (f"http://127.0.0.1:{self.port}", f"http://localhost:{self.port}") \
                or not secrets.compare_digest(self.headers.get("X-Strata-Token", ""), self.token):
            self._json(403, {"error": "refused"})
            return
        path = self.path.split("?")[0]
        if path == "/api/backup/start":
            self._json(200, {"started": self.backup.start()})
        elif path == "/api/backup/cancel":
            self.backup.cancel()
            self._json(200, {"cancelled": True})
        else:
            self._json(404, {"error": "no such call"})


def make_server(port, directory, backup, token):
    handler = functools.partial(Handler, directory=directory, backup=backup, token=token, port=port)
    return http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)


def main():
    here = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(description="the Strata flasher on http://127.0.0.1:PORT/, with its backup step")
    ap.add_argument("port", nargs="?", type=int, default=8000)
    ap.add_argument("--recovery", type=Path, default=here.parents[2] / "fm1-flipper-recovery")
    ap.add_argument("--backups", type=Path, default=Path.home() / "fm1-recovery")
    a = ap.parse_args()
    backup = Backup(a.recovery, a.backups)
    with make_server(a.port, str(here), backup, secrets.token_hex(16)) as srv:
        print(f"Strata flasher: open http://127.0.0.1:{a.port}/ in Chrome or Edge (this computer only). Ctrl+C stops.")
        why = backup.why_not()
        print("Backups: " + (f"into {a.backups}, with {a.recovery}" if not why else f"OFF: {why}"))
        try:
            srv.serve_forever()
        except KeyboardInterrupt:
            backup.cancel()


if __name__ == "__main__":
    main()
