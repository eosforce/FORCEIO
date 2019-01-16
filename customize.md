# Customize FORCEIO


FORCEIO is a high customizable blockchain frame base on EOSIO, 
This Document introduction how to customize FORCEIO to start a chain to solve special requirement from users. 

## 0. Build Params

### 0.1 Block Producer Number 

- MAX_PRODUCERS : the number of block producer, default is 23
- BLOCK_INTERVAL_MS : the time interval of produce blocks, in milliseconds, default is 3000, recommended more then 500 milliseconds
- PRODUCER_REPETITIONS : the number of block consecutive produced by a block producer in one turn, default is 1

For example,  in the EMLG EOS, `MAX_PRODUCERS` is 21, `BLOCK_INTERVAL_MS` is 500 and `PRODUCER_REPETITIONS` is 6.
and in EOSForce, `MAX_PRODUCERS` is 23, `BLOCK_INTERVAL_MS` is 3000 and `PRODUCER_REPETITIONS` is 1.

### 0.2 Main Symbol

FORCEIO support user to customize most symbols in chain.

**Core token symbol**

`CORE_SYMBOL_NAME` "-s"

**User 's publish/privte key format**

USE_PUB_KEY_LEGACY_PREFIX

PUB_KEY_LEGACY_PREFIX

PUB_KEY_BASE_PREFIX

SYSTEM_ACCOUNT_ROOT_KEY

**System accounts**

TODO

### 0.3 Chain params for genesis

SYSTEM_ACCOUNT_ROOT_KEY

CHAIN_INIT_TIMESTAMP

### 0.4 Token Inflation

BLOCK_REWARDS_BP 100000
UPDATE_CYCLE 100

### 0.5 Resource Model

**cpu and net**

RESOURCE_MODEL

 0 Free
 1 Use Fee
 2 Stake
 
**ram**

TODO

### 0.6 Vote

FROZEN_DELAY 3 * 24 * 60 * 20

USE_MULTIPLE_VOTE

USE_BONUS_TO_VOTE

## 1. Native Contract

FORCEIO support chain developer to create contract in navtive layer, it can run more fast then contract in wasm.

## 2. TODO