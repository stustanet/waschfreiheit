arm-none-eabi-gdb --batch --command=gdb_init.cfg main.elf
nemiver --remote=localhost:3333 --gdb-binary=/usr/bin/arm-none-eabi-gdb main.elf
