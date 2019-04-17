#ifndef FORCEIO_CONTRACTS_FORCE_RELAY_HPP
#define FORCEIO_CONTRACTS_FORCE_RELAY_HPP

#pragma once

#include <eosiolib/types.h>
#include <eosiolib/dispatcher.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/producer_schedule.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/contract_config.hpp>
#include <vector>
#include <array>



namespace force {

using namespace eosio;

class relay : public eosio::contract {
public:
   explicit relay( account_name self )
      : contract(self)
      {}


public:
   // action
   struct action {
      account_name               account;
      action_name                name;
      vector<permission_level>   authorization;
      bytes                      data;

      EOSLIB_SERIALIZE( action, (account)(name)(authorization)(data) )
   };

   // block
   struct block_type {
   public:
      account_name            producer = name{0};
      uint32_t                num = 0;
      checksum256             id;
      checksum256             previous;
      uint16_t                confirmed = 1;
      checksum256             transaction_mroot;
      checksum256             action_mroot;
      checksum256             mroot;

      bool is_nil() const {
         return this->producer == name{0};
      }

      uint32_t block_num() const {
         return num;
      }

      bool operator == (const block_type &m) const {
         return id == m.id
             && num == m.num
             && transaction_mroot == m.transaction_mroot
             && action_mroot == m.action_mroot
             && mroot == m.mroot;
      }

      EOSLIB_SERIALIZE( block_type,
            (producer)(num)(id)(previous)(confirmed)
            (transaction_mroot)(action_mroot)(mroot)
             )
   };

   struct unconfirm_block {
   public:
      block_type     base;
      asset          confirm = asset{0};
      vector<action> actions;

      bool operator < (const unconfirm_block &m) const {
         return base.num < m.base.num;
      }

      EOSLIB_SERIALIZE( unconfirm_block, (base)(confirm)(actions) )
   };

   // block relay stat
   struct block_relay_stat {
   public:
      name       chain;
      block_type last;
      vector<unconfirm_block> unconfirms;

      uint64_t primary_key() const { return chain; }

      EOSLIB_SERIALIZE( block_relay_stat, (chain)(last)(unconfirms) )
   };

   typedef eosio::multi_index<N(relaystat), block_relay_stat> relaystat_table;

   // map_handler
   struct map_handler {
      name         chain;
      name         name;
      account_name actaccount;
      action_name  actname;

      account_name account;
      bytes        data;

      uint64_t primary_key() const { return name; }

      EOSLIB_SERIALIZE( map_handler, (chain)(name)(actaccount)(actname)(account)(data) )
   };

   struct handler_action {
      name        chain;
      checksum256 block_id;
      action      act;

      EOSLIB_SERIALIZE( handler_action, (chain)(block_id)(act) )
   };

   struct relay_transfer {
      name         chain;
      account_name transfer;
      asset        deposit = asset{0};
      block_type   last;

      uint64_t primary_key() const { return transfer; }

      EOSLIB_SERIALIZE( relay_transfer, (chain)(transfer)(deposit)(last) )
   };

   // channel
   struct channel {
      name                   chain;
      checksum256            id;
      asset                  deposit_sum = asset{0};

      uint64_t primary_key() const { return chain; }

      EOSLIB_SERIALIZE( channel, (chain)(id)(deposit_sum) )
   };

   typedef eosio::multi_index<N(channels),  channel>         channels_table;
   typedef eosio::multi_index<N(transfers), relay_transfer>  transfers_table;
   typedef eosio::multi_index<N(handlers),  map_handler>     handlers_table;

private:
   void onblockimp( const name chain, const block_type& block, const vector<action>& actions );
   void onaction( const block_type& block, const action& act, const map_handler& handler );
   void new_transfer( name chain, account_name transfer, const asset& deposit );

public:
   void ontransfer( const account_name from, const account_name to,
            const asset& quantity, const std::string& memo);

public:
   /// @abi action
   void commit( const name chain,
                const account_name transfer,
                const block_type& block,
                const vector<action>& actions );
   /// @abi action
   void onblock( const name chain, const block_type& block );
   /// @abi action
   void newchannel( const name chain, const checksum256 id );
   /// @abi action
   void newmap( const name chain, const name type,
                const account_name act_account, const action_name act_name,
                const account_name account, const bytes data );
};


};




#endif //FORCEIO_CONTRACTS_FORCE_RELAY_HPP

