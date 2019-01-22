/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "trade.market.hpp"

namespace eosio {
   //新增一个交易对   
   void market_maker::addmarket(account_name market_maker,trade_type type,string coinbase_symbol,asset coinbase_amount,account_name coinbase_account,
         string coinmarket_symbol,asset coinmarket_amount,account_name coinmarket_account,double ratio) {
         //需要三个账户的权限
         require_auth(market_maker);
         require_auth(coinbase_account);
         require_auth(coinmarket_account);
         //校验币是否可用
         auto coinbase_sym = coinbase_amount.symbol;
         eosio_assert( coinbase_sym.is_valid(), "invalid symbol name" );
         eosio_assert( coinbase_amount.is_valid(), "invalid supply");
         eosio_assert( coinbase_amount.amount > 0, "max-supply must be positive");
         //校验币是否可用
         auto coinmarket_sym = coinbase_amount.symbol;
         eosio_assert( coinmarket_sym.is_valid(), "invalid symbol name" );
         eosio_assert( coinmarket_amount.is_valid(), "invalid supply");
         eosio_assert( coinmarket_amount.amount > 0, "max-supply must be positive");
         //校验type
         eosio_assert( type < trade_type::trade_type_count, "invalid trade type");
         eosio_assert( ratio > 0,"invalid ratio");

          tradepairs tradepair( _self,_self);
      //    //先生成要插入表的对象
         trade_pair trade;
         trade.trade_id = N(coinbase_symbol + coinmarket_symbol);
         trade.market_maker = market_maker;
         trade.coin_base.symbol = coinbase_symbol;
         trade.coin_base.amount = coinbase_amount;
         trade.coin_base.coin_maker = coinbase_account;
         trade.coin_market.symbol = coinmarket_symbol;
         trade.coin_market.amount = coinmarket_amount;
         trade.coin_market.coin_maker = coinmarket_account;

         trade.type = type;

         //插表的操作
         tradepair.emplace(trade.trade_id, [&]( trade_pair& s ) {
            //各种赋值的语句
            s = trade;
            if(s.type == trade_type::equal_ratio)
            {
                  equal_ratio* equal = new equal_ratio();
                  equal->SetRatio(ratio);
                  s.calfunc = equal;
            }
         });


   }

   void market_maker::addmortgage(int64_t trade_id,account_name account,asset Amount,coin_type type) {

   }

   void market_maker::claimmortgage(int64_t trade_id,account_name account,asset Amount,coin_type type) {

   }

   void market_maker::exchange(int64_t trade_id,account_name account_base,account_name account_market,asset amount,coin_type type) {

   }

   double equal_ratio::GetBuyAmount(double amount) {
         return amount / dRatio;
   }

   double equal_ratio::GetSellAmount(double amount) {
         return amount * dRatio;
   }

} /// namespace eosio


