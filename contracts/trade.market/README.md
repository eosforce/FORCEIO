# trade.market
Trade.market contract realizes the function of token exchange on the relay chain
## Functional description
Trade.market currently only offers two functions: 1. Equal conversion. 2.bancor exchange
1. Proportional exchange. Substitution of token 1 and token 2 in a fixed ratio
2. Bancor exchange according to bancor form（以后或许有改进）

## Instructions

### 1. Create market
Function：void addmarket(name trade,account_name trade_maker,trade_type type,name base_chain,asset base_amount,account_name base_account,uint64_t base_weight,
               name market_chain,asset market_amount,account_name market_account,uint64_t market_weight);
Example：cleos push action market addmarket '["eos.eosc","maker",1,"500.0000 SYS","maker",1,"1000.0000 SYS","maker",2]' -p maker@active
Parameter Description:
trade:the name of market
trade_maker：the account who create the market
type：the type of the market     1.Proportional exchange。    2.Bancor exchange
base_amount：the base coin amount    
base_account：the account which pay for the base coin  and when claim the base coin ,the account will recvive the base coin
base_weight：the weight of the base coin          
market_amount：the market coin amount
market_account：the account which pay for the market coin  and when claim the base coin ,the account will recvive the market coin
market_weight：the weight of the base coin
about the weight for example:if base_weight is 1 and market_weight is 2 then    1 base_coin can exchange 2 market_coin        

### 2. Add mortgage
Function：void addmortgage(name trade,account_name trade_maker,account_name recharge_account,asset recharge_amount,coin_type type);
Example：cleos push action market addmortgage '["eos.eosc","maker","maker","100.0000 SYS",1]' -p maker@active
Parameter Description：
trade:the name of market
trade_maker：the account who create the market
recharge_account：the account who pay the coin
recharge_amount：the account who receive the coin
type：the type of the coin            1 for base_coin and 2 for market_coin

### 3. claim mortgage
Function：void claimmortgage(name trade,account_name market_maker,asset claim_amount,coin_type type);
Example：cleos push action market claimmortgage '["eos.eosc","maker","100.0000 SYS",1]' -p maker@active
Parameter Description：
trade:the name of market
trade_maker：the account who create the market
recharge_amount：the amount to claim 
type：the type of the coin            1 for base_coin and 2 for market_coin
The token will be placed on the account specified at addmarket

### 4. exchange
Function：void exchange(name trade,account_name trade_maker,account_name account_covert,account_name account_recv,asset amount,coin_type type);
Example：cleos push action market exchange '["eos.eosc","maker","wang","zhang","100.0000 SYS",1]' -p wang@active
Parameter Description：
trade:the name of market
trade_maker：the account who create the market
account_covert:Payment account
account_recv：Receipt account
type：the type of the exchange      1 for pay base_coin  receive market_coin and 2 for pay market_coin receive base_coin

### 5. forzen the market
Function：void frozenmarket(name trade,account_name trade_maker);
Example：cleos push action market frozenmarket '["eos.eosc","maker"]' -p maker@active
Parameter Description：
trade:the name of market
trade_maker：the account who create the market
when the market is frozen the exchange function cannot be executed

### 6. thaw the market
Function：void trawmarket(name trade,account_name trade_maker);
Example：cleos push action market trawmarket '["eos.eosc","maker"]' -p maker@active
Parameter Description：
trade:the name of market
trade_maker：the account who create the market

### 7. set the fixed fee
Function：void setfixedfee(name trade,account_name trade_maker,asset base,asset market);
Example：cleos push action market setfixedfee '["eos.eosc","maker","0.1000 SYS","0.2000 SYS"]' -p maker@active
Parameter Description：
trade:the name of market
trade_maker：the account who create the market
base:the fee when buy the base coin
market：the fee when buy the market coin

### 8. set proportion fee
Function：void setprofee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio);
Example：cleos push action market setprofee '["eos.eosc","maker",20,30]' -p maker@active
Parameter Description：
trade:the name of market
trade_maker：the account who create the market
base_ratio:Proportion of fees charged for purchase of base_coin The base is 10000
market：Proportion of fees charged when purchasing market_coin  The base is 10000

### 9. set proportion with a Minimum fee
Function：void setprominfee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio,asset base,asset market);
Example：cleos push action market setprominfee '["eos.eosc","maker",20,30,"0.1000 SYS","0.2000 SYS"]' -p maker@active
Parameter Description：
trade:the name of market
trade_maker：the account who create the market
base_ratio:Proportion of fees charged for purchase of base_coin The base is 10000
market：Proportion of fees charged when purchasing market_coin  The base is 10000
base:the Minimum fee when buy the base coin
market：the Minimum fee when buy the market coin


