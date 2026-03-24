#!/usr/bin/env python3
"""Invia un pacchetto di test al multicast listener del proxy.

Formato header comune (16 byte, big-endian):
  [0-3]  messageId   (uint32)
  [4-7]  (sender << 16) | payloadLen  (uint32)
  [8-11] timestampSec  (uint32)
  [12-15] timestampUsec (uint32)

Messaggi supportati (selezionabili con --message-id / -m):
  1679949825  -> CS_LRAS_change_configuration_order_INS
                 payload: actionId(4)=0 + lradId(2) + configuration(2)
  1679949826  -> CS_LRAS_cueing_order_cancellation_INS
                 payload: actionId(4)=0 + lradId(2)
    1679949827  -> CS_LRAS_cueing_order_INS
                                 payload: actionId(4)=0 + lradId(2) + cueingType(2) + cstn(4) + kinematics(36)

Uso:
  python scripts/send_test_packet.py -m 1679949825 --lrad-id 2 --configuration 1
  python scripts/send_test_packet.py -m 1679949826 --lrad-id 1
"""

import argparse
import socket
import struct
import time

# ---------------------------------------------------------------------------
# Registro messaggi: message_id -> (description, payload_builder)
# Il payload builder riceve (lrad_id, configuration) e restituisce bytes.
# actionId è sempre 0x00000000 (non esposto come flag).
# ---------------------------------------------------------------------------
MESSAGES = {
    1679949825: {
        "description": "CS_LRAS_change_configuration_order_INS",
        "builder":     lambda lrad_id, configuration: struct.pack(">IHH", 0, lrad_id, configuration),
    },
    1679949826: {
        "description": "CS_LRAS_cueing_order_cancellation_INS",
        "builder":     lambda lrad_id, configuration: struct.pack(">IH", 0, lrad_id),
    },
}


def build_cueing_order_payload(args) -> bytes:
    """Build payload for CS_LRAS_cueing_order_INS (48 bytes)."""
    payload = bytearray(48)

    now = time.time()
    seconds = int(now)
    microseconds = int((now - seconds) * 1_000_000)

    # Base fields
    struct.pack_into(">I", payload, 0, 0)  # action_id (fixed to 0)
    struct.pack_into(">H", payload, 4, args.lrad_id)
    struct.pack_into(">H", payload, 6, args.cueing_type)
    struct.pack_into(">I", payload, 8, args.cstn)

    # Time of validity
    struct.pack_into(">I", payload, 12, seconds)
    struct.pack_into(">I", payload, 16, microseconds)

    # Kinematics union header
    struct.pack_into(">H", payload, 20, args.kinematics_type)

    # Cartesian fields (offsets aligned to the spec table)
    # x @ payload[24], y @ payload[28], z @ payload[32]
    struct.pack_into(">f", payload, 24, float(args.x))
    struct.pack_into(">f", payload, 28, float(args.y))
    if args.kinematics_type in (1, 2):
        struct.pack_into(">f", payload, 32, float(args.z))

    # Optional velocities for kinematics variants
    if args.kinematics_type == 1:
        struct.pack_into(">f", payload, 36, float(args.vx))
        struct.pack_into(">f", payload, 40, float(args.vy))
        struct.pack_into(">f", payload, 44, float(args.vz))
    elif args.kinematics_type == 3:
        struct.pack_into(">f", payload, 32, float(args.vx))
        struct.pack_into(">f", payload, 36, float(args.vy))

    return bytes(payload)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def build_header(message_id: int, payload: bytes, sender: int = 2) -> bytes:
    payload_len    = len(payload)
    timestamp_sec  = int(time.time())
    timestamp_usec = int((time.time() - timestamp_sec) * 1_000_000)
    return struct.pack(
        ">IIII",
        message_id,
        (sender << 16) | (payload_len & 0xFFFF),
        timestamp_sec,
        timestamp_usec,
    )

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Invia un messaggio binario al proxy UDP.",
        formatter_class=argparse.RawTextHelpFormatter,
    )

    # Selezione messaggio tramite ID numerico
    parser.add_argument(
        "-m", "--message-id",
        type=lambda x: int(x, 0),
        default=1679949825,
        help=(
            "ID numerico del messaggio da inviare:\n"
            "  1679949825  -> CS_LRAS_change_configuration_order_INS\n"
            "  1679949826  -> CS_LRAS_cueing_order_cancellation_INS\n"
            "  1679949827  -> CS_LRAS_cueing_order_INS\n"
            "(default: 1679949825)"
        ),
    )

    # Parametri di rete
    parser.add_argument("--group",  default="127.0.0.1", help="Indirizzo di destinazione (default: 127.0.0.1)")
    parser.add_argument("--port",   type=int, default=12345, help="Porta UDP (default: 12345)")
    parser.add_argument("--ttl",    type=int, default=1,     help="TTL multicast (ignorato per localhost)")
    parser.add_argument("--sender", type=int, default=2,     help="Sender Id nell'header (default: 2)")

    # Campi payload
    parser.add_argument("--lrad-id",       type=int, default=1, choices=[1, 2],
                        help="LRAD Id (1=LRAD1, 2=LRAD2, default: 1)")
    parser.add_argument("--configuration", type=lambda x: int(x, 0), default=0,
                        help="Configuration (uint16, solo per 1679949825, default: 0)")

    # Campi payload per 1679949827
    parser.add_argument("--cueing-type", type=int, default=1, choices=[1, 2],
                        help="Cueing Type (1=Position, 2=CST, solo per 1679949827)")
    parser.add_argument("--cstn", type=lambda x: int(x, 0), default=1,
                        help="Combat System Track Number (solo per 1679949827)")
    parser.add_argument("--kinematics-type", type=int, default=2,
                        choices=[1, 2, 3, 4],
                        help="Kinematics type (1/2/3/4 cartesiani supportati nello script)")
    parser.add_argument("--x", type=float, default=0.0, help="X (m) per 1679949827")
    parser.add_argument("--y", type=float, default=0.0, help="Y (m) per 1679949827")
    parser.add_argument("--z", type=float, default=0.0, help="Z (m) per 1679949827 (solo type 1/2)")
    parser.add_argument("--vx", type=float, default=0.0, help="Vx (m/s) opzionale")
    parser.add_argument("--vy", type=float, default=0.0, help="Vy (m/s) opzionale")
    parser.add_argument("--vz", type=float, default=0.0, help="Vz (m/s) opzionale")

    args = parser.parse_args()

    supported_ids = sorted(set(MESSAGES.keys()) | {1679949827})
    if args.message_id not in supported_ids:
        known = ', '.join(str(k) for k in supported_ids)
        parser.error(f"message-id {args.message_id} non riconosciuto. ID supportati: {known}")

    if args.message_id == 1679949827:
        msg_info = {"description": "CS_LRAS_cueing_order_INS"}
        payload = build_cueing_order_payload(args)
    else:
        msg_info = MESSAGES[args.message_id]
        payload = msg_info["builder"](args.lrad_id, args.configuration)
    header   = build_header(args.message_id, payload, sender=args.sender)
    packet   = header + payload

    print(f"Messaggio : {msg_info['description']} (id={args.message_id})")
    print(f"Bytes     : {packet.hex(' ').upper()}")
    print(f"Lunghezza : {len(packet)} byte")
    print(f"Invio a   : {args.group}:{args.port}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if args.group != "127.0.0.1":
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, struct.pack("b", args.ttl))
    sock.sendto(packet, (args.group, args.port))
    sock.close()
    print("Pacchetto inviato.")


if __name__ == "__main__":
    main()
