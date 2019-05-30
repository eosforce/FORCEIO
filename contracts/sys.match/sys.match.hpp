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
#include <force.token/force.token.hpp>
#include <relay.token/relay.token.hpp>
#include <string>


namespace exchange {
   using namespace eosio;
   using std::string;
   using eosio::asset;
   using eosio::symbol_type;
   const account_name relay_token_acc = N(relay.token);
   const uint32_t INTERVAL_BLOCKS = /*172800*/ 24 * 3600 * 1000 / config::block_interval_ms;

   typedef double real_type;
   
   inline int64_t precision(uint64_t decimals)
   {
      int64_t p10 = 1;
      int64_t p = (int64_t)decimals;
      while( p > 0  ) {
         p10 *= 10; --p;
      }
      return p10;
   }

   class exchange : public contract  {

   public:
      exchange(account_name self) : contract(self) {}
         
      void regex(account_name exc_acc);

      void create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym, account_name exc_acc);

      void open(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc);
      
      void close(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc);
      
      void close2(uint32_t pair_id, account_name exc_acc);

      void match( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, uint32_t bid_or_ask, account_name exc_acc, string referer, uint32_t fee_type );
      
      void cancel(account_name maker, uint32_t type, uint64_t order_or_pair_id);
      
      void done(uint64_t id, account_name taker_exc_acc, account_name maker_exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t pair_id, 
         uint64_t buy_order_id, asset buy_fee, uint64_t sell_order_id, asset sell_fee, uint32_t bid_or_ask, time_point_sec timestamp);
      
      void done_helper(account_name taker_exc_acc, account_name maker_exc_acc, name quote_chain, asset price, 
                              name base_chain, asset quantity, uint32_t pair_id, 
                              uint64_t buy_order_id, asset buy_fee, uint64_t sell_order_id, asset sell_fee, 
                              uint32_t bid_or_ask, time_point_sec timestamp);
      
      void morder(
                              uint64_t id,
                              account_name payer, 
                              account_name to, 
                              name chain,
                              asset totalQty,
                              uint32_t type,
                              account_name receiver,
                              asset price, 
                              uint32_t pair_id,
                              uint32_t bid_or_ask,
                              account_name taker_exc_acc,
                              string referer, 
                              asset matchedQty, 
                              uint32_t feeType,
                              time_point_sec timestamp,
                              uint32_t status) ;                       
                              
      void morder_helper(
                              uint64_t order_id, 
                              account_name payer, 
                              name chain,
                              asset quantity,
                              account_name receiver,
                              asset price, 
                              uint32_t pair_id,
                              uint32_t direction,
                              account_name taker_exc_acc,
                              string referer, 
                              asset totalQty, 
                              uint32_t feeType,
                              time_point_sec timestamp);
      
      void match_for_bid( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, account_name exc_acc, string referer, uint32_t fee_type);
      
      void match_for_ask( uint32_t pair_id, account_name payer, account_name receiver, asset base, asset price, account_name exc_acc, string referer, uint32_t fee_type);
      
      void mark(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym);
      
      void claim(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc, account_name fee_acc);
      
      void freeze(account_name exc_acc, uint32_t pair_id);
      
      void unfreeze(account_name exc_acc, uint32_t pair_id);
      
      bool is_pair_opened(account_name exc_acc, uint32_t pair_id);
      
      void setfee(account_name exc_acc, uint32_t pair_id, uint32_t rate);
      
      void enpoints(account_name exc_acc, uint32_t pair_id, symbol_type points_sym);
      
      void setminordqty(account_name exc_acc, uint32_t pair_id, asset min_qty);
      
      void withdraw(account_name to, asset quantity);
      
      void freeze4(account_name exc_acc, name action, uint32_t pair_id, asset quantity);
         
      void unfreeze4(account_name exc_acc, name action, uint32_t pair_id, asset quantity);
         
      asset calcfee(asset quant, uint64_t fee_rate);
      
      void freeze_exc(account_name exc_acc);
      
      void unfreeze_exc(account_name exc_acc);
      
      bool is_exc_registered(account_name exc_acc);
      
      bool is_exc_frozen(account_name exc_acc);
      
      asset get_frozen_asset(account_name exc_acc, name action, uint32_t pair_id);
      
      int charge_fee(uint32_t pair_id, account_name payer, asset quantity, account_name exc_acc, uint32_t fee_type, asset& deducted_fee);
      
      void sub_balance( account_name owner, asset value );

