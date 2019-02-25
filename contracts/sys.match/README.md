# exchange
基于EOS的去中心化交易合约<br>

##
一. 合约接口说明:
1. 创建交易对:     
void create(symbol_type base, symbol_type quote);    
base:   基础代币符号及精度, 例如：4,BTC    
quote:	报价代币符号及精度，例如：2,USDT
说明：比如 BTC/USDT 交易对，BTC 为基础代币，USDT 为报价代币    

2. 交易
void trade( account_name payer, asset base, asset price, uint32_t bid_or_ask);   
payer: 	下单账号    
base:   基础代币及数量
price: 	报价代币及数量  
bid_or_ask:	1: 买入基础代币, 0: 卖出基础代币

3. 撤销订单  
void cancel(uint64_t order_id);    
order_id: 订单编号  


##
二. 交易所操作步骤:合约账号:eosforce (可在主网上查看)

为方便操作设置客户端别名：
alias efc='/home/yinhp/work/FORCEIO/build/programs/cleos/cleos --wallet-url http://127.0.0.1:6666 --url http://127.0.0.1:8001'

1.编译合约      
eosiocpp -o ${FORCEIO}/contracts/etbexchange/exchange.wast  ${FORCEIO}/contracts/exchange/exchange.cpp
eosiocpp -g ${FORCEIO}/contracts/etbexchange/exchange.api  ${FORCEIO}/contracts/exchange/exchange.cpp

2. 部署交易合约  
部署前确保账户有足够内存
efc push action force freeze '{"voter":"eosforce", "stake":"10000.0000 SYS"}' -p eosforce
efc push action force vote4ram '{"voter":"eosforce", "bpname":"biosbpa","stake":"10000.0000 SYS"}' -p eosforce  

efc set contract eosforce ${FORCEIO}/build/contracts/exchange -p eosforce

3. 设置费用:  
efc set setfee eosforce create "0.0100 SYS" 100000 1000000 1000
efc set setfee eosforce trade "0.0100 SYS" 100000 1000000 1000
efc set setfee eosforce cancel "0.0100 SYS" 100000 1000000 1000


4、授权 
先授权给托管账户(例如：eosfund1):     
efc set account permission eosfund1 active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": [{"permission":{"actor":"eosforce","permission":"force.code"},"weight":1}]}' owner -p eosfund1

撤销授权:  
efc set account permission eosfund1 active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": []}' owner -p eosfund1     

5、创建交易对
efc push action eosforce create '["4,BTC", "2,USDT"]' -p eosforce

6. 查看交易所的币交易情况:     
efc get table eosforce eosforce orderbook       

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


##
三. 用户交易步骤:  
用户1:testb, 用户2:testa, 

1. 买币(买币前授权 force.code 给合约账号 eosforce,买币后撤销权限)              

授权: 
efc set account permission testb active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": [{"permission":{"actor":"eosforce","permission":"force.code"},"weight":1}]}' owner -p testb

买 BTC 币: 
efc push action eosforce trade '["testb", "10.0000 BTC", "3950.00 USDT", "1"]' -p testb

撤销授权: 
efc set account permission testb active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": []}' owner -p testb

2. 卖币   

授权: 
efc set account permission testa active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": [{"permission":{"actor":"eosforce","permission":"force.code"},"weight":1}]}' owner -p testa

卖 BTC 币: 
efc push action eosforce trade '["testa", "4.0000 BTC", "4000.00 USDT", "0"]' -p testa

撤销授权: 
efc set account permission testa active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": []}' owner -p testa


3. 撤销订单     

efc push action eosforce cancel '["0"]' -p testa

注意：只能撤销自己下的订单
