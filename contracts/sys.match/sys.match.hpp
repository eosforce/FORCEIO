//
// Created by root1 on 18-7-26.
//

#ifndef EOSIO_EXCHANGE_H
#define EOSIO_EXCHANGE_H

#pragma once


//#include <eosio.system/native.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/eosio.hpp>
#include <relay.token/relay.token.hpp>
#include <string>


namespace exchange {
   using namespace eosio;
   using std::string;
   using eosio::asset;
   using eosio::symbol_type;
   const account_name relay_token_acc = N(relay.token);

   typedef double real_type;

   class exchange : public contract  {

   public:
      exchange(account_name self) : contract(self) {}

      void create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym, uint32_t fee_rate, account_name exc_acc);

      void match( account_name payer, account_name receiver, asset base, asset price, uint32_t bid_or_ask );
      
      void cancel(account_name maker, uint32_t type, uint64_t order_id, uint32_t pair_id);
      
      void done(account_name exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask, time_point_sec timestamp);
      
      void done_helper(account_name exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask);
      
      void match_for_bid( account_name payer, account_name receiver, asset base, asset price);
      
      void match_for_ask( account_name payer, account_name receiver, asset base, asset price);
      
      void mark(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym);
      
      asset calcfee(asset quant, uint64_t fee_rate);

      inline symbol_type get_pair_base( uint32_t pair_id ) const;
      inline symbol_type get_pair_quote( uint32_t pair_id ) const;
      inline account_name get_exchange_account( uint32_t pair_id ) const;
      inline void get_pair( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) const;
      inline void get_mark( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) const;
      inline void upd_mark( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, asset sum, asset vol );

   //private:
      struct trading_pair{
          uint32_t id;
      
          symbol_type base;
          name        base_chain;
          symbol_type base_sym;

          symbol_type quote;
          name        quote_chain;
          symbol_type quote_sym;
          
          uint32_t    fee_rate;
          account_name exc_acc;
          
          uint32_t primary_key() const { return id; }
          uint128_t by_pair_sym() const { return (uint128_t(base.name()) << 64) | quote.name(); }
      };
      
      struct order {
         uint64_t        id;
         uint32_t        pair_id;
         account_name    maker;
         account_name    receiver;
         asset           base;
         asset           price;
         uint32_t        bid_or_ask;
         time_point_sec  timestamp;

         uint64_t primary_key() const { return id; }
         uint128_t by_pair_price() const { 
             //print("\n by_pair_price: order: id=", id, ", pair_id=", pair_id, ", bid_or_ask=", bid_or_ask,", base=", base,", price=", price,", maker=", maker, ", key=", (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 1 : 0)) << 64 | (uint64_t)price.amount);
             return (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 1 : 0)) << 64 | (uint64_t)price.amount; }
      };
      
      struct deal_info {
         uint32_t    id;
         name        base_chain;
         symbol_type base_sym;

         name        quote_chain;
         symbol_type quote_sym;

         asset       sum;
         asset       vol;
         
         uint32_t    reset_block_height = current_block_num();
         
         uint64_t primary_key() const { return id; }
      };
