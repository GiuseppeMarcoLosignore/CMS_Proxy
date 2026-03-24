# CMS_Proxy

Proxy C++ che riceve messaggi UDP (multicast), li converte in JSON e li inoltra via TCP verso i sistemi LRAD. Per ogni invio genera anche un ACK/NACK binario su multicast dedicato.

## Obiettivo

Il progetto implementa una pipeline di bridging protocollo:

1. ricezione di pacchetti binari da rete UDP;
2. parsing dell'header e conversione payload in formato applicativo JSON;
3. routing verso destinazioni TCP in base a `destinationLradId`;
4. pubblicazione di ACK/NACK su multicast di ritorno.

## Architettura

- `UdpMulticastReceiver`: ascolta su UDP e notifica i pacchetti ricevuti tramite callback asincrona.
- `BinaryConverter`: valida header, dispatch per `messageId`, converte payload in uno o piu messaggi JSON.
- `TcpSender`: risolve host/porta, gestisce reconnessione e invio TCP.
- `ProxyEngine`: orchestra il flusso end-to-end, gestisce tabella LRAD e invio ACK multicast.

Interfacce astratte usate:

- `IReceiver`
- `IProtocolConverter`
- `ISender`

Questo rende i componenti sostituibili e testabili separatamente.

## Flusso di funzionamento

1. `main.cpp` crea `io_context`, receiver, converter e sender.
2. `ProxyEngine` registra una callback sul receiver.
3. Alla ricezione di un `RawPacket`:
	 - estrae `source_message_id` dai primi 4 byte (big-endian);
	 - invoca `BinaryConverter::convert()`;
	 - per ogni output convertito recupera la destinazione LRAD da `getNetworkConfig()`;
	 - invia il JSON via `TcpSender::send()`;
	 - costruisce un pacchetto ACK/NACK (`build_ack_packet`) e lo pubblica su multicast ACK.

Se l'ID LRAD non e configurato, non invia su TCP ma genera comunque NACK.

Comportamento importante nel codice attuale:

- se l'header e invalido o il `messageId` non e presente nel dispatcher, `BinaryConverter::convert()` restituisce output vuoto e il pacchetto viene ignorato (nessun inoltro TCP, nessun ACK);
- un singolo datagramma UDP puo produrre piu messaggi JSON (uno per ogni blocco payload da 8 byte);
- l'ACK viene emesso solo per i messaggi effettivamente prodotti dal converter.

## Messaggi supportati (stato attuale)

Al momento il dispatcher del converter gestisce questo `messageId`:

- `1679949825` (`CS_LRAS_change_configuration_order_INS`)

Per ogni blocco payload da 8 byte (`actionId[4] + lradId[2] + configuration[2]`, big-endian) produce un JSON:

```json
{
	"header": "MASTER",
	"type": "CMD",
	"sender": "CMS",
	"param": {
		"mode": "RELEASE | REQ",
		"action_id": 123
	}
}
```

Regola campo `mode`:

- `configuration == 0` -> `RELEASE`
- altrimenti -> `REQ`

`destinationLradId` viene valorizzato dal campo `lradId` del payload e usato per il routing TCP.

## Formato ACK multicast

`ProxyEngine` genera un pacchetto binario da 28 byte:

- Header 16 byte
	- `messageId = 576879045` (`LRAS_CS_ack_INS`)
	- `messageLength = 12`
- Payload 12 byte
	- `action_id` (uint32, big-endian)
	- `source_message_id` (uint32, big-endian)
	- `ack_nack` (uint16): `1=ACK`, `2=NACK`
	- `nack_reason` (uint16): valorizzato in base all'errore di trasporto

Mappatura `nack_reason` nel codice:

- `0`: no statement / non classificato
- `2`: parametri errati (es. host/porta invalidi, LRAD non configurato)
- `3`: stato sistema errato (operazione gia avviata/in corso/abortita)
- `4`: sistema non pronto (timeout, try_again, would_block, not_connected)
- `5`: sistema non utilizzabile (connection_refused/reset, host/network unreachable, broken_pipe, eof)

Endpoint multicast ACK predefinito:

- IP: `239.0.0.50`
- Porta: `12346`

## Configurazione rete predefinita

In `main.cpp`:

- UDP listen: `127.0.0.1:12345`
- gruppo multicast sorgente: `239.0.0.1`
- target TCP di default: `127.0.0.1:9000`
- unicast target configurato nel sender: `127.0.0.1`

In `ProxyEngine::getNetworkConfig()`:

- LRAD `1` -> `127.0.0.1:9000`
- LRAD `2` -> `127.0.0.1:9000`

Nota: la tabella e attualmente statica nel codice.

## Build

Prerequisiti:

- CMake >= 3.15
- compilatore C++20
- Boost (`system`)
- `nlohmann_json`

Esempio build (Windows):

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## Esecuzione

1. Avvia il receiver TCP di debug:

```powershell
python scripts/tcp_receiver.py --host 127.0.0.1 --port 9000
```

2. Avvia il proxy (`CMS_Proxy.exe`).

3. Invia un pacchetto di test:

```powershell
python scripts/send_test_packet.py --group 127.0.0.1 --port 12345
```

Se il flusso e corretto, il receiver TCP mostra il JSON convertito e il proxy logga l'ACK/NACK inviato.

Nota operativa:

- nei default correnti il receiver UDP e bindato su `127.0.0.1:12345`; per test locale funziona con `scripts/send_test_packet.py --group 127.0.0.1 --port 12345`;
- il codice effettua comunque la join del gruppo multicast configurato (`239.0.0.1`) nel receiver.

## Limiti attuali

- Dispatcher converter non completo: e implementato solo un tipo messaggio.
- Configurazione destinazioni LRAD hardcoded.
- Nessun file di configurazione esterno (JSON/YAML) per rete/routing.
- Nessuna suite test automatizzata nel repository.