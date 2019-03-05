#!/bin/sh
cd /opt/FORCEIO/bin

#cp /genesis.json /opt/FORCEIO/bin/data-dir
#cp /activeacc.json /opt/FORCEIO/bin/data-dir
cp /force.msig.wasm /opt/FORCEIO/bin/data-dir
cp /force.msig.abi /opt/FORCEIO/bin/data-dir
cp /force.relay.wasm /opt/FORCEIO/bin/data-dir
cp /force.relay.abi /opt/FORCEIO/bin/data-dir
cp /force.system.wasm /opt/FORCEIO/bin/data-dir
cp /force.system.abi /opt/FORCEIO/bin/data-dir
cp /force.token.wasm /opt/FORCEIO/bin/data-dir
cp /force.token.abi /opt/FORCEIO/bin/data-dir

if [ -f '/opt/FORCEIO/bin/data-dir/config.ini' ]; then
    echo
  else
    cp /config.ini /opt/FORCEIO/bin/data-dir
  fi


while :; do
    case $1 in
        --config-dir=?*)
            CONFIG_DIR=${1#*=}
            ;;
        *)
            break
        esac
    shift
  done

if [ ! "$CONFIG_DIR" ]; then
    CONFIG_DIR="--config-dir=/opt/FORCEIO/bin/data-dir"
else
    CONFIG_DIR=""
fi

exec /opt/FORCEIO/bin/nodeos $CONFIG_DIR $@