/* 
   ordered_unique<tag<by_code_scope_table>,
            composite_key< table_id_object,
               member<table_id_object, account_name, &table_id_object::code>,
               member<table_id_object, scope_name,   &table_id_object::scope>,
               member<table_id_object, table_name,   &table_id_object::table>
            >
         >      
*/
      typedef eosio::multi_index<N(pairs), trading_pair,
         indexed_by< N(idxkey), const_mem_fun<trading_pair, uint128_t,  &trading_pair::by_pair_sym>>
      > trading_pairs;    
      typedef eosio::multi_index<N(orderbook), order,
         indexed_by< N(idxkey), const_mem_fun<order, uint128_t,  &order::by_pair_price>>
      > orderbook;    
      typedef eosio::multi_index<N(deals), deal_info> deals;    

      void insert_order(
                       orderbook &orders, 
                       uint32_t pair_id, 
                       uint32_t bid_or_ask, 
                       asset base, 
                       asset price, 
                       account_name payer, 
                       account_name receiver);

      static asset to_asset( account_name code, name chain, symbol_type sym, const asset& a );
      static asset convert( symbol_type expected_symbol, const asset& a );
      static int64_t precision(uint64_t decimals)
      {
         int64_t p10 = 1;
         int64_t p = (int64_t)decimals;
         while( p > 0  ) {
            p10 *= 10; --p;
         }
         return p10;
      }
   };
   
   symbol_type exchange::get_pair_base( uint32_t pair_id ) const {
      trading_pairs   trading_pairs_table(_self, _self);
     
      //auto itr1 = trading_pairs_table.find(pair_id);
      // eosio_assert(itr1 != trading_pairs_table.end(), "trading pair does not exist");
      
      auto walk_table_range = [&]( auto itr, auto end_itr ) {
         print("\n ---------------- begin to trading_pairs table: ----------------");
         for( ; itr != end_itr; ++itr ) {
             print("\n pair: id=", itr->id);
         }
         print("\n -------------------- walk through trading_pairs table ends ----------------:");
      };
      //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = trading_pairs_table.lower_bound( lower_key );
      //auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = trading_pairs_table.upper_bound( upper_key );
      //walk_table_range(lower, upper);
      
      for( auto itr = lower; itr != upper; ++itr ) {
          print("\n pair: id=", itr->id);
          if (itr->id == pair_id) return itr->base;
      }
          
      eosio_assert(false, "trading pair does not exist");
      
      return 0;
   }
   
   symbol_type exchange::get_pair_quote( uint32_t pair_id ) const {
      trading_pairs   trading_pairs_table(_self, _self);
     
      //auto itr1 = trading_pairs_table.find(pair_id);
      // eosio_assert(itr1 != trading_pairs_table.end(), "trading pair does not exist");
      
      auto walk_table_range = [&]( auto itr, auto end_itr ) {
         print("\n ---------------- begin to trading_pairs table: ----------------");
         for( ; itr != end_itr; ++itr ) {
             print("\n pair: id=", itr->id);
         }
         print("\n -------------------- walk through trading_pairs table ends ----------------:");
      };
      //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = trading_pairs_table.lower_bound( lower_key );
      //auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = trading_pairs_table.upper_bound( upper_key );
      //walk_table_range(lower, upper);
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         print("\n pair: id=", itr->id);
         if (itr->id == pair_id) return itr->quote;
      }
          
      eosio_assert(false, "trading pair does not exist");
      
      return 0;
   }
   
   account_name exchange::get_exchange_account( uint32_t pair_id ) const {
      trading_pairs   trading_pairs_table(_self, _self);
     
      //auto itr1 = trading_pairs_table.find(pair_id);
      // eosio_assert(itr1 != trading_pairs_table.end(), "trading pair does not exist");
      
      auto walk_table_range = [&]( auto itr, auto end_itr ) {
         print("\n ---------------- begin to trading_pairs table: ----------------");
         for( ; itr != end_itr; ++itr ) {
             print("\n pair: id=", itr->id);
         }
         print("\n -------------------- walk through trading_pairs table ends ----------------:");
      };
      //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = trading_pairs_table.lower_bound( lower_key );
      //auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = trading_pairs_table.upper_bound( upper_key );
      //walk_table_range(lower, upper);
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         print("\n pair: id=", itr->id);
         if (itr->id == pair_id) return itr->exc_acc;
      }
          
      eosio_assert(false, "trading pair does not exist");
      
      return 0;
   }
   
   void exchange::get_pair( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) const {
      trading_pairs   trading_pairs_table(_self, _self);
     
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = trading_pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = trading_pairs_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         if (itr->base_chain == base_chain && itr->base_sym == base_sym && 
            itr->quote_chain == quote_chain && itr->quote_sym == quote_sym) 
             print("exchange::get_pair -- pair: id=", itr->id, "\n");
            return;
      }
          
      eosio_assert(false, "trading pair does not exist");
      
      return;
   }
   
   void exchange::get_mark( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) const {
      deals   deals_table(_self, _self);
     
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = deals_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = deals_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         print("exchange::get_mark -- pair: id=", itr->id, "\n");
         if (itr->base_chain == base_chain && itr->base_sym.name() == base_sym.name() && 
            itr->quote_chain == quote_chain && itr->quote_sym.name() == quote_sym.name()) 
            eosio_assert(false, "pair mark already exist");;
      }
      
      auto pk = deals_table.available_primary_key();
      deals_table.emplace( _self, [&]( auto& d ) {
         d.id = (uint32_t)pk;
         
         d.base_chain   = base_chain;
         d.base_sym     = base_sym;
         d.quote_chain  = quote_chain;
         d.quote_sym    = quote_sym;
         d.sum          = to_asset(relay_token_acc, quote_chain, quote_sym, asset(0, quote_sym));
         d.vol          = to_asset(relay_token_acc, base_chain, base_sym, asset(0, base_sym));
         d.reset_block_height = current_block_num();
      });
      
      return ;
   }
   
   void exchange::upd_mark( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, asset sum, asset vol ) {
      deals   deals_table(_self, _self);
      const uint32_t INTERVAL_BLOCKS = 172800;
      uint32_t curr_block;
     
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = deals_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = deals_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         print("\n pair: id=", itr->id);
         if (itr->base_chain == base_chain && itr->base_sym.name() == base_sym.name() && 
               itr->quote_chain == quote_chain && itr->quote_sym.name() == quote_sym.name()) {
            curr_block = current_block_num();
            if (curr_block - itr->reset_block_height >= INTERVAL_BLOCKS) {
               deals_table.modify( itr, _self, [&]( auto& d ) {
                  d.sum = sum;
                  d.vol = vol;
                  d.reset_block_height = curr_block;
               });
            } else {
               deals_table.modify( itr, _self, [&]( auto& d ) {
                  d.sum += sum;
                  d.vol += vol;
               });
            }
            
            return;
         }
      }
      
      return ;
   }
}
#endif //EOSIO_EXCHANGE_H
