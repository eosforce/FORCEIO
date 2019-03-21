#include "force.system.hpp"

namespace eosiosystem {

    void system_contract::onblock( const block_timestamp, const account_name bpname, const uint16_t, const block_id_type,
                                  const checksum256, const checksum256, const uint32_t schedule_version ) {
      bps_table bps_tbl(_self, _self);
      schedules_table schs_tbl(_self, _self);

      account_name block_producers[NUM_OF_TOP_BPS] = {};
      get_active_producers(block_producers, sizeof(account_name) * NUM_OF_TOP_BPS);
      auto sch = schs_tbl.find(uint64_t(schedule_version));
      if( sch == schs_tbl.end()) {
         schs_tbl.emplace(bpname, [&]( schedule_info& s ) {
            s.version = schedule_version;
            s.block_height = current_block_num();           //这个地方有一个清零的过程 不管
            for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
               s.producers[i].amount = block_producers[i] == bpname ? 1 : 0;
               s.producers[i].bpname = block_producers[i];
            }
         });
      } else {
         schs_tbl.modify(sch, 0, [&]( schedule_info& s ) {
            for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
               if( s.producers[i].bpname == bpname ) {
                  s.producers[i].amount += 1;
                  break;
               }
            }
         });
      }

      INLINE_ACTION_SENDER(eosio::token, issue)( config::token_account_name, {{::config::system_account_name,N(active)}},
                                                 { ::config::system_account_name, 
                                                   asset(BLOCK_REWARDS_BP), 
                                                   "issue tokens for producer pay"} );

      print("UPDATE_CYCLE","---",current_block_num(),"---",UPDATE_CYCLE);
      if( current_block_num() % UPDATE_CYCLE == 0 ) {
         print("reward_bps\n");
         //先做奖励结算 然后再做BP换届
         //开发者账户   略
         //reward bps
         reward_bps();
         reward_block(schedule_version);
         //reward miners
         reward_mines();
         //update schedule
         update_elected_bps();
      }
   }

   void system_contract::updatebp( const account_name bpname, const public_key block_signing_key,
                                   const uint32_t commission_rate, const std::string& url ) {
      require_auth(bpname);
      eosio_assert(url.size() < 64, "url too long");
      eosio_assert(1 <= commission_rate && commission_rate <= 10000, "need 1 <= commission rate <= 10000");

      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      if( bp == bps_tbl.end()) {
         bps_tbl.emplace(bpname, [&]( bp_info& b ) {
            b.name = bpname;
            b.update(block_signing_key, commission_rate, url);
         });
      } else {
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.update(block_signing_key, commission_rate, url);
         });
      }
   }

   void system_contract::removebp( account_name bpname ) {
      require_auth(_self);

      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      if( bp == bps_tbl.end()) {
        eosio_assert(false,"bpname is not registered");
      } else {
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.deactivate();
         });
      }
   }

   void system_contract::update_elected_bps() {
      bps_table bps_tbl(_self, _self);

      std::vector<eosio::producer_key> vote_schedule;
      std::vector<int64_t> sorts(NUM_OF_TOP_BPS, 0);

      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         for( int i = 0; i < NUM_OF_TOP_BPS; ++i ) {
            if( sorts[size_t(i)] <= it->total_staked && it->isactive) {
               eosio::producer_key key;
               key.producer_name = it->name;
               key.block_signing_key = it->block_signing_key;
               vote_schedule.insert(vote_schedule.begin() + i, key);
               sorts.insert(sorts.begin() + i, it->total_staked);
               break;
            }
         }
      }

      if( vote_schedule.size() > NUM_OF_TOP_BPS ) {
         vote_schedule.resize(NUM_OF_TOP_BPS);
      }

      /// sort by producer name
      std::sort(vote_schedule.begin(), vote_schedule.end());
      bytes packed_schedule = pack(vote_schedule);
      set_proposed_producers(packed_schedule.data(), packed_schedule.size());
   }

   void system_contract::reward_mines() {
      eosio::action(
         vector<eosio::permission_level>{{N(relay.token),N(active)}},
         N(relay.token),
         N(rewardmine),
         std::make_tuple(
            asset(BLOCK_REWARDS_MINERS)
         )
      ).send();
   }

   // TODO it need change if no bonus to accounts

   void system_contract::reward_block(const uint32_t schedule_version ) {
      schedules_table schs_tbl(_self, _self);
      bps_table bps_tbl(_self, _self);
      print("reward_block",schedule_version,"\n");
      auto sch = schs_tbl.find(uint64_t(schedule_version));
      eosio_assert(sch != schs_tbl.end(),"cannot find schedule");
      int64_t total_block_out_age = 0;
      for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
         auto bp = bps_tbl.find(sch->producers[i].bpname);
         eosio_assert(bp != bps_tbl.end(),"cannot find bpinfo");
         print("reward_block --- ","modify bpinfo","\n");
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            if ( sch->producers[i].amount > b.last_block_amount ) {
               b.block_age +=  sch->producers[i].amount - b.last_block_amount;
               total_block_out_age += sch->producers[i].amount - b.last_block_amount;
            }
            else if (sch->producers[i].amount == b.last_block_amount || sch->producers[i].amount == 0) {
               //直接清零或者扣除保证金？ 不应该每次都清零吧
               total_block_out_age -= b.block_age;
               b.block_age = 0;
            }
            else {
               b.block_age +=  sch->producers[i].amount;
               total_block_out_age +=  sch->producers[i].amount;
            }
            b.last_block_amount = sch->producers[i].amount;
         });
      }
      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         reward_inf.emplace(_self, [&]( reward_info& s ) {
            s.id = REWARD_ID;
            s.reward_block_out = asset(BLOCK_REWARDS_BLOCK);
            s.reward_bp = asset(0);
            s.reward_develop = asset(0);
            s.total_block_out_age = total_block_out_age;
            s.total_bp_age = 0;
         });
      }
      else {
         reward_inf.modify(reward, 0, [&]( reward_info& s ) {
            s.reward_block_out += asset(BLOCK_REWARDS_BLOCK);
            s.total_block_out_age += total_block_out_age;
         });
      }

   }

   void system_contract::reward_bps() {
      bps_table bps_tbl(_self, _self);
      int64_t staked_all_bps = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         staked_all_bps += it->total_staked;
      }
      if( staked_all_bps <= 0 ) {
         return;
      }
      const auto rewarding_bp_staked_threshold = staked_all_bps / 200;
      int64_t total_bp_age = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         if( !it->isactive || it->total_staked <= rewarding_bp_staked_threshold || it->commission_rate < 1 ||
             it->commission_rate > 10000 ) {
            continue;
         }
         auto vote_reward = static_cast<int64_t>( BLOCK_REWARDS_VOTE  * double(it->total_staked) / double(staked_all_bps));
         //暂时先给所有的BP都增加BP奖励   还需要增加vote池的奖励
         const auto& bp = bps_tbl.get(it->name, "bpname is not registered");
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.bp_age += 1;
            b.rewards_pool += asset(vote_reward);
         });
         total_bp_age +=1;
      }

      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         reward_inf.emplace(_self, [&]( reward_info& s ) {
            s.id = REWARD_ID;
            s.reward_block_out = asset(0);
            s.reward_bp = asset(BLOCK_REWARDS_BP);
            s.reward_develop = asset(0);
            s.total_block_out_age = 0;
            s.total_bp_age = total_bp_age;
         });
      }
      else {
         reward_inf.modify(reward, 0, [&]( reward_info& s ) {
            s.reward_bp += asset(BLOCK_REWARDS_BP);
            s.total_bp_age += total_bp_age;
         });
      }
   }

   bool system_contract::is_super_bp( account_name block_producers[], account_name name ) {
      for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
         if( name == block_producers[i] ) {
            return true;
         }
      }
      return false;
   }
    
}
