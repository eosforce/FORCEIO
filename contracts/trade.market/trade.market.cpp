/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <force.token/force.token.hpp>
#include "trade.market.hpp"

namespace eosio {
   //新增一个交易对   
   void market_maker::addmarket(account_name market_maker,trade_type type,string coinbase_symbol,asset coinbase_amount,account_name coinbase_account,
         string coinmarket_symbol,asset coinmarket_amount,account_name coinmarket_account,uint64_t ratio) {
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
        auto coinmarket_sym = coinmarket_amount.symbol;
         eosio_assert( coinmarket_sym.is_valid(), "invalid symbol name" );
         eosio_assert( coinmarket_amount.is_valid(), "invalid supply");
         eosio_assert( coinmarket_amount.amount > 0, "max-supply must be positive");
      //暂时先使用相同的代币进行转换
     //    eosio_assert(coinbase_sym != coinmarket_sym,"a market must on two coin");
         //校验type
         eosio_assert( type < trade_type::trade_type_count, "invalid trade type");
         eosio_assert( ratio > 0,"invalid ratio");
          tradepairs tradepair( _self,market_maker);
          //先生成要插入表的对象
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
         trade.ratio = ratio;
         //打币操作
         INLINE_ACTION_SENDER(eosio::token, transfer)( 
               config::token_account_name, 
               {coinbase_account, N(active)},
               { coinbase_account, 
                 _self, 
                coinbase_amount, 
                 std::string("add market transfer coin base") } );
          //打币操作
          
         INLINE_ACTION_SENDER(eosio::token, transfer)( 
               config::token_account_name, 
               {coinmarket_account, N(active)},
               { coinmarket_account, 
                 _self, 
                coinmarket_amount, 
                 std::string("add market transfer coin market") } );
         //插表的操作
         tradepair.emplace(market_maker, [&]( trade_pair& s ) {
            //各种赋值的语句
            s = trade;
         });

   }

   void market_maker::addmortgage(int64_t trade_id,account_name account,asset amount,coin_type type) {

   }

   void market_maker::claimmortgage(int64_t trade_id,account_name account,asset amount,coin_type type) {

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


