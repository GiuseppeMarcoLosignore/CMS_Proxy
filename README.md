# CMS_Proxy

Proxy C++ che riceve messaggi binari via UDP, li converte in JSON e li inoltra via TCP verso i sistemi LRAD. Per ogni messaggio processato genera un pacchetto ACK/NACK binario inviato via UDP a un endpoint dedicato.

## Obiettivo

Il progetto implementa una pipeline di bridging protocollo:

1. ricezione di datagrammi binari (big-endian) da rete UDP;
2. parsing e validazione dell'header (16 byte) e dispatch per `messageId`;
3. conversione del payload in uno o piu messaggi JSON applicativi;
4. routing TCP verso la destinazione corretta in base a `lradId`;
5. invio di ACK/NACK binario all'endpoint UDP configurato.

## Architettura

| Componente | Interfaccia | Responsabilita |
|---|---|---|
| `UdpMulticastReceiver` | `IReceiver` | Ascolto UDP asincrono e notifica callback |
| `BinaryConverter` | `IProtocolConverter` | Parsing header, dispatch per `messageId`, produzione JSON e `StateUpdate` |
| `TcpSender` | `ISender` | Risoluzione host, reconnessione automatica, invio TCP |
| `UdpAckSender` | `IAckSender` | Invio pacchetto ACK/NACK binario via UDP |
| `SystemState` | — | Stato globale thread-safe del sistema (snapshot/apply/applyBatch) |
| `ProxyEngine` | — | Orchestrazione del flusso, routing LRAD, gestione thread delivery, applicazione `StateUpdate` |
| `AppConfig` / `loadAppConfig()` | — | Caricamento configurazione da file JSON esterno |

Le interfacce astratte (`IReceiver`, `IProtocolConverter`, `ISender`, `IAckSender`) rendono ogni componente sostituibile e testabile separatamente.

### Gestione dello stato

`BinaryConverter::convert()` riceve uno `SystemStateSnapshot` (copia immutabile point-in-time dello stato) e restituisce un `ConversionResult` che include un `vector<StateUpdate>` con i delta da applicare. `ProxyEngine` applica il batch in modo atomico tramite `SystemState::applyBatch()` soltanto dopo che tutti gli invii TCP sono riusciti.

```
SystemState ──getSnapshot()──► BinaryConverter::convert()
                                       │
                               ConversionResult.state_updates
                                       │
                               ProxyEngine::applyBatch()
                                       │
                              SystemState (aggiornato)
```

Strutture principali (in `SystemState.hpp` / `IInterfaces.hpp`):

- `StateUpdate` — delta opzionali per un LRAD: `lradId`, `engaged`, `configuration`, `cueingStatus`, `online`, `audioEnabled`, `ladEnabled`, `lrfEnabled`, `inibithionSector1/2`, `swVersion`, `systemMode`.
- `SystemStateSnapshot` — copia immutabile: `systemMode`, `map<lradId → StateUpdate>`, `timestampMs`.
- `SystemState` — classe thread-safe con `getSnapshot()`, `apply()`, `applyBatch()`, `getLastUpdatedMs()`.

## Flusso di funzionamento

`main.cpp` crea due `io_context` separati:

- `rx_io_ctx`: usato dal receiver UDP (thread principale);
- `delivery_io_ctx`: eseguito in un `std::jthread` dedicato, gestisce l'invio TCP e ACK in modo asincrono.

Alla ricezione di un datagramma UDP:

1. la callback del receiver posta il lavoro su `delivery_io_ctx` tramite `boost::asio::post`;
2. `ProxyEngine::processPacket()` estrae il `source_message_id` (primi 4 byte, big-endian);
3. acquisisce uno `SystemStateSnapshot` da `SystemState` e invoca `BinaryConverter::convert(input, snapshot)`;
4. se `ConversionResult` e vuoto (messageId non supportato o payload malformato), il pacchetto viene scartato senza ACK;
5. se il flag `ack_only` e true (es. cancellazione ordine), viene costruito solo l'ACK di successo senza tentare l'invio TCP;
6. altrimenti, per ogni `RawPacket` nell'output: cerca la destinazione LRAD nella tabella di routing, esegue `TcpSender::send()`, costruisce l'ACK/NACK tramite `ack_builder` e lo invia via `UdpAckSender`;
7. se tutti gli invii sono riusciti, applica atomicamente il batch di `StateUpdate` tramite `SystemState::applyBatch()`.

