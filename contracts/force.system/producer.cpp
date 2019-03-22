#include "force.system.hpp"
#include <relay.token/relay.token.hpp>

namespace eosiosystem {

    void system_contract::onblock( const block_timestamp, const account_name bpname, const uint16_t, const block_id_type,
                                  const checksum256, const checksum256, const uint32_t schedule_version ) {
      bps_table bps_tbl(_self, _self);
      schedules_table schs_tbl(_self, _self);

      account_name block_producers[NUM_OF_TOP_BPS] = {};
      get_active_producers(block_producers, sizeof(account_name) * NUM_OF_TOP_BPS);
      auto sch = schs_tbl.find(uint64_t(schedule_version));
      if( sch == schs_tbl.end()) {
         //换届
         schs_tbl.emplace(bpname, [&]( schedule_info& s ) {
            s.version = schedule_version;
            s.block_height = current_block_num();           //这个地方有一个清零的过程 不管
            for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
               s.producers[i].amount = block_producers[i] == bpname ? 1 : 0;
               s.producers[i].bpname = block_producers[i];
            }
         });
         reset_block_weight(block_producers);
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
         uint64_t  vote_power = get_vote_power();
         uint64_t  coin_power = get_coin_power();
         uint64_t total_power = vote_power + coin_power;
         print("before reward_block ",vote_power,"---",coin_power,"---",BLOCK_REWARDS_POWER,"\n");
         reward_block(schedule_version);
         //reward miners
         reward_mines(BLOCK_REWARDS_POWER * coin_power / total_power);
         reward_bps(BLOCK_REWARDS_POWER * vote_power / total_power);
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

   void system_contract::reward_mines(const uint64_t reward_amount) {
      eosio::action(
         vector<eosio::permission_level>{{N(relay.token),N(active)}},
         N(relay.token),
         N(rewardmine),
         std::make_tuple(
            asset(reward_amount)
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
         //eosio_assert(bp->mortgage > asset(88888888),"");
         if(bp->mortgage < asset(MORTGAGE)) {
            //抵押不足,没有出块奖励
            print("missing mortgage ",bp->name,"\n");
            continue;
         }
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            //先检测是否有漏块情况
            if(sch->producers[i].amount - b.last_block_amount != PER_CYCLE_AMOUNT && sch->producers[i].amount > b.last_block_amount) {
               b.block_age +=  (sch->producers[i].amount - b.last_block_amount) * b.block_weight;
               total_block_out_age += (sch->producers[i].amount - b.last_block_amount) * b.block_weight;
               b.block_weight = MORTGAGE;
               b.mortgage -= asset(0.2*10000);
               print("some block missing ",b.name,"\n");
            } else {
               b.block_age +=  (sch->producers[i].amount > b.last_block_amount ? sch->producers[i].amount - b.last_block_amount : sch->producers[i].amount) * b.block_weight;
               total_block_out_age +=  (sch->producers[i].amount > b.last_block_amount ? sch->producers[i].amount - b.last_block_amount : sch->producers[i].amount) * b.block_weight;
               b.block_weight += 1;
               print("normal reward block ",b.name,"\n");
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

   void system_contract::reward_bps(const uint64_t reward_amount) {
      bps_table bps_tbl(_self, _self);
      int64_t staked_all_bps = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         staked_all_bps += it->total_staked;
      }
      if( staked_all_bps <= 0 ) {
         print("staked_all_bps <= 0 renturn \n");
         return;
      }
      const auto rewarding_bp_staked_threshold = staked_all_bps / 200;
      int64_t total_bp_age = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         if( !it->isactive || it->total_staked <= rewarding_bp_staked_threshold || it->commission_rate < 1 ||
             it->commission_rate > 10000 ) {
            print("bp not normal comtinue \n");
            continue;
         }
         auto vote_reward = static_cast<int64_t>( reward_amount  * double(it->total_staked) / double(staked_all_bps));
         //暂时先给所有的BP都增加BP奖励   还需要增加vote池的奖励
         const auto& bp = bps_tbl.get(it->name, "bpname is not registered");
         print("bp age add 1 --",it->name,"---",vote_reward,"---",reward_amount,"\n");
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.bp_age += 1;
            b.rewards_pool += asset(vote_reward * b.commission_rate / 10000);
            b.rewards_block += asset(vote_reward * (10000 - b.commission_rate) / 10000);
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

   void system_contract::reset_block_weight(account_name block_producers[]) {
      bps_table bps_tbl(_self, _self);
      for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
         const auto& bp = bps_tbl.get(block_producers[i], "bpname is not registered");
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.block_weight = BLOCK_OUT_WEIGHT;
         });
      }
   }

   int64_t system_contract::get_coin_power() {
      int64_t total_power = 0; 
      rewards coin_reward(N(relay.token),N(relay.token));
      for( auto it = coin_reward.cbegin(); it != coin_reward.cend(); ++it ) {
         //根据it->chain  和it->supply 获取算力值     暂订算力值为supply.amount的0.1
         stats statstable(N(relay.token), it->chain);
         auto existing = statstable.find(it->supply.symbol.name());
         eosio_assert(existing != statstable.end(), "token with symbol already exists");
         total_power += existing->supply.amount * 0.1;
      }
      return total_power ;
   }

   int64_t system_contract::get_vote_power() {
      bps_table bps_tbl(_self, _self);
      int64_t staked_all_bps = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         staked_all_bps += it->total_staked;
      }
      print("get_vote_power --",staked_all_bps,"\n");
      return staked_all_bps* 10000;
   }
   //增加抵押
   void system_contract::addmortgage(const account_name bpname,const account_name payer,asset quantity) {
      require_auth(payer);
      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");
      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.mortgage += quantity;
         });

