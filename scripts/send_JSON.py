#!/usr/bin/env python3
"""Invia messaggi JSON generici a 127.0.0.1:56101 via TCP unicast."""

import socket
import json
import time
import argparse
import random

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 56101

# Message templates with their parameter ranges
MESSAGE_TEMPLATES = {
    "ERROR": {
        "header": "ERROR",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "code": lambda: str(random.randint(0, 255)),
            "message": lambda: random.choice(["OK", "WARNING", "CRITICAL"])
        }
    },
    "AUDIO": {
        "header": "AUDIO",
        "type": "CMD",
        "sender": "ACS",
        "param": {
            "gain": lambda: round(random.uniform(0.0, 20.0), 2),
            "mute": lambda: random.choice([True, False])
        }
    },
    "LAD": {
        "header": "LAD",
        "type": "CMD",
        "sender": "ACS",
        "param": {
            "mode": lambda: random.choice(["ON", "OFF", "STROBE"])
        }
    },
    "SEARCHLIGHT": {
        "header": "SEARCHLIGHT",
        "type": "CMD",
        "sender": "ACS",
        "param": {
            "power": lambda: str(random.randint(0, 100)),
            "focus": lambda: str(random.randint(0, 100)),
            "mode": lambda: random.choice(["ON", "OFF", "STROBE"])
        }
    },
    "LRF": {
        "header": "LRF",
        "type": "CMD",
        "sender": "ACS",
        "param": {
            "mode": lambda: random.choice(["ON", "OFF"]),
            "value": lambda: round(random.uniform(0.0, 20.0), 2)
        }
    },
    "STABIL": {
        "header": "STABIL",
        "type": "CMD",
        "sender": "ACS",
        "param": {
            "mode": lambda: random.choice(["ON", "OFF"])
        }
    },
    "SHADOW": {
        "header": "SHADOW",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "enabled": lambda: random.choice([True, False]),
            "start": lambda: str(random.randint(0, 359)),
            "stop": lambda: str(random.randint(0, 359))
        }
    },
    "ZOOM": {
        "header": "ZOOM",
        "type": "CMD",
        "sender": "ACS",
        "param": {
            "id": lambda: random.choice(["HD", "TH"]),
            "value": lambda: round(random.uniform(1.0, 20.0), 2)
        }
    },
    "MASTER": {
        "header": "MASTER",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "mode": lambda: random.choice(["REQ", "RELEASE", "ACCEPT", "REFUSE"])
        }
    },
    "CONTEXT": {
        "header": "CONTEXT",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "master": lambda: random.choice(["CMS", "ACS"]),
            "arcAz": lambda: str(random.randint(0, 359)),
            "arcEl": lambda: str(random.randint(0, 90)),
            "safetySwitch": lambda: random.choice(["OPEN", "CLOSE"]),
            "inShadow": lambda: random.choice(["YES", "NO"]),
            "cms": lambda: random.choice(["CONNECTED", "DISCONNECTED"])
        }
    },
    "POSITION": {
        "header": "POSITION",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "goTo": lambda: random.choice(["YES", "NO"]),
            "az": lambda: str(round(random.uniform(0.0, 359.99), 2)),
            "el": lambda: str(round(random.uniform(-10.0, 90.0), 2))
        }
    },
    "DELTA": {
        "header": "DELTA",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "start": lambda: str(random.randint(0, 359)),
            "stop": lambda: str(random.randint(0, 359))
        }
    },
    "TRACKING": {
        "header": "TRACKING",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "mode": lambda: random.choice(["AUTO", "MANUAL", "OFF"]),
            "target": lambda: random.choice(["PERSON", "VEHICLE", "UNKNOWN"]),
            "classification": lambda: random.choice(["HOSTILE", "NEUTRAL", "FRIENDLY"])
        }
    },
    "CONFIG": {
        "header": "CONFIG",
        "type": "CMD",
        "sender": "ACS",
        "param": {
            "name": lambda: random.choice(["PROFILE_A", "PROFILE_B", "PROFILE_C"]),
            "direction": lambda: random.choice(["LEFT", "RIGHT"]),
            "hwAzLeft": lambda: str(random.randint(0, 359)),
            "hwAzRight": lambda: str(random.randint(0, 359)),
            "hwElLeft": lambda: str(random.randint(0, 90)),
            "hwElRight": lambda: str(random.randint(0, 90))
        }
    },
    "IMU": {
        "header": "IMU",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "roll": lambda: str(round(random.uniform(-180.0, 180.0), 2)),
            "pitch": lambda: str(round(random.uniform(-90.0, 90.0), 2)),
            "heading": lambda: str(round(random.uniform(0.0, 359.99), 2))
        }
    },
    "HOURS": {
        "header": "HOURS",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "atom": lambda: str(random.randint(0, 100000)),
            "light": lambda: str(random.randint(0, 100000)),
            "lad": lambda: str(random.randint(0, 100000)),
            "lrf": lambda: str(random.randint(0, 100000)),
            "ahd": lambda: str(random.randint(0, 100000)),
            "logSession": lambda: str(random.randint(0, 100000))
        }
    },
    "ALIVE": {
        "header": "ALIVE",
        "type": "STATUS",
        "sender": "ACS",
        "param": {
            "name": lambda: random.choice(["PORT", "STARBOARD"]),
            "ipAddress": lambda: f"192.168.1.{random.randint(1, 254)}",
            "state": lambda: random.choice(["ONLINE", "OFFLINE", "DEGRADED"]),
            "mode": lambda: random.choice(["ACTIVE", "STANDBY", "MAINTENANCE"]),
            "swVersion": lambda: f"{random.randint(1,3)}.{random.randint(0,9)}.{random.randint(0,99)}"
        }
    }
}


