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

   #define CORE_SYMBOL_PRECISION 10000
   static constexpr uint32_t FROZEN_DELAY = CONTRACT_FROZEN_DELAY; // 3 * 24 * 60 * 20; //3*24*60*20*3s;
   static constexpr int NUM_OF_TOP_BPS = CONTRACT_NUM_OF_TOP_BPS;//23;
#ifdef BEFORE_ONLINE_TEST 
   static constexpr uint32_t CYCLE_PREHOUR = 10;
   static constexpr uint32_t CYCLE_PREBP_BLOCK = 3;
   static constexpr uint32_t CYCLE_PREDAY = 5;//5;//275;
   static constexpr uint32_t STABLE_DAY = 10;//2;//60;
   static constexpr uint64_t PRE_BLOCK_REWARDS = 58.6*CORE_SYMBOL_PRECISION;
   static constexpr uint64_t STABLE_BLOCK_REWARDS = 126*CORE_SYMBOL_PRECISION;
#else
   static constexpr uint32_t CYCLE_PREHOUR = 12;
   static constexpr uint32_t CYCLE_PREBP_BLOCK = 15;
   
   static constexpr uint32_t CYCLE_PREDAY = 275;//5;//275;
   static constexpr uint32_t STABLE_DAY = 60;//2;//60;
   static constexpr uint64_t STABLE_BLOCK_REWARDS = 630*CORE_SYMBOL_PRECISION;
   static constexpr uint64_t PRE_BLOCK_REWARDS = 143*CORE_SYMBOL_PRECISION;
