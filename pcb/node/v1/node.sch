EESchema Schematic File Version 4
LIBS:node-cache
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
L node_custom:bluepill IC1
U 1 1 59E9F1F7
P 3850 3200
F 0 "IC1" H 3850 4300 50  0000 C CNN
F 1 "bluepill" H 3850 2100 50  0000 C CNN
F 2 "Housings_DIP:DIP-40_W15.24mm" H 3850 3200 50  0001 C CNN
F 3 "" H 3850 3200 50  0001 C CNN
	1    3850 3200
	1    0    0    -1  
$EndComp
$Comp
L node_custom:Ra-01 IC2
U 1 1 59E9F282
P 6450 3600
F 0 "IC2" H 6450 4100 60  0000 C CNN
F 1 "Ra-01" H 6450 3100 60  0000 C CNN
F 2 "footprints:Ra-01" H 6350 4050 60  0001 C CNN
F 3 "" H 6350 4050 60  0001 C CNN
	1    6450 3600
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:Screw_Terminal_01x03 J2
U 1 1 59E9FF5E
P 7400 5200
F 0 "J2" H 7400 5400 50  0000 C CNN
F 1 "Sensor1" H 7400 5000 50  0000 C CNN
F 2 "Terminal_Blocks:TerminalBlock_Philmore_TB133_03x5mm_Straight" H 7400 5200 50  0001 C CNN
F 3 "" H 7400 5200 50  0001 C CNN
	1    7400 5200
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:R R4
U 1 1 59EA018C
P 6850 4900
F 0 "R4" V 6930 4900 50  0000 C CNN
F 1 "10K" V 6850 4900 50  0000 C CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0309_L9.0mm_D3.2mm_P12.70mm_Horizontal" V 6780 4900 50  0001 C CNN
F 3 "" H 6850 4900 50  0001 C CNN
	1    6850 4900
	0    1    1    0   
$EndComp
$Comp
L node-rescue:R R5
U 1 1 59EA01F7
P 7200 4700
F 0 "R5" V 7280 4700 50  0000 C CNN
F 1 "10R" V 7200 4700 50  0000 C CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0309_L9.0mm_D3.2mm_P12.70mm_Horizontal" V 7130 4700 50  0001 C CNN
F 3 "" H 7200 4700 50  0001 C CNN
	1    7200 4700
	-1   0    0    1   
$EndComp
$Comp
L node-rescue:C C8
U 1 1 59EA00E3
P 6550 5150
F 0 "C8" H 6575 5250 50  0000 L CNN
F 1 "100n" H 6575 5050 50  0000 L CNN
F 2 "Capacitors_ThroughHole:C_Disc_D5.0mm_W2.5mm_P2.50mm" H 6588 5000 50  0001 C CNN
F 3 "" H 6550 5150 50  0001 C CNN
	1    6550 5150
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:CP C7
U 1 1 59EA0678
P 6050 5350
F 0 "C7" H 6075 5450 50  0000 L CNN
F 1 "1000µ" H 6075 5250 50  0000 L CNN
F 2 "Capacitors_ThroughHole:CP_Radial_D10.0mm_P5.00mm" H 6088 5200 50  0001 C CNN
F 3 "" H 6050 5350 50  0001 C CNN
	1    6050 5350
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:C C6
U 1 1 59EA06D5
P 5800 5350
F 0 "C6" H 5825 5450 50  0000 L CNN
F 1 "100n" H 5825 5250 50  0000 L CNN
F 2 "Capacitors_ThroughHole:C_Disc_D5.0mm_W2.5mm_P2.50mm" H 5838 5200 50  0001 C CNN
F 3 "" H 5800 5350 50  0001 C CNN
	1    5800 5350
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:Screw_Terminal_01x03 J3
U 1 1 59EA1CF0
P 8600 5200
F 0 "J3" H 8600 5400 50  0000 C CNN
F 1 "Sensor2" H 8600 5000 50  0000 C CNN
F 2 "Terminal_Blocks:TerminalBlock_Philmore_TB133_03x5mm_Straight" H 8600 5200 50  0001 C CNN
F 3 "" H 8600 5200 50  0001 C CNN
	1    8600 5200
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:R R6
U 1 1 59EA1CF6
P 8050 4900
F 0 "R6" V 8130 4900 50  0000 C CNN
F 1 "10K" V 8050 4900 50  0000 C CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0309_L9.0mm_D3.2mm_P12.70mm_Horizontal" V 7980 4900 50  0001 C CNN
F 3 "" H 8050 4900 50  0001 C CNN
	1    8050 4900
	0    1    1    0   
