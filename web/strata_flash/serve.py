#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
"""Serve the Strata flasher to this computer only, at http://127.0.0.1:8000/ (Web MIDI's SysEx needs a secure
context, and a loopback address is one). Open that address in Chrome or Edge; Ctrl+C stops the server.

  python serve.py [PORT]            (Windows: py serve.py)
"""
import functools
import http.server
import os
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".fwsc": "application/octet-stream", ".md": "text/plain; charset=utf-8",
                      ".txt": "text/plain; charset=utf-8", "": "text/plain; charset=utf-8"}

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")   # (a rebuilt package is never an old one from the cache)
        super().end_headers()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    here = os.path.dirname(os.path.abspath(__file__))
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=here)) as srv:
        print(f"Strata flasher: open http://127.0.0.1:{port}/ in Chrome or Edge (this computer only). Ctrl+C stops.")
        try:
            srv.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
