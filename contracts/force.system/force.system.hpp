
#pragma once

#include <eosiolib/dispatcher.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/producer_schedule.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/contract_config.hpp>
#include <vector>


namespace eosiosystem {

   using eosio::asset;
   using eosio::print;
   using eosio::bytes;
   using eosio::block_timestamp;
   using std::string;
   using eosio::permission_level;
   using std::vector;

   static constexpr uint32_t FROZEN_DELAY = CONTRACT_FROZEN_DELAY; // 3 * 24 * 60 * 20; //3*24*60*20*3s;
   static constexpr int NUM_OF_TOP_BPS = CONTRACT_NUM_OF_TOP_BPS;//23;
   static constexpr int BLOCK_REWARDS_BP = CONTRACT_BLOCK_REWARDS_BP;
   static constexpr uint32_t UPDATE_CYCLE = CONTRACT_UPDATE_CYCLE;//100; //every 100 blocks update


   struct permission_level_weight {
      permission_level  permission;
      weight_type       weight;
   };

   struct key_weight {
      public_key key;
      weight_type     weight;
   };

   struct wait_weight {
      uint32_t     wait_sec;
      weight_type  weight;
   };



   struct authority {
      uint32_t                          threshold = 0;
      vector<key_weight>                keys;
      vector<permission_level_weight>   accounts;
      vector<wait_weight>               waits;
   };

   class system_contract : private eosio::contract {
   public:
      system_contract( account_name self ) : contract(self) {}

   private:

      struct vote_info {
         account_name bpname;
         asset staked = asset(0);
         uint32_t voteage_update_height = current_block_num();
         int64_t voteage = 0; // asset.amount * block height
         asset unstaking = asset(0);
         uint32_t unstake_height = current_block_num();

         uint64_t primary_key() const { return bpname; }

         EOSLIB_SERIALIZE(vote_info, ( bpname )(staked)(voteage)(voteage_update_height)(unstaking)(unstake_height))
      };

      struct vote4ram_info {
         account_name voter;
         asset staked = asset(0);
         uint64_t primary_key() const { return voter; }

         EOSLIB_SERIALIZE(vote4ram_info, (voter)(staked))
      };

      struct bp_info {
         account_name name;
         public_key block_signing_key;
         uint32_t commission_rate = 0; // 0 - 10000 for 0% - 100%
         int64_t total_staked = 0;
         asset rewards_pool = asset(0);
         asset rewards_block = asset(0);
         int64_t total_voteage = 0; // asset.amount * block height
         uint32_t voteage_update_height = current_block_num();
         std::string url;
         bool emergency = false;
         bool isactive = true;

         uint64_t primary_key() const { return name; }

         void update( public_key key, uint32_t rate, std::string u ) {
            block_signing_key = key;
            commission_rate = rate;
            url = u;
         }
         void     deactivate()       {isactive = false;}
         EOSLIB_SERIALIZE(bp_info, ( name )(block_signing_key)(commission_rate)(total_staked)
               (rewards_pool)(rewards_block)(total_voteage)(voteage_update_height)(url)(emergency)(isactive))
      };

      struct producer {
         account_name bpname;
         uint32_t amount = 0;
      };

      struct schedule_info {
         uint64_t version;
         uint32_t block_height;
         producer producers[NUM_OF_TOP_BPS];

         uint64_t primary_key() const { return version; }

         EOSLIB_SERIALIZE(schedule_info, ( version )(block_height)(producers))
      };

      typedef eosio::multi_index<N(votes), vote_info> votes_table;
      typedef eosio::multi_index<N(votes4ram), vote_info> votes4ram_table;
      typedef eosio::multi_index<N(vote4ramsum), vote4ram_info> vote4ramsum_table;
      typedef eosio::multi_index<N(bps), bp_info> bps_table;
      typedef eosio::multi_index<N(schedules), schedule_info> schedules_table;

      void update_elected_bps();

      void reward_bps( account_name block_producers[] );

      bool is_super_bp( account_name block_producers[], account_name name );

      //defind in delegate_bandwidth.cpp
      void changebw( account_name from, account_name receiver,
                      asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );

   public:
      // @abi action
      void updatebp( const account_name bpname, const public_key producer_key,
                     const uint32_t commission_rate, const std::string& url );

      // @abi action
      void vote( const account_name voter, const account_name bpname, const asset stake );

      // @abi action
      void unfreeze( const account_name voter, const account_name bpname );

      // @abi action
      void vote4ram( const account_name voter, const account_name bpname, const asset stake );

      // @abi action
      void unfreezeram( const account_name voter, const account_name bpname );

      // @abi action
      void claim( const account_name voter, const account_name bpname );

      // @abi action
      void onblock( const block_timestamp, const account_name bpname, const uint16_t,
                    const block_id_type, const checksum256, const checksum256, const uint32_t schedule_version );

      // @abi action
      void setparams( const eosio::blockchain_parameters& params );
      // @abi action
      void removebp( account_name producer );

      //native action
      // @abi action
      //,authority owner,authority active
      void newaccount(account_name creator,account_name name,authority owner,authority active);
      // @abi action
      // account_name account,permission_name permission,permission_name parent,authority auth
      void updateauth(account_name account,permission_name permission,permission_name parent,authority auth);
      // @abi action
      void deleteauth(account_name account,permission_name permission);
      // @abi action
      void linkauth(account_name account,account_name code,action_name type,permission_name requirement);
      // @abi action
      void unlinkauth(account_name account,account_name code,action_name type);
      // @abi action
      void canceldelay(permission_level canceling_auth,transaction_id_type trx_id);
      // @abi action
      void onerror(uint128_t sender_id,bytes sent_trx);
      // @abi action
      void setconfig(account_name typ,int64_t num,account_name key,asset fee);
      // @abi action
      void setcode(account_name account,uint8_t vmtype,uint8_t vmversion,bytes code);
      // @abi action
      void setfee(account_name account,action_name action,asset fee,uint32_t cpu_limit,uint32_t net_limit,uint32_t ram_limit);
      // @abi action
      void setabi(account_name account,bytes abi);


      // @abi action
      void delegatebw( account_name from, account_name receiver,
                     asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );
      // @abi action
      void undelegatebw( account_name from, account_name receiver,
                       asset unstake_net_quantity, asset unstake_cpu_quantity );
      // @abi action
      void refund( account_name owner );
   };

   EOSIO_ABI(system_contract,(updatebp)
                   (vote)(unfreeze)
                   (vote4ram)(unfreezeram)
                   (claim)
                   (onblock)
                   (setparams)(removebp)
                   (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)(setconfig)(setcode)(setfee)(setabi)
                   (delegatebw)(undelegatebw)(refund)
                 )
} /// eosiosystem (onfee)
//  (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)(setconfig)(setcode)(setfee)(setabi)(delegatebw)(undelegatebw)(refund)