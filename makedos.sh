#!/bin/bash

if [ -f memtest.bin ]
then
CSIZE="$(awk 'NR==16' mt86+_loader.asm | awk '{print $2}')";
NSIZE="$(ls -l memtest.bin | awk '{print $5}')";
sed "s/$CSIZE/$NSIZE/" mt86+_loader.asm > mt86+_loader.asm.new;
mv mt86+_loader.asm.new mt86+_loader.asm;
nasm mt86+_loader.asm;
fi
