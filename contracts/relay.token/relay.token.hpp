//
// Created by fy on 2019-01-29.
//

#ifndef FORCEIO_CONTRACT_RELAY_TOKEN_HPP
#define FORCEIO_CONTRACT_RELAY_TOKEN_HPP

#pragma once

#include <eosiolib/eosio.hpp>
#include "force.relay/force.relay.hpp"



namespace relay {

using namespace eosio;

using std::string;

   struct sys_bridge_addmort {
      name trade_name;
      account_name trade_maker;
      uint64_t type;
      void parse(const string memo);
   };

   struct sys_bridge_exchange {
      name trade_name;
      account_name trade_maker;
      account_name recv;
      uint64_t type;
      void parse(const string memo);
   };
   
   struct sys_match_match {
      account_name payer; 
      account_name receiver;
      asset base;
      asset price;
      uint32_t bid_or_ask;
      void parse(const string memo);
   };

   enum  class trade_type:uint64_t {
      match=1,
      bridge_addmortgage,
      bridge_exchange,
      trade_type_count
   };

   const account_name SYS_BRIDGE = N(sys.bridge);


class token : public eosio::contract {
public:
   using contract::contract;

   token( account_name self ) : contract(self) {}

   struct action {
      account_name from;
      account_name to;
      asset quantity;
      std::string memo;

      EOSLIB_SERIALIZE(action, (from)(to)(quantity)(memo))
   };

   /// @abi action
   void on( const name chain, const checksum256 block_id, const force::relay::action& act );

   void create( account_name issuer,
                name chain,
                asset maximum_supply );

   void issue( const name chain, account_name to, asset quantity, string memo );

   void transfer( account_name from,
                  account_name to,
                  name chain,
                  asset quantity,
                  string memo );
   
   inline asset get_supply( name chain, symbol_name sym )const;

   void trade(    account_name from,
                  account_name to,
                  name chain,
                  asset quantity,
                  trade_type type,
                  string memo);
                  
   void trade_imp( account_name payer, account_name receiver, asset base, asset price, uint32_t bid_or_ask );

private:
   inline static uint128_t get_account_idx(const name& chain, const asset& a) {
      return (uint128_t(uint64_t(chain)) << 64) + uint128_t(a.symbol.name());
   }

   struct account {
      uint64_t id;
      asset balance;
      name  chain;

      uint64_t  primary_key() const { return id; }
      uint128_t get_index_i128() const { return get_account_idx(chain, balance); }
   };

   struct account_next_id {
      uint64_t     id;
      account_name account;

      uint64_t  primary_key() const { return account; }
   };

   struct currency_stats {
      asset        supply;
      asset        max_supply;
      account_name issuer;
      name         chain;

      uint64_t primary_key() const { return supply.symbol.name(); }
   };

   typedef multi_index<N(accounts), account,
         indexed_by< N(bychain),
                     const_mem_fun<account, uint128_t, &account::get_index_i128 >>> accounts;
   typedef multi_index<N(stat), currency_stats> stats;
   typedef multi_index<N(accountid), account_next_id> account_next_ids ;

   void sub_balance( account_name owner, name chain, asset value );
   void add_balance( account_name owner, name chain, asset value, account_name ram_payer );

   

public:
   struct transfer_args {
      account_name from;
      account_name to;
      name chain;
      asset quantity;
      string memo;
   };

};

   asset token::get_supply( name chain, symbol_name sym )const
   {
      stats statstable( _self, chain );
      const auto& st = statstable.get( sym );
      return st.supply;
   }

};

#endif //FORCEIO_CONTRACT_RELAY_TOKEN_HPP
