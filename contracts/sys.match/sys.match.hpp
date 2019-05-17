//
// Created by root1 on 18-7-26.
//

#ifndef EOSIO_EXCHANGE_H
#define EOSIO_EXCHANGE_H

#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/eosio.hpp>

#include <force.token/force.token.hpp>
#include <relay.token/relay.token.hpp>

#include "exchange_pair.hpp"

#include <string>

// TODO by CODEREVIEW need unity force.token and relay.token
// TODO by CODEREVIEW need fix pair search logic
// TODO by CODEREVIEW need use a ext_sym to contain chain info and symbol info

namespace exchange {

   using namespace eosio;
   using std::string;
   using eosio::asset;
   using eosio::symbol_type;
   using real_type = double;

   const int64_t max_fee_rate         = 10000;
   const account_name escrow          = N(sys.match);
   const account_name relay_token_acc = N(relay.token);
   const uint32_t INTERVAL_BLOCKS     = /*172800*/ 24 * 3600 * 1000 / config::block_interval_ms;

   class exchange : public contract {
   public:
      explicit exchange( account_name self ) : contract( self ) {}

      void regex( account_name exc_acc );

      void create( symbol_type base,
                   name base_chain,
                   symbol_type base_sym,
                   symbol_type quote,
                   name quote_chain,
                   symbol_type quote_sym,
                   account_name exc_acc );

      void open( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc );

      void close( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc );

      void match( uint32_t pair_id,
                  account_name payer,
                  account_name receiver,
                  asset quantity,
                  asset price,
                  uint32_t bid_or_ask,
                  account_name exc_acc,
                  string referer,
                  uint32_t fee_type );

      void cancel( account_name maker, uint32_t type, uint64_t order_or_pair_id );

      void done( account_name taker_exc_acc,
                 account_name maker_exc_acc,
                 name quote_chain,
                 asset price,
                 name base_chain,
                 asset quantity,
                 uint32_t bid_or_ask,
                 time_point_sec timestamp );

      void done_helper( account_name taker_exc_acc,
                        account_name maker_exc_acc,
                        name quote_chain,
                        asset price,
                        name base_chain,
                        asset quantity,
                        uint32_t bid_or_ask );

      void match_for_bid( uint32_t pair_id,
                          account_name payer,
                          account_name receiver,
                          asset quantity,
                          asset price,
                          account_name exc_acc,
                          string referer,
                          uint32_t fee_type );

      void match_for_ask( uint32_t pair_id,
                          account_name payer,
                          account_name receiver,
                          asset base,
                          asset price,
                          account_name exc_acc,
                          string referer,
                          uint32_t fee_type );

      void mark( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym );

      void claim( name base_chain,
                  symbol_type base_sym,
                  name quote_chain,
                  symbol_type quote_sym,
                  account_name exc_acc,
                  account_name fee_acc );

      void freeze( account_name exc_acc, uint32_t pair_id );

      void unfreeze( account_name exc_acc, uint32_t pair_id );

      void setfee( account_name exc_acc, uint32_t pair_id, uint32_t rate );

      void enpoints( account_name exc_acc, uint32_t pair_id, symbol_type points_sym );

      void withdraw( account_name to, asset quantity );

      asset calcfee( asset quant, uint64_t fee_rate );

      asset charge_fee( uint32_t pair_id, account_name payer, asset quantity, account_name exc_acc, uint32_t fee_type );

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

      void inline_match( account_name from, asset quantity, string memo );

      inline asset get_avg_price( uint32_t block_height,
                                  name base_chain,
                                  symbol_type base_sym,
                                  name quote_chain = { 0 },
                                  symbol_type quote_sym = CORE_SYMBOL ) const;

      inline void upd_mark( name base_chain,
                            symbol_type base_sym,
                            name quote_chain,
                            symbol_type quote_sym,
                            asset sum,
                            asset vol );

      struct exc {
         account_name exc_acc;

         account_name primary_key() const { return exc_acc; }
      };

      //private:
      struct trading_pair {
         uint32_t     id;
         symbol_type  base;
         name         base_chain;
         symbol_type  base_sym;

         symbol_type  quote;
         name         quote_chain;
         symbol_type  quote_sym;

         account_name exc_acc;
         uint32_t     frozen;

         //uint32_t    fee_rate;
         //asset       fees_base;
         //asset       fees_quote;

         uint32_t primary_key() const { return id; }

         uint128_t by_pair_sym() const { return (uint128_t(base.name()) << 64) | quote.name(); }
      };

      struct order {
         uint64_t       id;
         uint32_t       pair_id;
         account_name   exc_acc;
         account_name   maker;
         account_name   receiver;
         asset          base;
         asset          price;
         uint32_t       bid_or_ask;
         uint32_t       fee_type;
         time_point_sec timestamp;

         uint64_t primary_key() const { return id; }

         uint128_t by_pair_price() const {
            return (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 1 : 0)) << 64 | (uint64_t) price.amount;
         }
      };

      struct deal_info {
         uint32_t id;
         uint32_t pair_id;

         asset sum;
         asset vol;

         // [reset_block_height .. block_height_end]
         uint32_t reset_block_height;// include
         uint32_t block_height_end;// include

         uint64_t primary_key() const { return id; }

         uint64_t by_pair_and_block_height() const {
            return (uint64_t(pair_id) << 32) | block_height_end;
         }
      };

