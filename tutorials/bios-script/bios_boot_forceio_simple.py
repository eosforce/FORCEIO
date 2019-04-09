#!/usr/bin/env python

import argparse
import json
import os
import subprocess
import sys
import time
from forceio import *

def startNode(nodeIndex, inits, keys):
    dir = datas.nodes_dir + '/'
    run('rm -rf ' + dir)
    run('mkdir -p ' + dir)

    otherOpts = ''.join(list(map(lambda i: (' --producer-name %s --signature-provider=%s=KEY:%s ' % (inits[i]['name'], inits[i]['bpkey'], keys[i][1])), range(len(inits)))))
    otherOpts += (
        '    --plugin eosio::history_plugin'
        '    --plugin eosio::history_api_plugin'
    )

    cmd = (
        datas.args.nodeos +
        '    --blocks-dir ' + dir + '/blocks'
        '    --config-dir ' + datas.config_dir + 
        '    --data-dir ' + dir +
        ('    --http-server-address 0.0.0.0:%d0%02d' % (datas.args.use_port, nodeIndex)) +
        ('    --p2p-listen-endpoint 0.0.0.0:%d1%02d' % (datas.args.use_port, nodeIndex)) +
        '    --max-clients ' + str(datas.maxClients) +
        '    --p2p-max-nodes-per-host ' + str(datas.maxClients) +
        '    --enable-stale-production ' +
        '    --contracts-console ' +
        '    --plugin eosio::http_plugin' +
        '    --plugin eosio::chain_api_plugin' +
        '    --plugin eosio::producer_plugin' +
        otherOpts)
    with open(dir + '/nodes.log', mode='w') as f:
        f.write(cmd + '\n\n')
    background(cmd + '    2>>' + dir + '/nodes.log')

def stepKillAll():
    run('killall keosd nodeos || true')
    sleep(.5)

def stepStartProducers():
    startNode(1, datas.initProducers, datas.initProducerSigKeys)
    sleep(3)

def stepLog():
    run('tail -n 1000 ' + datas.nodes_dir + '/nodes.log')
    cleos('get info')
    print('you can use \"alias cleost=\'%s\'\" to call cleos to testnet' % datas.args.cleos)

def stepMkConfig():
    global datas
    with open(datas.config_dir + '/genesis.json') as f:
        a = json.load(f)
        datas.initAccounts = a['initial_account_list']
        datas.initProducers = a['initial_producer_list']
    with open(datas.config_dir + '/keys/sigkey.json') as f:
        a = json.load(f)
        datas.initProducerSigKeys = a['keymap']
    with open(datas.config_dir + '/keys/key.json') as f:
        a = json.load(f)
        datas.initAccountsKeys = a['keymap']
    datas.maxClients = len(datas.initProducers) + 10

def cpContract(account):
    run('mkdir -p %s/%s/' % (datas.config_dir, account))
    run('cp ' + datas.contracts_dir + ('/%s/%s.abi ' % (account, account)) + datas.config_dir + "/" + account + "/")
    run('cp ' + datas.contracts_dir + ('/%s/%s.wasm ' % (account, account)) + datas.config_dir + "/" + account + "/")

def stepMakeGenesis():
    rm(datas.config_dir)
    run('mkdir -p ' + datas.config_dir)
    run('mkdir -p ' + datas.config_dir + '/keys/' )

    run('cp ' + datas.contracts_dir + '/force.token/force.token.abi ' + datas.config_dir)
    run('cp ' + datas.contracts_dir + '/force.token/force.token.wasm ' + datas.config_dir)
    run('cp ' + datas.contracts_dir + '/force.system/force.system.abi ' + datas.config_dir)
    run('cp ' + datas.contracts_dir + '/force.system/force.system.wasm ' + datas.config_dir)
    run('cp ' + datas.contracts_dir + '/force.msig/force.msig.abi ' + datas.config_dir)
    run('cp ' + datas.contracts_dir + '/force.msig/force.msig.wasm ' + datas.config_dir)
    run('cp ' + datas.contracts_dir + '/force.relay/force.relay.abi ' + datas.config_dir)
    run('cp ' + datas.contracts_dir + '/force.relay/force.relay.wasm ' + datas.config_dir)
    
    run('cp ' + datas.args.root + 'tutorials/bios-script/genesis-data/config.ini ' + datas.config_dir)

    cpContract('relay.token')
    cpContract('sys.bridge')
    cpContract('sys.match')
        
    run(datas.args.root + 'build/programs/genesis/genesis')
    run('mv ./genesis.json ' + datas.config_dir)
    run('mv ./activeacc.json ' + datas.config_dir)
    
    run('mv ./key.json ' + datas.config_dir + '/keys/')
    run('mv ./sigkey.json ' + datas.config_dir + '/keys/')

def clearData():
    stepKillAll()
    rm(os.path.abspath(datas.args.data_dir))

def restart():
    stepKillAll()
    stepMkConfig()
    stepStartProducers()
    stepLog()

# =======================================================================================================================
# Command Line Arguments  
commands = [
    ('k', 'kill',           stepKillAll,                False,   "Kill all nodeos and keosd processes"),
    ('c', 'clearData',      clearData,                  False,   "Clear all Data, del ./nodes and ./wallet"),
    ('r', 'restart',        restart,                    False,   "Restart all nodeos and keosd processes"),
    ('g', 'mkGenesis',      stepMakeGenesis,            True,    "Make Genesis"),
    ('m', 'mkConfig',       stepMkConfig,               True,    "Make Configs"),
    ('P', 'start-prod',     stepStartProducers,         True,    "Start producers"),
    ('l', 'log',            stepLog,                    True,    "Show tail of node's log"),
]

parser = argparse.ArgumentParser()

parser.add_argument('--cleos', metavar='', help="Cleos command", default='build/programs/cleos/cleos')
parser.add_argument('--nodeos', metavar='', help="Path to nodeos binary", default='build/programs/nodeos/nodeos')
parser.add_argument('--keosd', metavar='', help="Path to keosd binary", default='build/programs/keosd/keosd')

parserArgsAndRun(parser, commands)
