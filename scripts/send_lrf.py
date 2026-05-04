#!/usr/bin/env python3
"""Invia il messaggio LRF a 127.0.0.1:56101 via TCP unicast a 1 Hz."""

import socket
import json
import time
import argparse
import random

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 56101

def build_message(mode: str) -> bytes:
    msg = {
        "header": "LRF",
        "type": "CMD",
        "sender": "ACS",
        "param": {
            "mode": mode,
            "value": str(round(random.uniform(0.0, 20.0), 2))
        }
    }
    return (json.dumps(msg, separators=(',', ':')) + "\n").encode('utf-8')


def run(host: str, port: int, mode: str, rate_hz: float):
    interval = 1.0 / rate_hz
    payload = build_message(mode)

    print(f"Connessione a {host}:{port} ...")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        print(f"Connesso. Invio LRF mode={mode} a {rate_hz} Hz. Ctrl+C per interrompere.")
        try:
            while True:
                payload = build_message(mode)
                sock.sendall(payload)
                print(f"  >> {payload.decode()}")
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\nInterrotto dall'utente.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Invia messaggi LRF in TCP unicast.")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"IP destinazione (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"Porta destinazione (default: {DEFAULT_PORT})")
    parser.add_argument("--mode", choices=["ON", "OFF"], default="ON", help="Modalità LRF: ON o OFF (default: ON)")
    parser.add_argument("--rate", type=float, default=1.0, help="Frequenza di invio in Hz (default: 1.0)")
    args = parser.parse_args()

    run(args.host, args.port, args.mode, args.rate)