      struct fee_info {
         uint32_t     id;
         account_name exc_acc;
         uint32_t     pair_id;
         uint32_t     frozen;
         uint32_t     rate;
         asset        fees_base;
         asset        fees_quote;
         bool         points_enabled;
         asset        points;

         uint64_t primary_key() const { return id; }

         uint128_t by_exc_and_pair() const {
            return (uint128_t(exc_acc) << 64) | pair_id;
         }
      };

      struct account_info {
         asset balance;

         uint64_t primary_key() const { return balance.symbol.name(); }
      };

      using exchanges = eosio::multi_index<N(exchanges), exc>;

      using trading_pairs = eosio::multi_index<N(pairs), trading_pair,
            indexed_by<N(idxkey), const_mem_fun<trading_pair, uint128_t, &trading_pair::by_pair_sym>>
      >;

      using orderbook = eosio::multi_index<N(orderbook), order,
            indexed_by<N(idxkey), const_mem_fun<order, uint128_t, &order::by_pair_price>>
      >;

      using deals = eosio::multi_index<N(deals), deal_info,
            indexed_by<N(idxkey), const_mem_fun<deal_info, uint64_t, &deal_info::by_pair_and_block_height>>
      >;

      using fees = eosio::multi_index<N(fees), fee_info,
            indexed_by<N(idxkey), const_mem_fun<fee_info, uint128_t, &fee_info::by_exc_and_pair>>
      >;

      using accounts = eosio::multi_index<N(accounts), account_info>;

      template<typename Index>
      auto get_deal_by_num( Index& deals_idx, 
                            const uint32_t pair_id, 
                            const uint32_t block_num ) const {
         const auto lower_key = ( static_cast<uint64_t>( pair_id ) << 32) | static_cast<uint64_t>( block_num );
         return deals_idx.lower_bound(lower_key);
      }

      const asset REG_STAKE = asset(1000'0000);
      const asset OPEN_PAIR_STAKE = asset(1000'0000);

      void insert_order(
            orderbook& orders,
            uint32_t pair_id,
            account_name exc_acc,
            uint32_t bid_or_ask,
            asset base,
            asset price,
            account_name payer,
            account_name receiver,
            uint32_t fee_type );

      //static asset to_asset( account_name code, name chain, symbol_type sym, const asset& a );
      //static asset convert( symbol_type expected_symbol, const asset& a );
   };

   /*
   block_height: end block height
   */
   asset exchange::get_avg_price( uint32_t block_height, name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym ) const {
      deals deals_table(_self, _self);
      const auto idx_deals = deals_table.template get_index<N(idxkey)>();

      asset avg_price = asset(0, quote_sym);

      trading_pairs_t<trading_pairs> pairs_table( _self );
      const auto pair = pairs_table.find( base_chain, base_sym, quote_chain, quote_sym );
      if( pair == pairs_table.end() ) {
         return avg_price;
      }
      const auto pair_id = pair->id;

      auto itr1 = get_deal_by_num( idx_deals, pair_id, 0 );
      if( itr1 == idx_deals.end() || itr1->pair_id != pair_id ) {
         //print("exchange::get_avg_price: trading pair not marked!\n");
         return avg_price;
      }

      itr1 = get_deal_by_num( idx_deals, pair_id, block_height );
      if( itr1 == idx_deals.end() ) {
         --itr1;
      }

      if( itr1->vol.amount > 0 && block_height >= itr1->reset_block_height ) {
         avg_price = itr1->sum * precision(itr1->vol.symbol.precision()) / itr1->vol.amount;
      }
      /*print("exchange::get_avg_price pair_id=", itr1->pair_id, ", block_height=", block_height,
         ", reset_block_height=", itr1->reset_block_height, ", block_height_end=", itr1->block_height_end,
         ", sum=", itr1->sum, ", vol=", itr1->vol, ", avg_price=", avg_price,"\n");*/
      return avg_price;
   }

   void exchange::upd_mark( name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, asset sum, asset vol ) {
      deals deals_table(_self, _self);

      trading_pairs_t<trading_pairs> pairs_table( _self );

      auto pair_id   = pairs_table.get_pair_id(base_chain, base_sym, quote_chain, quote_sym);
      auto lower_key = ((uint64_t) pair_id << 32) | 0;
      auto idx_deals = deals_table.template get_index<N(idxkey)>();
      auto itr1      = idx_deals.lower_bound(lower_key);
      if( !(itr1 != idx_deals.end() && itr1->pair_id == pair_id) ) {
         //print("exchange::upd_mark trading pair not marked!\n");
         return;
      }

      uint32_t curr_block = current_block_num();
      lower_key = ((uint64_t) pair_id << 32) | curr_block;
      itr1 = idx_deals.lower_bound(lower_key);
      if( itr1 == idx_deals.cend() ) itr1--;
      if( curr_block <= itr1->block_height_end ) {
         idx_deals.modify(itr1, _self, [&]( auto& d ) {
            d.sum += sum;
            d.vol += vol;
         });
      } else {
         auto start_block = itr1->reset_block_height + (curr_block - itr1->reset_block_height) / INTERVAL_BLOCKS * INTERVAL_BLOCKS;
         auto pk = deals_table.available_primary_key();
         deals_table.emplace(_self, [&]( auto& d ) {
            d.id                 = (uint32_t) pk;
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

      return;
   }
}
#endif //EOSIO_EXCHANGE_H
