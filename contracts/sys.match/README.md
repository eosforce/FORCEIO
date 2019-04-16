# exchange
EOS-based decentralized exchange contract<br>

##
一. contract interface overview:
1. Creating trading pairs:  (located in sys.match contrade)
void create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym, uint32_t fee_rate, account_name exc_acc);    
base:       base token symbol and precision, such as 4,BTC
base_chain: from which chain   
base_sym:   from which token
quote:	   quote token symbol and precision，such as 2,USDT
quote_chain:from which chain
quote_sym:  from which token
fee_rate:   fee rate (for example，fee_rate is 10， fee rate is 10/10000)
exc_acc:    exchange account
note：for example, BTC/USDT pair，BTC is base token，USDT is quote token   

2. trade (located in relay.token contract)
void trade(account_name from, account_name to, name chain, asset quantity, trade_type type, string memo);   
from: 	transfer orginating account    
to:      escrow account
chain: 	from which chain  
quantity: transfer token amount
type:    trade type (1 is matching trading)
memo:    trade parameters, format is：payer;receiver;trading pair ID;price;buy or sell (1 is buying, 0 is selling)，for example, "testa;testa;0;4000.00 CUSDT;0"

3. Cancel the order
void cancel(account_name maker, uint32_t type, uint64_t order_or_pair_id);   
maker:            the account who made the order
type:             0 - cancel designated order, 1 - cancel designated pairs' order, 2 - cancel all orders
order_or_pair_id: order ID when type is 0, pair_id when type is 1

4、mark the trading pair for counting trading turnover
void mark(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym);
base_chain: from which chain   
base_sym:   from which token
quote_chain:from which chain
quote_sym:  from which token

5、 claim the trading commissions mannually
void claim(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc, account_name fee_acc);
base_chain: from which chain   
base_sym:   from which token
quote_chain:from which chain
quote_sym:  from which token
exc_acc:    exchange account
fee_acc:    fee account 

6、freeze the trading pair
void freeze(uint32_t id);
id: pair id

7、unfreeze the trading pair
id: pair id

##
二. exchange operation steps

Setting alias for convenience
alias efc='/home/yinhp/work/FORCEIO/build/programs/cleos/cleos --wallet-url http://127.0.0.1:6666 --url http://127.0.0.1:8001'

1.Compiling contracts      
eosiocpp -o ${FORCEIO}/contracts/sys.match/sys.match.wast  ${FORCEIO}/contracts/sys.match/sys.match.cpp
eosiocpp -g ${FORCEIO}/contracts/sys.match/sys.match.api  ${FORCEIO}/contracts/sys.match/sys.match.cpp

2. deploy sys.match contract  
Assuring sys.match account has sufficient RAM
efc push action force freeze '{"voter":"sys.match", "stake":"10000.0000 SYS"}' -p sys.match
efc push action force vote4ram '{"voter":"sys.match", "bpname":"biosbpa","stake":"10000.0000 SYS"}' -p sys.match  

efc set contract sys.match ${FORCEIO}/build/contracts/sys.match -p sys.match

3. set fees:  
efc set setfee sys.match create "0.0100 SYS" 100000 1000000 1000
efc set setfee relay.token trade "0.0100 SYS" 100000 1000000 1000
efc set setfee sys.match cancel "0.0100 SYS" 100000 1000000 1000
efc set setfee sys.match done "0.0100 SYS" 100000 1000000 1000
efc set setfee sys.match mark "0.0100 SYS" 100000 1000000 1000
efc set setfee sys.match claim "0.0100 SYS" 100000 1000000 1000
efc set setfee sys.match freeze "0.0100 SYS" 100000 1000000 1000
efc set setfee sys.match unfreeze "0.0100 SYS" 100000 1000000 1000

4、authorization 
exchange account authorization ( for example, exchange account is biosbpa )
efc set account permission biosbpa active '{"threshold": 1,"keys": [{"key": "FOSC8UaaTwjdoBETaDmwy1735avE3hLAkLUkyxHsGFnTjJs6MbvZ1n","weight": 1}],"accounts": [{"permission":{"actor":"relay.token","permission":"force.code"},"weight":1}]}' owner -p biosbpa

sys.match account authorization
efc set account permission sys.match active '{"threshold": 1,"keys": [{"key": "FOSC8J3iph4DnSWM1vvoEfBD9vRPBZEQv4Fd4ZdzhLGxEh6NzbxvNX","weight": 1}],"accounts": [{"permission":{"actor":"sys.match","permission":"force.code"},"weight":1}]}' owner -p sys.match

revoke authorization:  
#efc set account permission eosfund1 active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": []}' owner -p eosfund1     

5、create trading pairs
efc push action sys.match create '["4,BTC", "btc1", "4,CBTC", "2,USDT", "usdt1", "2,CUSDT", "10", "biosbpa"]' -p biosbpa

6. view orderbook:     
efc get table sys.match sys.match orderbook       

{
  "rows": [{
      "id": 1,
      "pair_id": 0,
      "maker": "testb",
      "base": "1.0000 BTC",
      "price": "3950.00 USDT",
      "bid_or_ask": 1
    },{
      "id": 3,
      "pair_id": 0,
      "maker": "testa",
      "base": "1.0000 BTC",
      "price": "4200.00 USDT",
      "bid_or_ask": 0
    }
  ],
  "more": false
}

7、mark the trading pair
efc push action sys.match mark '["eosforce", "4,EOS", "", "2,SYS"]' -p sys.match

8、claim fees
efc push action sys.match claim '["btc1", "4,CBTC", "usdt1", "2,CUSDT", "biosbpa", "biosbpb"]' -p biosbpa

9、freeze the trading pair
efc push action sys.match freeze '["0"]' -p biosbpa

10、unfreeze the trading pair
efc push action sys.match unfreeze '["0"]' -p biosbpa

##
三. user exchange steps:  
user 1:testb, user 2:testa, 

1. buy tokens           

efc push action relay.token trade '["testb", "sys.match", "usdt1", "39500.0000 CUSDT", "1", "testb;testb;0;3950.00 CUSDT;1"]' -p testb

2. sell tokens   

efc push action relay.token trade '["testa", "sys.match", "btc1", "4.0000 CBTC", "1", "testa;testa;0;4000.00 CUSDT;0"]' -p testa

3. cancel the order     

efc push action sys.match cancel '["0"]' -p testa

note：only can cancel orders made by themself

a) cancel the specified order (type is 0)
efc push action sys.match cancel '["testa", "0", "0"]' -p testa

or:

efc match cancel testa 0 0 -p testa

b) cancel designated pairs' order (type is 1)
efc push action sys.match cancel '["testa", "1", "1"]' -p testa

or:

efc match cancel testa 1 1 -p testa

c) cancel all orders (type is 2)
efc push action sys.match cancel '["testb", "2", "0"]' -p testb

or:

efc match cancel testb 2 0 -p testb
