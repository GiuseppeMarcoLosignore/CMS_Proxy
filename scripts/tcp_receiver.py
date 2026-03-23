#!/usr/bin/env python3
import argparse
import socket
import struct
import json
import threading
import sys
import time

def dump_packet(data: bytes):
    if not data:
        return
    print(f"\n{'='*40}")
    print(f"RICEVUTI {len(data)} BYTE")
    print(f"RAW (hex): {data.hex()}")
    
    # 1. Prova JSON
    try:
        json_str = data.decode('utf-8')
        json_data = json.loads(json_str)
        print("\n--- FORMATO: JSON ---")
        print(json.dumps(json_data, indent=2))
        return
    except (UnicodeDecodeError, json.JSONDecodeError):
        pass

    # 2. Prova Binario (Header 16 bytes + Payload)
    if len(data) >= 16:
        print("\n--- FORMATO: BINARIO ---")
        hdr = struct.unpack_from("<I I I I", data, 0)
        msg_id, sender_len, ts_sec, ts_usec = hdr
        sender = sender_len >> 16
        length = sender_len & 0xFFFF
        
        print(f"HEADER: ID={msg_id}, Sender={sender}, Len={length}, TS={ts_sec}.{ts_usec:06d}")

        if len(data) >= 16 + length:
            payload = data[16:16+length]
            if len(payload) >= 8:
                action_id, lrad_id, config = struct.unpack_from("<I H H", payload, 0)
                print(f"PAYLOAD: Action=0x{action_id:08X}, Lrad=0x{lrad_id:04X}, Config=0x{config:04X}")
    else:
        print("\n--- FORMATO: SCONOSCIUTO (Troppo corto) ---")

def client_handler(conn, addr):
    """Gestisce la singola connessione in un thread dedicato."""
    with conn:
        print(f"\n[+] Nuova connessione da {addr}")
        try:
            while True:
                data = conn.recv(4096)
                if not data:
                    break
                # Processiamo i dati immediatamente
                dump_packet(data)
        except Exception as e:
            print(f"\n[!] Errore durante la ricezione da {addr}: {e}")
        finally:
            print(f"\n[-] Connessione chiusa con {addr}")

def server_worker(host, port):
    """Thread che accetta le connessioni in entrata."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((host, port))
            s.listen(5)
            # Rendiamo il socket non bloccante per poter controllare il flag di uscita
            s.settimeout(1.0) 
            
            print(f"[*] Server in ascolto su {host}:{port}")
            
            while not stop_event.is_set():
                try:
                    conn, addr = s.accept()
                    # Avviamo un thread per ogni client per non bloccare l'ascolto di altri messaggi
                    t = threading.Thread(target=client_handler, args=(conn, addr), daemon=True)
                    t.start()
                except socket.timeout:
                    continue
    except Exception as e:
        print(f"\n[!] Errore del server: {e}")

stop_event = threading.Event()

def main():
    parser = argparse.ArgumentParser(description="TCP Debug Receiver")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9000)
    args = parser.parse_args()

    # Avviamo il thread del server
    srv_thread = threading.Thread(target=server_worker, args=(args.host, args.port), daemon=True)
    srv_thread.start()

    print("Premere CTRL-C per arrestare il server...")
    
    try:
        while True:
            time.sleep(0.5) # Il thread principale non fa nulla, aspetta solo il segnale
    except KeyboardInterrupt:
        print("\n[!] Spegnimento in corso...")
        stop_event.set()
        sys.exit(0)

if __name__ == "__main__":
    main()