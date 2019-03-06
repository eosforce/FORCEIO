# trade.market

trade.market合约实现在中继链上进行代币交换的功能
## 功能描述

trade.market目前只提供两种功能：1.等比例兑换。2.bancor兑换
+ 1.等比例兑换 提供代币1 和 代币2以一种固定比例的形式进行兑换
+ 2.bancor兑换  根据bancor形式进行兑换（暂时不建议使用bancor  等以后测试通过再说）

## 操作说明

### 1. 创建交易对

功能：
```C++
void addmarket(name trade,account_name trade_maker,trade_type type,name base_chain,asset base_amount,uint64_t base_weight,name market_chain,asset market_amount,uint64_t market_weight);
```
示例：
```bash
cleos push action sys.bridge addmarket '["eos.sys","biosbpa",1,"side","500.0000 EOS",1,"eosforce","1000.0000 SYS",2]' -p biosbpa@active
```
参数说明:
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ type：交易对的类型     1.等比例兑换。    2.bancor兑换
+ base_chain:第一种代币所在的链
+ base_amount：第一个代币的金额    amount仅仅指明币种
+ base_weight：第一个代币所占的权重          两个代币之间交换的比例是两个代币权重的比例决定的
+ market_chain:第二种代币所在的链
+ market_amount：第二个代币的金额      amount仅仅指明币种
+ market_weight：第二个代币所占的权重        两个代币之间交换的比例是两个代币权重的比例决定的

**关于权重详解：例如：base_weight=1   market_weight=2   则1个base_coin可以兑换2个market_coin**

### 2. 增加抵押

功能：
```C++
void addmortgage(name trade,account_name trade_maker,account_name recharge_account,name coin_chain，asset recharge_amount,coin_type type);
```
示例：
```bash
cleos push action relay.token trade '["eosforce","sys.bridge","side","10000.0000 EOS",2,"eos.sys;biosbpa;1"]' -p eosforce@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ recharge_account：付款的账户
+ recharge_amount：付款的金额
+ type：付款代币的类型            1代表base_coin 2代表market_coin

**说明：新修改后增加抵押使用给合约转账模式，调用relay.token合约的trade方法,该功能会由relay.token的trade方法进行调用         2代表 增加抵押的动作    memo--"eos.eosc;biosbpa;1"  是用；分割的三项 第一个是交易对名称，第二个是交易对的创建者，第三个代表充值的是第一个币还是第二个币**

### 3. 赎回抵押

功能：
```C++
void claimmortgage(name trade,account_name market_maker,account_name recv_account,asset claim_amount,coin_type type);
```
示例：
```bash
cleos push action sys.bridge claimmortgage '["eos.sys","biosbpa","eosforce","10.0000 EOS",1]' -p biosbpa@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ recv_account：赎回是转入的账户名称
+ recharge_amount：赎回的金额
+ type：赎回代币的类型            1代表base_coin 2代表market_coin


### 4. 交易

功能：
```C++
void exchange(name trade,account_name trade_maker,account_name account_covert,account_name account_recv,name coin_chain,asset amount,coin_type type);
```
示例：
```bash
cleos push action relay.token trade '["eosforce","sys.bridge","eosforce", "100.0000 SYS",3,"eos.sys;biosbpa;eosforce;2"]' -p eosforce@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ account_covert:付款的账户
+ account_recv：收款的账户
+ type：交易的类型      1代币支付base_coin 获得market_coin  2代币支付market_coin获得base_coin

该功能是唯一用户调用的功能
**说明：新修改后交易使用给合约转账模式，调用relay.token合约的trade方法         3代表 交易    memo--"eos.sys;biosbpa;eosforce;2"  是用；分割的三项 第一个是交易对名称，第二个是交易对的创建者，第三个参数是收款的账户，第四个参数代表冲的是第一个币还是第二个币**

### 5. 冻结交易

功能：
```C++
void frozenmarket(name trade,account_name trade_maker);
```
示例：
```bash
cleos push action sys.bridge frozenmarket '["eos.sys","biosbpa"]' -p biosbpa@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称

**被冻结的market 不能调用exchange功能**

### 6. 解冻交易

功能：
```C++
void trawmarket(name trade,account_name trade_maker);
```
示例：
```bash
cleos push action sys.bridge trawmarket '["eos.sys","biosbpa"]' -p biosbpa@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称

### 7. 设置固定费用

功能：
```C++
void setfixedfee(name trade,account_name trade_maker,asset base,asset market);
```
示例：
```bash
cleos push action sys.bridge setfixedfee '["eos.sys","biosbpa","0.0025 EOS","0.0036 SYS"]' -p biosbpa@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ base:购买base_coin的收取的费用
+ market：购买market_coin时收取的费用

### 8. 设置固定比例费用

功能：
```C++
void setprofee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio);
```
示例：
```bash
cleos push action sys.bridge setprofee '["eos.sys","biosbpa",5,6]' -p biosbpa@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ base_ratio:购买base_coin的收取的费用比例  基数为10000
+ market：购买market_coin时收取的费用比例  基数为10000

### 9. 设置固定比例费用含最低收费

功能：
```C++
void setprominfee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio,asset base,asset market);
```
示例：
```bash
cleos push action sys.bridge setprominfee '["eos.sys","biosbpa",5,6,"0.0025 EOS","0.0036 SYS"]' -p biosbpa@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ base_ratio:购买base_coin的收取的费用比例  基数为10000
+ market：购买market_coin时收取的费用比例  基数为10000
+ base:购买base_coin的收取的最低费用
+ market：购买market_coin时收取的最低费用

### 10. 设置两个币种之间的兑换比例

功能：
```C++
void setweight(name trade,account_name trade_maker,uint64_t base_weight,uint64_t market_weight);
```
示例：
```bash
cleos push action sys.bridge setweight '["eos.sys","biosbpa",5,6]' -p biosbpa@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ base_weight:第一个代币所占的权重          两个代币之间交换的比例是两个代币权重的比例决定的
+ market_weight:  第二个代币所占的权重          两个代币之间交换的比例是两个代币权重的比例决定的

### 11.设置代币使用转账的合约名

功能：
```C++
void settranscon(name chain,asset quantity,account_name contract_name);
```
示例：
```bash
cleos push action sys.bridge settranscon '["side","0.0000 EOS","relay.token"]' -p sys.bridge@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ contract_name：转账该币种的合约名

### 12.移除合约
功能：
```C++
void removemarket(name trade,account_name trade_maker,account_name base_recv,account_name maker_recv);
```
示例：
```bash
cleos push action sys.bridge removemarket '["eos.sys","biosbpa","eosforce","eosforce"]' -p biosbpa@active
```
参数说明：
+ trade:交易对名称
+ trade_maker：创建交易对的账户的名称
+ base_recv：当前市场中base_coin的余额转入的账户
+ maker_recv：当前市场中market_coin的余额转入的账户

**说明：移除合约之前会把对应的余额转入指定的账户，做市商不会受任何损失**