$EndComp
$Comp
L node-rescue:R R7
U 1 1 59EA1CFC
P 8400 4700
F 0 "R7" V 8480 4700 50  0000 C CNN
F 1 "10R" V 8400 4700 50  0000 C CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0309_L9.0mm_D3.2mm_P12.70mm_Horizontal" V 8330 4700 50  0001 C CNN
F 3 "" H 8400 4700 50  0001 C CNN
	1    8400 4700
	-1   0    0    1   
$EndComp
$Comp
L node-rescue:C C9
U 1 1 59EA1D06
P 7750 5150
F 0 "C9" H 7775 5250 50  0000 L CNN
F 1 "100n" H 7775 5050 50  0000 L CNN
F 2 "Capacitors_ThroughHole:C_Disc_D5.0mm_W2.5mm_P2.50mm" H 7788 5000 50  0001 C CNN
F 3 "" H 7750 5150 50  0001 C CNN
	1    7750 5150
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:TPS76301 IC3
U 1 1 59EA2578
P 5050 4900
F 0 "IC3" H 4900 5125 50  0000 C CNN
F 1 "TPS73201 " H 5050 5125 50  0000 L CNN
F 2 "TO_SOT_Packages_SMD:SOT-23-5" H 5050 5225 50  0001 C CIN
F 3 "" H 5050 4900 50  0001 C CNN
	1    5050 4900
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:R R2
U 1 1 59EA3635
P 5550 4750
F 0 "R2" V 5630 4750 50  0000 C CNN
F 1 "R" V 5550 4750 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 5480 4750 50  0001 C CNN
F 3 "" H 5550 4750 50  0001 C CNN
	1    5550 4750
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:R R3
U 1 1 59EA3696
P 5550 5300
F 0 "R3" V 5630 5300 50  0000 C CNN
F 1 "R" V 5550 5300 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 5480 5300 50  0001 C CNN
F 3 "" H 5550 5300 50  0001 C CNN
	1    5550 5300
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:CP C5
U 1 1 59EA484B
P 4400 5350
F 0 "C5" H 4425 5450 50  0000 L CNN
F 1 "4700µ" H 4425 5250 50  0000 L CNN
F 2 "Capacitors_ThroughHole:CP_Radial_D12.5mm_P5.00mm" H 4438 5200 50  0001 C CNN
F 3 "" H 4400 5350 50  0001 C CNN
	1    4400 5350
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:CP C4
U 1 1 59EA4974
P 4150 5350
F 0 "C4" H 4175 5450 50  0000 L CNN
F 1 "4700µ" H 4175 5250 50  0000 L CNN
F 2 "Capacitors_ThroughHole:CP_Radial_D12.5mm_P5.00mm" H 4188 5200 50  0001 C CNN
F 3 "" H 4150 5350 50  0001 C CNN
	1    4150 5350
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:C C3
U 1 1 59EA49D7
P 3900 5350
F 0 "C3" H 3925 5450 50  0000 L CNN
F 1 "1µ" H 3925 5250 50  0000 L CNN
F 2 "Capacitors_ThroughHole:C_Disc_D7.5mm_W5.0mm_P5.00mm" H 3938 5200 50  0001 C CNN
F 3 "" H 3900 5350 50  0001 C CNN
	1    3900 5350
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:Conn_01x04 J1
U 1 1 59EA5273
P 1600 2950
F 0 "J1" H 1600 3150 50  0000 C CNN
F 1 "Serial" H 1600 2650 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x04_Pitch2.54mm" H 1600 2950 50  0001 C CNN
F 3 "" H 1600 2950 50  0001 C CNN
	1    1600 2950
	-1   0    0    1   
