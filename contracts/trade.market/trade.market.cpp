/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <force.token/force.token.hpp>
#include "trade.market.hpp"

namespace eosio {
   //新增一个交易对   
   void market_maker::addmarket(account_name market_maker,trade_type type,string coinbase_symbol,asset coinbase_amount,account_name coinbase_account,uint64_t base_weight,
               string coinmarket_symbol,asset coinmarket_amount,account_name coinmarket_account,uint64_t market_weight) {
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
         eosio_assert( market_weight > 0,"invalid market_weight");
          eosio_assert( base_weight > 0,"invalid base_weight");
          tradepairs tradepair( _self,market_maker);
          //先生成要插入表的对象
         trade_pair trade;
         trade.trade_id = tradepair.available_primary_key();
         trade.market_maker = market_maker;
         trade.coin_base.symbol = coinbase_symbol;
         trade.coin_base.amount = coinbase_amount;
         trade.coin_base.coin_maker = coinbase_account;
         trade.coin_market.symbol = coinmarket_symbol;
         trade.coin_market.amount = coinmarket_amount;
         trade.coin_market.coin_maker = coinmarket_account;

         trade.type = type;
         trade.base_weight = base_weight;
         trade.market_weight = market_weight;
         trade.isactive = true;
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

   void market_maker::addmortgage(int64_t trade_id,account_name market_maker,account_name recharge_account,asset recharge_amount,coin_type type) {
      require_auth(recharge_account);
      tradepairs tradepair( _self,market_maker);
      auto existing = tradepair.find( trade_id );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );

       auto coinrecharge_sym = recharge_amount.symbol;
      eosio_assert( coinrecharge_sym.is_valid(), "invalid symbol name" );
      eosio_assert( recharge_amount.is_valid(), "invalid supply");
      eosio_assert( recharge_amount.amount > 0, "max-supply must be positive");

      if (type == coin_type::coin_base) {
            eosio_assert(coinrecharge_sym == existing->coin_base.amount.symbol,"recharge coin is not the same coin on the market");
      }
      else {
            eosio_assert(coinrecharge_sym == existing->coin_base.amount.symbol,"recharge coin is not the same coin on the market");
      }     

      INLINE_ACTION_SENDER(eosio::token, transfer)( 
               config::token_account_name, 
               {recharge_account, N(active)},
               { recharge_account, 
                 _self, 
                recharge_amount, 
                 std::string("add market transfer coin market") } );

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            if (type == coin_type::coin_base) {
                  s.coin_base.amount = s.coin_base.amount + recharge_amount;
            }
            else {
                  s.coin_market.amount = s.coin_market.amount + recharge_amount;
            }
      });
   }

   void market_maker::claimmortgage(int64_t trade_id,account_name market_maker,asset claim_amount,coin_type type) {
      require_auth(market_maker);
      tradepairs tradepair( _self,market_maker);
      auto existing = tradepair.find( trade_id );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );

       auto coinclaim_sym = claim_amount.symbol;
      eosio_assert( coinclaim_sym.is_valid(), "invalid symbol name" );
      eosio_assert( claim_amount.is_valid(), "invalid supply");
      eosio_assert( claim_amount.amount > 0, "max-supply must be positive");

      if (type == coin_type::coin_base) {
            eosio_assert(coinclaim_sym == existing->coin_base.amount.symbol,"recharge coin is not the same coin on the market");
            eosio_assert(claim_amount <= existing->coin_base.amount,"overdrawn balance");
      }
      else {
            eosio_assert(coinclaim_sym == existing->coin_market.amount.symbol,"recharge coin is not the same coin on the market");
             eosio_assert(claim_amount <= existing->coin_market.amount,"overdrawn balance");
      }

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            if (type == coin_type::coin_base) {
                  s.coin_base.amount = s.coin_base.amount - claim_amount;
            }
            else {
                  s.coin_market.amount = s.coin_market.amount - claim_amount;
            }
      });
      //如何获取self 的active权限
      INLINE_ACTION_SENDER(eosio::token, transfer)( 
               config::token_account_name, 
               {_self, N(active)},
               { _self, 
                 type == coin_type::coin_base?existing->coin_base.coin_maker:existing->coin_market.coin_maker, 
                claim_amount, 
                 std::string("claim market transfer coin market") } );      

   }

   void market_maker::frozenmarket(int64_t trade_id,account_name market_maker) {
      require_auth(market_maker);

      tradepairs tradepair( _self,market_maker);
      auto existing = tradepair.find( trade_id );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );
      eosio_assert( existing->isactive == true, "the market is not active" );

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.isactive = false;
      });
   }

   void market_maker::trawmarket(int64_t trade_id,account_name market_maker) {
      require_auth(market_maker);

      tradepairs tradepair( _self,market_maker);
      auto existing = tradepair.find( trade_id );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );
      eosio_assert( existing->isactive == false, "the market is already active" );

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.isactive = true;
      });
   }

   void market_maker::exchange(int64_t trade_id,account_name market_maker,account_name account_covert,account_name account_recv,asset convert_amount,coin_type type) {
      //require_auth(_self);
      require_auth(account_covert);

      tradepairs tradepair( _self,market_maker);
      auto existing = tradepair.find( trade_id );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );
      eosio_assert( existing->isactive == true, "the market is not active" );

      auto coinconvert_sym = convert_amount.symbol;
      eosio_assert( coinconvert_sym.is_valid(), "invalid symbol name" );
      eosio_assert( convert_amount.is_valid(), "invalid supply");
      eosio_assert( convert_amount.amount > 0, "max-supply must be positive");

      asset market_recv_amount = type != coin_type::coin_base ? existing->coin_base.amount : existing->coin_market.amount;
      //这个是固定比例的计算方式    bancor的只有计算方式和这个不一样  但是bancor是否可以自由提币充币有待考量
      auto recv_amount = type != coin_type::coin_base? (convert_amount.amount * existing->base_weight / existing->market_weight) : (convert_amount.amount * existing->market_weight / existing->base_weight);

      eosio_assert(recv_amount < market_recv_amount.amount,
      "the market do not has enouth dest coin");

      auto recv_asset = asset(recv_amount,market_recv_amount.symbol);

      INLINE_ACTION_SENDER(eosio::token, transfer)( 
            config::token_account_name, 
            {account_covert, N(active)},
            { account_covert, 
            _self, 
            convert_amount, 
            std::string("claim market transfer coin market") } );

     
      tradepair.modify( *existing, 0, [&]( auto& s ) {
            if (type == coin_type::coin_base) {
                  s.coin_base.amount = s.coin_base.amount + convert_amount;
                  s.coin_market.amount = s.coin_market.amount - recv_asset;
            }
            else {
                  s.coin_market.amount = s.coin_market.amount + convert_amount;
                  s.coin_base.amount = s.coin_base.amount - recv_asset;
            }
      });
      //两个转账操作
 
      INLINE_ACTION_SENDER(eosio::token, transfer)( 
            config::token_account_name, 
            {_self, N(active)},
            { _self, 
            account_recv, 
            recv_asset, 
            std::string("claim market transfer coin market") } );

   }


} /// namespace eosio


