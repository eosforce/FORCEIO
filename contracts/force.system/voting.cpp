#include "force.system.hpp"

namespace eosiosystem {
   void system_contract::freeze( const account_name voter, const asset stake ){
      require_auth(voter);

      eosio_assert(stake.symbol == CORE_SYMBOL, "only support CORE SYMBOL token");
      eosio_assert(0 <= stake.amount && stake.amount % 10000 == 0,
                   "need stake quantity >= 0 and quantity is integer");

      const auto curr_block_num = current_block_num();

      auto change = stake;
      freeze_table freeze_tbl(_self, _self);
      auto fts = freeze_tbl.find(voter);
      if( fts == freeze_tbl.end() ) {
         freeze_tbl.emplace(voter, [&]( freeze_info& v ) {
            v.voter          = voter;
            v.staked         = stake;
            v.unstake_height = curr_block_num;
         });
      } else {
         change -= fts->staked;
         freeze_tbl.modify(fts, 0, [&]( freeze_info& v ) {
            v.staked = stake;
            if( change < asset{0} ) {
               v.unstaking      += (-change);
               v.unstake_height =  curr_block_num;
            }
         });

         // process multiple vote
         auto mv = _voters.find(voter);
         if((mv != _voters.end()) && (change != asset{})){
            update_votes(voter, mv->producers, false);
         }
      }

      if( change > asset{0} ) {
         INLINE_ACTION_SENDER(eosio::token, transfer)(
               config::token_account_name,
               { voter, N(active) },
               { voter, ::config::system_account_name, asset(change), "freeze" });
      }
   }

   // vote vote to a bp from voter to bpname with stake EOSC
   void system_contract::vote( const account_name voter, const account_name bpname, const asset stake ) {
      require_auth(voter);

#if IS_ACTIVE_MULTIPLE_VOTE
      eosio_assert(false, "curr chain is active mutiple vote, no allow vote to simple bp");
#endif

      creation_producer creation_bp_tbl(_self,_self);
      auto create_bp = creation_bp_tbl.find(bpname);
      eosio_assert(create_bp == creation_bp_tbl.end(),"creation bp can not to be voted");
      
      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      eosio_assert(stake.symbol == CORE_SYMBOL, "only support CORE SYMBOL token");
      eosio_assert(0 <= stake.amount && stake.amount % 10000 == 0,
                   "need stake quantity >= 0 and quantity is integer");

      const auto curr_block_num = current_block_num();

      freeze_table freeze_tbl(_self, _self);

      auto change = stake;
      votes_table votes_tbl(_self, voter);
      auto vts = votes_tbl.find(bpname);
      if( vts == votes_tbl.end() ) {
         // First vote the bp, it will cause ram add
         votes_tbl.emplace(voter, [&]( vote_info& v ) {
            v.bpname = bpname;
            v.vote = stake;
            v.voteage_update_height = curr_block_num;
         });
      } else {
         change -= vts->vote;
         votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
            v.voteage += (v.vote.amount / 10000) * (curr_block_num - v.voteage_update_height);
            v.voteage_update_height = curr_block_num;
            v.vote = stake;
            if( change < asset{} ) {
               auto fts = freeze_tbl.find(voter);
               if( fts == freeze_tbl.end() ) {
                  freeze_tbl.emplace(voter, [&]( freeze_info& f ) {
                     f.voter          = voter;
                     f.staked         = (-change);
                     f.unstake_height = curr_block_num;
                  });
               } else {
                  freeze_tbl.modify(fts, 0, [&]( freeze_info& f ) {
                     f.staked += (-change);
                     f.unstake_height =  curr_block_num;
                  });
               }
            }
         });
      }

      eosio_assert(bp.isactive() || (!bp.isactive() && change < asset{0}), "bp is not active");

      if( change > asset{} ) {
         auto fts = freeze_tbl.find(voter);
         eosio_assert( fts != freeze_tbl.end() && fts->staked >= change, "voter freeze token < vote token" );
         freeze_tbl.modify( fts, 0, [&]( freeze_info& v ) {
            v.staked -= change;
         });
      }

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.total_voteage += b.total_staked * (curr_block_num - b.voteage_update_height);
         b.voteage_update_height = curr_block_num;
         b.total_staked += (change.amount / 10000);
      });
   }

   void system_contract::unfreeze( const account_name voter ) {
      require_auth(voter);

      freeze_table freeze_tbl(_self, _self);
      const auto itr = freeze_tbl.get(voter, "voter have not freeze token yet");

      const auto curr_block_num = current_block_num();

      eosio_assert(itr.unstake_height + FROZEN_DELAY < curr_block_num, "unfreeze is not available yet");
      eosio_assert(0 < itr.unstaking.amount, "need unstaking quantity > 0");

      INLINE_ACTION_SENDER(eosio::token, transfer)(
            config::token_account_name,
            { ::config::system_account_name, N(active) },
            { ::config::system_account_name, voter, itr.unstaking, "unfreeze" });

      freeze_tbl.modify(itr, 0, [&]( freeze_info& v ) {
         v.unstaking.set_amount(0);
         v.unstake_height = curr_block_num;
      });
   }
