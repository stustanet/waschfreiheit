# Waschen in Freiheit !!!
## Was ist das?
Dieses Projekt soll verhindern, dass die armen Studenten unnötig lange mit der Suche nach einer freien Waschmaschine verbringen. Dazu soll der Status aller Maschinen erfasst werden um diese dann online abfragbar zu machen. Will man dann Waschen kann man direkt zu einer freien Maschine laufen und muss nicht erst in jedem Waschraum vorbeischauen, nur um festzustellen, dass dieser schon belegt ist.

## Aufbau
Der aktuelle Plan sieh in etwa so aus:

```
                                     +---+
+---Raum------------------------+    |   |
|                               |    | F |
|  WM <- Sensor  \              |    | U |
|                 => Knoten <~~~|~~~>| N |
|  WM <- Sensor  /              |    | K |
|                               |    |   |
+-------------------------------+    |   |
                                     |   |     +--------+    +-----------+
                                     |   |<~~~>| Bridge |--->| Webserver |
+---Raum------------------------+    |   |     +--------+    +-----------+
|                               |    |   |
|  WM <- Sensor  \              |    |   |
|                 => Knoten <~~~|~~~>|   |
|  WM <- Sensor  /              |    |   |
|                               |    |   |
+-------------------------------+    |   |
                                     |   |
                                     |   |
+---Raum------------------------+    |   |
|                               |    |   |
|  WM <- Sensor ===> Knoten <~~~|~~~>|   |
|                               |    |   |
+-------------------------------+    |   |
                                     +---+
    
```

### Sensoren
Ein Sensor misst den Stromverbrauch einer Maschine. Dies soll mit einem Hallsensor am Stromkabel gemacht werden.

### Knoten
Ein Knoten besteht aus einem Mikroprozessor (STM32F103C8T6, Kleiner 32bit ARM) und einem 433Mhz Funkchip (Sx1276, LoRa Modulation).
An einen Knoten sind ein oder mehrere Sensoren angeschlossen. Die Sensordaten werden dann ausgewertet und bei einer Statusänderung wird ein Signal über die Funkschnittstelle gesendet.
Außerdem ist es die Aufgabe eines Knotens Nachrichten anderer Knoten weiterzuleiten, diese bilden quasi ein Mesh Netzwerk.

### Bridge
Es gibt im ganzen System genau eine Bridge (Gateway, Master Node), diese besteht aus einem Funkteil und einem Raspi. Die Aufgabe der Bridge ist es die Statusänderungen vom Mesh Netzwerk zu empfangen und diese dann per IP an einen Webserver weiterzuleiten, der diese dann anzeigt. Zusätzlich verwaltet die Bridge das Netzwerk und die Sensorparameter.

### Webserver
Der Webserver bekommt die Daten von der Bridge und zeigt den aktuellen Status an.

## Ich will das complien!
* RIOT-OS nach Anleitung herunterladen / installieren.
* Dieses Repo klonen.
* "wasch\_v1" Board-Definition (mcu\_riot / boards) in "boards" Ordner von RIOT kopieren
* Das Hauptprojekt liegt in mcu\_riot / node
  * make NODE=MASTER erstellt die Firmware für den Master-Knoten 
  * make NODE=SENSOR erstellt die Firmware für einen Sensor-Knoten 
* Zum flashen (bei angeschlossenem ST-LINK) make flash NODE=...

## WTF???
Das ist jetzt erstmal nur eine ganz kurze übersicht, aber wenn du fragen hast, dann frag!
