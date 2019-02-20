# trade.market
trade.market合约实现在中继链上进行代币交换的功能
## 功能描述
trade.market目前只提供两种功能：1.等比例兑换。2.bancor兑换
1.等比例兑换 提供代币1 和 代币2以一种固定比例的形式进行兑换
2.bancor兑换  根据bancor形式进行兑换（以后或许有改进）

## 操作说明

### 1. 创建交易对
功能：addmarket(name trade,account_name trade_maker,trade_type type,asset base_amount,account_name base_account,uint64_t base_weight,
               asset market_amount,account_name market_account,uint64_t market_weight)
示例：cleos push action market addmarket '["eos.eosc","maker",1,"500.0000 SYS","maker",1,"1000.0000 SYS","maker",2]' -p market@active maker@active
参数说明:
trade:交易对名称
trade_maker：创建交易对的账户的名称
type：交易对的类型     1.等比例兑换。    2.bancor兑换
base_amount：第一个代币的金额    amount本身也包含币种
base_account：第一个代币绑定的账户     该账户需要在创建交易对的时候先在交易对上充值一部分代币，每次从交易对取代币是打到这个账户上面
base_weight：第一个代币所占的权重          两个代币之间交换的比例是两个代币权重的比例决定的
market_amount：第二个代币的金额      amount本身也包含币种
market_account：第二个代币绑定的账户       该账户需要在创建交易对的时候先在交易对上充值一部分代币，每次从交易对取代币是打到这个账户上面
market_weight：第二个代币所占的权重        两个代币之间交换的比例是两个代币权重的比例决定的

### 2. 增加抵押
功能：void addmortgage(name trade,account_name trade_maker,account_name recharge_account,asset recharge_amount,coin_type type);
示例：cleos push action market addmortgage '["eos.eosc","maker","maker","100.0000 SYS",1]' -p maker@active
参数说明：
trade:交易对名称
trade_maker：创建交易对的账户的名称
recharge_account：付款的账户
recharge_amount：付款的金额
type：付款代币的类型            1代表base_coin 2代表market_coin

### 3. 赎回抵押
功能：void claimmortgage(name trade,account_name market_maker,asset claim_amount,coin_type type);
示例：cleos push action market claimmortgage '["eos.eosc","maker","100.0000 SYS",1]' -p maker@active
参数说明：
trade:交易对名称
trade_maker：创建交易对的账户的名称
recharge_amount：赎回的金额
type：赎回代币的类型            1代表base_coin 2代表market_coin
赎回抵押将自动将代币打到创建交易对绑定的账户上面

### 4. 交易
功能：void exchange(name trade,account_name trade_maker,account_name account_covert,account_name account_recv,asset amount,coin_type type);
示例：cleos push action market exchange '["eos.eosc","maker","wang","zhang","100.0000 SYS",1]' -p wang@active
参数说明：
trade:交易对名称
trade_maker：创建交易对的账户的名称
account_covert:付款的账户
account_recv：收款的账户
type：交易的类型      1代币支付base_coin 获得market_coin  2代币支付market_coin获得base_coin
该功能是唯一用户调用的功能

### 5. 冻结交易
功能：void frozenmarket(name trade,account_name trade_maker);
示例：cleos push action market frozenmarket '["eos.eosc","maker"]' -p maker@active
参数说明：
trade:交易对名称
trade_maker：创建交易对的账户的名称
被冻结的market 不能调用exchange功能

### 6. 解冻交易
功能：void trawmarket(name trade,account_name trade_maker);
示例：cleos push action market trawmarket '["eos.eosc","maker"]' -p maker@active
参数说明：
trade:交易对名称
trade_maker：创建交易对的账户的名称

### 7. 设置固定费用
功能：void setfixedfee(name trade,account_name trade_maker,asset base,asset market);
示例：cleos push action market setfixedfee '["eos.eosc","maker","0.1000 SYS","0.2000 SYS"]' -p maker@active
参数说明：
trade:交易对名称
trade_maker：创建交易对的账户的名称
base:购买base_coin的收取的费用
market：购买market_coin时收取的费用

### 8. 设置固定比例费用
功能：void setprofee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio);
示例：cleos push action market setprofee '["eos.eosc","maker",20,30]' -p maker@active
参数说明：
trade:交易对名称
trade_maker：创建交易对的账户的名称
base_ratio:购买base_coin的收取的费用比例  基数为10000
market：购买market_coin时收取的费用比例  基数为10000

### 9. 设置固定比例费用含最低收费
功能：void setprominfee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio,asset base,asset market);
示例：cleos push action market setprominfee '["eos.eosc","maker",20,30,"0.1000 SYS","0.2000 SYS"]' -p maker@active
参数说明：
trade:交易对名称
trade_maker：创建交易对的账户的名称
base_ratio:购买base_coin的收取的费用比例  基数为10000
market：购买market_coin时收取的费用比例  基数为10000
base:购买base_coin的收取的最低费用
market：购买market_coin时收取的最低费用


