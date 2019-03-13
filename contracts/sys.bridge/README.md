# trade.market

Trade.market contract realizes the function of token exchange on the relay chain

## Functional description

Trade.market currently only offers two functions: 1. Equal conversion. 2.bancor exchange
+ 1. Proportional exchange. Substitution of token 1 and token 2 in a fixed ratio

## Instructions

### 1. Create market
Function：
```C++
void addmarket(name trade,account_name trade_maker,trade_type type,name base_chain,asset base_amount,uint64_t base_weight,name market_chain,asset market_amount,uint64_t market_weight);
```
Example：
```bash
cleos push action sys.bridge addmarket '["eos.sys","biosbpa",1,"side","500.0000 EOS",1,"eosforce","1000.0000 SYS",2]' -p biosbpa@active
```
Parameter Description:
+ trade:the name of market
+ trade_maker：the account who create the market
+ type：the type of the market     1.Proportional exchange。    2.Bancor exchange
+ base_chain : the chain which base coin is created at
+ base_amount：the base coin amount    
+ base_weight：the weight of the base coin  
+ market_chain : the chain which market coin is created at        
+ market_amount：the market coin amount
+ market_weight：the weight of the base coin
**about the weight for example:if base_weight is 1 and market_weight is 2 then    1 base_coin can exchange 2 market_coin**        

### 2. Add mortgage
Function：
```C++
void addmortgage(name trade,account_name trade_maker,account_name recharge_account,name coin_chain，asset recharge_amount,coin_type type);
```
Example：
```bash
cleos push action relay.token trade '["eosforce","sys.bridge","side","10000.0000 EOS",2,"eos.sys;biosbpa;1"]' -p eosforce@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ recharge_account：the account who pay the coin
+ coin_chain : the chain which the coin is created at
+ recharge_amount：the account who receive the coin
+ type：the type of the coin            1 for base_coin and 2 for market_coin

**Description: After the new modification, the mortgage is added to the trade function of the token contract.2 represents the action of adding mortgage.Memo--"eos.eosc;biosbpa;1" is used; divided three items.The first is the name of the transaction pair, the second is the creator of the transaction pair, and the third is the first coin or the second currency.**

### 3. claim mortgage
Function：
```C++
void claimmortgage(name trade,account_name market_maker,account_name recv_account,asset claim_amount,coin_type type);
```
Example：
```bash
cleos push action sys.bridge claimmortgage '["eos.sys","biosbpa","eosforce","10.0000 EOS",1]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ recv_account: the accout who receive the coin
+ recharge_amount：the amount to claim 
+ type：the type of the coin            1 for base_coin and 2 for market_coin


### 4. exchange
Function：
```C++
void exchange(name trade,account_name trade_maker,account_name account_covert,account_name account_recv,name coin_chain,asset amount,coin_type type);
```
Example：
```bash
cleos push action relay.token trade '["eosforce","sys.bridge","eosforce", "100.0000 SYS",3,"eos.sys;biosbpa;eosforce;2"]' -p eosforce@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ account_covert:Payment account
+ account_recv：Receipt account
+ coin_chain : the chain which the coin is created at
+ amount:the amount you want to exchange
+ type：the type of the exchange      1 for pay base_coin  receive market_coin and 2 for pay market_coin receive base_coin

**Description: After the new modification, the mortgage is added to the trade function of the token contract.3 represents the transaction.Memo--"eos.sys;biosbpa;eosforce;2" is used; divided four items.The first is the name of the transaction pair, the second is the creator of the transaction pair, the third parameter is the account for the payment, and the fourth parameter represents whether the first coin or the second currency**

### 5. forzen the market
Function：
```C++
void frozenmarket(name trade,account_name trade_maker);
```
Example：
```bash
cleos push action sys.bridge frozenmarket '["eos.sys","biosbpa"]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market

**when the market is frozen the exchange function cannot be executed**

### 6. thaw the market
Function：
```C++
void trawmarket(name trade,account_name trade_maker);
```
Example：
```bash
cleos push action sys.bridge trawmarket '["eos.sys","biosbpa"]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market

### 7. set the fixed fee
Function：
```C++
void setfixedfee(name trade,account_name trade_maker,asset base,asset market);
```
Example：
```bash
cleos push action sys.bridge setfixedfee '["eos.sys","biosbpa","0.0025 EOS","0.0036 SYS"]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ base:the fee when buy the base coin
+ market：the fee when buy the market coin

### 8. set proportion fee
Function：
```C++
void setprofee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio);
```
Example：
```bash
cleos push action sys.bridge setprofee '["eos.sys","biosbpa",5,6]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ base_ratio:Proportion of fees charged for purchase of base_coin The base is 10000
+ market：Proportion of fees charged when purchasing market_coin  The base is 10000

### 9. set proportion with a Minimum fee
Function：
```C++
void setprominfee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio,asset base,asset market);
```
Example：
```bash
cleos push action sys.bridge setprominfee '["eos.sys","biosbpa",5,6,"0.0025 EOS","0.0036 SYS"]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ base_ratio:Proportion of fees charged for purchase of base_coin The base is 10000
+ market：Proportion of fees charged when purchasing market_coin  The base is 10000
+ base:the Minimum fee when buy the base coin
+ market：the Minimum fee when buy the market coin