def build_message(message_type: str) -> bytes:
    """Build a JSON message by type, with random parameters."""
    if message_type not in MESSAGE_TEMPLATES:
        raise ValueError(f"Tipo di messaggio sconosciuto: {message_type}. "
                        f"Disponibili: {', '.join(MESSAGE_TEMPLATES.keys())}")
    
    template = MESSAGE_TEMPLATES[message_type]
    msg = {}
    
    for key, value in template.items():
        if key == "param":
            msg[key] = {k: v() for k, v in value.items()}
        else:
            msg[key] = value
    
    return (json.dumps(msg, separators=(',', ':')) + "\n").encode('utf-8')


def run(host: str, port: int, message_type: str, rate_hz: float):
    interval = 1.0 / rate_hz

    print(f"Connessione a {host}:{port} ...")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        print(f"Connesso. Invio messaggi {message_type} a {rate_hz} Hz. Ctrl+C per interrompere.")
        try:
            while True:
                payload = build_message(message_type)
                sock.sendall(payload)
                print(f"  >> {payload.decode().strip()}")
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\nInterrotto dall'utente.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Invia messaggi JSON generici via TCP unicast.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Tipi di messaggio disponibili:\n  " + 
               "\n  ".join(MESSAGE_TEMPLATES.keys())
    )
    parser.add_argument(
        "message_type",
        help="Tipo di messaggio da inviare (ERROR, AUDIO, LAD, SEARCHLIGHT, LRF, STABIL, SHADOW, ZOOM, MASTER, CONTEXT, POSITION, DELTA, TRACKING, CONFIG, IMU, HOURS, ALIVE)"
    )
    parser.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help=f"IP destinazione (default: {DEFAULT_HOST})"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"Porta destinazione (default: {DEFAULT_PORT})"
    )
    parser.add_argument(
        "--rate",
        type=float,
        default=1.0,
        help="Frequenza di invio in Hz (default: 1.0)"
    )
    
    args = parser.parse_args()
    run(args.host, args.port, args.message_type, args.rate)
