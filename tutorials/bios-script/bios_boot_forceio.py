#!/usr/bin/env python

import argparse
import json
import os
import subprocess
import sys
import time

args = None
logFile = None

datas = {
      'initAccounts':[],
      'initProducers':[],
      'initProducerSigKeys':[],
      'initAccountsKeys':[],
      'maxClients':0
}

unlockTimeout = 999999999

def jsonArg(a):
    return " '" + json.dumps(a) + "' "

def run(args):
    print('bios-boot-tutorial.py:', args)
    logFile.write(args + '\n')
    if subprocess.call(args, shell=True):
        print('bios-boot-eosforce.py: exiting because of error')
        sys.exit(1)

def retry(args):
    while True:
        print('bios-boot-eosforce.py:', args)
        logFile.write(args + '\n')
        if subprocess.call(args, shell=True):
            print('*** Retry')
        else:
            break

def background(args):
    print('bios-boot-eosforce.py:', args)
    logFile.write(args + '\n')
    return subprocess.Popen(args, shell=True)

def sleep(t):
    print('sleep', t, '...')
    time.sleep(t)
    print('resume')

def replaceFile(file, old, new):
    try:
        f = open(file,'r+')
        all_lines = f.readlines()
        f.seek(0)
        f.truncate()
        for line in all_lines:
            line = line.replace(old, new)
            f.write(line)
        f.close()
    except Exception,e:
        print('bios-boot-eosforce.py: replace %s frome %s to %s err by ' % (file, old, new))
        print(e)
        sys.exit(1)

def importKeys():
    keys = {}
    for a in datas["initAccountsKeys"]:
        key = a[1]
        if not key in keys:
            keys[key] = True
            run(args.cleos + 'wallet import --private-key ' + key)

