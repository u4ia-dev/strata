#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
"""The flasher's backup step (web/strata_flash/serve.py) without an FM-1: a fake pc_backup.py (the real one's log and
files, from a flash image made of build/strata.fwsc's), a fake disk watcher, and the real recovery_plan.py from
flipper-fm1-recovery beside the strata checkout. The job's outcomes (verified; reads that differ; a backup that stops;
a boot head that does not check out; stopped while waiting), the receipt and the 12-hour rule, why it is off, and the
HTTP rules (Host, Origin, the token).
  python tests/serve_backup_test.py        (Windows: py; after tools/build.py --strata)"""
import hashlib
import http.client
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECOVERY = ROOT.parent / "fm1-flipper-recovery"
sys.path.insert(0, str(ROOT / "web" / "strata_flash"))
import serve  # noqa: E402

FAKE_PC_BACKUP = r'''
import argparse, os, sys, time
from pathlib import Path
ap = argparse.ArgumentParser()
for a in ("--device", "--reference", "--dependencies", "--output"): ap.add_argument(a, required=True)
a = ap.parse_args()
out = Path(a.output); out.mkdir(parents=True, exist_ok=True)
log = (out / "backup.log").open("a", encoding="utf8", buffering=1)
mode = os.environ.get("FAKE_MODE", "ok")
print("INQUIRY", a.device, file=log)
print("KEY %s FLASH {'type': 3, 'id': 8740884}" % ("1234" if mode == "badkey" else "980F"), file=log)
data = Path(os.environ["FAKE_FLASH"]).read_bytes()
for n in (1, 2):
    for off in range(0, 0x100000, 0x10000):
        print("BACKUP %d: %#x t=0.1s" % (n, off), file=log)
        if mode == "die" and n == 2: sys.exit(1)
    blob = data if not (mode == "differ" and n == 2) else data[:-1] + b"\x00"
    (out / ("flash-%d.bin" % n)).open("xb").write(blob)
print("BACKUPS VERIFIED. No flash writes. Keeping loader alive.", file=log)
while not (out / "stop").exists(): time.sleep(0.05)
'''


def finder_lines(*lines, then_sleep=0.0):
    script = "import sys, time\n" + "".join(f"print({line!r}, flush=True); time.sleep(0.05)\n" for line in lines) + \
             f"time.sleep({then_sleep})\n"
    return lambda: subprocess.Popen([sys.executable, "-c", script], stdout=subprocess.PIPE, text=True)


