# CMS_Proxy

Proxy C++ per bridge protocollo CMS/LRAD con architettura entity/event-driven.

Riceve datagrammi binari multicast, li converte in payload applicativi e coordina i flussi CMS e ACS tramite topic interni.

## Architettura (stato attuale)

Il progetto e organizzato attorno a entita attive. In particolare la logica CMS non e piu distribuita tra converter, sender e handler separati: vive dentro `CmsEntity`.

- `ProxyEngine`: orchestratore lifecycle di entita e handler ancora presenti.
- `CmsEntity`: ricezione multicast CMS, parsing header, conversione messaggi, publish sui topic, subscribe ai topic CMS, invio TCP verso LRAD, invio ACK UDP, periodic health status.
- `AcsEntity`: ricezione unicast ACS, publish eventi ACS, invio JSON verso destinazioni ACS e applicazione update di stato.
- `EventBus`: pub/sub thread-safe; ogni subscriber viene eseguito su thread separato.
- `SystemState`: stato condiviso thread-safe.

Schema logico:

```text
UdpMulticastReceiver (CmsEntity)
        |
        v
 CmsEntity::convertIncomingPacket()
        |
        v
      EventBus
   /      |      \
 ACS   STATE   CMS internal topics
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
3. Converte il messaggio in base al `messageId` direttamente dentro `CmsEntity`.
4. Pubblica su `EventBus`:
  - payload JSON verso ACS,
  - eventi di state update,
  - eventi CMS interni per dispatch e periodic processing.
5. `CmsEntity` si sottoscrive ai topic CMS che le servono e completa da sola invio TCP, ACK UDP e periodic unicast.
6. `AcsEntity` consuma i topic ACS in modo autonomo.

## Messaggi attualmente gestiti

Il dispatch interno di `CmsEntity` include al momento:

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

Se il server TCP non e attivo, il proxy resta operativo ma logga `connection refused` dal ramo TCP interno di `CmsEntity`.

## Roadmap breve

- completare la migrazione entity-centric anche per ACS, riducendo ulteriormente i handler esterni;
- valutare un envelope eventi piu snello per ridurre il numero di tipi CMS;
- test automatici su parser, dispatch e integrazione runtime.