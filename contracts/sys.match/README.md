# exchange
基于EOS的去中心化交易合约<br>

##
一. 合约接口说明:
1. 创建交易对:  （位于 sys.match 合约）   
void create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym, uint32_t fee_rate, account_name exc_acc);    
base:       显示基础代币符号及精度, 例如：4,BTC
base_chain: 映射自哪个链    
base_sym:   映射自哪种token
quote:	   显示报价代币符号及精度，例如：2,USDT
quote_chain:映射自哪个链
quote_sym:  映射自哪种token
fee_rate:   费率（万分之几， 比如，fee_rate 为 10， 手续费为万分之10）
exc_acc:    交易所账户
说明：比如 BTC/USDT 交易对，BTC 为基础代币，USDT 为报价代币    

2. 交易 （位于 relay.token 合约）
void trade(account_name from, account_name to, name chain, asset quantity, trade_type type, string memo);   
from: 	转账发起方账户    
to:      交易所托管账户
chain: 	转账代币映射自哪个链  
quantity: 转账代币数量
type:    交易类型 (1 为撮合交易)
memo:    交易参数, 格式为：payer;receiver;交易对ID;价格;买或卖（1为买, 0为卖），例： testa;testa;0;4000.00 CUSDT;0

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
efc push action force freeze '{"voter":"sys.match", "stake":"10000.0000 SYS"}' -p sys.match
efc push action force vote4ram '{"voter":"sys.match", "bpname":"biosbpa","stake":"10000.0000 SYS"}' -p sys.match  

efc set contract sys.match ${FORCEIO}/build/contracts/exchange -p sys.match

3. 设置费用:  
efc set setfee sys.match create "0.0100 SYS" 100000 1000000 1000
efc set setfee relay.token trade "0.0100 SYS" 100000 1000000 1000
efc set setfee sys.match cancel "0.0100 SYS" 100000 1000000 1000


4、授权 
交易所账户授权 （假设交易所账户为 biosbpa ）
efc set account permission biosbpa active '{"threshold": 1,"keys": [{"key": "FOSC8UaaTwjdoBETaDmwy1735avE3hLAkLUkyxHsGFnTjJs6MbvZ1n","weight": 1}],"accounts": [{"permission":{"actor":"relay.token","permission":"force.code"},"weight":1}]}' owner -p biosbpa

sys.match 账户授权
efc set account permission sys.match active '{"threshold": 1,"keys": [{"key": "FOSC8J3iph4DnSWM1vvoEfBD9vRPBZEQv4Fd4ZdzhLGxEh6NzbxvNX","weight": 1}],"accounts": [{"permission":{"actor":"sys.match","permission":"force.code"},"weight":1}]}' owner -p sys.match

撤销授权:  
#efc set account permission eosfund1 active '{"threshold": 1,"keys": [{"key": "FOSC7PpbGuYrXKxDVLrUUxRETjYZLb6bfe2MXKUBhZhVwM3P9JSgV5","weight": 1}],"accounts": []}' owner -p eosfund1     

5、创建交易对
efc push action sys.match create '["4,BTC", "btc1", "4,CBTC", "2,USDT", "usdt1", "2,CUSDT", "10", "biosbpa"]' -p biosbpa

6. 查看交易所的币交易情况:     
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


##
三. 用户交易步骤:  
用户1:testb, 用户2:testa, 

1. 买币(买币前授权 force.code 给合约账号 eosforce,买币后撤销权限)              

efc push action relay.token trade '["testb", "eosfund1", "usdt1", "39500.0000 CUSDT", "1", "testb;testb;0;3950.00 CUSDT;1"]' -p testb

2. 卖币   

efc push action relay.token trade '["testa", "eosfund1", "btc1", "4.0000 CBTC", "1", "testa;testa;0;4000.00 CUSDT;0"]' -p testa

3. 撤销订单     

efc push action eosforce cancel '["0"]' -p testa

注意：只能撤销自己下的订单
