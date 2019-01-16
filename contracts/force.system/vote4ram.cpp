#include "force.system.hpp"

namespace eosiosystem {
   // FIXME BY FanYang vote4ram in multiple vote
   // FIXME BY FanYang if chain no bonus to vote, it need another way to get ram
   void system_contract::vote4ram( const account_name voter, const account_name bpname, const asset stake ) {
      require_auth(voter);

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      eosio_assert(stake.symbol == CORE_SYMBOL, "only support CORE SYMBOL token");
      eosio_assert(0 <= stake.amount && stake.amount % 10000 == 0,
                   "need stake quantity >= 0 and quantity is integer");

      freeze_table freeze_tbl(_self, _self);

      auto change = stake;
      votes4ram_table votes_tbl(_self, voter);
      auto vts = votes_tbl.find(bpname);
      const auto curr_block_num = current_block_num();
      if( vts == votes_tbl.end() ) {
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
      eosio_assert(bp.isactive || (!bp.isactive && change < asset{}), "bp is not active");

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

      vote4ramsum_table vote4ramsum_tbl(_self, _self);
      auto vtss = vote4ramsum_tbl.find(voter);
      if( vtss == vote4ramsum_tbl.end() ) {
         vote4ramsum_tbl.emplace(voter, [&]( vote4ram_info& v ) {
            v.voter = voter;
            v.staked = stake; // for first vote all staked is stake
         });
      } else {
         vote4ramsum_tbl.modify(vtss, 0, [&]( vote4ram_info& v ) {
            v.staked += change;
         });
      }

      set_need_check_ram_limit(voter);
   }
}
