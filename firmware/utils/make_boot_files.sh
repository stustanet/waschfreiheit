if [ "$#" -ne 2 ]; then
	echo "USAGE: make_boot_files ELF DEST"
	echo "Generate image to flash with the wasch_v2 bootloader"
	exit
fi

arm-none-eabi-objcopy -I ihex -O binary $1 $2/FIRMWARE.BIN
crc32 $2/FIRMWARE.BIN > $2/CHECKSUM.CRC