### 10. Set the conversion ratio between two currencies

Function：
```C++
void setweight(name trade,account_name trade_maker,uint64_t base_weight,uint64_t market_weight);
```
Example：
```bash
cleos push action sys.bridge setweight '["eos.sys","biosbpa",5,6]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ base_weight：the weight of the base coin
+ market_weight：the weight of the base coin

### 11.Set the contract name for token transfer

Function：
```C++
void settranscon(name chain,asset quantity,account_name contract_name);
```
Example：
```bash
cleos push action sys.bridge settranscon '["side","0.0000 EOS","relay.token"]' -p sys.bridge@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ contract_name：the contract name to transfer the coin

### 12.remove the market
Function：
```C++
void removemarket(name trade,account_name trade_maker,account_name base_recv,account_name maker_recv);
```
Example：
```bash
cleos push action sys.bridge removemarket '["eos.sys","biosbpa","eosforce","eosforce"]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ base_recv：the account to receive the base coin
+ maker_recv：the account to receive the market coin

**Note: Before the contract is removed, the corresponding balance will be transferred to the designated account, and the market maker will not suffer any loss.**

### 13.force.token trade 
Function：
```C++
 void trade(    account_name from,account_name to,asset quantity,func_type type,string memo);
```
Example：
```bash
cleos push action force.token trade '["eosforce","sys.bridge","10000.0000 EOS",2,"eos.sys;biosbpa;1"]' -p eosforce@active
```
Parameter Description：
+ from:the account to transfer the coin
+ to：the account to receive the coin      the account must be the contract account 
+ quantity：the amount of the coin
+ type：the type of the trade     1 for create function of contract sys.match.  2 for addmortgage function of sys.bridge. 3 for exchange function of  contract sys.bridge 
+ memo：Memo is used to store the parameters of related functions, use; split related parameters to view related functions

**Description: when  passing the chain,the value of chain is self on force.token**

### 14.relay.token trade
Function：
```C++
 void trade(    account_name from,account_name to,name chain,asset quantity,trade_type type,string memo);
```
Example：
```bash
cleos push action relay.token trade '["eosforce","sys.bridge","side","10000.0000 EOS",2,"eos.sys;biosbpa;1"]' -p eosforce@active
```
Parameter Description：
+ from:the account to transfer the coin
+ to：the account to receive the coin      the account must be the contract account 
+ quantity：the amount of the coin
+ chain : the chain which coin is created at
+ type：the type of the trade     1 for create function of contract sys.match.  2 for addmortgage function of sys.bridge. 3 for exchange function of  contract sys.bridge 
+ memo：Memo is used to store the parameters of related functions, use; split related parameters to view related functions

### 15.Take fees
Function：
```C++
void claimfee(name trade,account_name trade_maker,account_name recv_account,coin_type type);
```
Example：
```bash
cleos push action sys.bridge claimfee '["eos.sys","biosbpa","eosforce",2]' -p biosbpa@active
```
Parameter Description：
+ trade:the name of market
+ trade_maker：the account who create the market
+ recv_account：the account to receive the  coin
+ type：the type of the coin            1 for base_coin and 2 for market_coin


## Market maker market making process
### 1.New trade pair
```bash
cleos push action sys.bridge addmarket '["eos.sys","biosbpa",1,"side","500.0000 EOS",1,"eosforce","1000.0000 SYS",2]' -p biosbpa@active
```
### 2.Recharge coin to the trade pair
```bash
cleos push action relay.token trade '["eosforce","sys.bridge","side","10000.0000 EOS",2,"eos.sys;biosbpa;1"]' -p eosforce@active
```
**Note: The trade pair can be used after the transaction has the above currency.**
### 3.forzen the market
```bash
cleos push action sys.bridge frozenmarket '["eos.sys","biosbpa"]' -p biosbpa@active
```
**After freezing, the user cannot perform the exchange operation, and the market maker can modify the relevant parameters of the trade pair.**
### 4.Set transaction fee
```bash
cleos push action sys.bridge setprofee '["eos.sys","biosbpa",5,6]' -p biosbpa@active
```
**This will set the cost of redeeming base_coin to five ten thousandth and the cost of redeeming market_coin to six ten thousandths.**
### 5.thaw the market
```bash
cleos push action sys.bridge trawmarket '["eos.sys","biosbpa"]' -p biosbpa@active
```
### 6.User trading
```bash
cleos push action relay.token trade '["eosforce","sys.bridge","eosforce", "100.0000 SYS",3,"eos.sys;biosbpa;eosforce;2"]' -p eosforce@active
```
User eosforce spent 100.0000 SYS on the eosforce chain on this transaction pair in exchange for the side chain 49.9750 EOS
### 7.Take coins
```bash
cleos push action sys.bridge claimmortgage '["eos.sys","biosbpa","eosforce","10.0000 SYS",2]' -p biosbpa@active
```
**This operation can take out the 100.0000 SYS that the user just traded.**
### 8.Take Fees
```bash
cleos push action sys.bridge claimfee '["eos.sys","biosbpa","eosforce",2]' -p biosbpa@active
```