class BackupJob(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pkg, plan = ROOT / "build" / "strata.fwsc", RECOVERY / "tools" / "recovery_plan.py"
        if not pkg.is_file() or not plan.is_file():
            raise unittest.SkipTest(f"needs {pkg} (tools/build.py --strata) and {plan}")
        sys.path.insert(0, str(plan.parent))
        import recovery_plan
        cls.image = recovery_plan.extract_firmware(pkg.read_bytes())[:0x93000] + b"\xff" * (0x100000 - 0x93000)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        t = Path(self.tmp.name)
        self.recovery, self.backups = t / "recovery", t / "backups"
        (self.recovery / "tools").mkdir(parents=True)
        (self.recovery / "tools" / "pc_backup.py").write_text(FAKE_PC_BACKUP, encoding="utf-8")
        (self.recovery / "tools" / "recovery_plan.py").write_bytes((RECOVERY / "tools" / "recovery_plan.py").read_bytes())
        blob = self.recovery / ".local/jl-uboot-tool/data/loaderblobs/usb/wl82loader.bin"
        blob.parent.mkdir(parents=True)
        blob.write_bytes(b"fake loader")
        for m in ("crcmod", "Cryptodome"):
            (self.recovery / ".local/python" / m).mkdir(parents=True)
        self.flash = t / "flash.bin"
        self.flash.write_bytes(self.image)
        self.saved = serve.LOADER_SHA256, os.environ.get("FAKE_MODE"), os.environ.get("FAKE_FLASH")
        serve.LOADER_SHA256 = hashlib.sha256(b"fake loader").hexdigest()
        os.environ["FAKE_FLASH"] = str(self.flash)
        os.environ["FAKE_MODE"] = "ok"

    def tearDown(self):
        serve.LOADER_SHA256 = self.saved[0]
        for k, v in (("FAKE_MODE", self.saved[1]), ("FAKE_FLASH", self.saved[2])):
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        self.tmp.cleanup()

    def job(self, finder=None, admin=True):
        return serve.Backup(self.recovery, self.backups, admin=lambda: admin,
                            finder=finder or finder_lines("FAILED Unknown USB Device (Device Descriptor Request Failed)",
                                                          r"DISK \\.\PHYSICALDRIVE9"))

    def until_done(self, b, seconds=30):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            r = b.report()
            if r["state"] not in ("waiting", "reading"):
                return r
            time.sleep(0.05)
        self.fail("the backup job did not finish")

    def test_verified_backup_and_its_receipt(self):
        b = self.job()
        self.assertEqual(b.why_not(), "")
        self.assertTrue(b.start())
        r = self.until_done(b)
        self.assertEqual(r["state"], "verified", r["message"])
        self.assertEqual(r["progress"], 32)
        self.assertEqual(r["failed"], ["Unknown USB Device (Device Descriptor Request Failed)"])
        folder = Path(r["dir"])
        receipt = json.loads((folder / "backup.json").read_text(encoding="utf-8"))
        self.assertEqual(receipt["sha256"], hashlib.sha256(self.image).hexdigest())
        self.assertEqual((receipt["key"], receipt["flash_id"], receipt["device"]), ("980F", "856014", r"\\.\PHYSICALDRIVE9"))
        self.assertTrue((folder / "stop").exists())                      # the FM-1 let go
        L = r["latest"]
        self.assertTrue(L["ok"] and L["files_ok"] and L["dir"] == str(folder) and L["age_h"] < 0.1)

    def test_reads_that_differ_fail(self):
        os.environ["FAKE_MODE"] = "differ"
        b = self.job()
        b.start()
        r = self.until_done(b)
        self.assertEqual(r["state"], "failed")
        self.assertIn("not identical", r["message"])
        self.assertIsNone(r["latest"])

    def test_a_backup_that_stops_fails(self):
        os.environ["FAKE_MODE"] = "die"
        b = self.job()
        b.start()
        r = self.until_done(b)
        self.assertEqual(r["state"], "failed")
        self.assertIn("did not finish", r["message"])
        self.assertIn("BACKUP 2", r["message"])                          # (the log's last lines)
        self.assertIsNone(r["latest"])

    def test_wrong_chip_key_fails(self):
        os.environ["FAKE_MODE"] = "badkey"
        b = self.job()
        b.start()
        r = self.until_done(b)
        self.assertEqual(r["state"], "failed")
        self.assertIn("chip key", r["message"])

    def test_a_boot_head_that_does_not_check_out_fails(self):
        self.flash.write_bytes(b"\xff" * 0x100000)                       # (two identical reads of a blank chip)
        b = self.job()
        b.start()
        r = self.until_done(b)
        self.assertEqual(r["state"], "failed")
        self.assertIn("boot head does not check out", r["message"])
        self.assertIsNone(r["latest"])

    def test_stopped_while_waiting_reads_nothing(self):
        b = self.job(finder=finder_lines(then_sleep=30))
        b.start()
        time.sleep(0.4)
        self.assertEqual(b.report()["state"], "waiting")
        b.cancel()
        r = self.until_done(b, 10)
        self.assertEqual(r["state"], "cancelled")
        self.assertIn("nothing was read", r["message"])
        self.assertFalse(self.backups.exists() and any(self.backups.iterdir()))

    def test_no_disk_in_time_reads_nothing(self):
        b = self.job(finder=finder_lines("FAILED Unknown USB Device"))      # (the watcher ends: its time is up)
        b.start()
        r = self.until_done(b)
        self.assertEqual(r["state"], "failed")
        self.assertIn("no WL82 UBOOT1.00 disk appeared", r["message"])

    def test_twelve_hours_and_files_that_changed(self):
        b = self.job()
        b.start()
        folder = Path(self.until_done(b)["dir"])
        receipt = json.loads((folder / "backup.json").read_text(encoding="utf-8"))
        receipt["made"] -= 13 * 3600
        (folder / "backup.json").write_text(json.dumps(receipt), encoding="utf-8")
        L = b.latest()
        self.assertTrue(L["files_ok"] and not L["ok"] and L["age_h"] > 12)
        receipt["made"] += 13 * 3600
        (folder / "backup.json").write_text(json.dumps(receipt), encoding="utf-8")
        with (folder / "flash-2.bin").open("r+b") as f:
            f.seek(0x50000)
            f.write(b"\x00")
        L = b.latest()
        self.assertFalse(L["ok"] or L["files_ok"])

    def test_why_it_is_off(self):
        self.assertIn("administrator", self.job(admin=False).why_not())
        (self.recovery / "tools" / "pc_backup.py").unlink()
        self.assertIn("pc_backup.py", self.job().why_not())
        b = self.job(admin=False)
        b.start()
        r = self.until_done(b)
        self.assertEqual(r["state"], "failed")
        self.assertIn("administrator", r["message"])


class Http(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        d = Path(self.tmp.name)
        (d / "index.html").write_text('<script>const TOKEN = "__STRATA_TOKEN__";</script>', encoding="utf-8")
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0))
            self.port = s.getsockname()[1]
        self.started = []
        backup = serve.Backup(d / "none", d / "backups", admin=lambda: False)
        backup.start = lambda: self.started.append(1) or True
        self.srv = serve.make_server(self.port, str(d), backup, "tok123")
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def tearDown(self):
        self.srv.shutdown()
        self.srv.server_close()
        self.tmp.cleanup()

    def ask(self, method, path, **headers):
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        c.putrequest(method, path, skip_host=True)
        for k, v in {"Host": f"127.0.0.1:{self.port}", **headers}.items():
            if v is not None:
                c.putheader(k, v)
        c.putheader("Content-Length", "0")
        c.endheaders()
        r = c.getresponse()
        body = r.read().decode()
        c.close()
        return r.status, body

    def test_the_page_carries_the_token(self):
        status, body = self.ask("GET", "/")
        self.assertEqual(status, 200)
        self.assertIn('"tok123"', body)
        self.assertNotIn("__STRATA_TOKEN__", body)

    def test_report(self):
        status, body = self.ask("GET", "/api/backup")
        r = json.loads(body)
        self.assertEqual(status, 200)
        self.assertEqual((r["state"], r["latest"], r["max_age_h"]), ("idle", None, 12))
        self.assertIn("administrator", r["why_not"])

    def test_other_hosts_refused(self):
        self.assertEqual(self.ask("GET", "/api/backup", Host="evil.example")[0], 403)
        self.assertEqual(self.ask("GET", "/", Host=f"evil.example:{self.port}")[0], 403)

    def test_start_needs_the_token_and_this_origin(self):
        here = f"http://127.0.0.1:{self.port}"
        self.assertEqual(self.ask("POST", "/api/backup/start", Origin=here)[0], 403)
        self.assertEqual(self.ask("POST", "/api/backup/start", Origin=here, **{"X-Strata-Token": "nope"})[0], 403)
        self.assertEqual(self.ask("POST", "/api/backup/start", Origin="http://evil.example",
                                  **{"X-Strata-Token": "tok123"})[0], 403)
        self.assertEqual(self.ask("POST", "/api/backup/start", **{"X-Strata-Token": "tok123"})[0], 403)   # (no Origin)
        self.assertEqual(self.started, [])
        status, body = self.ask("POST", "/api/backup/start", Origin=here, **{"X-Strata-Token": "tok123"})
        self.assertEqual((status, json.loads(body)), (200, {"started": True}))
        self.assertEqual(self.started, [1])


if __name__ == "__main__":
    unittest.main(verbosity=2)
