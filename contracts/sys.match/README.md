# exchange
EOS-based decentralized exchange contract<br>

##
一. contract interface overview:
1. register exchange account
void regex(account_name exc_acc);
exc_acc:    exchange account
note: you  must freeze 1000 or more CDX to register exchange account, only exchange account can create trading pairs

2. Creating trading pairs:  (located in sys.match contract)
void create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym, account_name exc_acc);    
base:       base token symbol and precision, such as 4,BTC
base_chain: from which chain   
base_sym:   from which token
quote:	   quote token symbol and precision，such as 2,USDT
quote_chain:from which chain
quote_sym:  from which token
exc_acc:    exchange account
note：for example, BTC/USDT pair，BTC is base token，USDT is quote token   

3. open trading pair 
void open(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc);
base_chain: from which chain   
base_sym:   from which token
quote_chain:from which chain
quote_sym:  from which token
exc_acc:    exchange account
note: you  must freeze 1000 or more CDX to open trading pair, only exchange account can open trading pair

4、freeze the trading pair
void freeze(uint32_t id);
id: pair id

5、unfreeze the trading pair
id: pair id

6. close trading pair 
void close(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc);
base_chain: from which chain   
base_sym:   from which token
quote_chain:from which chain
quote_sym:  from which token
exc_acc:    exchange account

7. deposit exchange token
void trade(account_name from, account_name to, asset quantity, trade_type type, string memo);
from: 	transfer orginating account    
to:      escrow account
quantity: transfer token amount
type:    trade type (1 is matching trading)
memo:    trade parameters, format points，for example, "points"

8. withdraw exchange token
void withdraw(account_name to, asset quantity);
from:       transfer orginating account 
quantity:   transfer token amount

9. set trading pair fee rate
void setfee(account_name exc_acc, uint32_t pair_id, uint32_t rate);
exc_acc: exchange account
pair_id: pair id
rate:    fee rate, for example, 10 means  10 / 10000

10. enable exchange tokens
void enpoints(account_name exc_acc, uint32_t pair_id, symbol_type points_sym);
exc_acc:    exchange account
pair_id:    pair id
points_sym: exchange token symbol

11. set minimum trading quantity
void setminordqty(account_name exc_acc, uint32_t pair_id, asset min_qty);
exc_acc:    exchange account
pair_id:    pair id
min_qty:    minimum base trading quantity

11. trade (located in relay.token contract)
void trade(account_name from, account_name to, name chain, asset quantity, trade_type type, string memo);   
from: 	transfer orginating account    
to:      escrow account
chain: 	from which chain  
quantity: transfer token amount
type:    trade type (1 is matching trading)
memo:    trade parameters, format trade;receiver;trading pair ID;price;buy or sell (1 is buying, 0 is selling);exchange account;referer;fee type(1 means pay by ratio, 2 means pay by exchange token)，for example, "trade;testa;0;4000.00 CUSDT;0;biosbpa;testa;1"

12. Cancel the order
void cancel(account_name maker, uint32_t type, uint64_t order_or_pair_id);   
maker:            the account who made the order
type:             0 - cancel designated order, 1 - cancel designated pairs' order, 2 - cancel all orders
order_or_pair_id: order ID when type is 0, pair_id when type is 1

13、mark the trading pair for counting trading turnover
void mark(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym);
base_chain: from which chain   
base_sym:   from which token
quote_chain:from which chain
quote_sym:  from which token

14、 claim the trading commissions mannually
void claim(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc, account_name fee_acc);
base_chain: from which chain   
base_sym:   from which token
quote_chain:from which chain
quote_sym:  from which token
exc_acc:    exchange account
fee_acc:    fee account 

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
efc set account permission biosbpa active '{"threshold": 1,"keys": [{"key": "CDX5muUziYrETi5b6G2Ev91dCBrEm3qir7PK4S2qSFqfqcmouyzCr","weight": 1}],"accounts": [{"permission":{"actor":"sys.match","permission":"force.code"},"weight":1}]}' owner -p biosbpa

sys.match account authorization
efc set account permission sys.match active '{"threshold": 1,"keys": [{"key": "FOSC8J3iph4DnSWM1vvoEfBD9vRPBZEQv4Fd4ZdzhLGxEh6NzbxvNX","weight": 1}],"accounts": [{"permission":{"actor":"sys.match","permission":"force.code"},"weight":1}]}' owner -p sys.match

revoke authorization:  
#efc set account permission eosfund1 active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": []}' owner -p eosfund1     

5. register exchange account_name
efc push action force freeze '{"voter":"biosbpa", "stake":"10000.0000 CDX"}' -p biosbpa
efc push action sys.match regex '["biosbpa"]' -p biosbpa

6、 create trading pairs
efc push action sys.match create '["4,BTC", "btc1", "4,CBTC", "2,USDT", "usdt1", "2,CUSDT", "biosbpa"]' -p biosbpa

7、 open trading pairs
efc push action force freeze '{"voter":"biosbpa", "stake":"10000.0000 CDX"}' -p biosbpa
efc push action sys.match open '["btc1", "4,CBTC", "usdt1", "2,CUSDT", "biosbpa"]' -p biosbpa

8、set exchange trading fees
efc push action sys.match setfee '["biosbpa", "1", "10"]' -p biosbpa

9、set minimum trading quantity
efc push action sys.match setminordqty '["biosbpa", "1", "10.0 CBTC"]' -p biosbpa

10. enable exchange tokens
efc push action sys.match enpoints '["biosbpa", "1", "4,CDX"]' -p biosbpa

11. close trading pairs
efc push action sys.match close '["btc1", "4,CBTC", "usdt1", "2,CUSDT", "biosbpa"]'

12、freeze the trading pair
efc push action sys.match freeze '["0"]' -p biosbpa

13、unfreeze the trading pair
efc push action sys.match unfreeze '["0"]' -p biosbpa

14、view orderbook:     
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

15、mark the trading pair
efc push action sys.match mark '["eosforce", "4,EOS", "", "2,SYS"]' -p sys.match

16、claim fees
efc push action sys.match claim '["btc1", "4,CBTC", "usdt1", "2,CUSDT", "biosbpa", "biosbpb"]' -p biosbpa

##
三. user exchange steps:  
user 1:testb, user 2:testa, 

1. deposit exchange tokens
efc push action force.token transfer '["testb", "sys.match", "4000.0000 CDX", "points"]' -p testb

2. withdraw exchange tokens
efc push action sys.match withdraw '["testb", "1000.0000 CDX"]' -p testb

3. buy tokens           

efc push action relay.token trade '["testb", "sys.match", "usdt1", "39500.0000 CUSDT", "1", "trade;testb;1;3950.00 CUSDT;1;biosbpa;;2"]' -p testb

4. sell tokens   

efc push action relay.token trade '["testa", "sys.match", "btc1", "4.0000 CBTC", "1", "trade;testa;1;4000.00 CUSDT;0;biosbpa;testc;1"]' -p testa

5. cancel the order     

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