      INLINE_ACTION_SENDER(eosio::token, transfer)(
               config::token_account_name,
               { payer, N(active) },
               { payer, ::config::system_account_name, asset(quantity), "add mortgage" });
   }
   //提取抵押
   void system_contract::claimmortgage(const account_name bpname,const account_name receiver,asset quantity) {
      require_auth(bpname);
      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");
      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.mortgage -= quantity;
         });
      
      INLINE_ACTION_SENDER(eosio::token, transfer)(
         config::token_account_name,
         { ::config::system_account_name, N(active) },
         { ::config::system_account_name, receiver, quantity, "claim bp mortgage" });
   }
   //BP领取分红
   void system_contract::claimbp(const account_name bpname,const account_name receiver) {
      require_auth(bpname);
      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");
      //出块分红
      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      eosio_assert(reward != reward_inf.end(),"reward info do not find");

      asset reward_block_out = reward->reward_block_out;
      asset reward_bp = reward->reward_bp;
      int64_t total_block_out_age = reward->total_block_out_age;
      int64_t total_bp_age = reward->total_bp_age;

      int64_t block_age = bp->block_age;
      int64_t bp_age = bp->bp_age;

      //auto total_reward = reward_block_out * block_age/total_block_out_age + reward_bp * bp_age / total_bp_age;
      auto claim_block = reward_block_out * block_age/total_block_out_age;
      auto claim_bp = reward_bp * bp_age / total_bp_age;
      print("claim bp -----",reward_block_out,"---",block_age,"---",total_block_out_age,"---",claim_block,"\n");
      print("claim bp -----",reward_bp,"---",bp_age,"---",total_bp_age,"---",claim_bp,"\n");
      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.block_age = 0;
            b.bp_age = 0;
         });

      reward_inf.modify(reward, 0, [&]( reward_info& s ) {
            s.reward_block_out -= claim_block;
            s.reward_bp -= claim_bp;
            s.total_block_out_age -= block_age;
            s.total_bp_age -= bp_age;
         });

      INLINE_ACTION_SENDER(eosio::token, transfer)(
         config::token_account_name,
         { ::config::system_account_name, N(active) },
         { ::config::system_account_name, receiver, claim_block + claim_bp, "claim Dividend" });
   }

   void system_contract::claimvote(const account_name bpname,const account_name receiver) {
      require_auth(receiver);

// #if !IS_ACTIVE_BONUS_TO_VOTE
//       eosio_assert(false, "curr chain no bonus to account who vote");
// #endif

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      votes_table votes_tbl(_self, receiver);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      const auto curr_block_num = current_block_num();
      const auto last_devide_num = curr_block_num - (curr_block_num % UPDATE_CYCLE);

      const auto newest_voteage =
            static_cast<int128_t>(vts.voteage + vts.vote.amount / 10000 * (last_devide_num - vts.voteage_update_height));
      const auto newest_total_voteage =
            static_cast<int128_t>(bp.total_voteage + bp.total_staked * (last_devide_num - bp.voteage_update_height));
      print("claimvote voteage --- ",vts.voteage,"---",vts.vote.amount,"---",last_devide_num,"---",vts.voteage_update_height,"\n");
      print("claimvote total voteage --- ",bp.total_voteage,"---",bp.total_staked,"---",last_devide_num,"---",bp.voteage_update_height,"\n");
      eosio_assert(0 < newest_total_voteage, "claim is not available yet");

      //奖池构成  15%给BP  其他给投票人   commission_rate / 10000 来奖励BP
      const auto amount_voteage = static_cast<int128_t>(bp.rewards_pool.amount) * newest_voteage;
      asset reward = asset(static_cast<int64_t>( amount_voteage / newest_total_voteage ));
      print("claimvote ---",bp.rewards_pool.amount,"---",bp.commission_rate,"---",newest_voteage,"---",newest_total_voteage,"\n");
      eosio_assert(asset{} <= reward && reward <= bp.rewards_pool,
                   "need 0 <= claim reward quantity <= rewards_pool");

      auto reward_all = reward;
      if( receiver == bpname ) {
         reward_all += bp.rewards_block;
      }

      print("claimvote ---",bp.rewards_pool.amount,"---",bp.commission_rate,"---",reward_all,"\n");
      eosio_assert(reward_all > asset{}, "no any reward!");
      INLINE_ACTION_SENDER(eosio::token, transfer)(
            config::token_account_name,
            { ::config::system_account_name, N(active) },
            { ::config::system_account_name, receiver, reward_all, "claim Dividend" });

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.voteage = 0;
         v.voteage_update_height = last_devide_num;
      });

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.rewards_pool -= reward_all;
         b.total_voteage = static_cast<int64_t>(newest_total_voteage - newest_voteage);
         b.voteage_update_height = last_devide_num;
         if( receiver == bpname ) {
            b.rewards_block = asset(0);
         }
      });
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
