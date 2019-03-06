#!/bin/sh
cd /opt/FORCEIO/bin

cp /contracts/force.msig/force.msig.wasm /opt/FORCEIO/bin/data-dir
cp /contracts/force.msig/force.msig.abi /opt/FORCEIO/bin/data-dir
cp /contracts/force.relay/force.relay.wasm /opt/FORCEIO/bin/data-dir
cp /contracts/force.relay/force.relay.abi /opt/FORCEIO/bin/data-dir
cp /contracts/force.system/force.system.wasm /opt/FORCEIO/bin/data-dir
cp /contracts/force.system/force.system.abi /opt/FORCEIO/bin/data-dir
cp /contracts/force.token/force.token.wasm /opt/FORCEIO/bin/data-dir
cp /contracts/force.token/force.token.abi /opt/FORCEIO/bin/data-dir
mkdir /opt/FORCEIO/bin/data-dir/relay.token
cp /contracts/relay.token/relay.token.wasm /opt/FORCEIO/bin/data-dir/relay.token
cp /contracts/relay.token/relay.token.abi /opt/FORCEIO/bin/data-dir/relay.token

if [ -f '/opt/FORCEIO/bin/data-dir/config.ini' ]; then
    echo
else
    cp /config.ini /opt/FORCEIO/bin/data-dir
fi

if [ -f '/opt/FORCEIO/bin/data-dir/genesis.json' ]; then
    echo
else
    cp /opt/genesis.json  /opt/FORCEIO/bin/data-dir
fi

if [ -f '/opt/FORCEIO/bin/data-dir/activeacc.json' ]; then
    echo
else
    cp /opt/activeacc.json /opt/FORCEIO/bin/data-dir
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
