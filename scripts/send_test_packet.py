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


# Identificatore fissato per inviare esattamente un CS_LRAS_change_configuration_order_INS.
MESSAGE_ID_CS_LRAS_CHANGE_CONFIGURATION_ORDER_INS = 1679949825


def build_test_packet(sender=2, payload=None):
    # payload: bytes
    if payload is None:
        # default payload (big endian)
        # actionId=0x00000003, lradId=0x0002, configuration=0x0001
        payload = struct.pack(
            ">IHH",  # big-endian: uint32, uint16, uint16
            0x00000000,
            0x0002,
            0x0000,
        )

    payload_len = len(payload)
    timestamp_sec = int(time.time())
    timestamp_usec = int((time.time() - timestamp_sec) * 1_000_000)

    header = struct.pack(
        ">I I I I",  # all big-endian uint32
        MESSAGE_ID_CS_LRAS_CHANGE_CONFIGURATION_ORDER_INS,
        (sender << 16) | (payload_len & 0xFFFF),
        timestamp_sec,
        timestamp_usec,
    )

    return header + payload


def main():
    parser = argparse.ArgumentParser(description="Invia un messaggio CS_LRAS_change_configuration_order_INS al proxy UDP multicast")
    parser.add_argument("--group", default="127.0.0.1", help="Indirizzo di destinazione (localhost per test)")
    parser.add_argument("--port", type=int, default=12345, help="Porta UDP di destinazione")
    parser.add_argument("--ttl", type=int, default=1, help="TTL del pacchetto (ignorato per localhost)")
    parser.add_argument("--sender", type=int, default=2, help="Sender Id del pacchetto")
    args = parser.parse_args()

    packet = build_test_packet(sender=args.sender)
    print(f"Invio pacchetto CS_LRAS_change_configuration_order_INS di {len(packet)} byte a {args.group}:{args.port}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Per localhost non serve TTL multicast
    if args.group != "127.0.0.1":
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, struct.pack("b", args.ttl))
    sock.sendto(packet, (args.group, args.port))
    sock.close()
    print("Pacchetto inviato.")


if __name__ == "__main__":
    main()