$EndComp
$Comp
L node-rescue:R R1
U 1 1 59EA56C8
P 2050 3150
F 0 "R1" V 2130 3150 50  0000 C CNN
F 1 "1K" V 2050 3150 50  0000 C CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0309_L9.0mm_D3.2mm_P12.70mm_Horizontal" V 1980 3150 50  0001 C CNN
F 3 "" H 2050 3150 50  0001 C CNN
	1    2050 3150
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:C C1
U 1 1 59EA696A
P 3200 5350
F 0 "C1" H 3225 5450 50  0000 L CNN
F 1 "100n" H 3225 5250 50  0000 L CNN
F 2 "Capacitors_ThroughHole:C_Disc_D5.0mm_W2.5mm_P2.50mm" H 3238 5200 50  0001 C CNN
F 3 "" H 3200 5350 50  0001 C CNN
	1    3200 5350
	1    0    0    -1  
$EndComp
$Comp
L node-rescue:CP C2
U 1 1 59EA68E7
P 3450 5350
F 0 "C2" H 3475 5450 50  0000 L CNN
F 1 "1000µ" H 3475 5250 50  0000 L CNN
F 2 "Capacitors_ThroughHole:CP_Radial_D10.0mm_P5.00mm" H 3488 5200 50  0001 C CNN
F 3 "" H 3450 5350 50  0001 C CNN
	1    3450 5350
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR01
U 1 1 59EA8000
P 3900 4750
F 0 "#PWR01" H 3900 4600 50  0001 C CNN
F 1 "+5V" H 3900 4890 50  0000 C CNN
F 2 "" H 3900 4750 50  0001 C CNN
F 3 "" H 3900 4750 50  0001 C CNN
	1    3900 4750
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR02
U 1 1 59EA8139
P 3450 5150
F 0 "#PWR02" H 3450 5000 50  0001 C CNN
F 1 "+3.3V" H 3450 5290 50  0000 C CNN
F 2 "" H 3450 5150 50  0001 C CNN
F 3 "" H 3450 5150 50  0001 C CNN
	1    3450 5150
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR03
U 1 1 59EA8551
P 3900 5600
F 0 "#PWR03" H 3900 5350 50  0001 C CNN
F 1 "GND" H 3900 5450 50  0000 C CNN
F 2 "" H 3900 5600 50  0001 C CNN
F 3 "" H 3900 5600 50  0001 C CNN
	1    3900 5600
	1    0    0    -1  
$EndComp
Wire Wire Line
	4450 3050 5100 3050
Wire Wire Line
	5100 3050 5100 2700
Wire Wire Line
	5100 2700 7550 2700
Wire Wire Line
	7550 2700 7550 3450
Wire Wire Line
	7550 3450 7050 3450
Wire Wire Line
	4450 3150 5200 3150
Wire Wire Line
	5200 3150 5200 2800
Wire Wire Line
	5200 2800 7450 2800
Wire Wire Line
	7450 2800 7450 3550
Wire Wire Line
	7450 3550 7050 3550
Wire Wire Line
	4450 3250 5300 3250
Wire Wire Line
	5300 3250 5300 2900
Wire Wire Line
	5300 2900 7350 2900
Wire Wire Line
	7350 2900 7350 3650
Wire Wire Line
	7350 3650 7050 3650
Wire Wire Line
	4450 3350 5400 3350
Wire Wire Line
	5400 3350 5400 3000
Wire Wire Line
	5400 3000 7250 3000
Wire Wire Line
	7250 3000 7250 3350
Wire Wire Line
	7250 3350 7050 3350
