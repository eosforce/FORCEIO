# Customize FORCEIO


FORCEIO is a high customizable blockchain frame base on EOSIO, 
This Document introduction how to customize FORCEIO to start a chain to solve special requirement from users. 

## 0. Build Params

There is lots build params to set base function for the user's chain, 
notice this params cannot change after bios the chain.

Chain developer can set this params in `eosio_build.sh`.

### 0.1 Block Producer Number 

- `MAX_PRODUCERS` : the number of block producer, default is 23
- `BLOCK_INTERVAL_MS` : the time interval of produce blocks, in milliseconds, default is 3000, recommended more then 500 milliseconds
- `PRODUCER_REPETITIONS` : the number of block consecutive produced by a block producer in one turn, default is 1

For example,  in the EMLG EOS, `MAX_PRODUCERS` is 21, `BLOCK_INTERVAL_MS` is 500 and `PRODUCER_REPETITIONS` is 6.
and in EOSForce, `MAX_PRODUCERS` is 23, `BLOCK_INTERVAL_MS` is 3000 and `PRODUCER_REPETITIONS` is 1.

### 0.2 Main Symbol

FORCEIO support user to customize most symbols in chain.

**Core token symbol**

Like EOSIO, user can set `CORE_SYMBOL_NAME` or use "-s" to set core token symbol.

**User 's publish/privte key format**

FORCEIO support two key format: legacy format with chain symbol prefix and common format for all chain base on EOSIO.

chain developer can use legacy format with set `USE_PUB_KEY_LEGACY_PREFIX` to 1 or to use common format with set `USE_PUB_KEY_LEGACY_PREFIX` to 0.

then set `PUB_KEY_LEGACY_PREFIX` to set prefix string for legacy format, and set `PUB_KEY_BASE_PREFIX` for common format.

**System accounts**

TODO

### 0.3 Chain params for genesis

For user's chain, we need set some params in genesis data. 

- `SYSTEM_ACCOUNT_ROOT_KEY` : the key to system root account, default is `XXX1111111111111111111111111111111114T1Anm`, this mean no one can use root account.
- `CHAIN_INIT_TIMESTAMP` : chain init timestamp.

### 0.4 Token Inflation

- `BLOCK_REWARDS_BP` : reward token to bp per block. default is 100000, if core token is (EOS,4), it mean 10.0000 EOS per block will give to bp.

### 0.5 Resource Model

FORCEIO allow chain developer to select the resource model to the chain.

There is three resource in chain: cpu,net and ram. developer can set `RESOURCE_MODEL` to select the model they need.

**cpu and net**

 - `RESOURCE_MODEL` 0 : free model, there is just globe resource limit to keep block can produce for chain.
 - `RESOURCE_MODEL` 1 : fee model, just like eosforce, every action will cost fee from user.
 - `RESOURCE_MODEL` 2 : stake model, just like elmg eos, user stake token to get cpu and net resource.
 
**ram**

TODO

### 0.6 Vote

FORCEIO support chain developer to select the way to vote on the chain.

- `FROZEN_DELAY` user token stake forzen delay block num, default is 3 * 24 * 60 * 20

the default vote way is just like eosforce, developer can set `USE_MULTIPLE_VOTE` to 1 to use multiple vote like emlg eos.

developer can set `USE_BONUS_TO_VOTE` to 1 or 0 to select if send bonus token to accounts which voted.

*TODO*

## 1. Native Contract

FORCEIO support chain developer to create contract in navtive layer, it can run more fast then contract in wasm.

## 2. Plugins

## 3. Chain Features