      void add_balance( account_name owner, asset value, account_name ram_payer );
      
      asset get_balance( account_name owner, symbol_type points_sym );
      
      void force_token_transfer( account_name from,
                      account_name to,
                      asset quantity,
                      string memo );
                      
      void relay_token_transfer( account_name from,
                      account_name to,
                      name chain,
                      asset quantity,
                      string memo );

      void force_token_trade( account_name from,
                      account_name to,
                      asset quantity,
                      uint32_t type,
                      string memo );
                      
      void relay_token_trade( account_name from,
                      account_name to,
                      name chain,
                      asset quantity,
                      uint32_t type,
                      string memo );
                      
      void inline_match(account_name from, asset quantity, string memo);

      inline void get_pair(uint32_t pair_id, name &base_chain, symbol_type &base_sym, name &quote_chain, symbol_type &quote_sym) const;
      inline symbol_type get_pair_base( uint32_t pair_id ) const;
      inline symbol_type get_pair_quote( uint32_t pair_id ) const;
      inline void check_pair( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym );
      inline uint32_t get_pair_id( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) const;
      inline asset get_avg_price( uint32_t block_height, name base_chain, symbol_type base_sym, name quote_chain = {0}, symbol_type quote_sym = CORE_SYMBOL ) const;
      
      inline void upd_mark( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, asset sum, asset vol );
      
      mutable uint64_t _next_primary_key;
      enum next_primary_key_tags : uint64_t {
         no_available_primary_key = static_cast<uint64_t>(-2), // Must be the smallest uint64_t value compared to all other tags
         unset_next_primary_key = static_cast<uint64_t>(-1)
      };
      
      inline uint64_t available_primary_key(name table_name);

      struct exc {
         account_name exc_acc;
         bool frozen = false;
         
         account_name primary_key() const { return exc_acc; }
      };

   //private:
      struct trading_pair{
          uint32_t id;
          
          symbol_type base;
          name        base_chain;
          symbol_type base_sym;

          symbol_type quote;
          name        quote_chain;
          symbol_type quote_sym;
          
          account_name exc_acc;
          uint32_t    frozen;
          
          uint32_t primary_key() const { return id; }
          uint128_t by_pair_sym() const { return (uint128_t(base.name()) << 64) | quote.name(); }
      };
      
      struct order {
         uint64_t        id;
         uint32_t        pair_id;
         account_name    exc_acc;
         account_name    maker;
         account_name    receiver;
         asset           base;
         asset           price;
         uint32_t        bid_or_ask;
         uint32_t        fee_type;
         time_point_sec  timestamp;