Se l'ID LRAD non e presente nella tabella, non avviene l'invio TCP ma viene comunque generato un NACK con `nack_reason=2`.

## Messaggi supportati

Il dispatcher del `BinaryConverter` gestisce attualmente quattro `messageId`:

### 1. `CS_LRAS_change_configuration_order_INS` — `1679949825`

Payload: uno o piu blocchi da 8 byte (`actionId[4] + lradId[2] + configuration[2]`, big-endian). Ogni blocco produce un JSON separato inviato via TCP al LRAD corrispondente.

```json
{
  "header": "MASTER",
  "type": "CMD",
  "sender": "CMS",
  "param": {
    "mode": "RELEASE"
  }
}
```

- `mode = "RELEASE"` se `configuration == 0`, altrimenti `"REQ"`
- `StateUpdate`: aggiorna `engaged` del LRAD (`false` se RELEASE, `true` se REQ)
- `ack_only = false`

### 2. `CS_LRAS_cueing_order_cancellation_INS` — `1679949826`

Payload: `actionId[4] + lradId[2]` (6 byte). Produce un JSON di cancellazione tracking.

```json
{
  "header": "TRCK",
  "type": "CMD",
  "sender": "CMS",
  "param": {
    "mode": "READY",
    "target": []
  }
}
```

- `ack_only = true`: nessun invio TCP; viene emesso direttamente un ACK di successo.

### 3. `CS_LRAS_cueing_order_INS` — `1679949827`

Payload con almeno 22 byte dopo l'header. Estrae `actionId`, `lradId`, `cueingType`, `cstn`, `kinematicsType` e coordinate cartesiane (se disponibili). Le coordinate (x, y, z in metri) vengono convertite in azimuth/elevazione in gradi tramite `CueingMath`.

Il formato JSON dipende da `cueingType`:

**`cueingType == 1` → POSITION (MOVE)**

```json
{
  "header": "MOVE",
  "type": "CMD",
  "sender": "CC",
  "param": {
    "goTo": "ABS",
    "az": 45.0,
    "el": 10.0
  },
  "meta": {
    "action_id": 3,
    "lrad_id": 1,
    "cueing_type": 1,
    "cstn": 100,
    "kinematics_type": 1
  }
}
```

- `goTo = "ABS"` se sono presenti coordinate cartesiane valide, altrimenti `"HOME"`.
- `az` / `el` in gradi, normalizzati in `[0, 360)`.

**`cueingType != 1` → DELTA**

```json
{
  "header": "DELTA",
  "type": "CMD",
  "sender": "CC",
  "param": {
    "az": 45,
    "el": 10
  },
  "meta": {
    "action_id": 3,
    "lrad_id": 1,
    "cueing_type": 2,
    "cstn": 100,
    "kinematics_type": 1
  }
}
```

- `az` / `el` sono interi `uint16` codificati come `round(angle_deg) mod 65536`.

Tipi di cinematica con coordinate cartesiane supportati: `1` (3D Kinematics), `2` (3D Position), `3` (2D Kinematics), `4` (2D Position).

- `ack_only = false`

### 4. `CS_LRAS_emission_control_INS` — `1679949828`

Payload minimo 838 byte. Contiene tutti i parametri di controllo emissione audio, laser, luce, LRF, zoom camera e rifermento orizzontale.

