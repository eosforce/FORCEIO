import argparse
import json
import os
import subprocess
import sys
import time
import copy
    
class dataSet(object):
   """data for forceio script"""
   def __init__(self):
      self.initAccounts = []
      self.initProducers = []
      self.initProducerSigKeys = []
      self.initAccountsKeys = []
      self.maxClients = 0
      self.logFile = {}
      self.args = {}   
      
      self.config_dir = ""
      
   def processData(self, args, logs):
      self.args = args
      self.logFile = logs
      
      self.config_dir = os.path.abspath(args.config_dir)
      self.nodes_dir = os.path.abspath(args.nodes_dir)
      self.wallet_dir = os.path.abspath(args.wallet_dir)
      self.log_path = os.path.abspath(args.log_path)
      self.contracts_dir = args.contracts_dir
   
      
datas = dataSet()

def jsonArg(a):
    return " '" + json.dumps(a) + "' "
    
def log2File(l):
   print('forceio script:', l)
   datas.logFile.write(l + '\n')

def run(args):
    log2File(args)
    if subprocess.call(args, shell=True):
        print('bios-boot-eosforce.py: exiting because of error')
        sys.exit(1)
        
def rm(path_to_del):
   run('rm -rf ' + path_to_del)

def retry(args):
    while True:
        log2File(args)
        if subprocess.call(args, shell=True):
            print('*** Retry')
        else:
            break

def background(args):
    log2File(args)
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

def cleos(cmd):
    run(datas.args.cleos + cmd)
    
def intToCurrency(i):
    return '%d.%04d %s' % (i // 10000, i % 10000, datas.args.symbol)

def pushAction(account, action, auth, data ):
    cleos("push action %s %s '%s' -p %s" % (account, action, data, auth))

def setFuncStartBlock(func_typ, num):
    pushAction("force", "setconfig", "force.config", 
        '{"typ":"%s","num":%s,"key":"","fee":"%s"}' % (func_typ, num, intToCurrency(0)))

def setFee(account, act, fee, cpu, net, ram):
    cleos(
        'set setfee ' +
        ('%s %s ' % (account, act)) +
        '"' + intToCurrency(fee) + '" ' +
        ('%d %d %d' % (cpu, net, ram)))

def getRAM(account, ram):
    cleos("push action force freeze '{\"voter\":\"%s\", \"stake\":\"%s\"}' -p %s" % (account, intToCurrency(ram), account))
    cleos("push action force vote4ram '{\"voter\":\"%s\",\"bpname\":\"biosbpa\",\"stake\":\"%s\"}' -p %s" % (account, intToCurrency(ram), account))

def setContract(account):
    getRAM(account, 5000 * 10000)
    cleos('set contract %s %s/%s/' % (account, datas.config_dir, account))

def parserArgsAndRun(parser, commands):
    parser.add_argument('--root', metavar='', help="Eosforce root dir from git", default='../../')
    parser.add_argument('--contracts-dir', metavar='', help="Path to contracts directory", default='build/contracts/')
    parser.add_argument('--log-path', metavar='', help="Path to log file", default='./output.log')
    parser.add_argument('--nodes-dir', metavar='', help="Path to nodes directory", default='./nodes/')
    parser.add_argument('--wallet-dir', metavar='', help="Path to wallet directory", default='./wallet/')
    parser.add_argument('--config-dir', metavar='', help="Path to config directory", default='./config')
    parser.add_argument('--symbol', metavar='', help="The core symbol", default='SYS')
    parser.add_argument('--pr', metavar='', help="The Public Key Start Symbol", default='FOSC')
    parser.add_argument('-a', '--all', action='store_true', help="Do everything marked with (*)")
    parser.add_argument('--use-port', metavar='', help="port X to listen, http X001-X099, p2p X101-X199 and wallet X666", default='8')
    
    for (flag, command, function, inAll, help) in commands:
        prefix = ''
        if inAll: prefix += '*'
        if prefix: help = '(' + prefix + ') ' + help
        if flag:
             parser.add_argument('-' + flag, '--' + command, action='store_true', help=help, dest=command)
        else:
            parser.add_argument('--' + command, action='store_true', help=help, dest=command)

    args = parser.parse_args()
    
    args.use_port = int(args.use_port)
    
    if args.use_port >= 50 or args.use_port <= 4 :
       print("args --use-port should between 5-50")
       sys.exit(1)
    
    args.cleos += ' --wallet-url http://127.0.0.1:%d666' % args.use_port
    args.cleos += ' --url http://127.0.0.1:%d001 ' % args.use_port
    args.cleos = args.root + args.cleos
    args.nodeos = args.root + args.nodeos
    args.keosd = args.root + args.keosd
    args.contracts_dir = args.root + args.contracts_dir

    logFile = open(args.log_path, 'a')
    logFile.write('\n\n' + '*' * 80 + '\n\n\n')

    global datas
    datas.processData(args, logFile)
    
    haveCommand = False
    for (flag, command, function, inAll, help) in commands:
        if getattr(args, command) or inAll and args.all:
            if function:
                haveCommand = True
                function()
    if not haveCommand:
      print('bios-boot-eosforce.py: Tell me what to do. -a does almost everything. -h shows options.')