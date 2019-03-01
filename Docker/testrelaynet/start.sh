#!/bin/sh
cd /opt/FORCEIO/
./bios_boot_relay_chain.py -a --root ./ --data-dir relaynet --use-port 10
tail -f ./relaynet/nodes/biosbpa.log