Wire Wire Line
	4450 3450 5400 3450
Wire Wire Line
	5400 3450 5400 3650
Wire Wire Line
	5400 3650 5850 3650
Wire Wire Line
	4450 3550 5300 3550
Wire Wire Line
	5300 3550 5300 3750
Wire Wire Line
	5300 3750 5850 3750
Wire Wire Line
	4450 3650 5200 3650
Wire Wire Line
	5200 3650 5200 3850
Wire Wire Line
	5200 3850 5850 3850
Wire Wire Line
	4450 3750 5100 3750
Wire Wire Line
	5100 3750 5100 3950
Wire Wire Line
	5100 3950 5850 3950
Wire Wire Line
	4450 3850 5000 3850
Wire Wire Line
	5000 3850 5000 4050
Wire Wire Line
	5000 4050 5750 4050
Wire Wire Line
	5750 4050 5750 3550
Wire Wire Line
	5750 3550 5850 3550
Wire Wire Line
	5800 3350 5850 3350
Wire Wire Line
	5800 2350 5800 2600
Wire Wire Line
	5800 2600 7100 2600
Wire Wire Line
	7100 2600 7100 3250
Wire Wire Line
	7100 3250 7050 3250
Wire Wire Line
	7100 3950 7050 3950
Connection ~ 7100 3250
Wire Wire Line
	4450 2350 4500 2350
Connection ~ 5800 2600
Wire Wire Line
	5850 3450 5700 3450
Wire Wire Line
	5700 3450 5700 2450
Wire Wire Line
	5700 2450 4800 2450
Wire Wire Line
	7200 4850 7200 5100
Wire Wire Line
	6550 5300 7200 5300
Wire Wire Line
	7100 5200 7200 5200
Wire Wire Line
	7100 4900 7100 5200
Wire Wire Line
	7100 4900 7000 4900
Wire Wire Line
	6700 4900 6550 4900
Wire Wire Line
	6050 5200 5800 5200
Connection ~ 6550 5300
Wire Wire Line
	6050 4550 6050 5200
Wire Wire Line
	5350 4550 5550 4550
Connection ~ 6050 5500
Wire Wire Line
	8400 4850 8400 5100
Wire Wire Line
	7750 5300 8400 5300
Wire Wire Line
	8300 5200 8400 5200
Wire Wire Line
	8300 4900 8300 5200
Wire Wire Line
	8300 4900 8200 4900
Wire Wire Line
	7750 4900 7900 4900
Connection ~ 7750 5300
Connection ~ 7200 4550
Wire Wire Line
	7750 5500 7750 5300
Wire Wire Line
	6550 5500 6550 5300
Connection ~ 6550 5500
Wire Wire Line
	5350 4800 5300 4800
Wire Wire Line
	5350 4800 5350 4550
Connection ~ 6050 4550
Connection ~ 5800 5500
Wire Wire Line
	5550 5500 5550 5450
Connection ~ 5550 5500
Wire Wire Line
	5550 4900 5550 5000
Wire Wire Line
	5550 4600 5550 4550
Connection ~ 5550 4550
Connection ~ 5550 5000
Wire Wire Line
	5350 4900 5350 5000
Wire Wire Line
	5350 5000 5550 5000
Wire Wire Line
	5050 5500 5050 5200
Wire Wire Line
	5000 2250 5000 2950
Connection ~ 6550 4900
Wire Wire Line
	4900 2150 4900 2850
Connection ~ 7750 4900
Wire Wire Line
	5250 4800 5250 4900
Wire Wire Line
	4650 4900 4750 4900
Connection ~ 5050 5500
Connection ~ 4400 5500
Connection ~ 4150 5500
Wire Wire Line
	3900 4750 3900 4800
Wire Wire Line
	4150 4800 4150 5200
Connection ~ 4150 4800
Wire Wire Line
	4400 4800 4400 5200
Connection ~ 4400 4800
Wire Wire Line
	1900 2950 1900 3350
Wire Wire Line
	1900 2950 1800 2950
