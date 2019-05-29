#include "force.system.hpp"
#include <relay.token/relay.token.hpp>
#include <sys.match/sys.match.hpp>
#include <cmath>

namespace eosiosystem {
    const account_name SYS_MATCH = N(sys.match);
    
    void system_contract::onblock( const block_timestamp, const account_name bpname, const uint16_t, const block_id_type,
                                  const checksum256, const checksum256, const uint32_t schedule_version ) {
      bps_table bps_tbl(_self, _self);
      schedules_table schs_tbl(_self, _self);

      account_name block_producers[NUM_OF_TOP_BPS] = {};
      get_active_producers(block_producers, sizeof(account_name) * NUM_OF_TOP_BPS);
      auto sch = schs_tbl.find(uint64_t(schedule_version));
      uint32_t pre_bp_block_out = 0;
      bool force_change = false;
      auto reward_block_version = schedule_version;
      if( sch == schs_tbl.end()) {
         force_change = true;
         reward_block_version -=1;
      } else {
         for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
            if( sch->producers[i].bpname == bpname ) {
               pre_bp_block_out = sch->producers[i].amount;
               break;
            }
         }

         auto bp = bps_tbl.find(bpname);
         if (bp == bps_tbl.end()) {
            eosio_assert(false,"bpname is not registered");
         }
         else {
            pre_bp_block_out = pre_bp_block_out - bp->last_block_amount;
         }
      }
      print("onblock \n");
      if( pre_bp_block_out == CYCLE_PREBP_BLOCK || (force_change && schedule_version != 0)) {
         reward_table reward_inf(_self,_self);
         auto reward = reward_inf.find(REWARD_ID);
         if(reward == reward_inf.end()) {
            init_reward_info();
            reward = reward_inf.find(REWARD_ID);
         }
         
         auto cycle_block_out = current_block_num() - reward->last_reward_block_num;
         int64_t block_rewards = reward->cycle_reward * cycle_block_out / UPDATE_CYCLE;
         auto reward_times = reward->total_reward_time + 1;
         bool reward_update = false;

         if(reward_times == CYCLE_PREDAY * STABLE_DAY) {
            reward_update = true;
            update_reward_stable();
         }
         else if(reward_times % CYCLE_PREDAY == 0) {
            reward_update = true;
            reward_inf.modify(reward, 0, [&]( reward_info& s ) {
               s.cycle_reward = static_cast<int64_t>(s.cycle_reward * s.gradient / REWARD_RATIO_PRECISION);
            });
         }

         INLINE_ACTION_SENDER(eosio::token, issue)( config::token_account_name, {{::config::system_account_name,N(active)}},
                                             { ::config::reward_account_name, 
                                             asset(block_rewards), 
                                             "issue tokens for producer pay"} );
         reward_table reward_info_temp(_self,_self);
         auto reward_temp = reward_info_temp.find(REWARD_ID);
         reward_info_temp.modify(reward_temp, 0, [&]( reward_info& s ) {
            s.total_reward_time +=1;
            s.last_reward_block_num = current_block_num();
            s.last_producer_name = bpname;
            s.reward_block_num.push_back(current_block_num());
            if (REWARD_RECORD_SIZE < s.reward_block_num.size()) {
               s.reward_block_num.erase(std::begin(s.reward_block_num),std::begin(s.reward_block_num) + REWARD_RECORD_SIZE / 2);
            }
         });
         

         reward_develop(block_rewards * REWARD_DEVELOP / REWARD_RATIO_PRECISION);
         reward_block(reward_block_version,block_rewards * REWARD_BP / REWARD_RATIO_PRECISION,force_change);
   
         if (reward_update) {
            cycle_block_out = current_block_num() - reward->reward_block_num[reward->reward_block_num.size() - 1 - CYCLE_PREDAY];
            block_rewards = reward->cycle_reward * cycle_block_out / UPDATE_CYCLE * REWARD_MINE / REWARD_RATIO_PRECISION ;
            settlebpvote();
            INLINE_ACTION_SENDER(relay::token, settlemine)( ::config::relay_token_account_name, {{::config::system_account_name,N(active)}},
                                    { ::config::system_account_name} );

            INLINE_ACTION_SENDER(eosiosystem::system_contract, rewardmine)( ::config::system_account_name, {{::config::system_account_name,N(active)}},
                        { block_rewards} );

            INLINE_ACTION_SENDER(relay::token, activemine)( ::config::relay_token_account_name, {{::config::system_account_name,N(active)}},
                        { ::config::system_account_name} );

         }

         // print("logs:",block_rewards,"---",block_rewards * REWARD_DEVELOP / 10000,"---",block_rewards * REWARD_BP / 10000,"---",(block_rewards * REWARD_MINE / 10000) * coin_power / total_power,"---",
         // (block_rewards * REWARD_MINE / 10000) * vote_power / total_power,"\n");
         if (!force_change)   update_elected_bps();


      }

