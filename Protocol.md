# Nachrichten und Routing
## Übersicht
Die Schichten sind an das OSI Modell angelehnt.
### Physical Layer (Layer 1)
Für den Datentransport wird 433MHz Funk mit LoRa Modulation verwednet. Alle Knoten senden und empfangen auf der selben Frequenz.
### Data Link Layer (Layer 2)
Die Verwendeten Funkmdule (Sx127x) kümmern sich bereits um das Verpacken der Daten in Pakete. Jedes Paket besitzt eine CRC Prüfsumme und eine Längenangabe (unterschiedlich lange Pakete sind also möglich),
### Network / Routing Layer (Layer 3)
Jeder Knoten hat eine eindeutige Adresse, alle Pakete haben folgende drei Knoten-Adressen:
* Quelle
* Ziel
* Next-Hop
Die Quelladresse ist dabei die Adresse des ursprünglichen Senders, die Zieladresse die des finalen Ziels. Next-Hop ist die Adresse des Knotens, der dieses Paket empfangen soll um es dann entweder zu verarbeiten (Next-Hop == Ziel) oder weiterzuleiten (Ziel in der Routing-Tabelle suchen und mit dem / an den entsprechendem Next-Hop weitersenden).
Eine Adressse ist eine 8-bit Zahl.
### Payload type (Layer 4)
(Jetzt nicht mehr OSI)
Typ-ID der Payload.
Je nach Typ wird auch ein MAC über den Layer4 (type + data) + Source und Destiantion Adresse gebiltet.
### Payload data (Layer 5)
(optional)
Je nach Payload-Typ unterschiedliche Daten.

## Aufbau des Netzwerks
### Master node
Der Koordinator (Master) hat die Aufgabe das Netzwerk aufzubauen und zu verwalten und dient gleichzeitig als Gateway zwischen dem Funk-Netz und dem Internet.
Das Routing ist (vorest) statisch, aber zentral verwaltet, d.h. der Master kennt alle Knoten (diese sind in seiner config festgelegt) und alle Routing-Tabelle der Knoten (ebenfalls aus config).
Zum Initialisieren des Netzes werden nacheinander die Knoten vom Master aus angesprochen und die Routing-Tabellen der Knoten konfiguriert. Diese Verbinung wird ggf. über bereits konfigurierte Knoten (wenn das Ziel weiter weg ist) geroutet.
Nach dieser Initialisierung hat jeder Knoten eine (hoffentlich gültige und sinvolle) Routing-Tabelle und weis somit wohin er eine Nachricht die bei ihn ankommt weiterleiten soll.

### Acknowledgements
Es gibt keine allgemeinen Acknowledgements, jedoch müssen Nachrihten die signiert sind bestätigt werden. Dazu wird ein signiertes Ack Paket zurück gesendet.

### Aufbau eines Layer 3 Pakets
Layer 3 ist das Routing-Layer und das niedrigste Layer um das sich der Node kümmert.
Wenn ein Paket weitergeleitet wird, wird nur der Next-Hop geändert und das Paket ansonsten unverändert erneut gesendet.

Die Zahl in Klammern ist die Bit-Anzahl des jeweiligen Feldes.

```
+--------------+
| Next-Hop (8) |
|--------------|
| Ziel     (8) |
|--------------|
| Quelle   (8) |
|--------------|
|              |
| Layer 4  (n) |
|              |
+--------------+
```

### Authentication Protocol
A und B besitzen einen gemeinsamen Geheimen Schlüssel. Eine Verbndung ist nur unidirektional, für eine Verbindung in die andere Richtung muss ein zweiter Kanal (mit einem anderen Schlüssle aufgebaut werden.)

A möchte eine Verbindung aufbauen um Daten nach B zu übertragen, sodass B sicher sein kann, dass diese von A kommen.
(z.B. A möchte die Konfiguration von B ändern).

```
A                                      B
|                                      |
|----------> REQ_NONCE, M ------------>|
|                                      |
|<--------- N, M, MAC(N, M) -----------|
|                                      |
|---- CMD, N + 1, MAC(CMD, N + 1) ---->|
|                                      |
|<--- ACK, N + 2, MAC(ACK, N + 2) -----|
|                                      |
|---- CMD, N + 3, MAC(CMD, N + 3) ---->|
|                                      |
|<--- ACK, N + 4, MAC(ACK, N + 4) -----|
|                                      |
                                      
```

### Aufbau eines Layer 4 Pakets
Anhand der Payload-ID wird entschieden was für Daten das Paket enthält und ob es eine MAC hat.

```
+---------------------*
| Payload-ID (8)      |
|---------------------|
|                     |
| Layer 5 / Daten (n) |
|                     |
|---------------------|
| [Nonce (64)]        |
|---------------------|
| [MAC (64)]          |
+---------------------+
```

