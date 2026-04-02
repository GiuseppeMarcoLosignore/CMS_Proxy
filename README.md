# CMS_Proxy

Proxy C++ per bridge protocollo CMS/LRAD con architettura entity/event-driven.

Riceve datagrammi binari multicast, fa dispatch per header, converte i payload e pubblica eventi interni; handler separati gestiscono invio TCP, ACK UDP e aggiornamento stato.

## Architettura (stato attuale)

Il progetto e stato rifattorizzato da pipeline monolitica a orchestrazione per entita.

- `ProxyEngine`: orchestratore lifecycle (start/stop) di entita e handler.
- `CmsEntity`: entita CMS in ascolto multicast su thread dedicato.
- `EventBus`: pub/sub thread-safe; ogni subscriber viene eseguito su thread separato.
- `TcpSendEventHandler`: invio TCP verso LRAD in base a `destinationLradId`.
- `AckSendEventHandler`: invio ACK/NACK binario via UDP.
- `StateUpdateEventHandler`: applicazione `StateUpdate` su `SystemState`.
- `BinaryConverter`: parsing header, dispatch `messageId`, generazione messaggi applicativi e delta di stato.

Schema logico:

```text
UdpMulticastReceiver (CmsEntity)
        |
        v
  BinaryConverter::convert()
        |
        v
      EventBus
   /      |      \
 TCP    ACK     STATE
handler handler  handler
```

## Configurazione (nuovo formato)

La rete ora e configurata per entita/handler nel file `config/network_config.json`.

```json
{
  "cms": {
    "listen_ip": "127.0.0.1",
    "multicast_group": "226.1.1.30",
    "multicast_port": 55000,
    "handlers": {
      "tcp_send": {
        "lrad_destinations": [
          { "id": 1, "ip": "127.0.0.1", "port": 9000 },
          { "id": 2, "ip": "127.0.0.1", "port": 9000 }
        ]
      },
      "ack_send": {
        "target_ip": "226.1.1.43",
        "target_port": 55010
      }
    }
  }
}
```

Note:

- tutti i campi sono obbligatori;
- porte valide solo nel range `1..65535`;
- `lrad_destinations` non puo essere vuoto.

## Flusso runtime

1. `CmsEntity` riceve un pacchetto UDP multicast.
2. Estrae `source_message_id` dall'header.
3. Invoca `BinaryConverter::convert()`.
4. Pubblica su `EventBus`:
   - eventi di outgoing packet per TCP,
   - eventi ACK (ack-only o post-send),
   - eventi state update.
5. Gli handler consumano in modo indipendente i rispettivi topic.

## Messaggi attualmente gestiti

Il dispatch del `BinaryConverter` include al momento:

- `1679949825` (`CS_LRAS_change_configuration_order_INS`)
- `1679949826` (`CS_LRAS_cueing_order_cancellation_INS`, ack-only)
- `1679949827` (`CS_LRAS_cueing_order_INS`)
- `1679949828` (`CS_LRAS_emission_control_INS`)

## Formato ACK/NACK

Pacchetto binario da 28 byte (big-endian):

- header 16 byte con `messageId = 576879045` (`LRAS_CS_ack_INS`),
- payload 12 byte con `action_id`, `source_message_id`, `ack_nack`, `nack_reason`.

Mappatura `nack_reason` principale:

- `0`: errore non classificato / assente
- `2`: parametri errati o LRAD non configurato
- `3`: stato operazione non valido
- `4`: sistema non pronto
- `5`: errore trasporto/connessione

## Build

Prerequisiti:

- CMake >= 3.15
- compilatore C++20
- Boost (`system`)
- `nlohmann_json`
- vcpkg toolchain (come nel progetto)

Configurazione consigliata su Windows:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/Users/HP840G8/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug
```

## Esecuzione

1. Avvia un TCP receiver su una destinazione configurata (esempio `127.0.0.1:9000`).

```powershell
python scripts/tcp_receiver.py --host 127.0.0.1 --port 9000
```

2. Avvia il proxy.

```powershell
.\build\Debug\CMS_Proxy.exe
```

3. Invia un pacchetto di test verso il multicast CMS.

```powershell
python scripts/send_test_packet.py --group 226.1.1.30 --port 55000
```

Se il server TCP non e attivo, il proxy resta operativo ma logga `connection refused` sul ramo `TcpSendEventHandler`.

## Roadmap breve

- aggiunta entita ACS e NAV sullo stesso pattern;
- migrazione del `BinaryConverter` da output JSON a payload tipizzati (event payload structs);
- test automatici su parser/dispatch/handler.