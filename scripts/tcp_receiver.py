#!/usr/bin/env python3
"""Semplice server TCP che stampa i bytes ricevuti.

Questo script è utile per vedere cosa il proxy invia dopo la conversione.

Uso:
  1) Avvia questo script (listener su 127.0.0.1:9000).
  2) Avvia il proxy nel repo (che inoltra a 127.0.0.1:9000).
  3) Manda un messaggio di test con scripts/send_test_packet.py.

Il proxy dovrebbe connettersi e inviare i bytes convertiti.
"""

import argparse
import socket
import struct


def dump_packet(data: bytes):
    print(f"Ricevuti {len(data)} byte")
    print("Raw:", data.hex())

    if len(data) >= 24:
        # Interpretazione in ordine host (little-endian su sistemi x86)
        hdr = struct.unpack_from("<I I I I", data, 0)
        msg_id = hdr[0]
        sender_len = hdr[1]
        sender = sender_len >> 16
        length = sender_len & 0xFFFF
        ts_sec = hdr[2]
        ts_usec = hdr[3]
        print("Header (host order):")
        print(f"  messageId={msg_id}, sender={sender}, length={length}, ts={ts_sec}.{ts_usec:06d}")

        if len(data) >= 16 + length:
            payload = data[16:16+length]
            if len(payload) >= 8:
                action_id, lrad_id, config = struct.unpack_from("<I H H", payload, 0)
                print("Payload (host order):")
                print(f"  actionId=0x{action_id:08X}, lradId=0x{lrad_id:04X}, config=0x{config:04X}")


def main():
    parser = argparse.ArgumentParser(description="Ricevitore TCP di prova per il proxy.")
    parser.add_argument("--host", default="127.0.0.1", help="Indirizzo su cui ascoltare")
    parser.add_argument("--port", type=int, default=9000, help="Porta su cui ascoltare")
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((args.host, args.port))
        s.listen(1)
        print(f"In ascolto su {args.host}:{args.port}... (CTRL-C per uscire)")
        conn, addr = s.accept()
        with conn:
            print(f"Connessione stabilita da {addr}")
            while True:
                data = conn.recv(4096)
                if not data:
                    break
                dump_packet(data)


if __name__ == "__main__":
    main()
