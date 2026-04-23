# CMS_Proxy

Proxy C++20 event-driven per integrazione CMS, ACS e NAVS.

Il sistema riceve pacchetti binari, esegue parsing e conversione, pubblica eventi interni e inoltra output JSON/binary verso i canali configurati.

## Architettura attuale

L'architettura in uso e completamente entity-based.

- `ProxyEngine`: orchestratore lifecycle (start/stop) delle entity registrate.
- `CmsEntity`: ricezione UDP multicast CMS, parsing header/payload, conversione per `messageId`, publish su `EventBus`, invio ACK/status multicast LRAS periodici.
- `AcsEntity`: ricezione ACS (UDP multicast + TCP unicast), inoltro verso destinazioni TCP/multicast ACS e applicazione aggiornamenti di stato da payload JSON.
- `NavsEntity`: ricezione UDP multicast NAVS, parsing header e publish su topic configurabili.
- `SystemState`: storage thread-safe dello stato (per LRAD e sistema), aggiornato da eventi topic-based.
- `EventBus`: pub/sub thread-safe con dispatch asincrono dei subscriber.

Schema logico:

```text
CMS UDP in ---> CmsEntity ---+
                            |
ACS UDP/TCP in -> AcsEntity |--> EventBus --> SystemState
                            |
NAVS UDP in --> NavsEntity -+

Eventi in uscita principali:
- JSON ACS verso TCP/multicast ACS
- ACK e status LRAS su multicast dedicato
```

## Flusso runtime

1. `main` carica `config/network_config.json`.
2. Vengono create e avviate `CmsEntity`, `AcsEntity`, `NavsEntity` tramite `ProxyEngine`.
3. `CmsEntity` converte i messaggi CMS supportati e pubblica:
   - evento `acs.outgoing_json` per routing ACS,
   - evento topic-specific per dispatch applicativo,
   - eventuali `StateUpdate` per aggiornare `SystemState`.
4. `AcsEntity` inoltra i payload alle destinazioni ACS configurate e aggiorna lo stato quando riceve JSON con campi di update.
5. `CmsEntity` emette periodicamente pacchetti di stato LRAS su multicast (`226.1.1.43:55010`).

## Configurazione

File: `config/network_config.json`

Esempio coerente con il codice attuale:

```json
{
  "cms": {
    "multicast_group": "226.1.1.30",
    "multicast_port": 55000,
    "unicast_relays": []
  },
  "acs": {
    "multicast_group": "225.0.0.25",
    "multicast_port": 2525,
    "multicast_tx": {
      "group": "225.0.0.25",
      "port": 2525
    },
    "tcp_unicast": {
      "listen_ip": "127.0.0.1",
      "listen_port": 56101
    },
    "destinations": [
      { "id": 1, "ip": "127.0.0.1", "port": 9000 },
      { "id": 2, "ip": "127.0.0.1", "port": 9000 }
    ]
  },
  "navs": {
    "multicast_group": "239.192.44.173",
    "multicast_port": 55437,
    "topic_bindings": []
  }
}
```

Note:

- tutte le porte validate nel range `1..65535`;
- la sezione `cms` e obbligatoria;
- `acs` e `navs` sono opzionali ma, se presenti, vengono attivate.

## Messaggi CMS gestiti

`CmsEntity` gestisce i seguenti `messageId` in ingresso:

- `1679949825` `CS_LRAS_change_configuration_order_INS`
- `1679949826` `CS_LRAS_cueing_order_cancellation_INS`
- `1679949827` `CS_LRAS_cueing_order_INS`
- `1679949828` `CS_LRAS_emission_control_INS`
- `1679949829` `CS_LRAS_emission_mode_INS`
- `1679949830` `CS_LRAS_inhibition_sectors_INS`
- `1679949831` `CS_LRAS_joystick_control_lrad_1_INS`
- `1679949832` `CS_LRAS_joystick_control_lrad_2_INS`
- `1679949833` `CS_LRAS_recording_command_INS`
- `1679949834` `CS_LRAS_request_engagement_capability_INS`
- `1679949835` `CS_LRAS_request_full_status_INS`
- `1679949836` `CS_LRAS_request_message_table_INS`
- `1679949837` `CS_LRAS_request_software_version_INS`
- `1679949838` `CS_LRAS_request_thresholds_INS`
- `1679949839` `CS_LRAS_request_translation_INS`
- `1679949840` `CS_LRAS_video_tracking_command_INS`
- `1679949841` `CS_LRAS_request_emission_mode_INS`
- `1679949842` `CS_LRAS_request_installation_data_INS`
- `1684229565` `CS_MULTI_health_status_INS`
- `1684229569` `CS_MULTI_update_cst_kinematics_INS`

## Output LRAS generati da CmsEntity

- `LRAS_CS_ack_INS` (`576879045`), multicast su `226.1.1.43:55010`
- `LRAS_CS_lrad_1_status_INS` (`576978949`), periodico
- `LRAS_CS_lrad_2_status_INS` (`576978950`), periodico
- `LRAS_MULTI_full_status_v2_INS` (`576913411`), periodico
- `LRAS_MULTI_health_status_INS` (`576913410`), periodico

## Build

Prerequisiti:

- CMake >= 3.15
- compilatore C++20
- Boost (`system`)
- `nlohmann_json`
- toolchain vcpkg configurata nel progetto

Comandi (Windows):

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/Users/HP840G8/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug
```

## Esecuzione

1. Avvia un receiver TCP di test (esempio su destinazione ACS `127.0.0.1:9000`).

```powershell
python scripts/tcp_receiver.py --host 127.0.0.1 --port 9000
```

2. Avvia il proxy.

```powershell
.\build\Debug\CMS_Proxy.exe
```

3. Invia un pacchetto CMS di test.

```powershell
python scripts/send_test_packet.py --group 226.1.1.30 --port 55000
```