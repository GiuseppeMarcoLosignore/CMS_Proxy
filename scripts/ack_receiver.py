#!/usr/bin/env python3
"""Riceve e decodifica pacchetti multicast dal proxy.

Header comune (16 byte, big-endian):
  [0-3]   messageId      uint32
  [4-7]   payloadLength  uint32
  [8-11]  riservato      uint32
  [12-15] riservato      uint32

Payload decodificato per LRAS_CS_ack_INS (messageId=576879045, 12 byte):
  [16-19] action_id         uint32
  [20-23] source_message_id uint32
  [24-25] ack_nack          uint16  (1=Ack accettato, 2=Nack non eseguito)
  [26-27] nack_reason       uint16  (0=No statement, 2=Parametri errati,
                                     3=Stato sistema errato, 4=Sistema non pronto,
                                     5=Sistema non utilizzabile, 6=Cueing in blind arc)

Per tutti gli altri msgId viene stampato solo l'header e il payload grezzo.

Uso:
  python scripts/ack_receiver.py
  python scripts/ack_receiver.py --group 226.1.1.43 --port 55010 --iface 0.0.0.0
"""

import argparse
import signal
import socket
import struct
import sys
import time

MSG_ID_LRAS_CS_ACK_INS = 576978945
EXPECTED_TOTAL_SIZE = 28

ACK_NACK_LABELS = {
    1: "ACK accettato",
    2: "NACK non eseguito",
}

NACK_REASON_LABELS = {
    0: "No statement",
    2: "Parametri errati",
    3: "Stato del sistema errato",
    4: "Sistema non pronto",
    5: "Sistema non utilizzabile",
    6: "Cueing in blind arc",
}

SOURCE_MESSAGE_LABELS = {
    1679949825: "CS_LRAS_change_configuration_order_INS",
    1679949826: "CS_LRAS_cueing_order_cancellation_INS",
    1679949827: "CS_LRAS_cueing_order_INS",
    1679949828: "CS_LRAS_emission_control_INS",
    1679949829: "CS_LRAS_emission_mode_INS",
    1679949830: "CS_LRAS_inhibition_sectors_INS",
    1679949831: "CS_LRAS_joystick_control_lrad_1_INS",
    1679949832: "CS_LRAS_joystick_control_lrad_2_INS",
    1679949833: "CS_LRAS_recording_command_INS",
    1679949834: "CS_LRAS_request_engagement_capability_INS",
    1679949835: "CS_LRAS_request_full_status_INS",
    1679949836: "CS_LRAS_request_message_table_INS",
    1679949837: "CS_LRAS_request_software_version_INS",
    1679949838: "CS_LRAS_request_thresholds_INS",
    1679949839: "CS_LRAS_request_translation_INS",
    1679949840: "CS_LRAS_video_tracking_command_INS",
    1679949841: "CS_LRAS_request_emission_mode_INS",
    1679949842: "CS_LRAS_request_installation_data_INS",
    1684229565: "CS_MULTI_health_status_INS",
    1684229569: "CS_MULTI_update_cst_kinematics_INS",
}


# Registry dei nomi noti per messageId
MESSAGE_ID_LABELS = {
    MSG_ID_LRAS_CS_ACK_INS: "LRAS_CS_ack_INS",
}


def decode_header(data: bytes) -> tuple | None:
    """Decodifica l'header comune (16 byte). Ritorna (msg_id, payload_len, res1, res2) o None."""
    if len(data) < 16:
        print("  [!] Pacchetto troppo corto per l'header (attesi >= 16 byte)")
        return None
    return struct.unpack_from(">IIII", data, 0)


def decode_ack_payload(data: bytes, payload_len: int):
    """Decodifica il payload specifico di LRAS_CS_ack_INS."""
    if len(data) < 16 + payload_len:
        print(f"  [!] Dati insufficienti per il payload (attesi {16 + payload_len}, ricevuti {len(data)})")
        return
    if payload_len < 12:
        print(f"  [!] Payload troppo corto (attesi >= 12, presenti {payload_len})")
        return

    action_id, source_msg_id, ack_nack, nack_reason = struct.unpack_from(">IIHH", data, 16)
    source_label = SOURCE_MESSAGE_LABELS.get(source_msg_id, f"SCONOSCIUTO ({source_msg_id})")
    ack_label = ACK_NACK_LABELS.get(ack_nack, f"SCONOSCIUTO ({ack_nack})")
    reason_label = NACK_REASON_LABELS.get(nack_reason, f"SCONOSCIUTO ({nack_reason})")

    print(f"  Payload:")
    print(f"    action_id         = {action_id}")
    print(f"    source_message_id = {source_msg_id}  -> {source_label}")
    print(f"    ack_nack          = {ack_nack}  -> {ack_label}")
    if ack_nack == 2:
        print(f"    nack_reason       = {nack_reason}  -> {reason_label}")


def decode_packet(data: bytes, src_addr: tuple):
    ts = time.strftime("%H:%M:%S")
    print(f"\n{'='*56}")
    print(f"[{ts}] Pacchetto da {src_addr[0]}:{src_addr[1]}  ({len(data)} byte)")
    print(f"  RAW: {data.hex()}")

    header = decode_header(data)
    if header is None:
        return
    msg_id, payload_len, _res1, _res2 = header

    msg_label = MESSAGE_ID_LABELS.get(msg_id, f"SCONOSCIUTO ({msg_id})")
    print(f"  Header:")
    print(f"    messageId      = {msg_id}  -> {msg_label}")
    print(f"    payloadLength  = {payload_len}")

    if msg_id == MSG_ID_LRAS_CS_ACK_INS:
        decode_ack_payload(data, payload_len)
    else:
        # Payload grezzo per messaggi non ancora decodificati
        payload = data[16:16 + payload_len] if len(data) >= 16 + payload_len else data[16:]
        if payload:
            print(f"  Payload (raw): {payload.hex()}")
        else:
            print(f"  Payload: vuoto")


def main():
    parser = argparse.ArgumentParser(
        description="Listener multicast per pacchetti LRAS_CS_ack_INS"
    )
    parser.add_argument("--group", default="226.1.1.43", help="Gruppo multicast (default: 226.1.1.43)")
    parser.add_argument("--port",  type=int, default=55010, help="Porta UDP (default: 55010)")
    parser.add_argument("--iface", default="0.0.0.0", help="Interfaccia locale (default: 0.0.0.0)")
    parser.add_argument("--bufsize", type=int, default=4096)
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(1.0)
    sock.bind(("", args.port))

    mreq = struct.pack("4s4s",
                       socket.inet_aton(args.group),
                       socket.inet_aton(args.iface))
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    print(f"[*] In ascolto su {args.group}:{args.port} (iface={args.iface})")
    print("Premere CTRL-C per uscire...\n")

    running = True

    def _shutdown(sig, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, _shutdown)

    while running:
        try:
            data, addr = sock.recvfrom(args.bufsize)
            decode_packet(data, addr)
        except socket.timeout:
            continue
        except OSError:
            break

    print("\n[*] Arresto.")
    sock.close()


if __name__ == "__main__":
    main()
