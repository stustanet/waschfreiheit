EESchema Schematic File Version 4
LIBS:amp-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L amp-rescue:Conn_01x03-conn IC1
U 1 1 596261D7
P 750 950
F 0 "IC1" H 750 1150 50  0000 C CNN
F 1 "49E" V 850 950 50  0000 C CNN
F 2 "TO_SOT_Packages_THT:TO-92_Inline_Narrow_Oval" H 750 950 50  0001 C CNN
F 3 "" H 750 950 50  0001 C CNN
	1    750  950 
	-1   0    0    -1  
$EndComp
$Comp
L amp-rescue:LM358-linear IC2
U 2 1 59626325
P 1550 1150
F 0 "IC2" H 1550 1350 50  0000 L CNN
F 1 "LM358" H 1550 950 50  0000 L CNN
F 2 "Housings_SSOP:VSSOP-8_3.0x3.0mm_Pitch0.65mm" H 1550 1150 50  0001 C CNN
F 3 "" H 1550 1150 50  0001 C CNN
	2    1550 1150
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR01
U 1 1 59626378
P 1450 750
F 0 "#PWR01" H 1450 600 50  0001 C CNN
F 1 "+5V" H 1450 890 50  0000 C CNN
F 2 "" H 1450 750 50  0001 C CNN
F 3 "" H 1450 750 50  0001 C CNN
	1    1450 750 
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR02
U 1 1 59626392
P 1450 2050
F 0 "#PWR02" H 1450 1800 50  0001 C CNN
F 1 "GND" H 1450 1900 50  0000 C CNN
F 2 "" H 1450 2050 50  0001 C CNN
F 3 "" H 1450 2050 50  0001 C CNN
	1    1450 2050
	1    0    0    -1  
$EndComp
$Comp
L amp-rescue:CP_Small-device C2
U 1 1 596263AE
P 1200 1900
F 0 "C2" H 1210 1970 50  0000 L CNN
F 1 "22µ" H 1210 1820 50  0000 L CNN
F 2 "Resistors_SMD:R_0805" H 1200 1900 50  0001 C CNN
F 3 "" H 1200 1900 50  0001 C CNN
	1    1200 1900
	1    0    0    -1  
$EndComp
$Comp
L amp-rescue:R-device R2
U 1 1 596263F4
P 1700 1500
F 0 "R2" V 1780 1500 50  0000 C CNN
F 1 "10K" V 1700 1500 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 1630 1500 50  0001 C CNN
F 3 "" H 1700 1500 50  0001 C CNN
	1    1700 1500
	0    1    1    0   
$EndComp
$Comp
L amp-rescue:R-device R1
U 1 1 5962646B
P 1200 1650
F 0 "R1" V 1280 1650 50  0000 C CNN
F 1 "1K" V 1200 1650 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 1130 1650 50  0001 C CNN
F 3 "" H 1200 1650 50  0001 C CNN
	1    1200 1650
	1    0    0    -1  
$EndComp
$Comp
L amp-rescue:R-device R3
U 1 1 596264A2
P 2100 1150
F 0 "R3" V 2180 1150 50  0000 C CNN
F 1 "4K7" V 2100 1150 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 2030 1150 50  0001 C CNN
F 3 "" H 2100 1150 50  0001 C CNN
	1    2100 1150
	0    1    1    0   
$EndComp
$Comp
L amp-rescue:Conn_01x03-conn J1
U 1 1 59626CEC
P 3400 1150
F 0 "J1" H 3400 1350 50  0000 C CNN
F 1 "TO MCU" V 3500 1150 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x03_Pitch2.54mm" H 3400 1150 50  0001 C CNN
F 3 "" H 3400 1150 50  0001 C CNN
	1    3400 1150
	1    0    0    -1  
$EndComp
$Comp
L amp-rescue:CP_Small-device C1
U 1 1 59626EF0
P 2350 1900
F 0 "C1" H 2360 1970 50  0000 L CNN
F 1 "1000µ" H 2360 1820 50  0000 L CNN
F 2 "Resistors_SMD:R_1206" H 2350 1900 50  0001 C CNN
F 3 "" H 2350 1900 50  0001 C CNN
	1    2350 1900
	1    0    0    -1  
$EndComp
$Comp
L amp-rescue:C_Small-device C3
U 1 1 596270AF
P 2650 1900
F 0 "C3" H 2660 1970 50  0000 L CNN
F 1 "100n" H 2660 1820 50  0000 L CNN
F 2 "Resistors_SMD:R_0805" H 2650 1900 50  0001 C CNN
F 3 "" H 2650 1900 50  0001 C CNN
	1    2650 1900
	1    0    0    -1  
$EndComp
Wire Wire Line
	1950 1150 1850 1150
Wire Wire Line
	950  850  1450 850 
Wire Wire Line
	3200 2050 3200 1250
Connection ~ 2650 2050
Wire Wire Line
	2650 2050 2650 2000
Connection ~ 2650 850 
Wire Wire Line
	2650 1800 2650 850 
Connection ~ 2350 850 
Wire Wire Line
	2350 1800 2350 850 
Connection ~ 2350 2050
Wire Wire Line
	2350 2050 2350 2000
Wire Wire Line
	1200 2000 1200 2050
Connection ~ 1850 1150
Connection ~ 1200 1500
Connection ~ 1450 850 
Connection ~ 1450 2050
Connection ~ 1200 2050
Wire Wire Line
	1000 950  1000 2050
Wire Wire Line
	950  950  1000 950 
Wire Wire Line
	1450 850  1450 750 
Wire Wire Line
	1850 1150 1850 1500
Wire Wire Line
	1450 2050 1450 1450
Wire Wire Line
	1200 1500 1550 1500
Wire Wire Line
	1200 1250 1200 1500
Wire Wire Line
	1250 1250 1200 1250
Wire Wire Line
	1000 2050 1200 2050
Wire Wire Line
	950  1050 1250 1050
$Comp
L amp-rescue:R-device R4
U 1 1 59627566
P 2850 850
F 0 "R4" V 2930 850 50  0000 C CNN
F 1 "3R3" V 2850 850 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 2780 850 50  0001 C CNN
F 3 "" H 2850 850 50  0001 C CNN
	1    2850 850 
	0    1    1    0   
$EndComp
NoConn ~ 2050 2700
$Comp
L amp-rescue:LM358-linear IC2
U 1 1 59EC36E3
P 2050 2550
F 0 "IC2" H 2050 2750 50  0000 L CNN
F 1 "LM358" H 2050 2350 50  0000 L CNN
F 2 "Housings_SSOP:VSSOP-8_3.0x3.0mm_Pitch0.65mm" H 2050 2550 50  0001 C CNN
F 3 "" H 2050 2550 50  0001 C CNN
	1    2050 2550
	1    0    0    -1  
$EndComp
Wire Wire Line
	2250 1150 2900 1150
Wire Wire Line
	2900 1150 2900 1050
Wire Wire Line
	2900 1050 3200 1050
Wire Wire Line
	3000 850  3050 850 
Wire Wire Line
	3050 850  3050 1150
Wire Wire Line
	3050 1150 3200 1150
Wire Wire Line
	2650 2050 3200 2050
Wire Wire Line
	2650 850  2700 850 
Wire Wire Line
	2350 850  2650 850 
Wire Wire Line
	2350 2050 2650 2050
Wire Wire Line
	1450 850  2350 850 
Wire Wire Line
	1450 2050 2350 2050
Wire Wire Line
	1200 2050 1450 2050
$EndSCHEMATC
