/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   enum  class trade_type:int64_t {
      equal_ratio=1,
      trade_type_count
   };

   enum  class coin_type:int64_t {
      coin_base=1,
      coin_market,
      trade_type_count
   };

   class base_calculate {  //所有计算的基类
      public:
         virtual double GetBuyAmount(double amount);      //获取买的时候的结果
         virtual double GetSellAmount(double amount);     //获取卖的时候的结果
         virtual ~base_calculate() {}
   };

   class equal_ratio : public base_calculate {  //等比例兑换
      public:
         double GetBuyAmount(double amount) override;
         double GetSellAmount(double amount) override;
         void SetRatio(double ratio);
      private:
         double dRatio;                         //兑换比例
   };



   class market_maker : public contract {
      public:
         market_maker( account_name self ):contract(self){}
         void addmarket(account_name market_maker,trade_type type,string coinbase_symbol,asset coinbase_amount,account_name coinbase_account,
               string coinmarket_symbol,asset coinmarket_amount,account_name coinmarket_account);//新增一个交易对    

         void addmortgage(int64_t trade_id,account_name account,asset Amount,coin_type type);//增加抵押
         void claimmortgage(int64_t trade_id,account_name account,asset Amount,coin_type type);//取出抵押     是否需要冻结一段时间有待考虑

         //需要有一个查询功能  根据做市商查询这个做市商所有的交易对
         //散户使用的功能就是做交易
         void exchange(int64_t trade_id,account_name account_base,account_name account_market,asset amount,coin_type type);
      private:
         struct coin {
            string symbol;    //币种
            asset  amount;    //抵押数额
            account_name   coin_maker;//做市商账户
         };

         struct trade_pair {  //交易对
            uint64_t trade_id;        //交易对的唯一ID     这个会写入内存表里面
            trade_type type;        //交易类型 用于表明是等比例或者bancor或者其他交易类型  这个也用于计算公式的设置
            coin  coin_base;        //一般指共识程度较高的币种    做市上一般用这个币种来担保coin_market
            coin  coin_market;      //一般指做市上自己的币种   和coin_base属于相互担保的关系
            account_name   market_maker;// 做市商的账户     抵押中继链资源等都需要这个账户   暂时没有抵押资源这些操作
            base_calculate calfunc;       //计算公式

            uint64_t primary_key()const { return trade_id; }
         };
         
         typedef eosio::multi_index<N(tradepairs), trade_pair> tradepairs;
   };

   EOSIO_ABI( market_maker, (addmarket)(addmortgage)(claimmortgage)(exchange) )
} /// namespace eosio
