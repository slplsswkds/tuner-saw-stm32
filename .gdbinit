file build/Release/tuner-code.elf

target extended-remote :3333
monitor reset halt
load
tui enable
break main
continue

