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
            s.block_height = current_block_num();
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

      INLINE_ACTION_SENDER(eosio::token, issue)( N(eosio.token), {{N(eosio),N(active)}},
                                                 {N(eosio), asset(BLOCK_REWARDS_BP), std::string("issue tokens for producer pay")} );
      //reward bps
      reward_bps(block_producers);

      if( current_block_num() % UPDATE_CYCLE == 0 ) {
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

   void system_contract::reward_bps( account_name block_producers[] ) {
      bps_table bps_tbl(_self, _self);
      schedules_table schs_tbl(_self, _self);

      //calculate total staked all of the bps
      int64_t staked_all_bps = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         staked_all_bps += it->total_staked;
      }
      if( staked_all_bps <= 0 ) {
         return;
      }
      //0.5% of staked_all_bps
      const auto rewarding_bp_staked_threshold = staked_all_bps / 200;

      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         if( !it->isactive || it->total_staked <= rewarding_bp_staked_threshold || it->commission_rate < 1 ||
             it->commission_rate > 10000 ) {
            continue;
         }

         auto bp_reward = static_cast<int64_t>( BLOCK_REWARDS_BP * double(it->total_staked) / double(staked_all_bps));

         //reward bp account
         auto bp_account_reward = bp_reward * 15 / 100 + bp_reward * 70 / 100 * it->commission_rate / 10000;
         if( is_super_bp(block_producers, it->name)) {
            bp_account_reward += bp_reward * 15 / 100;
         }

         //reward bp and pool
         auto bp_rewards_pool = bp_reward * 70 / 100 * ( 10000 - it->commission_rate ) / 10000;
         const auto& bp = bps_tbl.get(it->name, "bpname is not registered");
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.rewards_pool += asset(bp_rewards_pool);
            b.rewards_block += asset(bp_account_reward);
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