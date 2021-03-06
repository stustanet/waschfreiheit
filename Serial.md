# Serielles Interface

## Allgemeine Befehle

#### routes DST1:HOP1,DST2:HOP2,...
Setzt die routen die vom Aktuellen Knoten verwendet werden.
* DSTn:HOPn Pakete für DSTn werden an HOPn weitergeleitet.

Dieser Befehl ist auf den Sensorknoten nur zu testzwecken verfügbar. Diese Knoten bekonnen ihre Routen normalerweie über das Netzwerk vom Master.

#### ping NODE\_ID
Sendet einen Echo-Request an einen Knoten.
* NODE\_ID Adresse des Knotens.

Damit der Reply auch ankommt, muss eine aus beiden Richtungen gültige Route existieren.


## Sensor
Der Sensor kennt aktuell nur einen Befehl:
#### config NODE\_ID KEY\_STATUS KEY\_CONFIG
Ändert die Netzwerk-Configuration eines Sensorknotens.
* NODE\_ID ist die Adresse dieses Knotens. Eine Adresse darf nur einmal vergeben werden.
* KEY\_STATUS ist der Gemeinsame Schlüssel der für den Status-Kanal verwendet wird.
* KEY\_CONFIG ist der Gemeinsame Schlüssel der für den Konfigurations-Kanal verwendet wird.
* Beide Schlüssel sind 16 Byte lang und werden als Hex angegeben.

## Master
Der Master spricht ein gemischtes (Mensch- und Maschinenlesbares) Protokoll.
Dazu gibt es zusätzlich zu den "normalen" Befehlen spezielle Nachrichten die vom Knoten gesendet werden um den Host über Statusänderungen und Ergebnisse von Befehlen zu informieren.
Diese werden im folgenden Updates genannt.
Es darf immer nur einen Ausstehenden Befehl pro Node geben, dieser muss erst ACK'ed werden (ACK update) bevor der Nächste gesendet wird.
Die einzigen Ausnahmen sind master\_routes, ping und connect.

### Befehle

#### connect NODE\_ID RETURN\_HOP TIMEOUT
Stellt eine Verbindung zu einem Knoten her.
* NODE\_ID Adresse des Knotens zu dem die Verbindung aufgebaut werden soll.
* RETURN\_HOP Erster Knoten auf dem Rückweg (Pfad von NODE\_ID zum Master). Dies wird als temporäre Route verwendet bis die vollständigen Routen gesetzt wurden.
* TIMEOUT Ist die länge des timeouts. Nach der angegebenen Zeit wird ds TIMEOUT Update gesendet und retransmit darf aufgerufen werden.

#### retransmit NODE\_ID
Sendet das letzte Paket für einen Knoten erneut.
* NODE\_ID Adresse des Knotens

Dies darf nur aufgerufen erden, nachdem ein Knoten einen TIMEOUT gemeldet hat.

#### reset\_routes NODE\_ID DST1:HOP1,DST2,HOP2,...
#### set\_routes NODE\_ID DST1:HOP1,DST2,HOP2,...
Setzt / Ändert die Routen eines Knotens. reset\_routes setzt dabei zuerst den Knoten (incl. aller Routen und Sensoreinstellungen) zurück. add\_routes ändert nur die bestehenden Routen.
* NODE\_ID Adresse des Knotens der Konfiguriert wird. Bevor ein Knoten konfiguriert wird, muss eine Verbindung (connect) aufgebaut werden.
* DSTn:HOPn Pakete für DSTn werden an HOPn weitergeleitet. Es können pro Aufruf maximal 20 Routen angegeben werden.

Die Route die bei connect angegeben wurde muss hier erneut spezifiziert werden.

#### configure\_sensor NODE\_ID CHANNEL INPUT\_FILTER ST\_MATRIX WND\_SIZES REJECT\_FILTER
Ändert die Konfiguration eines Sensors. Genauere Informationen zu den Parametern finden sich im state\_estimation haeder.
* NODE\_ID Adresse des Knotens der Konfiguriert wird.
* CHANNEL Nummer des Sensor-Kanals der Konfiguriert wrid.
* INPUT\_FILTER 3 Werte (uint16): MID\_ADJUSTMENT,LOWPASS\_WEIGHT,NUM\_SAMPLES
* ST\_MATRIX 12 Werte (int16), Bedingungen für Zustandsübergänge.
* WND\_SIZES 4 Werte (uint16, max 1536), Festergrößen in den entsperchenden Zuständen.
* REJECT\_FILTER 2 Werte (uint16) REJECT\_THRESHOLD,REJECT\_CONSEC

#### enable\_sensor NODE\_ID CHANNEL\_MASK SAMPLES\_PER\_SEC
Startet / Aktiviert die Sensoren.
* NODE\_ID Adresse des Knotens der Konfiguriert wird.
* CHANNEL\_MASK Bitfeld, ein gesetztes Bit steht für einen aktiven Sensor.
* SAMPLES\_PER\_SEC Sampling-Rate des ADCs, diese gilt für alle Sensoren.

#### raw\_frames NODE\_ID CHANNEL NUM\_OF\_FRAMES
Fragt Sensordaten von einem Knoten an. Das ist nur zum Kalibrieren des Sensors gedacht.
* NODE\_ID Adresse des Knotens.
* CHANNEL Sensorkanal
* NUM\_OF\_FRAMES Anzahl an Frames zum Senden.

#### authping NODE\_ID
Fragt an ob ein Knoten noch aktiv ist.
* NODE\_ID Adresse des Knotens.

### Updates / Signale
Updates haben das Prefix ### um diese leicht zu parsen.
#### ACK\<ID\>-\<CODE\>
Eine Netzwerk-Anfrage an Knoten ID wurde bestätigt. CODE ist der ACK-Code, dieser ist Abhängig vom Request. 0 oder 128 bedeuten, dass die Anfrage erfolgreich war.
Nach einem ACK kann an den Node ID der nächste Befehl gesendet werden.
#### ERR
Der letzte Befehl war ungültig. Es braucht nicht auf ein ACK gewartet werden.
#### TIMEOUT\<ID\>
Es ist ein Timeout aufgetreten. Es sollte deshalb bei Zeiten für diesen Knoten einen retransmit Aufruf geben. Dieser kann entweder direkt nach dem Timeout kommen oder zwischen durch kann auch noch anderes gemacht werden.
#### STATUS\<ID\>-\<STATUS\>
Statusupdate von Knoten ID. Status ist ein Bitfeld, ein gesetztes Bit steht für eine Aktive Maschine.
#### PEND\<ID\>
Ein PEND Signal kommt als direkte Antwort auf einen Befehl, falls ein Packet versendet wurde welches bestätigt werden muss. Daher folgt auf ein PEND immer ein ACK oder ein TIMEOUT.