#endif
   //static constexpr uint32_t STABLE_BLOCK_HEIGHT = UPDATE_CYCLE * CYCLE_PREDAY * STABLE_DAY;
   static constexpr uint32_t PRE_GRADIENT = 10250;
   static constexpr uint32_t STABLE_GRADIENT = 10010;
   //static constexpr uint32_t REWARD_MODIFY_COUNT = UPDATE_CYCLE * CYCLE_PREDAY;
   static constexpr uint32_t UPDATE_CYCLE =CYCLE_PREBP_BLOCK * NUM_OF_TOP_BPS;

   static constexpr uint64_t REWARD_ID = 1;
   static constexpr uint64_t BLOCK_OUT_WEIGHT = 1000;
   static constexpr uint64_t MORTGAGE = 8228;

   #define REWARD_RATIO_PRECISION 10000
   static constexpr uint32_t REWARD_DEVELOP = 900;
   static constexpr uint32_t REWARD_BP = 300;
   static constexpr uint32_t REWARD_FUND = 100;
   static constexpr uint32_t REWARD_MINE = REWARD_RATIO_PRECISION - REWARD_DEVELOP - REWARD_BP;

   static constexpr uint64_t OTHER_COIN_WEIGHT = 500;

   #define LACKMORTGAGE_FREEZE UPDATE_CYCLE * CYCLE_PREHOUR
   #define PUNISH_BP_FEE   asset(100*CORE_SYMBOL_PRECISION)
   #define BAIL_PUNISH_FEE   asset(10*CORE_SYMBOL_PRECISION)

   #define  REWARD_RECORD_SIZE   2000
   #define  BP_REWARD_RECORD_SIZE  360

   

   struct creation_producer {
      account_name bp_name;
      int64_t      total_staked    = 0;
      int64_t      mortgage = 0;
   };

   static constexpr creation_producer CREATION_BP[26] = {
      {N(biosbpa),400000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpb),400000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpc),400000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpd),400000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpe),600000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpf),600000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpg),600000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbph),600000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpi),1300000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpj),1300000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpk),1300000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpl),2100000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpm),2100000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpn),10000000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpo),10000000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpp),10000000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpq),10000000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpr),10000000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbps),10000000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpt),10000000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpu),10000000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpv),100000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpw),100000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpx),100000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpy),100000,200000*CORE_SYMBOL_PRECISION},
      {N(biosbpz),100000,200000*CORE_SYMBOL_PRECISION}
   };
   

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

   enum  class active_type:int32_t {
      Normal=0,
      Punish,
      LackMortgage,
      Removed
   };

   class system_contract : private eosio::contract {
   public:
      system_contract( account_name self )
         : contract(self)
         , _voters(_self, _self) {}

   private:

      struct vote_info {
         asset        vote                  = asset{0};
         account_name bpname                = 0;
         int64_t      voteage               = 0;         // asset.amount * block height
         uint32_t     voteage_update_height = 0;
         asset        total_reward = asset(0);

         uint64_t primary_key() const { return bpname; }

         EOSLIB_SERIALIZE(vote_info, (bpname)(vote)(voteage)(voteage_update_height)(total_reward))
      };

      struct votes_info {
         account_name                owner = 0; /// the voter
         std::vector<account_name>   producers; /// the producers approved by this voter
         asset                       staked;    /// the staked to this producers

         uint64_t primary_key()const { return owner; }

         EOSLIB_SERIALIZE( votes_info, (owner)(producers)(staked) )
      };

      struct freeze_info {
         asset        staked         = asset{0};
         asset        unstaking      = asset{0};
         account_name voter          = 0;
         uint32_t     unstake_height = 0;

         uint64_t primary_key() const { return voter; }

         EOSLIB_SERIALIZE(freeze_info, (staked)(unstaking)(voter)(unstake_height))
      };

      struct vote4ram_info {
         account_name voter  = 0;
         asset        staked = asset(0);

         uint64_t primary_key() const { return voter; }

         EOSLIB_SERIALIZE(vote4ram_info, (voter)(staked))
      };

      struct vote_reward_info {
         int64_t total_voteage;
         asset total_reward = asset(0);
         int32_t  reward_block_num;
         uint64_t primary_key() const { return reward_block_num; }
      };

      struct bp_info {
         account_name name;
         public_key   block_signing_key;
         uint32_t     commission_rate = 0; // 0 - CORE_SYMBOL_PRECISION for 0% - 100%
         int64_t      total_staked    = 0;
     //    asset        rewards_pool    = asset(0);
         asset        rewards_block   = asset(0);
         int64_t      total_voteage   = 0; // asset.amount * block height
         uint32_t     voteage_update_height = current_block_num();
         std::string  url;
         bool emergency = false;
         int32_t active_type = 0;

         int64_t      block_age = 0;
         uint32_t     last_block_amount = 0;
         int64_t      block_weight = BLOCK_OUT_WEIGHT;   
         asset        mortgage = asset(0);

         int32_t     total_drain_block = 0;
         asset       remain_punish = asset(0);
         int32_t     active_change_block_num = 0;
         int32_t     reward_size = 0;
       //  vector<vote_reward_info> reward_vote    ;

         uint64_t primary_key() const { return name; }

         void update( public_key key, uint32_t rate, std::string u ) {
            block_signing_key = key;
            commission_rate = rate;
            url = u;
         }
         void     deactivate()       {active_type = static_cast<int32_t>(active_type::Removed);}
         bool     isactive() const {
            if (active_type == static_cast<int32_t>(active_type::Removed)) return false;
            return true;
         }
         EOSLIB_SERIALIZE(bp_info, ( name )(block_signing_key)(commission_rate)(total_staked)
               (rewards_block)(total_voteage)(voteage_update_height)(url)(emergency)(active_type)
               (block_age)(last_block_amount)(block_weight)(mortgage)(total_drain_block)(remain_punish)(active_change_block_num)(reward_size))
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

      struct reward_info {
         uint64_t     id;
         asset reward_block_out = asset(0);
         asset reward_develop = asset(0);
         asset reward_budget = asset(0);
         int64_t total_block_out_age = 0;
         int64_t cycle_reward = 0;
         int32_t   gradient = 0;
         int32_t   total_reward_time = 0;
         int32_t   last_reward_block_num = 0;
         account_name  last_producer_name;
         vector<int32_t> reward_block_num;

         uint64_t primary_key() const { return id; }
         EOSLIB_SERIALIZE(reward_info, ( id )(reward_block_out)(reward_develop)(reward_budget)(total_block_out_age)(cycle_reward)(gradient)
         (total_reward_time)(last_reward_block_num)(last_producer_name)(reward_block_num))
      };

      struct creation_bp {
         account_name bpname;
         int64_t      total_staked    = 0;
         int64_t      mortgage = 0;
         uint64_t primary_key() const { return bpname; }

         EOSLIB_SERIALIZE(creation_bp, (bpname)(total_staked)(mortgage))
      };

      struct punish_bp_info {
         account_name initiator;
         account_name bpname;
         int32_t  drain_num = 0;
         int32_t  update_block_num = 0;
         uint64_t primary_key() const { return bpname; }

         EOSLIB_SERIALIZE(punish_bp_info, (initiator)(bpname)(drain_num)(update_block_num))
      };

      struct last_drain_block {
         account_name bpname;
         int32_t  drain_num = 0;
         int32_t  update_block_num = 0;
         uint64_t primary_key() const { return bpname; }

         EOSLIB_SERIALIZE(last_drain_block, (bpname)(drain_num)(update_block_num))
      };

      struct approve_punish_bp {
         account_name bpname;
         vector<account_name> approve_producer;
         uint64_t primary_key() const { return bpname; }
         EOSLIB_SERIALIZE(approve_punish_bp, (bpname)(approve_producer))
      };

      /** from relay.token begin*/
      inline static uint128_t get_account_idx(const eosio::name& chain, const asset& a) {
         return (uint128_t(uint64_t(chain)) << 64) + uint128_t(a.symbol.name());
      }
      struct reward_mine_info {
         int128_t total_mineage = 0;
         asset    reward_pool = asset(0);
         int32_t  reward_block_num = 0;
         uint64_t primary_key() const { return reward_block_num; }
      };

      struct currency_stats {
         asset        supply;
         asset        max_supply;
         account_name issuer;
         eosio::name         chain;

         account_name side_account;
         action_name  side_action;

         int128_t     total_mineage               = 0; // asset.amount * block height
         uint32_t     total_mineage_update_height = 0;
         uint64_t     reward_scope;
         int32_t      reward_size = 0;
         //vector<reward_mine_info>   reward_mine;

         uint64_t primary_key() const { return supply.symbol.name(); }
      };
      struct reward_currency {
         uint64_t     id;
         eosio::name         chain;
         asset        supply;
         bool         reward_now = true;

         uint64_t primary_key() const { return id; }
         uint128_t get_index_i128() const { return get_account_idx(chain, supply); }
      };
      
      typedef eosio::multi_index<N(stat), currency_stats> stats;
      typedef eosio::multi_index<N(reward), reward_currency,
         eosio::indexed_by< N(bychain),
                     eosio::const_mem_fun<reward_currency, uint128_t, &reward_currency::get_index_i128 >>> rewards;
      typedef eosio::multi_index<N(minereward), reward_mine_info> reward_mine ;
      /** from relay.token end*/
      typedef eosio::multi_index<N(freezed),     freeze_info>   freeze_table;
      typedef eosio::multi_index<N(votes),       vote_info>     votes_table;
      typedef eosio::multi_index<N(mvotes),      votes_info>    mvotes_table;
      typedef eosio::multi_index<N(votes4ram),   vote_info>     votes4ram_table;
      typedef eosio::multi_index<N(vote4ramsum), vote4ram_info> vote4ramsum_table;
      typedef eosio::multi_index<N(bps),         bp_info>       bps_table;
      typedef eosio::multi_index<N(schedules),   schedule_info> schedules_table;
      typedef eosio::multi_index<N(reward),   reward_info> reward_table;
      typedef eosio::multi_index<N(creationbp),   creation_bp> creation_producer;
      typedef eosio::multi_index<N(lastdrainbp),   last_drain_block> last_drain_bp;
      typedef eosio::multi_index<N(punishbps),   punish_bp_info> punish_bps;
      typedef eosio::multi_index<N(apppunishbps),   approve_punish_bp> approve_punish_bps;
      typedef eosio::multi_index<N(votereward),   vote_reward_info> bp_vote_reward;

      mvotes_table _voters;

      //reward_info reward;
      void init_creation_bp();
      void update_elected_bps();

      void reward_bps(const int64_t reward_amount);
      void reward_block(const uint32_t schedule_version,const int64_t reward_amount,bool force_change);
      void reward_mines(const int64_t reward_amount);
      void reward_develop(const int64_t reward_amount);

      bool is_super_bp( account_name block_producers[], account_name name );

      bool is_super_bp( account_name bpname);
      int32_t effective_approve_num(account_name punishbpname); 
      void exec_punish_bp(account_name punishbpname);

      //defind in delegate_bandwidth.cpp
      void changebw( account_name from, account_name receiver,
                      asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );

      void update_votes( const account_name voter, const std::vector<account_name>& producers, bool voting );

      void reset_block_weight(account_name block_producers[]);
      int128_t get_coin_power();
      int128_t get_vote_power();

      void init_reward_info();
      void update_reward_stable();

      void settlebpvote();
      void settlevoter(const account_name voter, const account_name bpname);
   
   public:
      inline asset get_freezed( account_name voter )const;

   public:
      // @abi action
      void updatebp( const account_name bpname, const public_key producer_key,
                     const uint32_t commission_rate, const std::string& url );

      // @abi action
      void freeze( const account_name voter, const asset stake );

      // @abi action
      void unfreeze( const account_name voter );

      // @abi action
      void vote( const account_name voter, const account_name bpname, const asset stake );

      // @abi action
      void vote4ram( const account_name voter, const account_name bpname, const asset stake );

      // @abi action
      // void claim( const account_name voter, const account_name bpname );

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
      void addmortgage(const account_name bpname,const account_name payer,asset quantity);
      // @abi action
      void claimmortgage(const account_name bpname,const account_name receiver,asset quantity);
      // @abi action
      void claimvote(const account_name bpname,const account_name receiver);
      // @abi action
      void claimbp(const account_name bpname,const account_name receiver);
      // @abi action
      void claimdevelop(const account_name develop);
#if CONTRACT_RESOURCE_MODEL == RESOURCE_MODEL_DELEGATE
      // @abi action
      void delegatebw( account_name from, account_name receiver,
                     asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );
      // @abi action
      void undelegatebw( account_name from, account_name receiver,
                       asset unstake_net_quantity, asset unstake_cpu_quantity );
      // @abi action
      void refund( account_name owner );
#endif

      // @abi action
      void voteproducer( const account_name voter, const std::vector<account_name>& producers );
      
      // @abi action
      void fee( const account_name payer, const account_name bpname, int64_t voteage );
      // @abi action
      void punishbp(const account_name initiator,const account_name bpname);
      // @abi action
      void canclepunish(const account_name initiator,const account_name bpname);
      // @abi action
      void apppunish(const account_name bpname,const account_name punishbpname);
      // @abi action
      void unapppunish(const account_name bpname,const account_name punishbpname);
      // @abi action
      void bailpunish(const account_name bpname);
      // @abi action
      void rewardmine(int64_t reward_num);


   };
   
   asset system_contract::get_freezed( account_name voter )const
   {
      freeze_table freeze_tbl(_self, _self);
      auto fts = freeze_tbl.find(voter);
      eosio_assert(fts != freeze_tbl.end(), "voter has not freezed tokens");
      
      return fts->staked;
   }
};
