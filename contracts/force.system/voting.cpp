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
      }

      if( change > asset{0} ) {
         INLINE_ACTION_SENDER(eosio::token, transfer)(
               N(eosio.token),
               { voter, N(active) },
               { voter, N(eosio), asset(change), "freeze" });
      }
   }

   // vote vote to a bp from voter to bpname with stake EOSC
   void system_contract::vote( const account_name voter, const account_name bpname, const asset stake ) {
      require_auth(voter);

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      eosio_assert(stake.symbol == CORE_SYMBOL, "only support CORE SYMBOL token");
      eosio_assert(0 <= stake.amount && stake.amount % 10000 == 0,
                   "need stake quantity >= 0 and quantity is integer");

      const auto curr_block_num = current_block_num();

      auto change = stake;
      votes_table votes_tbl(_self, voter);
      auto vts = votes_tbl.find(bpname);
      if( vts == votes_tbl.end() ) {
         // First vote the bp, it will cause ram add
         votes_tbl.emplace(voter, [&]( vote_info& v ) {
            v.bpname = bpname;
            v.staked = stake;
            v.unstake_height        = curr_block_num;
            v.voteage_update_height = curr_block_num;
         });
      } else {
         change -= vts->staked;
         votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
            v.voteage += (v.staked.amount / 10000) * (curr_block_num - v.voteage_update_height);
            v.voteage_update_height = curr_block_num;
            v.staked = stake;
            if( change < asset{0} ) {
               v.unstaking += (-change);
               v.unstake_height = curr_block_num;
            }
         });
      }
      eosio_assert(bp.isactive || (!bp.isactive && change < asset{0}), "bp is not active");
      if( change > asset{0} ) {
         INLINE_ACTION_SENDER(eosio::token, transfer)(
               N(eosio.token),
               { voter, N(active) },
               { voter, N(eosio), asset(change), "vote" });
      }

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.total_voteage += b.total_staked * (curr_block_num - b.voteage_update_height);
         b.voteage_update_height = curr_block_num;
         b.total_staked += (change.amount / 10000);
      });
   }

   void system_contract::unfreeze( const account_name voter, const account_name bpname ) {
      require_auth(voter);

      votes_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      const auto curr_block_num = current_block_num();

      eosio_assert(vts.unstake_height + FROZEN_DELAY < curr_block_num, "unfreeze is not available yet");
      eosio_assert(0 < vts.unstaking.amount, "need unstaking quantity > 0");

      INLINE_ACTION_SENDER(eosio::token, transfer)(N(eosio.token), { N(eosio), N(active) },
                                                   { N(eosio), voter, vts.unstaking, std::string("unfreeze") });

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.unstaking.set_amount(0);
         v.unstake_height = curr_block_num;
      });
   }

   void system_contract::claim( const account_name voter, const account_name bpname ) {
      require_auth(voter);

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      votes_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      const auto curr_block_num = current_block_num();

      const auto newest_voteage =
            static_cast<int128_t>(vts.voteage + vts.staked.amount / 10000 * (curr_block_num - vts.voteage_update_height));
      const auto newest_total_voteage =
            static_cast<int128_t>(bp.total_voteage + bp.total_staked * (curr_block_num - bp.voteage_update_height));
      eosio_assert(0 < newest_total_voteage, "claim is not available yet");

      const auto amount_voteage = static_cast<int128_t>(bp.rewards_pool.amount) * newest_voteage;
      asset reward = asset(static_cast<int64_t>( amount_voteage / newest_total_voteage ));
      eosio_assert(asset{} <= reward && reward <= bp.rewards_pool,
                   "need 0 <= claim reward quantity <= rewards_pool");

      auto reward_all = reward;
      if( voter == bpname ) {
         reward_all += bp.rewards_block;
      }

      eosio_assert(reward_all > asset{}, "no any reward!");
      INLINE_ACTION_SENDER(eosio::token, transfer)(
            N(eosio.token),
            { N(eosio), N(active) },
            { N(eosio), voter, reward_all, "claim" });

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.voteage = 0;
         v.voteage_update_height = curr_block_num;
      });

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.rewards_pool -= reward;
         if( voter == bpname ) {
            b.rewards_block.set_amount(0);
         }
         b.total_voteage = static_cast<int64_t>(newest_total_voteage - newest_voteage);
         b.voteage_update_height = curr_block_num;
      });
   }
}