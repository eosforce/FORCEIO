/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <math.h>
//#include <force.token/force.token.hpp>
#include <relay.token/relay.token.hpp>
#include "sys.bridge.hpp"
#include <force.token/force.token.hpp>


namespace eosio {
   void market::addmarket(name trade,account_name trade_maker,trade_type type,name base_chain,asset base_amount,uint64_t base_weight,
               name market_chain,asset market_amount,uint64_t market_weight) {
         
         require_auth(trade_maker);
         
         auto coinbase_sym = base_amount.symbol;
         eosio_assert( coinbase_sym.is_valid(), "invalid symbol name" );
         eosio_assert( base_amount.is_valid(), "invalid supply");
         eosio_assert( base_amount.amount >= 0, "max-supply must be positive");
        
         auto coinmarket_sym = market_amount.symbol;
         eosio_assert( coinmarket_sym.is_valid(), "invalid symbol name" );
         eosio_assert( market_amount.is_valid(), "invalid supply");
         eosio_assert( market_amount.amount >= 0, "max-supply must be positive");
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
         trademarket.base.weight = base_weight;
         trademarket.market.chain = market_chain;
         trademarket.market.amount = asset(0,coinmarket_sym);
         trademarket.market.weight = market_weight;

         trademarket.type = type;
         trademarket.isactive = true;

         trademarket.fee.base = asset(0,coinbase_sym);
         trademarket.fee.market = asset(0,coinmarket_sym);
         trademarket.fee.fee_type = fee_type::fixed;
         //no transfer here
         
         tradepair.emplace(trade_maker, [&]( trade_pair& s ) {
            s = trademarket;
         });
   }

   void market::addmortgage(name trade,account_name trade_maker,account_name recharge_account,name coin_chain,asset recharge_amount,coin_type type) {
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
            eosio_assert(coin_chain == existing->base.chain,"recharge chain is not the same chain on the market");
            
            chain_name = existing->base.chain;
      }
      else {
            eosio_assert(coinrecharge_sym == existing->market.amount.symbol,"recharge coin is not the same coin on the market");
            eosio_assert(coin_chain == existing->market.chain,"recharge chain is not the same chain on the market");
            
            chain_name = existing->market.chain;
      }
      // recharge_account transfer to self
      tradepair.modify( *existing, 0, [&]( auto& s ) {
            if (type == coin_type::coin_base) {
                  s.base.amount = s.base.amount + recharge_amount;
            }
            else {
                  s.market.amount = s.market.amount + recharge_amount;
            }
      });
   }

   void market::claimmortgage(name trade,account_name trade_maker,account_name recv_account,asset claim_amount,coin_type type) {
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

       send_transfer_action(chain_name,recv_account,claim_amount,
            std::string("claim market transfer coin market")); 
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

      eosio_assert(existing->isactive == false,"only frozen market can set fixedfee");

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

      eosio_assert(existing->isactive == false,"only frozen market can set proportion fee");
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

      eosio_assert(existing->isactive == false,"only frozen market can set proportion and min fee");

      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.fee.base_ratio = base_ratio;
            s.fee.market_ratio = market_ratio;
            s.fee.base = base;
            s.fee.market = market;
            s.fee.fee_type = fee_type::proportion_mincost;
      });
   }

   void market::setweight(name trade,account_name trade_maker,uint64_t  base_weight,uint64_t  market_weight)
   {
      require_auth(trade_maker); 
      tradepairs tradepair( _self,trade_maker);
      auto existing = tradepair.find( trade );
      eosio_assert( existing != tradepair.end(), "the market is not exist" );

      eosio_assert( market_weight > 0,"invalid market_weight");
      eosio_assert( base_weight > 0,"invalid base_weight");

      eosio_assert(existing->isactive == false,"only frozen market can set weight");
      tradepair.modify( *existing, 0, [&]( auto& s ) {
            s.base.weight = base_weight;
            s.market.weight = market_weight;
      });
   }

   void market::exchange(name trade,account_name trade_maker,account_name account_covert,account_name account_recv,name coin_chain,asset convert_amount,coin_type type) {
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
            eosio_assert(coin_chain == existing->base.chain,"covert chain is not the same chain on the base");
      }
      else {
            eosio_assert(coinconvert_sym == existing->market.amount.symbol,"covert coin is not the same coin on the market");
            eosio_assert(coin_chain == existing->market.chain,"covert chain is not the same chain on the market");
      }

      asset market_recv_amount = type != coin_type::coin_base ? existing->base.amount : existing->market.amount;
      uint64_t recv_amount;
      if (existing->type == trade_type::equal_ratio) {
            recv_amount = type != coin_type::coin_base? (convert_amount.amount * existing->base.weight / existing->market.weight) :
             (convert_amount.amount * existing->market.weight / existing->base.weight);
      }
      else if(existing->type == trade_type::bancor) 
      {
            if (type != coin_type::coin_base) { 
                  auto tempa = 1 + (double)convert_amount.amount/existing->market.amount.amount;
                  auto cw = (double)existing->market.weight/existing->base.weight;
                  recv_amount = existing->base.amount.amount * (pow(tempa,cw) - 1);
            }
            else {
                  auto tempa = 1 + (double)convert_amount.amount/existing->base.amount.amount;
                  auto cw = (double)existing->base.weight/existing->market.weight;
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
      // account_covert transfer to self
     
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
 
      send_transfer_action(type == coin_type::coin_market ? existing->base.chain:existing->market.chain,account_recv,recv_asset,
            std::string("claim market transfer coin market"));
   }

   void market::settranscon(name chain,asset quantity,account_name contract_name) {
      require_auth(_self);
      transcon trans(_self,_self);
      auto idx = trans.get_index<N(bychain)>();
      auto con = idx.find(get_contract_idx(chain, quantity));

      if( con == idx.end() ) {
         trans.emplace(_self, [&]( auto& a ) {
            a.id = trans.available_primary_key();;
            a.quantity = quantity;
            a.contract_name = contract_name;
            a.chain = chain;
         });
      }
      else {
         idx.modify(con, 0, [&]( auto& a ) {
            a.contract_name = contract_name;
         });
      }
   }

   void market::send_transfer_action(name chain,account_name recv,asset quantity,string memo) {
      transcon transfer_contract(_self, _self);
      auto idx = transfer_contract.get_index<N(bychain)>();
      const auto& contract = idx.get(get_contract_idx(chain, quantity), "contract object not found");
      if (contract.contract_name == N(relay.token)) {
            INLINE_ACTION_SENDER(relay::token, transfer)( 
              contract.contract_name, 
               {_self, N(active)},
               { _self, 
               recv, 
               chain,
               quantity, 
               memo } );
      }
      else if(contract.contract_name == N(force.token)) {
            INLINE_ACTION_SENDER(eosio::token, transfer)( 
              contract.contract_name, 
               {_self, N(active)},
               { _self, 
               recv, 
               quantity, 
               memo } );
      }
      else {
         eosio_assert(false,
            "the contract is invalid");
      }
   }

} /// namespace eosio