#if 0
   void system_contract::claim( const account_name voter, const account_name bpname ) {
      require_auth(voter);

#if !IS_ACTIVE_BONUS_TO_VOTE
      eosio_assert(false, "curr chain no bonus to account who vote");
#endif

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      votes_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      const auto curr_block_num = current_block_num();

      const auto newest_voteage =
            static_cast<int128_t>(vts.voteage + vts.vote.amount / 10000 * (curr_block_num - vts.voteage_update_height));
      const auto newest_total_voteage =
            static_cast<int128_t>(bp.total_voteage + bp.total_staked * (curr_block_num - bp.voteage_update_height));
      eosio_assert(0 < newest_total_voteage, "claim is not available yet");

      const auto amount_voteage = static_cast<int128_t>(bp.rewards_pool.amount) * newest_voteage;
      asset reward = asset(static_cast<int64_t>( amount_voteage / newest_total_voteage ));
      eosio_assert(asset{} <= reward && reward <= bp.rewards_pool,
                   "need 0 <= claim reward quantity <= rewards_pool");

      auto reward_all = reward;
      // if( voter == bpname ) {
      //    reward_all += bp.rewards_block;
      // }

      eosio_assert(reward_all > asset{}, "no any reward!");
      INLINE_ACTION_SENDER(eosio::token, transfer)(
            config::token_account_name,
            { ::config::system_account_name, N(active) },
            { ::config::system_account_name, voter, reward_all, "claim" });

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.voteage = 0;
         v.voteage_update_height = curr_block_num;
      });

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.rewards_pool -= reward;
         // if( voter == bpname ) {
         //    b.rewards_block.set_amount(0);
         // }
         b.total_voteage = static_cast<int64_t>(newest_total_voteage - newest_voteage);
         b.voteage_update_height = curr_block_num;
      });
   }
   #endif
   
   /*
   charge fee by voteage
   */
   void system_contract::fee( const account_name payer, const account_name bpname, int64_t voteage ) {
      require_auth(payer);

      /*eosio::name payer_n, bpname_n;
      payer_n.value = payer; bpname_n.value = bpname;
      print("system_contract::fee: payer=", payer_n.to_string().c_str(), ", bpname=", bpname_n.to_string().c_str(), ", voteage=", voteage, "\n");*/

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");
      const auto curr_block_num = current_block_num();
      const auto newest_total_voteage =
            static_cast<int128_t>(bp.total_voteage + bp.total_staked * (curr_block_num - bp.voteage_update_height));

      //print("system_contract::fee: bp total_voteage=", bp.total_voteage, ", bp total_staked=", bp.total_staked, ", curr_block_num=", curr_block_num, ", bp voteage_update_height=", bp.voteage_update_height, ", bp newest_total_voteage=", newest_total_voteage, "\n");

      votes_table votes_tbl(_self, payer);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");
      
      int64_t newest_voteage = vts.voteage + (vts.vote.amount / 10000) * (curr_block_num - vts.voteage_update_height);
      
      //print("system_contract::fee: voter voteage=", vts.voteage, ", voter vote=", vts.vote.amount / 10000, ", voteage_update_height=", vts.voteage_update_height, ", voter newest_voteage=", newest_voteage, "\n");
      
      eosio_assert(voteage > 0 && voteage <= newest_voteage, "voteage must be greater than zero and have sufficient voteage!");
      
      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.voteage = newest_voteage - voteage;
         v.voteage_update_height = curr_block_num;
      });

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.total_voteage = static_cast<int64_t>(newest_total_voteage - voteage);
         b.voteage_update_height = curr_block_num;
      });
   }
}