Wire Wire Line
	1800 3050 1800 3950
Wire Wire Line
	2050 3300 2050 3350
Wire Wire Line
	2050 3350 1900 3350
Connection ~ 1900 3350
Wire Wire Line
	1800 3950 3100 3950
Wire Wire Line
	3100 4800 3100 3950
Connection ~ 3100 3950
Connection ~ 3900 4800
Wire Wire Line
	3000 4050 3000 5500
Connection ~ 3000 4050
Connection ~ 3900 5500
Connection ~ 3450 5500
Connection ~ 3200 5500
Wire Wire Line
	3200 5200 3450 5200
Connection ~ 3200 5200
Wire Wire Line
	3450 5200 3450 5150
Wire Wire Line
	3200 4150 3200 4350
Wire Wire Line
	1900 4050 3000 4050
Wire Wire Line
	3200 4150 3250 4150
Wire Wire Line
	3900 5500 3900 5600
Wire Wire Line
	4500 1900 4500 1950
Wire Wire Line
	4500 2250 4450 2250
Connection ~ 4500 2350
Wire Wire Line
	4500 1950 3200 1950
Wire Wire Line
	3200 1950 3200 4050
Connection ~ 3200 4050
Connection ~ 4500 2250
Wire Wire Line
	4800 2450 4800 4350
Wire Wire Line
	4800 4350 3200 4350
Connection ~ 3200 4350
Connection ~ 4800 2450
Wire Wire Line
	3000 5500 3200 5500
Wire Wire Line
	4650 4800 4650 4900
Wire Wire Line
	3100 4800 3900 4800
Connection ~ 4650 4800
Wire Wire Line
	6550 4200 6550 4900
Wire Wire Line
	7650 4200 7650 2250
Wire Wire Line
	7650 2250 5000 2250
Wire Wire Line
	7750 2150 7750 4900
Wire Wire Line
	7750 2150 4900 2150
Wire Wire Line
	6550 4200 7650 4200
Wire Wire Line
	1950 2150 1950 2850
Wire Wire Line
	1950 2850 1800 2850
Wire Wire Line
	2050 2050 2050 2800
Wire Wire Line
	2050 2800 1800 2800
Wire Wire Line
	1800 2800 1800 2750
Wire Wire Line
	5000 2950 4450 2950
Wire Wire Line
	4900 2850 4450 2850
$Comp
L node-rescue:Conn_01x15 J4
U 1 1 59F0A7BB
P 2250 3150
F 0 "J4" H 2250 3950 50  0000 C CNN
F 1 "EXT" H 2250 2350 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x15_Pitch2.54mm" H 2250 3150 50  0001 C CNN
F 3 "" H 2250 3150 50  0001 C CNN
	1    2250 3150
	-1   0    0    1   
$EndComp
Wire Wire Line
	2450 3850 3250 3850
Wire Wire Line
	2450 3750 3250 3750
Wire Wire Line
	3250 3650 2450 3650
Wire Wire Line
	2450 3550 3250 3550
Wire Wire Line
	3250 3450 2450 3450
Wire Wire Line
	2450 3350 3250 3350
Wire Wire Line
	3250 3250 2450 3250
Wire Wire Line
	2450 3150 3250 3150
Wire Wire Line
	3250 3050 2450 3050
Wire Wire Line
	2450 2950 3250 2950
Wire Wire Line
	2450 2450 2500 2450
Wire Wire Line
	2500 2450 2500 2250
Wire Wire Line
	2500 2250 3250 2250
Wire Wire Line
	3250 2350 2600 2350
Wire Wire Line
	2600 2350 2600 2550
Wire Wire Line
	2600 2550 2450 2550
Wire Wire Line
	2450 2650 2700 2650
Wire Wire Line
	2700 2650 2700 2450
Wire Wire Line
	2700 2450 3250 2450
Wire Wire Line
	3250 2550 2800 2550
Wire Wire Line
	2800 2550 2800 2750
