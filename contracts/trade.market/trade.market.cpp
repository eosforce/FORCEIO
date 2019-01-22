/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "trade.market.hpp"

namespace eosio {
   //新增一个交易对   
   void market_maker::addmarket(account_name market_maker,trade_type type,string coinbase_symbol,asset coinbase_amount,account_name coinbase_account,
         string coinmarket_symbol,asset coinmarket_amount,account_name coinmarket_account) {

   }

   void market_maker::addmortgage(int64_t trade_id,account_name account,asset Amount,coin_type type) {

   }

   void market_maker::claimmortgage(int64_t trade_id,account_name account,asset Amount,coin_type type) {

   }

   void market_maker::exchange(int64_t trade_id,account_name account_base,account_name account_market,asset amount,coin_type type) {

   }

} /// namespace eosio