      if( sch == schs_tbl.end()) {
         schs_tbl.emplace(bpname, [&]( schedule_info& s ) {
            s.version = schedule_version;
            s.block_height = current_block_num();          
            for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
               s.producers[i].amount = block_producers[i] == bpname ? 1 : 0;
               s.producers[i].bpname = block_producers[i];
            }
         });
         reset_block_weight(block_producers);
      }
      else {
         schs_tbl.modify(sch, 0, [&]( schedule_info& s ) {
            for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
               if( s.producers[i].bpname == bpname ) {
                  s.producers[i].amount += 1;
                  break;
               }
            }
         });
      }
      
   }

   void system_contract::rewardmine(int64_t reward_num) {
      print("reward mine \n");
      require_auth(::config::system_account_name);
      uint64_t  vote_power = get_vote_power();
      uint64_t  coin_power = get_coin_power();
      uint64_t total_power = vote_power + coin_power;
      if (total_power != 0) {
         int64_t reward_bp = static_cast<int64_t>(static_cast<int128_t>(reward_num) * vote_power / total_power);
         reward_mines(reward_num - reward_bp);
         reward_bps(reward_bp);
      }
   }

   void system_contract::updatebp( const account_name bpname, const public_key block_signing_key,
                                   const uint32_t commission_rate, const std::string& url ) {
      require_auth(bpname);
      eosio_assert(url.size() < 64, "url too long");
      eosio_assert(1 <= commission_rate && commission_rate <= REWARD_RATIO_PRECISION, "need 1 <= commission rate <= 10000");

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

   void system_contract::init_creation_bp() {
      creation_producer creation_bp_tbl(_self,_self);
      for (int i=0;i!=26;++i) {
         creation_bp_tbl.emplace(_self, [&]( creation_bp& b ) {
            b.bpname = CREATION_BP[i].bp_name;
            b.total_staked = CREATION_BP[i].total_staked;
            b.mortgage = CREATION_BP[i].mortgage;
         });
      }
   }

   void system_contract::update_elected_bps() {
      bps_table bps_tbl(_self, _self);
      
      std::vector<eosio::producer_key> vote_schedule;
      std::vector<int64_t> sorts(NUM_OF_TOP_BPS, 0);

      creation_producer creation_bp_tbl(_self,_self);
      auto create_bp = creation_bp_tbl.find(CREATION_BP[0].bp_name);
      if (create_bp == creation_bp_tbl.end()) {
         init_creation_bp();
      }

      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         init_reward_info();
         reward = reward_inf.find(REWARD_ID);
      }

      int64_t reward_pre_block = reward->cycle_reward / UPDATE_CYCLE;
      auto block_mortgege = reward_pre_block * MORTGAGE;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         for( int i = 0; i < NUM_OF_TOP_BPS; ++i ) {
            auto total_shaked = it->total_staked;
            auto bp_mortgage = it->mortgage;
            if (it->isactive() && bp_mortgage <= asset(0)) {
               create_bp = creation_bp_tbl.find(it->name);
               if (create_bp != creation_bp_tbl.end()) {
                  total_shaked = create_bp->total_staked;
                  bp_mortgage = asset(create_bp->mortgage);
               }
            }
            if (it->active_type == static_cast<int32_t>(active_type::LackMortgage) && it->active_change_block_num < (current_block_num() - LACKMORTGAGE_FREEZE)) {
               bps_tbl.modify(it, _self, [&]( bp_info& b ) {
                  b.active_type = static_cast<int32_t>(active_type::Normal);
               });
            }

            if (bp_mortgage < asset(block_mortgege,CORE_SYMBOL) && is_super_bp(it->name) && it->active_type == static_cast<int32_t>(active_type::Normal)) {
               bps_tbl.modify(it, _self, [&]( bp_info& b ) {
                  b.active_type = static_cast<int32_t>(active_type::LackMortgage);
                  b.active_change_block_num = current_block_num();
               });
            }

            if( sorts[size_t(i)] <= total_shaked && it->isactive() && it->active_type != static_cast<int32_t>(active_type::Punish) 
                  && bp_mortgage > asset(block_mortgege)) {
               eosio::producer_key key;
               key.producer_name = it->name;
               key.block_signing_key = it->block_signing_key;
               vote_schedule.insert(vote_schedule.begin() + i, key);
               sorts.insert(sorts.begin() + i, total_shaked);
               break;
            }
         }
      }

      if( vote_schedule.size() > NUM_OF_TOP_BPS ) {
         vote_schedule.resize(NUM_OF_TOP_BPS);
      }

      std::sort(vote_schedule.begin(), vote_schedule.end());
      bytes packed_schedule = pack(vote_schedule);
      set_proposed_producers(packed_schedule.data(), packed_schedule.size());
   }

   void system_contract::reward_mines(const int64_t reward_amount) {
      eosio::action(
         vector<eosio::permission_level>{{_self,N(active)}},
         N(relay.token),
         N(rewardmine),
         std::make_tuple(
            asset(reward_amount)
         )
      ).send();
   }

   void system_contract::reward_develop(const int64_t reward_amount) {
      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         init_reward_info();
         reward = reward_inf.find(REWARD_ID);
      }

      reward_inf.modify(reward, 0, [&]( reward_info& s ) {
         s.reward_develop += asset(reward_amount);    
      });
   }

   // TODO it need change if no bonus to accounts

   void system_contract::reward_block(const uint32_t schedule_version,const int64_t reward_amount,bool force_change) {
      schedules_table schs_tbl(_self, _self);
      bps_table bps_tbl(_self, _self);
      auto sch = schs_tbl.find(uint64_t(schedule_version));
      eosio_assert(sch != schs_tbl.end(),"cannot find schedule");
      int64_t total_block_out_age = 0;
      asset total_punish = asset(0);
      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         init_reward_info();
         reward = reward_inf.find(REWARD_ID);
      }
      auto current_block = current_block_num();
      last_drain_bp drain_bp_tbl(_self,_self);
      int64_t reward_pre_block = reward->cycle_reward / UPDATE_CYCLE;
      for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
         auto bp = bps_tbl.find(sch->producers[i].bpname);
         eosio_assert(bp != bps_tbl.end(),"cannot find bpinfo");

         auto bp_mortgage = bp->mortgage;
         if (bp_mortgage <= asset(0)) {
            for (int i = 0;i!=26;++i) {
               if (CREATION_BP[i].bp_name == bp->name) {
                  bp_mortgage = asset(CREATION_BP[i].mortgage);
                  break;
               }
            }
         }
         
         if(bp_mortgage < asset(MORTGAGE * reward_pre_block)) {
            print(sch->producers[i].bpname," Insufficient mortgage, please replenish in time \n");
            continue;
         }
         auto drain_block_num = CYCLE_PREBP_BLOCK + bp->last_block_amount - sch->producers[i].amount;
         if (force_change) drain_block_num = 0;
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.block_age +=  (sch->producers[i].amount >= b.last_block_amount ? sch->producers[i].amount - b.last_block_amount : sch->producers[i].amount) * b.block_weight;
            total_block_out_age += (sch->producers[i].amount >= b.last_block_amount ? sch->producers[i].amount - b.last_block_amount : sch->producers[i].amount) * b.block_weight;
               
            if(drain_block_num != 0) {
               b.block_weight = BLOCK_OUT_WEIGHT;
               b.mortgage -= asset(reward_pre_block * 2 * drain_block_num,CORE_SYMBOL);
               b.remain_punish += asset(reward_pre_block * 2 * drain_block_num,CORE_SYMBOL);
               b.total_drain_block += drain_block_num;
            } else {
              b.block_weight += 1;
            }
            b.last_block_amount = sch->producers[i].amount;
         });

         auto drainbp = drain_bp_tbl.find(sch->producers[i].bpname);
         if (drain_block_num != 0) {
            if (drainbp == drain_bp_tbl.end()) {
               drain_bp_tbl.emplace(_self, [&]( last_drain_block& s ) {
                  s.bpname = sch->producers[i].bpname;
                  s.drain_num = drain_block_num;
                  s.update_block_num = current_block;
               });
            }
            else {
               drain_bp_tbl.modify(drainbp, _self, [&]( last_drain_block& s ) {
                  s.drain_num += drain_block_num;
                  s.update_block_num = current_block;
               });
            }
         }
         else {
            if (drainbp != drain_bp_tbl.end()) {
               drain_bp_tbl.erase(drainbp);
            }
         }
      }

      reward_inf.modify(reward, 0, [&]( reward_info& s ) {
         s.reward_block_out += asset(reward_amount);
         s.total_block_out_age += total_block_out_age;
      });
   }

   void system_contract::reward_bps(const int64_t reward_amount) {
      bps_table bps_tbl(_self, _self);
      int64_t staked_all_bps = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         bp_vote_reward bpvote_reward(_self,it->name);
         auto reward_bp = bpvote_reward.get(current_block_num(),"reward info can not find");
         staked_all_bps += reward_bp.total_voteage;
      }
      if( staked_all_bps <= 0 ) {
         return;
      }
      const auto rewarding_bp_staked_threshold = staked_all_bps / 200;
      auto budget_reward = asset{0,CORE_SYMBOL};
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         bp_vote_reward bpvote_reward(_self,it->name);
         auto reward_bp = bpvote_reward.get(current_block_num(),"reward info can not find");
         if( reward_bp.total_voteage <= rewarding_bp_staked_threshold || it->commission_rate < 1 ||
             it->commission_rate > REWARD_RATIO_PRECISION ) {
            continue;
         }

         auto vote_reward = static_cast<int64_t>( reward_amount  * (double(reward_bp.total_voteage) / double(staked_all_bps)));
         if (it->active_type == static_cast<int32_t>(active_type::Punish) || it->active_type == static_cast<int32_t>(active_type::Removed)) {
            budget_reward += asset(vote_reward,CORE_SYMBOL);
            continue;
         }
         
         const auto& bp = bps_tbl.get(it->name, "bpname is not registered");
         //auto ireward_index = bp.reward_vote.size() - 1;
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            //b.reward_vote[ireward_index].total_reward += asset(vote_reward * (REWARD_RATIO_PRECISION - b.commission_rate) / REWARD_RATIO_PRECISION);
            b.rewards_block += asset(vote_reward * b.commission_rate / REWARD_RATIO_PRECISION);
         });

         bpvote_reward.modify(reward_bp,0,[&]( vote_reward_info& b ) {
            b.total_reward += asset(vote_reward * (REWARD_RATIO_PRECISION - bp.commission_rate) / REWARD_RATIO_PRECISION);
         });
      }
      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         init_reward_info();
         reward = reward_inf.find(REWARD_ID);
      }
      reward_inf.modify(reward, _self, [&]( reward_info& s ) {
         s.reward_budget += budget_reward;
      });
   }

   void system_contract::init_reward_info() {
      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      if (reward == reward_inf.end()) {
         reward_inf.emplace(_self, [&]( reward_info& s ) {
            s.id = REWARD_ID;
            s.reward_block_out = asset(0);
            s.reward_develop = asset(0);
            s.total_block_out_age = 0;
            s.reward_budget = asset(0);
            s.cycle_reward = PRE_BLOCK_REWARDS;
            s.gradient = PRE_GRADIENT;
            s.reward_block_num.reserve(REWARD_RECORD_SIZE);
            s.reward_block_num.push_back(0);
         });
      }
   }

   void system_contract::update_reward_stable() {
      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      if (reward != reward_inf.end()) { 
         reward_inf.modify(reward, 0, [&]( reward_info& s ) {
            s.cycle_reward = STABLE_BLOCK_REWARDS;
            s.gradient = STABLE_GRADIENT;
         });
      }
   }

   void system_contract::reset_block_weight(account_name block_producers[]) {
      bps_table bps_tbl(_self, _self);
      for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
         const auto& bp = bps_tbl.get(block_producers[i], "bpname is not registered");
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.block_weight = BLOCK_OUT_WEIGHT;
            b.last_block_amount = 0;
         });
      }
   }
   
   int64_t precision(int64_t decimals)
   {
      int64_t p10 = 1;
      int64_t p = decimals;
      while( p > 0  ) {
         p10 *= 10; --p;
      }
      return p10;
   }

   int128_t system_contract::get_coin_power() {

      int128_t total_power = 0;
      rewards coin_reward(N(relay.token),N(relay.token));
      exchange::exchange t(SYS_MATCH);
      for( auto it = coin_reward.cbegin(); it != coin_reward.cend(); ++it ) {
         if (!it->reward_now) continue;
         stats statstable(::config::relay_token_account_name, it->chain);
         auto existing = statstable.find(it->supply.symbol.name());
         eosio_assert(existing != statstable.end(), "token with symbol already exists");
         auto price = t.get_avg_price(current_block_num(),existing->chain,existing->supply.symbol).amount;
         price = 10000;
         auto today_index = existing->reward_mine.size() - 1;
         int128_t power = existing->reward_mine[today_index].total_mineage;
         power = power * (int128_t)OTHER_COIN_WEIGHT;
         power = power / (int128_t)REWARD_RATIO_PRECISION;
         power = power * (int128_t)price;
         power = power / (int128_t)precision(existing->supply.symbol.precision());

         total_power += power;
      }
      return total_power;
   }

   int128_t system_contract::get_vote_power() {
      bps_table bps_tbl(_self, _self);
      int128_t staked_all_bps = 0;
      auto current_block = current_block_num();
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         bp_vote_reward bpvote_reward(_self,it->name);
         auto reward_bp = bpvote_reward.get(current_block_num(),"reward info can not find");
         staked_all_bps += reward_bp.total_voteage;
      }
      return static_cast<int128_t>(staked_all_bps)* CORE_SYMBOL_PRECISION;
   }

   void system_contract::addmortgage(const account_name bpname,const account_name payer,asset quantity) {
      require_auth(payer);

      creation_producer creation_bp_tbl(_self,_self);
      auto create_bp = creation_bp_tbl.find(bpname);
      eosio_assert(create_bp == creation_bp_tbl.end(),"creation bp cannot add mortgage");
      
      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");
      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.mortgage += quantity;
         });

      INLINE_ACTION_SENDER(eosio::token, transfer)(
               config::token_account_name,
               { payer, N(active) },
               { payer, ::config::reward_account_name, asset(quantity), "add mortgage" });
   }

   void system_contract::claimmortgage(const account_name bpname,const account_name receiver,asset quantity) {
      require_auth(bpname);
      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");
      eosio_assert(bp->mortgage > quantity,"the quantity is bigger then bp mortgage");
      
      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.mortgage -= quantity;
         });
      
      INLINE_ACTION_SENDER(eosio::token, transfer)(
         config::token_account_name,
         { ::config::reward_account_name, N(active) },
         { ::config::reward_account_name, receiver, quantity, "claim bp mortgage" });
   }

   void system_contract::claimdevelop(const account_name develop) {
      require_auth(develop);
      eosio_assert (develop == N(fosdevelop),"invaild develop account");
      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      eosio_assert(reward != reward_inf.end(),"reward info do not find");

      auto reward_develop = reward->reward_develop;
      reward_inf.modify(reward, 0, [&]( reward_info& s ) {
         s.reward_develop = asset(0);
      });
      eosio_assert(reward_develop > asset(100000),"claim amount must > 10");
      INLINE_ACTION_SENDER(eosio::token, castcoin)(
         config::token_account_name,
         { ::config::reward_account_name, N(active) },
         { ::config::reward_account_name, develop, reward_develop });
   }

   void system_contract::claimbp(const account_name bpname,const account_name receiver) {
      require_auth(bpname);
      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");

      reward_table reward_inf(_self,_self);
      auto reward = reward_inf.find(REWARD_ID);
      eosio_assert(reward != reward_inf.end(),"reward info do not find");

      asset reward_block_out = reward->reward_block_out;
      int64_t total_block_out_age = reward->total_block_out_age;
      int64_t block_age = bp->block_age;

      auto claim_block = reward_block_out * block_age/total_block_out_age;

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.block_age = 0;
         });

      reward_inf.modify(reward, 0, [&]( reward_info& s ) {
            s.reward_block_out -= claim_block;
            s.total_block_out_age -= block_age;
         });
      eosio_assert(claim_block > asset(100000),"claim amount must > 10");
      if (bp->mortgage == asset(0)) {
         reward_inf.modify(reward, 0, [&]( reward_info& s ) {
            s.reward_budget += claim_block;
         });
      }
      else {
         INLINE_ACTION_SENDER(eosio::token, castcoin)(
            config::token_account_name,
            { ::config::reward_account_name, N(active) },
            { ::config::reward_account_name, receiver, claim_block });
      }

   }

   void system_contract::claimvote(const account_name bpname,const account_name receiver) {
      require_auth(receiver);
      settlevoter(receiver,bpname);
      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      votes_table votes_tbl(_self, receiver);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      auto reward_all = vts.total_reward;
      if( receiver == bpname ) {
         reward_all += bp.rewards_block;
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.rewards_block = asset(0);
         });
      }

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.total_reward = asset(0);
      });

      eosio_assert(reward_all> asset(100000),"claim amount must > 10");
      INLINE_ACTION_SENDER(eosio::token, castcoin)(
            config::token_account_name,
            { ::config::reward_account_name, N(active) },
            { ::config::reward_account_name, receiver, reward_all});
      
   }

   bool system_contract::is_super_bp( account_name block_producers[], account_name name ) {
      for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
         if( name == block_producers[i] ) {
            return true;
         }
      }
      return false;
   }

   bool system_contract::is_super_bp( account_name bpname) {
      schedules_table schs_tbl(_self, _self);
      auto producer_schs = schs_tbl.crbegin();
      int32_t i = 0;
      for ( i = 0;i!= NUM_OF_TOP_BPS; ++i) {
         if (producer_schs->producers[i].bpname == bpname) {
            return true;
         }
      }
      return false;
   }

   int32_t system_contract::effective_approve_num(account_name punishbpname) {
      punish_bps punish_bp(_self,_self);
      auto bp_punish = punish_bp.find(punishbpname);
      eosio_assert(bp_punish != punish_bp.end(), "the bp is not be punish");

      approve_punish_bps app_punish_bp(_self,_self);
      auto app_punish = app_punish_bp.find(punishbpname);
      eosio_assert(app_punish != app_punish_bp.end(), "no producer approve to punish the bp");

      schedules_table schs_tbl(_self, _self);
      auto producer_schs = schs_tbl.crbegin();
      int32_t i = 0,j = 0,result = 0;
      for (i=0;i!=app_punish->approve_producer.size();++i) {
         for (j=0;j!=NUM_OF_TOP_BPS;++j) {
            if (producer_schs->producers[j].bpname == app_punish->approve_producer[i]) {
               result++;
               break;
            }
         }
      }

      return result;
   }

   void system_contract::exec_punish_bp(account_name punishbpname) {
      auto effective_approve = effective_approve_num(punishbpname);
      if (effective_approve < NUM_OF_TOP_BPS * 2 / 3) {
         print("Need confirmation from other producers to punish the bp");
         return ;
      }

      bps_table bps_tbl(_self, _self);
      auto punishbp = bps_tbl.find(punishbpname);
      if (punishbp == bps_tbl.end()) {
         print("can not find punish bp");
         return ;
      }

      auto total_reward = punishbp->remain_punish;
      bps_tbl.modify(punishbp, _self, [&]( bp_info& b ) {
         b.remain_punish = asset{0,CORE_SYMBOL};
         b.active_type = static_cast<int32_t>(active_type::Punish);
         b.active_change_block_num = current_block_num();
      });

      auto reward_initiator = total_reward / 2;

      punish_bps punish_bp(_self,_self);
      auto bp_punish = punish_bp.find(punishbpname);
      eosio_assert(bp_punish != punish_bp.end(), "the bp is not be punish");
      INLINE_ACTION_SENDER(force::token, transfer)(
         config::token_account_name,
         { ::config::system_account_name, N(active) },
         { ::config::system_account_name, bp_punish->initiator, reward_initiator + PUNISH_BP_FEE, "cancle punishbp deposit" });
      punish_bp.erase(bp_punish);

      auto reward_approve = total_reward / 2 / effective_approve;
      approve_punish_bps app_punish_bp(_self,_self);
      auto app_punish = app_punish_bp.find(punishbpname);
      eosio_assert(app_punish != app_punish_bp.end(), "no producer approve to punish the bp");
      auto iSize = app_punish->approve_producer.size();
      for(int i=0;i!=iSize;++i) {
         if (is_super_bp(app_punish->approve_producer[i])) {
            INLINE_ACTION_SENDER(force::token, transfer)(
               config::token_account_name,
               { ::config::system_account_name, N(active) },
               { ::config::system_account_name, app_punish->approve_producer[i], reward_approve, "cancle punishbp deposit" });
         }
      }
      app_punish_bp.erase(app_punish);

      last_drain_bp drain_bp_tbl(_self,_self);
      auto drainbp = drain_bp_tbl.find(punishbpname); 
      if (drainbp != drain_bp_tbl.end()) {
         drain_bp_tbl.erase(drainbp);
      }
   }

   // @abi action
   void system_contract::punishbp(const account_name initiator,const account_name bpname) {
      require_auth(initiator);

      INLINE_ACTION_SENDER(force::token, transfer)(
         ::config::token_account_name,
         { initiator, N(active) },
         { initiator, ::config::system_account_name, PUNISH_BP_FEE, "punishbp deposit" });

      last_drain_bp drain_bp_tbl(_self,_self);
      auto drainbp = drain_bp_tbl.find(bpname); 
      eosio_assert(drainbp != drain_bp_tbl.end(),"the bp cannot be punished");

      creation_producer creation_bp_tbl(_self,_self);
      auto create_bp = creation_bp_tbl.find(bpname);
      eosio_assert(create_bp == creation_bp_tbl.end(),"creation bp cannot be punished");

      punish_bps punish_bp(_self,_self);
      auto bp_punish = punish_bp.find(bpname);
      if (bp_punish != punish_bp.end()) {
         eosio_assert(bp_punish->update_block_num < static_cast<int32_t>(current_block_num() - UPDATE_CYCLE * CYCLE_PREDAY),"the punish motion has exist");

         INLINE_ACTION_SENDER(force::token, transfer)(
            ::config::token_account_name,
            { ::config::system_account_name, N(active) },
            { ::config::system_account_name, bp_punish->initiator, PUNISH_BP_FEE, "cancle punishbp return deposit" });

         punish_bp.erase(bp_punish);

         approve_punish_bps app_punish_bp(_self,_self);
         auto app_punish = app_punish_bp.find(bpname);
         if (app_punish != app_punish_bp.end()) {
            app_punish_bp.erase(app_punish);
         }
      }

      punish_bp.emplace(initiator,[&]( punish_bp_info& s) {
         s.bpname = bpname;
         s.initiator = initiator;
         s.drain_num = drainbp->drain_num;
         s.update_block_num = current_block_num();
      });
   }
   // @abi action
   void system_contract::canclepunish(const account_name initiator,const account_name bpname) {
      require_auth(initiator);

      punish_bps punish_bp(_self,_self);
      auto bp_punish = punish_bp.find(bpname);
      eosio_assert(bp_punish != punish_bp.end(), "the bp is not be punish");
      eosio_assert(initiator == bp_punish->initiator,"only the initiator can cancle the punish proposal");
      
      if ( bp_punish->update_block_num < static_cast<int32_t>(current_block_num() - UPDATE_CYCLE * CYCLE_PREDAY)) {
         INLINE_ACTION_SENDER(force::token, transfer)(
            ::config::token_account_name,
            { ::config::system_account_name, N(active) },
            { ::config::system_account_name, initiator, PUNISH_BP_FEE, "cancle punishbp return deposit" });
      }
      
      punish_bp.erase(bp_punish);

      approve_punish_bps app_punish_bp(_self,_self);
      auto app_punish = app_punish_bp.find(bpname);
      if (app_punish != app_punish_bp.end()) {
         app_punish_bp.erase(app_punish);
      }
   }
   // @abi action
   void system_contract::apppunish(const account_name bpname,const account_name punishbpname) {
      require_auth(bpname);

      eosio_assert(is_super_bp(bpname),"only super bp can approve to punish bp");

      punish_bps punish_bp(_self,_self);
      auto bp_punish = punish_bp.find(punishbpname);
      eosio_assert(bp_punish != punish_bp.end(), "the bp is not be punish");
      eosio_assert(bp_punish->update_block_num > static_cast<int32_t>(current_block_num() - UPDATE_CYCLE * CYCLE_PREDAY), "the punish motion is already expired");

      approve_punish_bps app_punish_bp(_self,_self);
      auto app_punish = app_punish_bp.find(punishbpname);
      if (app_punish == app_punish_bp.end()) {
         app_punish_bp.emplace(bpname,[&]( approve_punish_bp& s) {
            s.bpname = punishbpname;
            s.approve_producer.push_back(bpname);
         });
      }
      else {
         auto iter = std::find(app_punish->approve_producer.begin(),app_punish->approve_producer.end(),bpname);
         eosio_assert(iter == app_punish->approve_producer.end(),"the bp has approved");
         app_punish_bp.modify(app_punish, _self, [&]( approve_punish_bp& s ) {
               s.approve_producer.push_back(bpname);
            });
      }

      app_punish = app_punish_bp.find(punishbpname);
      if (app_punish->approve_producer.size() > NUM_OF_TOP_BPS * 2 / 3 ) {
         exec_punish_bp(punishbpname);
      }
   }
   // @abi action
   void system_contract::unapppunish(const account_name bpname,const account_name punishbpname) {
      require_auth(bpname);
     
      approve_punish_bps app_punish_bp(_self,_self);
      auto app_punish = app_punish_bp.find(punishbpname);
      eosio_assert(app_punish != app_punish_bp.end(), "can find the approve punish record");
      app_punish_bp.erase(app_punish);
   }
   // @abi action
   void system_contract::bailpunish(const account_name punishbpname) {
      require_auth(punishbpname);

      INLINE_ACTION_SENDER(force::token, transfer)(
         ::config::token_account_name,
         { punishbpname, N(active) },
         { punishbpname, ::config::system_account_name, BAIL_PUNISH_FEE, "bail punish fee" });

      bps_table bps_tbl(_self, _self);
      auto punishbp = bps_tbl.find(punishbpname);
      eosio_assert(punishbp != bps_tbl.end(),"can not find bp");

      if (punishbp->active_type == static_cast<int32_t>(active_type::Punish) 
         && punishbp->active_change_block_num + UPDATE_CYCLE*CYCLE_PREDAY*2 < current_block_num()) {
         bps_tbl.modify(punishbp, _self, [&]( bp_info& b ) {
            b.active_type = static_cast<int32_t>(active_type::Normal);
         });
      }
      else {
         eosio_assert(false,"the bp can not to be bailed");
      }
   }

   void system_contract::settlebpvote() {
      bps_table bps_tbl(_self, _self);
      auto current_block = current_block_num();

      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         bp_vote_reward bpvote_reward(_self,it->name);
         auto reward_bp = bpvote_reward.find(current_block);
         if (reward_bp == bpvote_reward.end()) {
            bpvote_reward.emplace(_self, [&]( vote_reward_info& s ) {
               s.reward_block_num = current_block;
               s.total_voteage = it->total_voteage + it->total_staked * (current_block - it->voteage_update_height);
            });
         }
         print("settle bp \n");
         bps_tbl.modify(*it, 0, [&]( bp_info& b ) {
            b.voteage_update_height = current_block;
            b.total_voteage = 0;
         });
      }
   }

   void system_contract::settlevoter(const account_name voter, const account_name bpname) {
      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");
      auto current_block = current_block_num();

      votes_table votes_tbl(_self, voter);
      auto vts = votes_tbl.find(bpname);
      eosio_assert(vts != votes_tbl.end(),"settlevoter wrong");

      bp_vote_reward bpvote_reward(_self,bpname);
     // auto reward_bp = bpvote_reward.find(current_block);
  
      auto last_voteage = vts->voteage;
      auto last_voteage_update_height = vts->voteage_update_height;
      auto total_reward = asset(0);
      bool cross_day = false;
      for (auto it = bpvote_reward.begin();it != bpvote_reward.end();++it) {
         if (last_voteage_update_height < it->reward_block_num) {
            auto day_voteage = last_voteage + vts->vote.amount / CORE_SYMBOL_PRECISION * (it->reward_block_num - last_voteage_update_height);
            auto reward = it->total_reward * day_voteage / it->total_voteage;
            total_reward += reward;
            last_voteage_update_height = it->reward_block_num;
            last_voteage = 0;
            // bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            //    b.reward_vote[i].total_voteage -= day_voteage;
            //    b.reward_vote[i].total_reward -= reward;
            // });
            bpvote_reward.modify(*it,0,[&]( vote_reward_info& b ) {
               b.total_voteage -= day_voteage;
               b.total_reward -= reward;
            });
            cross_day = true;
         }
      }
      //modify 
      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.total_reward += total_reward;
         if (cross_day) {
            v.voteage = v.vote.amount / CORE_SYMBOL_PRECISION * (current_block - last_voteage_update_height);
         }
         else {
            v.voteage += v.vote.amount / CORE_SYMBOL_PRECISION * (current_block - last_voteage_update_height);
         }
         
         v.voteage_update_height = current_block;
      });
   }
    
}
