/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <math.h>
//#include <force.token/force.token.hpp>
#include <relay.token/relay.token.hpp>
#include "sys.bridge.hpp"
#include <eosiolib/action.hpp>

namespace eosio {

   void market::addmarket(name trade,account_name trade_maker,trade_type type,name base_chain,asset base_amount,account_name base_account,uint64_t base_weight,
               name market_chain,asset market_amount,account_name market_account,uint64_t market_weight) {
         
         require_auth(trade_maker);
         require_auth(base_account);
         require_auth(market_account);
         
        auto coinbase_sym = base_amount.symbol;
         eosio_assert( coinbase_sym.is_valid(), "invalid base symbol name" );
         eosio_assert( base_amount.is_valid(), "invalid base supply");
        
        auto coinmarket_sym = market_amount.symbol;
         eosio_assert( coinmarket_sym.is_valid(), "invalid symbol name" );
         eosio_assert( market_amount.is_valid(), "invalid base supply");
      //暂时先使用相同的代币进行转换
         eosio_assert(coinbase_sym != coinmarket_sym || base_chain != market_chain,"a market must on two coin");
         
         eosio_assert( type < trade_type::trade_type_count, "invalid trade type");
         eosio_assert( market_weight > 0,"invalid market_weight");
         eosio_assert( base_weight > 0,"invalid base_weight");
         tradepairs tradepair( _self,trade_maker);
         trade_pair trademarket;
         trademarket.trade_name = trade;
         trademarket.trade_maker = trade_maker;
         trademarket.base.chain = base_chain;
         trademarket.base.amount = asset(0,coinbase_sym);
         trademarket.base.coin_maker = base_account;
         trademarket.market.chain = market_chain;
         trademarket.market.amount = asset(0,coinmarket_sym);
         trademarket.market.coin_maker = market_account;

         trademarket.type = type;
         trademarket.base_weight = base_weight;
         trademarket.market_weight = market_weight;
         trademarket.isactive = true;

         trademarket.fee.base = asset(0,coinbase_sym);
         trademarket.fee.market = asset(0,coinmarket_sym);
         trademarket.fee.fee_type = fee_type::fixed;
         //no transfer here
      
         tradepair.emplace(trade_maker, [&]( trade_pair& s ) {
            s = trademarket;
         });
   }

   void market::addmortgage(name trade,account_name trade_maker,account_name recharge_account,asset recharge_amount,coin_type type) {
      require_auth(_self);
      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );

       auto coinrecharge_sym = recharge_amount.symbol;
      eosio_assert( coinrecharge_sym.is_valid(), "invalid symbol name" );
      eosio_assert( recharge_amount.is_valid(), "invalid supply");
      eosio_assert( recharge_amount.amount > 0, "max-supply must be positive");
      name chain_name;
      if (type == coin_type::coin_base) {
            eosio_assert(coinrecharge_sym == existing->base.amount.symbol,"recharge coin is not the same coin on the market");
            chain_name = existing->base.chain;
      }
      else {
            eosio_assert(coinrecharge_sym == existing->market.amount.symbol,"recharge coin is not the same coin on the market");
            chain_name = existing->market.chain;
      }     
      //recharge transfer to self

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            if (type == coin_type::coin_base) {
                  s.base.amount = s.base.amount + recharge_amount;
            }
            else {
                  s.market.amount = s.market.amount + recharge_amount;
            }
      });
   }

   void market::claimmortgage(name trade,account_name trade_maker,asset claim_amount,coin_type type) {
      require_auth(trade_maker);
      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );

       auto coinclaim_sym = claim_amount.symbol;
      eosio_assert( coinclaim_sym.is_valid(), "invalid symbol name" );
      eosio_assert( claim_amount.is_valid(), "invalid supply");
      eosio_assert( claim_amount.amount > 0, "max-supply must be positive");
      name chain_name;
      if (type == coin_type::coin_base) {
            eosio_assert(coinclaim_sym == existing->base.amount.symbol,"recharge coin is not the same coin on the market");
            eosio_assert(claim_amount <= existing->base.amount,"overdrawn balance");
            chain_name = existing->base.chain;
      }
      else {
            eosio_assert(coinclaim_sym == existing->market.amount.symbol,"recharge coin is not the same coin on the market");
             eosio_assert(claim_amount <= existing->market.amount,"overdrawn balance");
             chain_name = existing->market.chain;
      }

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            if (type == coin_type::coin_base) {
                  s.base.amount = s.base.amount - claim_amount;
            }
            else {
                  s.market.amount = s.market.amount - claim_amount;
            }
      });
      INLINE_ACTION_SENDER(relay::token, transfer)( 
               TOKEN, 
               {_self, N(active)},
               { _self, 
                 type == coin_type::coin_base?existing->base.coin_maker:existing->market.coin_maker,
                 chain_name, 
                claim_amount, 
                 std::string("claim market transfer coin market") } );      

   }

   void market::frozenmarket(name trade,account_name trade_maker) {
      require_auth(trade_maker);

      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );
      eosio_assert( existing->isactive == true, "the market is not active" );

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.isactive = false;
      });
   }

   void market::trawmarket(name trade,account_name trade_maker) {
      require_auth(trade_maker);

      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );
      eosio_assert( existing->isactive == false, "the market is already active" );

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.isactive = true;
      });
   }

   void market::setfixedfee(name trade,account_name trade_maker,asset base,asset market) {
      require_auth(trade_maker); 
      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );
      
      auto base_sym = base.symbol;
      eosio_assert( base_sym.is_valid(), "invalid symbol name" );
      eosio_assert( base.is_valid(), "invalid supply");
      eosio_assert( base.amount >= 0, "max-supply must be positive");

      auto market_sym = market.symbol;
      eosio_assert( market_sym.is_valid(), "invalid symbol name" );
      eosio_assert( market.is_valid(), "invalid supply");
      eosio_assert( market.amount >= 0, "max-supply must be positive");

      eosio_assert(existing->fee.base.symbol == base_sym,"base asset is different coin with fee base");
      eosio_assert(existing->fee.market.symbol == market_sym,"base asset is different coin with fee base");

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.fee.base = base;
            s.fee.market = market;
            s.fee.fee_type = fee_type::fixed;
      });
   }

   void market::setprofee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio) {
      require_auth(trade_maker); 
      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );

      eosio_assert(base_ratio >= 0 && base_ratio < PROPORTION_CARD,"base_ratio must between 0 and 10000");
      eosio_assert(market_ratio >= 0 && market_ratio < PROPORTION_CARD,"market_ratio must between 0 and 10000");

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.fee.base_ratio = base_ratio;
            s.fee.market_ratio = market_ratio;
            s.fee.fee_type = fee_type::proportion;
      });
   }

   void market::setprominfee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio,asset base,asset market) {
      require_auth(trade_maker); 
      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );
      
      auto base_sym = base.symbol;
      eosio_assert( base_sym.is_valid(), "invalid symbol name" );
      eosio_assert( base.is_valid(), "invalid supply");
      eosio_assert( base.amount >= 0, "max-supply must be positive");

      auto market_sym = market.symbol;
      eosio_assert( market_sym.is_valid(), "invalid symbol name" );
      eosio_assert( market.is_valid(), "invalid supply");
      eosio_assert( market.amount >= 0, "max-supply must be positive");

      eosio_assert(existing->fee.base.symbol == base_sym,"base asset is different coin with fee base");
      eosio_assert(existing->fee.market.symbol == market_sym,"base asset is different coin with fee base");

      eosio_assert(base_ratio >= 0 && base_ratio < PROPORTION_CARD,"base_ratio must between 0 and 10000");
      eosio_assert(market_ratio >= 0 && market_ratio < PROPORTION_CARD,"market_ratio must between 0 and 10000");

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.fee.base_ratio = base_ratio;
            s.fee.market_ratio = market_ratio;
            s.fee.base = base;
            s.fee.market = market;
            s.fee.fee_type = fee_type::proportion_mincost;
      });
   }
   void market::exchange(name trade,account_name trade_maker,account_name account_covert,account_name account_recv,asset convert_amount,coin_type type) {
      require_auth(_self);

      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );
      eosio_assert( existing->isactive == true, "the market is not active" );

      auto coinconvert_sym = convert_amount.symbol;
      eosio_assert( coinconvert_sym.is_valid(), "invalid symbol name" );
      eosio_assert( convert_amount.is_valid(), "invalid supply");
      eosio_assert( convert_amount.amount > 0, "max-supply must be positive");

      if (type == coin_type::coin_base) {
            eosio_assert(coinconvert_sym == existing->base.amount.symbol,"covert coin is not the same coin on the base");
      }
      else {
            eosio_assert(coinconvert_sym == existing->market.amount.symbol,"covert coin is not the same coin on the market");
      }

      asset market_recv_amount = type != coin_type::coin_base ? existing->base.amount : existing->market.amount;
      uint64_t recv_amount;
      if (existing->type == trade_type::equal_ratio) {
            recv_amount = type != coin_type::coin_base? (convert_amount.amount * existing->base_weight / existing->market_weight) :
             (convert_amount.amount * existing->market_weight / existing->base_weight);
      }
      else if(existing->type == trade_type::bancor) 
      {
            if (type != coin_type::coin_base) { 
                  auto tempa = 1 + (double)convert_amount.amount/existing->market.amount.amount;
                  auto cw = (double)existing->market_weight/existing->base_weight;
                  recv_amount = existing->base.amount.amount * (pow(tempa,cw) - 1);
            }
            else {
                  auto tempa = 1 + (double)convert_amount.amount/existing->base.amount.amount;
                  auto cw = (double)existing->base_weight/existing->market_weight;
                  recv_amount = existing->market.amount.amount * (pow(tempa,cw) - 1);
            }
      }

      //fee
      if (type != coin_type::coin_base) { 
         if(existing->fee.fee_type == fee_type::fixed) {
            recv_amount -= existing->fee.base.amount;
         }
         else if(existing->fee.fee_type == fee_type::proportion) {
            recv_amount -= recv_amount*existing->fee.base_ratio/PROPORTION_CARD;
         }
         else if(existing->fee.fee_type == fee_type::proportion_mincost){
            if (recv_amount*existing->fee.base_ratio/PROPORTION_CARD > existing->fee.base.amount)
               recv_amount -= recv_amount*existing->fee.base_ratio/PROPORTION_CARD;
            else
            {
                  recv_amount -= existing->fee.base.amount;
            }
            
         }
      }
      else
      {
            if(existing->fee.fee_type == fee_type::fixed) {
            recv_amount -= existing->fee.market.amount;
         }
         else if(existing->fee.fee_type == fee_type::proportion) {
            recv_amount -= recv_amount*existing->fee.market_ratio/PROPORTION_CARD;
         }
         else if(existing->fee.fee_type == fee_type::proportion_mincost){
            if (recv_amount*existing->fee.market_ratio/PROPORTION_CARD > existing->fee.market.amount)
               recv_amount -= recv_amount*existing->fee.market_ratio/PROPORTION_CARD;
            else
            {
                  recv_amount -= existing->fee.market.amount;
            }
            
         }
      }
      
      
      eosio_assert(recv_amount < market_recv_amount.amount,
      "the market do not has enough dest coin");

      auto recv_asset = asset(recv_amount,market_recv_amount.symbol);

      // account_convert transfer to self
     
      tradepair.modify( *existing, 0, [&]( auto& s ) {
            if (type == coin_type::coin_base) {
                  s.base.amount = s.base.amount + convert_amount;
                  s.market.amount = s.market.amount - recv_asset;
            }
            else {
                  s.market.amount = s.market.amount + convert_amount;
                  s.base.amount = s.base.amount - recv_asset;
            }
      });
 
      INLINE_ACTION_SENDER(relay::token, transfer)( 
              TOKEN, 
            {_self, N(active)},
            { _self, 
            account_recv, 
            type == coin_type::coin_market ? existing->base.chain:existing->market.chain,
            recv_asset, 
            std::string("claim market transfer coin market") } );

   }

   void splitMemo(std::vector<std::string>& results, const std::string& memo,char separator) {
      auto start = memo.cbegin();
      auto end = memo.cend();

      for (auto it = start; it != end; ++it) {
         if (*it == separator) {
            results.emplace_back(start, it);
            start = it + 1;
         }
      }
      if (start != end) results.emplace_back(start, end);
   }

   void market::onTransfer(account_name from,name chain,asset quantity,string memo) {
      std::vector<std::string> memo_parts;
      splitMemo(memo_parts, memo, ';');
      int64_t action_type = atoi(memo_parts[0].c_str());
      name trade_name;
      trade_name.value = ::eosio::string_to_name(memo_parts[1].c_str());
      account_name trade_maker = ::eosio::string_to_name(memo_parts[2].c_str());
      if (action_type == (int64_t)action_type::addmortgage) {
         int64_t coin_type  = atoi(memo_parts[3].c_str());
         eosio::action(
            vector<eosio::permission_level>{{_self,N(active)}},
            _self,
            N(addmortgage),
            std::make_tuple(
                  trade_name,trade_maker,from,quantity,coin_type
            )
         ).send();
         
      }
      else if(action_type == (int64_t)action_type::exchange) {
         account_name recv = ::eosio::string_to_name(memo_parts[3].c_str());
         int64_t coin_type  = atoi(memo_parts[4].c_str());
         eosio::action(
            vector<eosio::permission_level>{{_self,N(active)}},
            _self,
            N(exchange),
            std::make_tuple(
                  trade_name,trade_maker,from,recv,quantity,coin_type
            )
         ).send();
      }
      else {
         eosio_assert(false, "the type from memo is invalided");
      }
   }

   void market::apply(account_name contract, action_name act) {
      if (contract == N(relay.token) && act == N(transfer)) {
         auto transfer = unpack_action_data<transfer_args>();

        // Only deal with transactions that transfer money to self
         if(transfer.to == _self)
         {
            onTransfer(transfer.from,transfer.chain,transfer.quantity,transfer.memo);
         }
         return;
      }

      if (contract != _self) return;

      // needed for EOSIO_API macro
      auto &thiscontract = *this;
      switch (act) {
            // first argument is name of CPP class, not contract
            EOSIO_API( eosio::market, (addmarket)(addmortgage)(claimmortgage)(exchange)(frozenmarket)(trawmarket)(setfixedfee)(setprofee)(setprominfee) ) 
      } 
  };

} /// namespace eosio


extern "C" {
 [[noreturn]]  void apply( uint64_t  receiver , uint64_t code, uint64_t action ) {
      eosio::market market_maker(receiver);
      market_maker.apply(code, action);
      eosio_exit(0);
   }
}

