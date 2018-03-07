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
Ein Sensor misst den Stromverbrauch einer Maschine. Dies geschieht über einen Hallsensor am Stromkabel.

### Knoten
Ein Knoten besteht aus einem Mikroprozessor (STM32F103C8T6, Kleiner 32bit ARM) und einem 433Mhz Funkchip (Sx1276, LoRa Modulation).
An einen Knoten sind ein oder mehrere Sensoren angeschlossen. Die Sensordaten werden dann ausgewertet und bei einer Statusänderung wird ein Signal über die Funkschnittstelle gesendet.
Außerdem ist es die Aufgabe eines Knotens Nachrichten anderer Knoten weiterzuleiten, diese bilden quasi ein Mesh Netzwerk.

### Bridge
Es gibt im ganzen System genau eine Bridge (Gateway, Master Node), diese besteht aus einem Funkteil und einem Raspi. Die Aufgabe der Bridge ist es die Statusänderungen vom Mesh Netzwerk zu empfangen und diese dann per IP an einen Webserver weiterzuleiten, der diese dann anzeigt. Zusätzlich verwaltet die Bridge das Netzwerk und die Sensorparameter.

### Webserver
Der Webserver bekommt die Daten von der Bridge und zeigt den aktuellen Status an.

## Statuserkennung
Die Statuserkennung basiert auf Messwerten, die ein am Stromkabel befestigter Hall-Sensor liefert.
Um aus diesen Werten schlussendlich zu erkennen ob eine Maschine in Benutzung ist, durchläuft das Signal eine Reihe von Filtern.

### Analoger Teil
#### Wechselspannungs-Verstärker
Zuerst wird das Signal vom Hall-Sensor verstärkt. Der Verstärker hat für DC einen Verstärkungsfaktor von 1, es werden also nur Änderungen verstärkt.
#### Tiefpass
Bevor das Signal vom ADC digitalisiert wird, wird es mit einem RC-Filter geglättet. Dieser Tiefpass darf nicht zu träge sein, eine höhere Trägheit die Amplitude verringert.

### Digitaler Filter, Stufe 1 (Input)
Die erste Stufe bekommt die Messwerte des ADCs, sie wird für jede Messung einmal aufgerufen.
#### Absolutwertbestimmung
Da das Signal eine Wechselspannung mit dem Mittelpunkt von ca. der halben Betriebsspannung ist, muss zuerst der Absolutwert (Abweichung vom Mittelwert) bestimmt werden.
Dazu wird ein Mittelwert bestimmt, der für jeden Messwert, der drüber liegt um einen kleinen Wert erhöht wird (bzw. verringert wird, wenn der Messwert niedrieger ist).
Die Differenz zwischen dem Mittelwert und dem aktuellen Messwert ist der Absolutwert.
#### Digitaler Tiefpass
Der Absolutwert wird über einen Digitalen Tiefpass geglättet. Hier ist eine wesentlich stärkere Glättung möglich als vor dem Absolutwertfilter, da keine Amplitude verloren geht.

### Digitaler Filter, Stufe 2 (Frames)
In bestimmten Abständen (Frames) wird der aktuelle Tiefpasswert der Stufe 1 an die Stufe 2 weitergegeben. Der Abstand ist die "Framesize"
Die Stufe 2 wird einmal pro Frame aktualisiert. Die letzten "Window Size" Frame-Werte werden gespeichert, aus diesen wird der aktuelle Wert des Filters berechnet.
#### Blockfilter (Consec reject filter)
Alle Werte die nicht größer als ein Grenzwert ("Reject Threshold") sind und nicht mind. "Reject Consec" Werte davor auch über dem Grenzwert liegen werden bei der Berechnung des aktuellen Wertes ignoriert.

Beispiel:
Angenommen der Threshold wäre 5 und der Consec count wäre 3, dann würden nur die markierten Werte verwendet werden
    3 9 8 2 7 8 9 6 3 4 9 2 1 6 7 8 9 9 9 3 4 7 8 2
            ^ ^ ^ ^           ^ ^ ^ ^ ^ ^

#### Durchschnittswert
Der aktuelle Filterwert ist der Durchschnitt aller Werte im aktuellen Fenster. Werte die vom Blockfilter ignoriert werden gehen mit 0 in den Durchschnitt ein.
#### Interner Status
Abhängig vom aktuellen Filterwert und dem aktuellen internen Status wird anhand von festgelegten Grenzwerten bestimmt, ob ein Zustandsübergang stattfindet.
Bei einem Zustandsübergang wird ggf. die "Window Size" angepasst. Für jeden internen Zustand ist festgelegt, ob die Maschine in diesem Zustand belegt oder frei ist.
Nur Änderungen des Belegtzustandes werden über das Netzwerk gesendet.

## Ich will das complien!
Um dieses Projekt zu bauen wird eine ARM-Toolchain vorrausgesetzt.
* RIOT-OS nach Anleitung herunterladen / installieren.
* Dieses Repo klonen.
* "wasch\_v1" Board-Definition (mcu\_riot / boards) in "boards" Ordner von RIOT kopieren
* Das Hauptprojekt liegt in mcu\_riot / node
  * make NODE=MASTER erstellt die Firmware für den Master-Knoten 
  * make NODE=SENSOR erstellt die Firmware für einen Sensor-Knoten 
* Zum flashen (bei angeschlossenem ST-LINK) make flash NODE=...

## WTF???
Das ist jetzt erstmal nur eine ganz kurze übersicht, aber wenn du fragen hast, dann frag!
