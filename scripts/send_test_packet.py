#!/usr/bin/env python3
"""Invia un pacchetto di test al multicast listener del proxy.

Questo script costruisce un messaggio nel formato atteso da BinaryConverter:
- Header (16 byte): messageId, sender+length, timestampSec, timestampUsec (tutti big-endian)
- Payload (8 byte): actionId (4), lradId (2), configuration (2) (tutti big-endian)

Per usarlo:
  1) Avvia il proxy (binario compilato) da questo repo.
  2) Avvia un TCP server di ricezione (vedi scripts/tcp_receiver.py) su 127.0.0.1:9000.
  3) Esegui questo script: python scripts/send_test_packet.py

Il proxy riceverà il pacchetto UDP multicast e lo inoltrerà (dopo conversione) via TCP.
"""

import argparse
import socket
import struct
import time


def build_test_packet(message_id=1, sender=2, payload=None):
    # payload: bytes
    if payload is None:
        # default payload (big endian)
        # actionId=0xAABBCCDD, lradId=0x1234, configuration=0x5678
        payload = struct.pack(
            ">IHH",  # big-endian: uint32, uint16, uint16
            0xAABBCCDD,
            0x1234,
            0x5678,
        )

    payload_len = len(payload)
    timestamp_sec = int(time.time())
    timestamp_usec = int((time.time() - timestamp_sec) * 1_000_000)

    header = struct.pack(
        ">I I I I",  # all big-endian uint32
        message_id,
        (sender << 16) | (payload_len & 0xFFFF),
        timestamp_sec,
        timestamp_usec,
    )

    return header + payload


def main():
    parser = argparse.ArgumentParser(description="Invia un messaggio di test al proxy UDP multicast")
    parser.add_argument("--group", default="239.0.0.1", help="Indirizzo multicast di destinazione")
    parser.add_argument("--port", type=int, default=12345, help="Porta UDP di destinazione")
    parser.add_argument("--ttl", type=int, default=1, help="TTL del pacchetto multicast")
    parser.add_argument("--message-id", type=int, default=1, help="MessageId del pacchetto")
    parser.add_argument("--sender", type=int, default=2, help="Sender Id del pacchetto")
    args = parser.parse_args()

    packet = build_test_packet(message_id=args.message_id, sender=args.sender)
    print(f"Invio pacchetto di {len(packet)} byte a {args.group}:{args.port}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, struct.pack("b", args.ttl))
    sock.sendto(packet, (args.group, args.port))
    sock.close()
    print("Pacchetto inviato.")


if __name__ == "__main__":
    main()
