#!/bin/sh
cd /opt/eosio/bin

cp /genesis.json /opt/eosio/bin/data-dir
cp /activeacc.json /opt/eosio/bin/data-dir
cp /force.msig.wasm /opt/eosio/bin/data-dir
cp /force.msig.abi /opt/eosio/bin/data-dir
cp /force.relay.wasm /opt/eosio/bin/data-dir
cp /force.relay.abi /opt/eosio/bin/data-dir
cp /force.system.wasm /opt/eosio/bin/data-dir
cp /force.system.abi /opt/eosio/bin/data-dir
cp /force.token.wasm /opt/eosio/bin/data-dir
cp /force.token.abi /opt/eosio/bin/data-dir

if [ -f '/opt/eosio/bin/data-dir/config.ini' ]; then
    echo
  else
    cp /config.ini /opt/eosio/bin/data-dir
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
    CONFIG_DIR="--config-dir=/opt/eosio/bin/data-dir"
else
    CONFIG_DIR=""
fi

exec /opt/eosio/bin/nodeos $CONFIG_DIR $@