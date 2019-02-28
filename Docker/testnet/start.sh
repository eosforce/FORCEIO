#!/bin/sh
cd /opt/FORCEIO/
./bios_boot_forceio.py -a --root ./ --data-dir testnet
tail -f ./testnet/nodes/biosbpa.log