```json
{
  "header": "EMISS",
  "type": "CMD",
  "sender": "CMS",
  "message_name": "CS_LRAS_emission_control_INS",
  "message_id": 1679949828,
  "param": {
    "action_id": 0,
    "lrad_id": 1,
    "audio_mode_validity": 1,
    "audio_mode": {
      "volume_mode": { "level": 80, "audio_volume_db": -3.0, "mute": 0, "audio_mode": 1 },
      "recorded_message_tone": { "message_id": 5, "language": 0, "loop": 1 },
      "free_text": {
        "text": { "language_in": 0, "language_out": 0, "message_text": "Alert" },
        "loop": 0
      }
    },
    "laser_mode_validity": 1,
    "laser_mode": 2,
    "light_mode_validity": 1,
    "light_mode": { "light_power": 100, "light_zoom": 5 },
    "lrf_mode_validity": 1,
    "lrf_on_off": 1,
    "camera_zoom_validity": 1,
    "camera_zoom": 3,
    "horizontal_reference_validity": 1,
    "horizontal_reference": 0
  }
}
```

- `ack_only = false`

## Formato pacchetto ACK/NACK

Il pacchetto binario e 28 byte, big-endian:

**Header (16 byte)**

| Offset | Dimensione | Valore |
|---|---|---|
| 0 | 4 | `messageId = 576879045` (`LRAS_CS_ack_INS`) |
| 4 | 4 | `messageLength = 12` |
| 8 | 8 | riservato (0x00) |

**Payload (12 byte)**

| Offset | Dimensione | Campo |
|---|---|---|
| 16 | 4 | `action_id` (uint32) |
| 20 | 4 | `source_message_id` (uint32) |
| 24 | 2 | `ack_nack`: `1=ACK`, `2=NACK` |
| 26 | 2 | `nack_reason` |

Mappatura `nack_reason`:

- `0`: nessun errore classificato
- `2`: parametri errati (indirizzo/porta invalidi, LRAD non configurato)
- `3`: stato sistema errato (operazione gia avviata/in corso/abortita)
- `4`: sistema non pronto (timeout, try_again, would_block, not_connected)
- `5`: sistema non utilizzabile (connection_refused/reset, host/network unreachable, broken_pipe, eof)

## Configurazione rete

La configurazione viene letta all'avvio dal file `config/network_config.json`. E possibile passare un path alternativo come primo argomento:

```powershell
CMS_Proxy.exe percorso\alternativo\config.json
```

Struttura del file:

```json
{
  "udp": {
    "listen_ip": "127.0.0.1",
    "multicast_group": "239.0.0.1",
    "multicast_port": 12345
  },
  "tcp": {
    "default_target_ip": "127.0.0.1",
    "default_target_port": 9000,
    "unicast_target_ip": "127.0.0.1"
  },
  "ack": {
    "ip": "127.0.0.1",
    "port": 12346
  },
  "lrad_destinations": [
    { "id": 1, "ip": "127.0.0.1", "port": 9000 },
    { "id": 2, "ip": "127.0.0.1", "port": 9000 }
  ]
}
```

Note:
- La sezione `ack` accetta anche `ack_multicast` (retrocompatibilita).
- Tutti i campi sono obbligatori; porte fuori range `1-65535` causano errore all'avvio.
- Per ambienti reali modificare solo il JSON senza ricompilare.

## Build

Prerequisiti:

- CMake >= 3.15
- compilatore C++20
- Boost (`system`)
- `nlohmann_json`

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## Esecuzione

1. Avvia il receiver TCP di debug:

```powershell
python scripts/tcp_receiver.py --host 127.0.0.1 --port 9000
```

2. Avvia il proxy:

```powershell
.\build\Debug\CMS_Proxy.exe
```

3. Invia un pacchetto di test (`CS_LRAS_change_configuration_order_INS`):

```powershell
python scripts/send_test_packet.py --group 127.0.0.1 --port 12345
```

Se il flusso e corretto il receiver TCP mostra il JSON convertito e il proxy logga l'ACK/NACK inviato.

## Limiti attuali

- Il dispatcher gestisce 4 dei circa 20 `messageId` definiti nel codice (mancano handler per `emission_mode`, `inhibition_sectors`, controllo joystick, richieste di status, ecc.).
- Nessuna suite test automatizzata nel repository.