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
};

#endif //FORCEIO_CONTRACT_RELAY_TOKEN_HPP