         uint64_t primary_key() const { return id; }
         uint128_t by_pair_price() const { 
             //print("\n by_pair_price: order: id=", id, ", pair_id=", pair_id, ", bid_or_ask=", bid_or_ask,", base=", base,", price=", price,", maker=", maker, ", key=", (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 1 : 0)) << 64 | (uint64_t)price.amount);
             return (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 1 : 0)) << 64 | (uint64_t)price.amount; }
      };
      
      struct deal_info {
         uint32_t    id;
         uint32_t    pair_id;

         asset       sum;
         asset       vol;
         
         // [reset_block_height .. block_height_end]
         uint32_t    reset_block_height;// include 
         uint32_t    block_height_end;// include
         
         uint64_t primary_key() const { return id; }
         uint64_t by_pair_and_block_height() const {
            return (uint64_t(pair_id) << 32) | block_height_end;
         }
      };
      
      struct fee_info {
         uint32_t    id;
         account_name exc_acc;
         uint32_t    pair_id;
         uint32_t    frozen;
         
         uint32_t    rate;
         
         asset       fees_base;
         asset       fees_quote;
         
         bool        points_enabled;
         asset       points;
         
         asset       min_qty;
         
         uint64_t primary_key() const { return id; }
         uint128_t by_exc_and_pair() const {
            return (uint128_t(exc_acc) << 64) | pair_id;
         }
      };
      
      struct account_info {
         asset    balance;

         uint64_t primary_key()const { return balance.symbol.name(); }
      };
      
      struct freeze_info {
         uint32_t     id;
         account_name exc_acc        = 0;
         name         action;
         uint32_t     pair_id        = 0;
         asset        staked         = asset{0};

         uint64_t primary_key() const { return id; }
         uint128_t by_act_and_pair() const {
            return (uint128_t(action.value) << 64) | pair_id;
         }
      };
      
      struct id_info {
         name     table_name;
         uint64_t id;
         
         uint64_t primary_key() const { return table_name; }
      };

      typedef eosio::multi_index<N(exchanges), exc> exchanges;
      typedef eosio::multi_index<N(pairs), trading_pair,
         indexed_by< N(idxkey), const_mem_fun<trading_pair, uint128_t, &trading_pair::by_pair_sym>>
      > trading_pairs;    
      typedef eosio::multi_index<N(orderbook), order,
         indexed_by< N(idxkey), const_mem_fun<order, uint128_t, &order::by_pair_price>>
      > orderbook;    
      typedef eosio::multi_index<N(deals), deal_info,
         indexed_by< N(idxkey), const_mem_fun<deal_info, uint64_t, &deal_info::by_pair_and_block_height>>
      > deals;
      typedef eosio::multi_index<N(fees), fee_info,
         indexed_by< N(idxkey), const_mem_fun<fee_info, uint128_t, &fee_info::by_exc_and_pair>>
      > fees;
      typedef eosio::multi_index<N(accounts), account_info> accounts;
      typedef eosio::multi_index<N(freezed), freeze_info,
         indexed_by< N(idxkey), const_mem_fun<freeze_info, uint128_t, &freeze_info::by_act_and_pair>>
         > freeze_table;
      typedef eosio::multi_index<N(ids), id_info> ids;
      
      const asset REG_STAKE         = asset(1000'0000);
      const asset OPEN__PAIR_STAKE  = asset(1000'0000);

      void insert_order(
                              orderbook &orders, 
                              uint64_t order_id,
                              uint32_t pair_id, 
                              account_name exc_acc,
                              uint32_t bid_or_ask, 
                              asset base, 
                              asset price, 
                              account_name payer, 
                              account_name receiver,
                              uint32_t fee_type,
                              time_point_sec timestamp);

      //static asset to_asset( account_name code, name chain, symbol_type sym, const asset& a );
      //static asset convert( symbol_type expected_symbol, const asset& a );
      
      
   };
   
   void exchange::check_pair( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) {
      trading_pairs   pairs_table(_self, _self);

      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = pairs_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         if (itr->base_chain == base_chain && itr->base_sym.name() == base_sym.name() && 
               itr->quote_chain == quote_chain && itr->quote_sym.name() == quote_sym.name()) {
            eosio_assert(false, "trading pair already exist");
            return;
         }
      }

      return;
   }
   
   uint32_t exchange::get_pair_id( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) const {
      trading_pairs   pairs_table(_self, _self);

      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = pairs_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         if (itr->base_chain == base_chain && itr->base_sym.name() == base_sym.name() && 
               itr->quote_chain == quote_chain && itr->quote_sym.name() == quote_sym.name()) {
            return itr->id;   
         }
      }
          
      eosio_assert(false, "pair does not exist");

      return 0;
   }
   
   void exchange::get_pair(uint32_t pair_id, name &base_chain, symbol_type &base_sym, name &quote_chain, symbol_type &quote_sym) const {
      trading_pairs   pairs_table(_self, _self);

      auto itr = pairs_table.find(pair_id);
      eosio_assert(itr != pairs_table.cend(), "pair does not exist");
      
      base_chain  = itr->base_chain;
      base_sym    = itr->base_sym;
      quote_chain = itr->quote_chain;
      quote_sym   = itr->quote_sym;

      return;
   }
   
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
          //print("\n pair: id=", itr->id);
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
         //print("\n pair: id=", itr->id);
         if (itr->id == pair_id) return itr->quote;
      }
          
      eosio_assert(false, "trading pair does not exist");
      
      return 0;
   }
   
   /*
   block_height: end block height
   */
   asset exchange::get_avg_price( uint32_t block_height, name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) const {
      deals   deals_table(_self, _self);
      asset   avg_price = asset(0, quote_sym);

      uint32_t pair_id = 0xFFFFFFFF;
      
      trading_pairs   pairs_table(_self, _self);

      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = pairs_table.upper_bound( upper_key );
      auto itr = lower;
      
      for ( itr = lower; itr != upper; ++itr ) {
         if (itr->base_chain == base_chain && itr->base_sym.name() == base_sym.name() && 
               itr->quote_chain == quote_chain && itr->quote_sym.name() == quote_sym.name()) {
            //print("\n exchange::get_avg_price -- pair: id=", itr->id, "\n");
            pair_id = itr->id;
            break;
         }
      }
      if (itr == upper) {
         //print("\n exchange::get_avg_price: trading pair not exist! base_chain=", base_chain.to_string().c_str(), ", base_sym=", base_sym, ", quote_chain", quote_chain.to_string().c_str(), ", quote_sym=", quote_sym, "\n");
         return avg_price;
      }
      
      lower_key = ((uint64_t)pair_id << 32) | 0;
      auto idx_deals = deals_table.template get_index<N(idxkey)>();
      auto itr1 = idx_deals.lower_bound(lower_key);
      if (!(itr1 != idx_deals.end() && itr1->pair_id == pair_id)) {
         //print("exchange::get_avg_price: trading pair not marked!\n");
         return avg_price;
      }
      
      lower_key = ((uint64_t)pair_id << 32) | block_height;
      itr1 = idx_deals.lower_bound(lower_key);
      if (itr1 == idx_deals.cend()) itr1--;

      if (itr1->vol.amount > 0 && block_height >= itr1->reset_block_height) 
         avg_price = itr1->sum * precision(itr1->vol.symbol.precision()) / itr1->vol.amount;
      /*print("exchange::get_avg_price pair_id=", itr1->pair_id, ", block_height=", block_height, 
         ", reset_block_height=", itr1->reset_block_height, ", block_height_end=", itr1->block_height_end, 
         ", sum=", itr1->sum, ", vol=", itr1->vol, ", avg_price=", avg_price,"\n");*/
      return avg_price;
   }
 
   void exchange::upd_mark( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, asset sum, asset vol ) {
      deals   deals_table(_self, _self);
      
      auto pair_id = get_pair_id(base_chain, base_sym, quote_chain, quote_sym);
     
      auto lower_key = ((uint64_t)pair_id << 32) | 0;
      auto idx_deals = deals_table.template get_index<N(idxkey)>();
      auto itr1 = idx_deals.lower_bound(lower_key);
      if (!( itr1 != idx_deals.end() && itr1->pair_id == pair_id )) {
         //print("exchange::upd_mark trading pair not marked!\n");
         return;
      }
      
      uint32_t curr_block = current_block_num();
      lower_key = ((uint64_t)pair_id << 32) | curr_block;
      itr1 = idx_deals.lower_bound(lower_key);
      if (itr1 == idx_deals.cend()) itr1--;
      if ( curr_block <= itr1->block_height_end ) {
         idx_deals.modify( itr1, _self, [&]( auto& d ) {
            d.sum += sum;
            d.vol += vol;
         });
      } else {
         auto start_block =  itr1->reset_block_height + (curr_block - itr1->reset_block_height) / INTERVAL_BLOCKS * INTERVAL_BLOCKS;
         auto pk = deals_table.available_primary_key();
         deals_table.emplace( _self, [&]( auto& d ) {
            d.id                 = (uint32_t)pk;
            d.pair_id            = pair_id;
            d.sum                = sum;
            d.vol                = vol;
            d.reset_block_height = start_block;
            d.block_height_end   = start_block + INTERVAL_BLOCKS - 1;
         });   
      }
      
      // test
      /*{
         name test_base_chain; test_base_chain.value = N(test1);
         symbol_type test_base_sym = S(2, TESTA);
         name test_quote_chain; test_quote_chain.value = N(test2);
         symbol_type test_quote_sym = S(2, TESTB);
         
         get_avg_price( curr_block, test_base_chain, test_base_sym, test_quote_chain, test_quote_sym );
         get_avg_price( curr_block, base_chain, base_sym, quote_chain, quote_sym );
         get_avg_price( curr_block-10, base_chain, base_sym, quote_chain, quote_sym );
         get_avg_price( curr_block+10, base_chain, base_sym, quote_chain, quote_sym );
      }*/
      
      return ;
   }
   
   uint64_t exchange::available_primary_key(name table_name) {
      ids id_tbl(_self, table_name.value);
      auto itr = id_tbl.find(table_name);
      
      uint64_t _next_primary_key = 0;
      
      if (itr == id_tbl.end()) {
         id_tbl.emplace( _self, [&]( auto& i ) {
            i.table_name = table_name;
            i.id = _next_primary_key;
         });  
      } else {
         _next_primary_key = itr->id + 1;
         id_tbl.modify( itr, _self, [&]( auto& i ) {
            i.id = _next_primary_key;
         });
      }

      eosio_assert( _next_primary_key < no_available_primary_key, "next primary key in table is at autoincrement limit");
      return _next_primary_key;
   }
}
#endif //EOSIO_EXCHANGE_H
