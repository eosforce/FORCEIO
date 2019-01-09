
#pragma once

#include <eosiolib/dispatcher.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/producer_schedule.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/contract_config.hpp>


namespace eosiosystem {

   using eosio::asset;
   using eosio::print;
   using eosio::bytes;
   using eosio::block_timestamp;
   using std::string;

   static constexpr uint32_t FROZEN_DELAY = CONTRACT_FROZEN_DELAY; // 3 * 24 * 60 * 20; //3*24*60*20*3s;
   static constexpr int NUM_OF_TOP_BPS = CONTRACT_NUM_OF_TOP_BPS;//23;
   static constexpr int BLOCK_REWARDS_BP = CONTRACT_BLOCK_REWARDS_BP;
   static constexpr uint32_t UPDATE_CYCLE = CONTRACT_UPDATE_CYCLE;//100; //every 100 blocks update

   class system_contract : private eosio::contract {
   public:
      system_contract( account_name self ) : contract(self) {}

   private:

      struct vote_info {
         asset        staked                = asset{0};
         asset        unstaking             = asset{0};
         account_name bpname                = 0;
         int64_t      voteage               = 0;         // asset.amount * block height
         uint32_t     voteage_update_height = 0;
         uint32_t     unstake_height        = 0;

         uint64_t primary_key() const { return bpname; }

         EOSLIB_SERIALIZE(vote_info, (bpname)(staked)(voteage)(voteage_update_height)(unstaking)(unstake_height))
      };

      struct vote4ram_info {
         account_name voter  = 0;
         asset        staked = asset(0);

         uint64_t primary_key() const { return voter; }

         EOSLIB_SERIALIZE(vote4ram_info, (voter)(staked))
      };

      struct bp_info {
         account_name name;
         public_key   block_signing_key;
         uint32_t     commission_rate = 0; // 0 - 10000 for 0% - 100%
         int64_t      total_staked    = 0;
         asset        rewards_pool    = asset(0);
         asset        rewards_block   = asset(0);
         int64_t      total_voteage   = 0; // asset.amount * block height
         uint32_t     voteage_update_height = current_block_num();
         std::string  url;

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

      typedef eosio::multi_index<N(votes),       vote_info>     votes_table;
      typedef eosio::multi_index<N(votes4ram),   vote_info>     votes4ram_table;
      typedef eosio::multi_index<N(vote4ramsum), vote4ram_info> vote4ramsum_table;
      typedef eosio::multi_index<N(bps),         bp_info>       bps_table;
      typedef eosio::multi_index<N(schedules),   schedule_info> schedules_table;

      void update_elected_bps();

      void reward_bps( account_name block_producers[] );

      bool is_super_bp( account_name block_producers[], account_name name );

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
      void newaccount();
      // @abi action
      void updateauth();
      // @abi action
      void deleteauth();
      // @abi action
      void linkauth();
      // @abi action
      void unlinkauth();
      // @abi action
      void canceldelay();
      // @abi action
      void onerror();
      // @abi action
      void setconfig();
      // @abi action
      void setcode();
      // @abi action
      void setfee();
      // @abi action
      void setabi();
      // @abi action
      void onfee();

   };

   EOSIO_ABI(system_contract,(updatebp)
                   (vote)(unfreeze)
                   (vote4ram)(unfreezeram)
                   (claim)
                   (onblock)
                   (setparams)(removebp)
                   (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)(setconfig)(setcode)(setfee)(setabi)(onfee)
                 )
} /// eosiosystem (onfee)
//  (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)(setconfig)(setcode)(setfee)(setabi)