def intToCurrency(i):
    return '%d.%04d %s' % (i // 10000, i % 10000, args.symbol)

def createNodeDir(nodeIndex, bpaccount, key):
    dir = args.nodes_dir + ('%02d-' % nodeIndex) + bpaccount['name'] + '/'
    run('rm -rf ' + dir)
    run('mkdir -p ' + dir)

def createNodeDirs(inits, keys):
    for i in range(0, len(inits)):
        createNodeDir(i + 1, datas["initProducers"][i], keys[i])

def startNode(nodeIndex, bpaccount, key):
    dir = args.nodes_dir + ('%02d-' % nodeIndex) + bpaccount['name'] + '/'
    otherOpts = ''.join(list(map(lambda i: '    --p2p-peer-address 127.0.0.1:' + str(9001 + i), range(nodeIndex - 1))))
    if not nodeIndex: otherOpts += (
        '    --plugin eosio::history_plugin'
        '    --plugin eosio::history_api_plugin'
    )


    print('bpaccount ', bpaccount)
    print('key ', key, ' ', key[1])

    cmd = (
        args.nodeos +
        '    --blocks-dir ' + os.path.abspath(dir) + '/blocks'
        '    --config-dir ' + os.path.abspath(dir) + '/../../config'
        '    --data-dir ' + os.path.abspath(dir) +
        '    --http-server-address 0.0.0.0:' + str(8000 + nodeIndex) +
        '    --p2p-listen-endpoint 0.0.0.0:' + str(9000 + nodeIndex) +
        '    --max-clients ' + str(datas["maxClients"]) +
        '    --p2p-max-nodes-per-host ' + str(datas["maxClients"]) +
        '    --enable-stale-production'
        '    --producer-name ' + bpaccount['name'] +
        '    --signature-provider=' + bpaccount['bpkey'] + '=KEY:' + key[1] +
        '    --contracts-console ' +
        '    --plugin eosio::http_plugin' +
        '    --plugin eosio::chain_api_plugin' +
        '    --plugin eosio::producer_plugin' +
        otherOpts)
    with open(dir + '../' + bpaccount['name'] + '.log', mode='w') as f:
        f.write(cmd + '\n\n')
    background(cmd + '    2>>' + dir + '../' + bpaccount['name'] + '.log')

def startProducers(inits, keys):
    for i in range(0, len(inits)):
        startNode(i + 1, datas["initProducers"][i], keys[i])

def listProducers():
    run(args.cleos + 'get table eosio eosio bps')

def stepKillAll():
    run('killall keosd nodeos || true')
    sleep(.5)

def stepStartWallet():
    run('rm -rf ' + os.path.abspath(args.wallet_dir))
    run('mkdir -p ' + os.path.abspath(args.wallet_dir))
    background(args.keosd + ' --unlock-timeout %d --http-server-address 0.0.0.0:6666 --wallet-dir %s' % (unlockTimeout, os.path.abspath(args.wallet_dir)))
    sleep(.4)

def stepCreateWallet():
    run('mkdir -p ' + os.path.abspath(args.wallet_dir))
    run(args.cleos + 'wallet create --file ./pw')

def stepStartProducers():
    startProducers(datas["initProducers"], datas["initProducerSigKeys"])
    sleep(7)
    stepSetFuncs()

def stepCreateNodeDirs():
    createNodeDirs(datas["initProducers"], datas["initProducerSigKeys"])
    sleep(0.5)

def stepLog():
    run('tail -n 1000 ' + args.nodes_dir + 'biosbpa.log')
    run(args.cleos + ' get info')
    print('you can use \"alias cleost=\'%s\'\" to call cleos to testnet' % args.cleos)

def stepMkConfig():
    with open(os.path.abspath(args.config_dir) + '/genesis.json') as f:
        a = json.load(f)
        datas["initAccounts"] = a['initial_account_list']
        datas["initProducers"] = a['initial_producer_list']
    with open(os.path.abspath(args.config_dir) + '/keys/sigkey.json') as f:
        a = json.load(f)
        datas["initProducerSigKeys"] = a['keymap']
    with open(os.path.abspath(args.config_dir) + '/keys/key.json') as f:
        a = json.load(f)
        datas["initAccountsKeys"] = a['keymap']
    datas["maxClients"] = len(datas["initProducers"]) + 10

def cpContract(account):
    run('mkdir -p %s/%s/' % (os.path.abspath(args.config_dir), account))
    run('cp ' + args.contracts_dir + ('/%s/%s.abi ' % (account, account)) + os.path.abspath(args.config_dir) + "/" + account + "/")
    run('cp ' + args.contracts_dir + ('/%s/%s.wasm ' % (account, account)) + os.path.abspath(args.config_dir) + "/" + account + "/")

def stepMakeGenesis():
    run('rm -rf ' + os.path.abspath(args.config_dir))
    run('mkdir -p ' + os.path.abspath(args.config_dir))
    run('mkdir -p ' + os.path.abspath(args.config_dir) + '/keys/' )

    run('cp ' + args.contracts_dir + '/force.token/force.token.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/force.token/force.token.wasm ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/force.system/force.system.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/force.system/force.system.wasm ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/force.msig/force.msig.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/force.msig/force.msig.wasm ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/force.relay/force.relay.abi ' + os.path.abspath(args.config_dir))
    run('cp ' + args.contracts_dir + '/force.relay/force.relay.wasm ' + os.path.abspath(args.config_dir))

    cpContract('relay.token')
    cpContract('sys.match')
    cpContract('sys.bridge')

    #run('cp ./genesis-data/genesis.json ' + os.path.abspath(args.config_dir))
    #replaceFile(os.path.abspath(args.config_dir) + "/genesis.json", "#CORE_SYMBOL#", args.symbol)
    #replaceFile(os.path.abspath(args.config_dir) + "/genesis.json", "#PUB#", args.pr)
    #run('cp ./genesis-data/key.json ' + os.path.abspath(args.config_dir) + '/keys/')
    #run('cp ./genesis-data/sigkey.json ' + os.path.abspath(args.config_dir) + '/keys/')
    run('''echo "## Notify Plugin
plugin = eosio::notify_plugin
# notify-filter-on = account:action
notify-filter-on = b1:
notify-filter-on = b1:transfer
notify-filter-on = eosio:delegatebw
# http endpoint for each action seen on the chain.
notify-receive-url = http://127.0.0.1:8080/notify
# Age limit in seconds for blocks to send notifications. No age limit if set to negative.
# Used to prevent old actions from trigger HTTP request while on replay (seconds)
notify-age-limit = -1
# Retry times of sending http notification if failed.
notify-retry-times = 3" > ''' + os.path.abspath(args.config_dir) + '/config.ini')
        
    run(args.root + 'build/programs/genesis/genesis')
    run('mv ./genesis.json ' + os.path.abspath(args.config_dir))
    run('mv ./activeacc.json ' + os.path.abspath(args.config_dir))
    
    run('mv ./key.json ' + os.path.abspath(args.config_dir) + '/keys/')
    run('mv ./sigkey.json ' + os.path.abspath(args.config_dir) + '/keys/')

def cleos(cmd):
    run(args.cleos + cmd)

def pushAction(account, action, auth, data ):
    cleos("push action %s %s '%s' -p %s" % (account, action, data, auth))

def setFuncStartBlock(func_typ, num):
    pushAction("force", "setconfig", "force.config", 
        '{"typ":"%s","num":%s,"key":"","fee":"%s"}' % (func_typ, num, intToCurrency(0)))

def setFee(account, act, fee, cpu, net, ram):
    run(args.cleos +
        'set setfee ' +
        ('%s %s ' % (account, act)) +
        '"' + intToCurrency(fee) + '" ' +
        ('%d %d %d' % (cpu, net, ram)))

def getRAM(account, ram):
    cleos("push action force freeze '{\"voter\":\"%s\", \"stake\":\"%s\"}' -p %s" % (account, intToCurrency(ram), account))
    cleos("push action force vote4ram '{\"voter\":\"%s\",\"bpname\":\"biosbpa\",\"stake\":\"%s\"}' -p %s" % (account, intToCurrency(ram), account))

def setContract(account):
    getRAM(account, 50000 * 10000)
    cleos('set contract %s %s/%s/' % (account, os.path.abspath(args.config_dir), account))

def createMap(chain, token_account):
    pushAction("force.relay", "newchannel", "eosforce", 
        '{"chain":"%s","checker":"biosbpa","id":"","mroot":""}' % (chain))
    pushAction("force.relay", "newmap", "eosforce", 
        '{"chain":"%s","type":"token","id":"","act_account":"%s","act_name":"transfer","account":"relay.token","data":""}' % (chain, token_account))

def createMapToken(chain, issuer, asset):
    pushAction('relay.token', 'create', issuer,
        '{"issuer":"%s","chain":"%s","maximum_supply":"%s"}' % (issuer,chain,asset))

def relayTokenAddReward(chain,asset):
    pushAction('relay.token', 'addreward', "relay.token",
        '{"chain":"%s","supply":"%s"}' % (chain,asset))

def matchCreate(base,baseChain,baseSym,quote,quoteChain,quoteSym,feeRate,execAcc):
    pushAction('sys.match', 'create', execAcc,
        '{"base":"%s","base_chain":"%s","base_sym":"%s","quote":"%s","quote_chain":"%s","quote_sym":"%s","fee_rate":"%s","exc_acc":"%s"}' % (base,baseChain,baseSym,quote,quoteChain,quoteSym,feeRate,execAcc))

def matchMark(baseChain,baseSym,quoteChain,quoteSym):
    pushAction('sys.match', 'mark', "sys.match",
        '{"base_chain":"%s","base_sym":"%s","quote_chain":"%s","quote_sym":"%s"}' % (baseChain,baseSym,quoteChain,quoteSym))

def relayTokenTrade(tokenFrom,tokenTo,chain,quantity,type,memo):
    pushAction('relay.token', 'trade', tokenFrom,
        '{"from":"%s","to":"%s","chain":"%s","quantity":"%s","type":"%s","memo":"%s"}' % (tokenFrom,tokenTo,chain,quantity,type,memo))

def forceTokenTrade(tokenFrom,tokenTo,quantity,type,memo):
    pushAction('force.token', 'trade', tokenFrom,
        '{"from":"%s","to":"%s","quantity":"%s","type":"%s","memo":"%s"}' % (tokenFrom,tokenTo,quantity,type,memo))

def relayTokenIssue(chain,tokenTo,quantity,memo):
    pushAction('relay.token', 'issue', "eosforce",
        '{"chain":"%s","to":"%s","quantity":"%s","memo":"%s"}' % (chain,tokenTo,quantity,memo))

def stepSetFuncs():
    # we need set some func start block num
    # setFee('eosio', 'setconfig', 100, 100000, 1000000, 1000)

    # some config to set
    print('stepSetFuncs')

    pubKeys = {}
    for a in datas["initAccounts"]:
        pubKeys[a['name']] = str(a['key'])
    print(pubKeys['eosforce'])

    cleos(('set account permission %s active ' + 
          '\'{"threshold": 1,"keys": [],"accounts": [{"permission":{"actor":"force.prods","permission":"active"},"weight":1},{"permission":{"actor":"relay.token","permission":"force.code"},"weight":1}]}\'') % 
          ("eosforce"))

    cleos(('set account permission %s active ' + 
          '\'{"threshold": 1,"keys": [{"key": "%s","weight": 1}],"accounts": [{"permission":{"actor":"force.token","permission":"force.code"},"weight":1},{"permission":{"actor":"relay.token","permission":"force.code"},"weight":1}]}\'') % 
          ("eosforce", pubKeys['eosforce']))
    
    cleos(('set account permission %s active ' + 
          '\'{"threshold": 1,"keys": [{"key": "%s","weight": 1}],"accounts": [{"permission":{"actor":"force","permission":"force.code"},"weight":1},{"permission":{"actor":"force.token","permission":"force.code"},"weight":1},{"permission":{"actor":"relay.token","permission":"force.code"},"weight":1}]}\'') % 
          ("force.reward", pubKeys['force.reward']))

    cleos(('set account permission %s active ' + 
          '\'{"threshold": 1,"keys": [{"key": "%s","weight": 1}],"accounts": [{"permission":{"actor":"force.token","permission":"force.code"},"weight":1},{"permission":{"actor":"relay.token","permission":"force.code"},"weight":1},{"permission":{"actor":"sys.match","permission":"force.code"},"weight":1}]}\'') % 
          ("sys.match", pubKeys['sys.match']))

    setContract('relay.token')
    setFee('relay.token', 'on',       15000, 0, 0, 0)
    setFee('relay.token', 'create',   15000, 0, 0, 0)
    setFee('relay.token', 'issue',    15000, 0, 0, 0)
    setFee('relay.token', 'transfer', 1000,  0, 0, 0)
    setFee('relay.token', 'trade',    1000,  0, 0, 0)
    setFee('relay.token', 'addreward',    1500, 0, 0, 0)
    setFee('relay.token', 'claim', 1000,  0, 0, 0)
    setFee('relay.token', 'rewardmine',    1000,  0, 0, 0)

    setContract('sys.bridge')
    setFee('sys.bridge', 'addmarket',     15000, 0, 0, 0)
    setFee('sys.bridge', 'addmortgage',   15000, 0, 0, 0)
    setFee('sys.bridge', 'claimmortgage', 15000, 0, 0, 0)
    setFee('sys.bridge', 'exchange',      5000,  0, 0, 0)
    setFee('sys.bridge', 'frozenmarket',  15000, 0, 0, 0)
    setFee('sys.bridge', 'trawmarket',    15000, 0, 0, 0)
    setFee('sys.bridge', 'setfixedfee',   15000, 0, 0, 0)
    setFee('sys.bridge', 'setprofee',     15000, 0, 0, 0)
    setFee('sys.bridge', 'setprominfee',  15000, 0, 0, 0)
    setFee('sys.bridge', 'setweight',     15000, 0, 0, 0)
    setFee('sys.bridge', 'settranscon',     15000, 0, 0, 0)
    setFee('sys.bridge', 'removemarket',     15000, 0, 0, 0)

    setContract('sys.match')
    setFee('sys.match', 'create',  15000, 0, 0, 0)
    setFee('sys.match', 'match',   5000,  0, 0, 0)
    setFee('sys.match', 'cancel',  15000, 0, 0, 0)
    setFee('sys.match', 'mark',  5000, 0, 0, 0)
    setFee('sys.match', 'done',  5000, 0, 0, 0)

    createMap("eosforce", "force.token")
    #createMap("side", "force.token")
    sleep(3)
    createMapToken('eosforce','eosforce', "10000000.0000 EOS")
    createMapToken('eosforce','eosforce', "10000000.0000 EOSC")
    createMapToken('eosforce','eosforce', "10000000.0000 SYS")
    createMapToken('eosforce','eosforce', "10000000.0000 SSS")
    relayTokenAddReward('eosforce',"0.0000 EOS")
    relayTokenAddReward('eosforce',"0.0000 EOSC")
    relayTokenIssue("eosforce","eosforce","1000000.0000 EOS","test")
    relayTokenIssue("eosforce","eosforce","1000000.0000 EOSC","test")
    relayTokenIssue("eosforce","testb","1000000.0000 EOS","test")
    relayTokenIssue("eosforce","testb","1000000.0000 EOSC","test")
    relayTokenIssue("eosforce","testa","1000000.0000 EOS","test")
    relayTokenIssue("eosforce","testa","1000000.0000 EOSC","test")
    sleep(3)
    matchCreate("4,EOS", "eosforce", "4,EOS", "2,SYS", "", "2,SYS", "0", "eosforce")
    matchCreate("4,EOSC", "eosforce", "4,EOSC", "2,SYS", "", "2,SYS", "0", "eosforce")
    matchMark("eosforce", "4,EOS", "", "2,SYS")
    matchMark("eosforce", "4,EOSC", "", "2,SYS")
    sleep(3)
    relayTokenTrade("testa", "sys.match", "eosforce", "4.0000 EOS", "1", "testa;testa;0;100.00 SYS;0")
    forceTokenTrade("eosforce", "sys.match", "500.0000 SYS", "1", "eosforce;eosforce;0;50.00 SYS;1")
    relayTokenTrade("testa", "sys.match", "eosforce", "1.0000 EOS", "1", "testa;testa;0;  50.00   SYS;0")
    sleep(3)
    relayTokenTrade("testa", "sys.match", "eosforce", "4.0000 EOSC", "1", "testa;testa;1;10.00 SYS;0")
    forceTokenTrade("eosforce", "sys.match", "50.0000 SYS", "1", "eosforce;eosforce;1;5.00 SYS;1")
    relayTokenTrade("testa", "sys.match", "eosforce", "1.0000 EOSC", "1", "testa;testa;1;  5.00   SYS;0")

def clearData():
    stepKillAll()
    run('rm -rf ' + os.path.abspath(args.config_dir))
    run('rm -rf ' + os.path.abspath(args.nodes_dir))
    run('rm -rf ' + os.path.abspath(args.wallet_dir))
    run('rm -rf ' + os.path.abspath(args.log_path))
    run('rm -rf ' + os.path.abspath('./pw'))
    run('rm -rf ' + os.path.abspath('./config.ini'))

def restart():
    stepKillAll()
    stepMkConfig()
    stepStartWallet()
    stepCreateWallet()
    importKeys()
    stepStartProducers()
    stepLog()

# =======================================================================================================================
# Command Line Arguments
parser = argparse.ArgumentParser()

commands = [
    ('k', 'kill',           stepKillAll,                False,   "Kill all nodeos and keosd processes"),
    ('c', 'clearData',      clearData,                  False,   "Clear all Data, del ./nodes and ./wallet"),
    ('r', 'restart',        restart,                    False,   "Restart all nodeos and keosd processes"),
    ('g', 'mkGenesis',      stepMakeGenesis,            True,    "Make Genesis"),
    ('m', 'mkConfig',       stepMkConfig,               True,    "Make Configs"),
    ('w', 'wallet',         stepStartWallet,            True,    "Start keosd, create wallet, fill with keys"),
    ('W', 'createWallet',   stepCreateWallet,           True,    "Create wallet"),
    ('i', 'importKeys',     importKeys,                 True,    "importKeys"),
    ('D', 'createDirs',     stepCreateNodeDirs,         True,    "create dirs for node and log"),
    ('P', 'start-prod',     stepStartProducers,         True,    "Start producers"),
    ('l', 'log',            stepLog,                    True,    "Show tail of node's log"),
]

parser.add_argument('--root', metavar='', help="Eosforce root dir from git", default='../../')
parser.add_argument('--contracts-dir', metavar='', help="Path to contracts directory", default='build/contracts/')
parser.add_argument('--cleos', metavar='', help="Cleos command", default='build/programs/cleos/cleos --wallet-url http://127.0.0.1:6666 ')
parser.add_argument('--nodeos', metavar='', help="Path to nodeos binary", default='build/programs/nodeos/nodeos')
parser.add_argument('--nodes-dir', metavar='', help="Path to nodes directory", default='./nodes/')
parser.add_argument('--keosd', metavar='', help="Path to keosd binary", default='build/programs/keosd/keosd')
parser.add_argument('--log-path', metavar='', help="Path to log file", default='./output.log')
parser.add_argument('--wallet-dir', metavar='', help="Path to wallet directory", default='./wallet/')
parser.add_argument('--config-dir', metavar='', help="Path to config directory", default='./config')
parser.add_argument('--symbol', metavar='', help="The core symbol", default='SYS')
parser.add_argument('--pr', metavar='', help="The Public Key Start Symbol", default='FOSC')
parser.add_argument('-a', '--all', action='store_true', help="Do everything marked with (*)")

for (flag, command, function, inAll, help) in commands:
    prefix = ''
    if inAll: prefix += '*'
    if prefix: help = '(' + prefix + ') ' + help
    if flag:
        parser.add_argument('-' + flag, '--' + command, action='store_true', help=help, dest=command)
    else:
        parser.add_argument('--' + command, action='store_true', help=help, dest=command)

args = parser.parse_args()

args.cleos += '--url http://127.0.0.1:%d ' % 8001

args.cleos = args.root + args.cleos
args.nodeos = args.root + args.nodeos
args.keosd = args.root + args.keosd
args.contracts_dir = args.root + args.contracts_dir

logFile = open(args.log_path, 'a')

logFile.write('\n\n' + '*' * 80 + '\n\n\n')

haveCommand = False
for (flag, command, function, inAll, help) in commands:
    if getattr(args, command) or inAll and args.all:
        if function:
            haveCommand = True
            function()

if not haveCommand:
    print('bios-boot-eosforce.py: Tell me what to do. -a does almost everything. -h shows options.')