Wire Wire Line
	2800 2750 2450 2750
Wire Wire Line
	2450 2850 2900 2850
Wire Wire Line
	2900 2850 2900 2650
Wire Wire Line
	2900 2650 3250 2650
Wire Wire Line
	1950 2150 3000 2150
Wire Wire Line
	3000 2150 3000 2750
Wire Wire Line
	3000 2750 3250 2750
Wire Wire Line
	2050 2050 3100 2050
Wire Wire Line
	3100 2050 3100 2850
Wire Wire Line
	3100 2850 3250 2850
Connection ~ 2050 2800
Wire Wire Line
	4450 2550 4650 2550
Wire Wire Line
	4450 2750 4750 2750
$Comp
L node-rescue:Conn_01x05 J5
U 1 1 59F0D84C
P 4800 1700
F 0 "J5" H 4800 2000 50  0000 C CNN
F 1 "EXT2" H 4800 1400 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x05_Pitch2.54mm" H 4800 1700 50  0001 C CNN
F 3 "" H 4800 1700 50  0001 C CNN
	1    4800 1700
	0    -1   -1   0   
$EndComp
Wire Wire Line
	4600 1900 4500 1900
Connection ~ 4500 1950
Wire Wire Line
	4700 1900 4700 1950
Wire Wire Line
	4700 1950 4600 1950
Wire Wire Line
	4600 1950 4600 2450
Connection ~ 4600 2450
Wire Wire Line
	4800 1900 4750 1900
Wire Wire Line
	4750 1900 4750 2000
Wire Wire Line
	4750 2000 4650 2000
Wire Wire Line
	4650 2000 4650 2550
Wire Wire Line
	4450 2650 4700 2650
Wire Wire Line
	4700 2650 4700 2050
Wire Wire Line
	4700 2050 4900 2050
Wire Wire Line
	4900 2050 4900 1900
Wire Wire Line
	5000 1900 5000 2100
Wire Wire Line
	5000 2100 4750 2100
Wire Wire Line
	4750 2100 4750 2750
Wire Wire Line
	7100 3250 7100 3950
Wire Wire Line
	5800 2600 5800 3350
Wire Wire Line
	6050 5500 6550 5500
Wire Wire Line
	7200 4550 8400 4550
Wire Wire Line
	6550 5500 7750 5500
Wire Wire Line
	6050 4550 7200 4550
Wire Wire Line
	5800 5500 6050 5500
Wire Wire Line
	5550 5500 5800 5500
Wire Wire Line
	5550 4550 6050 4550
Wire Wire Line
	5550 5000 5550 5150
Wire Wire Line
	6550 4900 6550 5000
Wire Wire Line
	7750 4900 7750 5000
Wire Wire Line
	5050 5500 5550 5500
Wire Wire Line
	4400 5500 5050 5500
Wire Wire Line
	4150 5500 4400 5500
Wire Wire Line
	4150 4800 4400 4800
Wire Wire Line
	4400 4800 4650 4800
Wire Wire Line
	1900 3350 1900 4050
Wire Wire Line
	3100 3950 3250 3950
Wire Wire Line
	3900 4800 3900 5200
Wire Wire Line
	3900 4800 4150 4800
Wire Wire Line
	3000 4050 3200 4050
Wire Wire Line
	3900 5500 4150 5500
Wire Wire Line
	3450 5500 3900 5500
Wire Wire Line
	3200 5500 3450 5500
Wire Wire Line
	4500 2350 5800 2350
Wire Wire Line
	3200 4050 3250 4050
Wire Wire Line
	4500 2250 4500 2350
Wire Wire Line
	3200 4350 3200 5200
Wire Wire Line
	4800 2450 4600 2450
Wire Wire Line
	4650 4800 4750 4800
Wire Wire Line
	2050 2800 2050 3000
Wire Wire Line
	4500 1950 4500 2250
Wire Wire Line
	4600 2450 4450 2450
$EndSCHEMATC
