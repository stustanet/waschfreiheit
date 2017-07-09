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
Ein Knoten besteht aus einem Mikroprozessor (Aktuell geplant STM32F103C8T6) und einem 433Mhz Funkchip (aktuell Si4463 / RFM26).
An einen Knoten sind ein oder mehrere Sensoren angeschlossen. Die Sensordaten werden dann ausgewertet und bei einer Statusänderung wird ein Signal über die Funkschnittstelle gesendet.
Außerdem ist es die Aufgabe eines Knotens Nachrichten anderer Knoten weiterzuleiten, diese bilden quasi ein Mesh Netzwerk.

### Bridge
Es gibt im ganzen System genau eine Bridge, dies besteht aus einem Funkteil und einem Raspi. Die Aufgabe der Bridge ist es die Statusänderungen vom Mesh Netzwerk zu empfangen und diese dann per IP an einen Webserver weiterzuleiten, der diese dann anzeigt.

### Webserver
Der Webserver bekommt die Daten von der Bridge und zeigt den aktuellen Status an.


## TODO
* Sensorhalterung (3D Druck Teil um den Hall Sensor mit dem Ferritkern am Kabel zu halten)
* Software + Hardware für Knoten
* Protokolldesign (für Funkschnittstelle)
* Software für Funk <-> Lan Bridge (Raspi)
* Webserver

## WTF???
Das ist jetzt erstmal nur eine ganz kurze übersicht, aber wenn du fragen hast, dann frag!
