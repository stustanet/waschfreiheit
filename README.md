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
Ein Knoten besteht aus einem Mikroprozessor (STM32F103C8T6 (V1) oder STM32F401RBT6(V2), Kleiner 32bit ARM) und einem 433Mhz Funkchip (Sx1276, LoRa Modulation).
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
Angenommen der Threshold wäre 5 und der "Consec count" wäre 3, dann würden nur die markierten Werte verwendet werden.

    3 9 8 2 7 8 9 6 3 4 9 2 1 6 7 8 9 9 9 3 4 7 8 2
            ^ ^ ^ ^           ^ ^ ^ ^ ^ ^

#### Durchschnittswert
Der aktuelle Filterwert ist der Durchschnitt aller Werte im aktuellen Fenster. Werte die vom Blockfilter ignoriert werden gehen mit 0 in den Durchschnitt ein.
#### Interner Status
Abhängig vom aktuellen Filterwert und dem aktuellen internen Status wird anhand von festgelegten Grenzwerten bestimmt, ob ein Zustandsübergang stattfindet.
Bei einem Zustandsübergang wird ggf. die "Window Size" angepasst. Für jeden internen Zustand ist festgelegt, ob die Maschine in diesem Zustand belegt oder frei ist.
Nur Änderungen des Belegtzustandes werden über das Netzwerk gesendet.

## Ich will das complien!
* Ggf. ARM Toolchain installieren.
* Dieses Repo klonen.
* git submodule init
* git submodule update
* "make all" im Ordner firmware baut Master (V1 / V2), Sensor (V1 / V2) und den Bootloader (nur V2)

## Flashen
Es gibt eine Reihe verschiedener Möglichkeiten die Firmware zu flashen.
Manche sind nur auf V1 oder V2 verfügbar.

### ST-LINK
Die bequemste Möglichkeit diese Controller zu flashen ist ein ST-LINK. Dies ist ein spezieller Programmer / Debugger für ST Controller. Um einen ST-LINK zu verwenden muss OpenOCD installiert sein.

#### Benötigte Anschlüsse
Um mit einen ST-LINK zu flashen müssen folgende 3 / 4 Leitungen mit dem Board verbunden werden:
* GND
* SWIO
* SWCLK
* Ggf. 3.3V

Flashen geht dann einfach mit
```
Sensor V1
make flash_sv1

Sensor V2
make USE_BOOTLOADER=FALSE flash_sv2

Master V1
make flash_mv1

Master V2
make flash_mv2

Bootloader V2
make flash_blv2
```

### Serieller Bootloader
Das Board muss über ein Serielles Kabel mit dem PC verbunden werden.

Der flasher Tool stm32loader.py befindet sich als submodule in firmware/utils.

Das Kabel wird wie folgt angesteckt:
* RX von Kabel an TX vom Board
* TX von Kabel an RX vom Board
* GND an GND
* Ggf. die 5V von USB and die 5V(!) vom Board anschließen

Um das Board in den Bootloader Modus zu bringen:
#### V1
* Den BOOT0 Jumper auf 1 setzen
* Reset kurz drücken
* Nach dem Flashen den BOOT0 Jumper auf 0 zurüksetzen

#### V2
* Boot drücken und halten
* Reset kurz drücken
* Boot wieder loslassen

Zum flashen dann
```
Binary erstellen mit
arm-none-eabi-objcopy -I ihex -O binary <ELF> /tmp/image.bin

flashen mit
./stm32loader.py -p /dev/ttyUSB0 -evw /tmp/image.bin
```


### USB DFU Bootloader (Nur V2)

Dazu muss zuerst das tool dfu-util gebaut werden.
Dieses ist als submodule in firmware/utils eingebunden.

```
cd firmware/utils
./autogen.sh
./configure
make
```

Das Board wird dann über ein Micro-USB Kabel mit dem PC verbunden.
Um das Board in den Bootloader Modus zu bringen:
* Boot drücken und halten
* Reset kurz drücken
* Boot wieder loslassen

Dann sollte es sich am PC als DFU Gerät anmelden.
Zum flashen dann
```
Binary erstellen mit
arm-none-eabi-objcopy -I ihex -O binary <ELF> /tmp/image.bin

flashen mit
sudo ./dfu-util -a 0 -s 0x08000000:leave -D /tmp/image.bin
```

### USB Host Bootloader (Nur Sensor V2)
Der USB Host Bootloader ist ein eigener Bootloader für diese Boards, der es erlaubt ein Firmware image auf einen USB-Stick zu laden und dann ohne einen PC ein Update einzuspielen.
Um diesen zu verwenden, muss das Firmware Image für den Bootloader gebaut werden (ohne USE_BOOTLOADER=FALSE) und der Bootloader muss geflasht sein (das geht mit einer der anderen Methoden).

Auf einen FAT32 formatierten USB Stick müssen sich folgende Dateien befinden:
* FIRMWARE.BIN  Neue Firmware im Binärformat
* CHECKSUM.CRC  CRC32 Prüfsumme der Firmware

Diese beiden Dateien können bequem mit make_boot_files.sh in firmware/utils aus einer ELF erstellt werden.

Dann den Stick einfach an das Board anstecken und resetten. Die Eingangs-LEDs sollten während des Updates der Reihe nach in den Farben Grün, Violett, Rot und Gelb aufleuchten.
Nach dem Flashen wird das Programm normal gestartet.

Der USB Stick wird *nicht* erkannt, wenn dass Board über den Micro-USB Anschluss mit Strom versorgt wird.

### Flasher command (Nur V1)
Die V1 Boards haben einen integrierten Flash-Befehl mit dem die Firmware aus dem laufenden Betrieb heraus geupdated werden kann.
Da dazu nur die Serielle Schnittstelle benötigt wird und nicht extra ein Boot-Modus aktiviert werden muss ist dieser Modus praktisch um die alten Nodes zu flashen, da diese schlecht zu Öffnen sind.
Der Nachteil ist, dass sich der Flasher selbst überschreibt, d.h. sollte etwas schiefgehen, ist der Knoten über diesen Weg nicht erneut flashbar.

Das Tool zum flashen ist firmware/utils/flasher_v1.py

Zum flashen
```
Binary erstellen mit
arm-none-eabi-objcopy -I ihex -O binary <ELF> /tmp/image.bin

flashen mit
./flasher_v1.py /dev/ttyUSB0 /tmp/image.bin
```
