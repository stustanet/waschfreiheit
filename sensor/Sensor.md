Das Funktionsprinzip der Sensoren ist das einer Stromzange, die Messung beruht darauf, dass ein Stromdurchflossener Leiter ein Magnetfeld erzeugt welches dann von einem Hallsensor gemessen werden kann.
Dabei wird um das Kabel ein Ferritkern gelegt, dieser hat einen Spalt, in den dann der eigentliche Sensor gesteckt wird.

Als Sensoren werden die analogen Hallsensoren "49E" verwendet. Diese Sensoren sollten bei keinem Magnetfeld eine Ausgangsspannung von exakt der halben Betriebsspannung liefern. Wenn sind das Magnetfeld ändert, ändert sich auch die Ausgangsspannung (ca. 1.8 mV / G).
Damit mit diesen am ADC des Mikroprozessors auch bei geringen Strömen ein sinnvoller Ausschlag messbar ist muss das Ausgangssignal noch zusätzlich verstärkt werden.
Der aktuelle Entwurf für diesen Verstärker ist ein Wechselspannungsverstärker, der nur Änderungen verstärkt und für Gleichspannung einen Verstärkungsfaktor von 1 hat.
Das hat den Vorteil, dass eventuelle Offsets der Sensorspannung nicht so sehr stören, leider ist diese Konstruktion jedoch sehr empfindlich gegenüber Störungen auf der Versorgungsspannung, hier besteht also noch Verbesserungspotential.
Eine einfache Konstruktion, die die Differenz von der Sensorspannung und der halben Betriebsspannung verstärkt, ist allerdings nicht möglich, da die Sensoren nicht *genau* die halbe Betriebsspannung liefern, wenn kein Magnetfeld da ist.
Diese Abweichung ist so groß, dass diese (bei dem gewünschten Verstärkungsfaktor) nicht einfach in Software korrigierbar ist.

![Schaltplan](/sensor/amp.svg)
