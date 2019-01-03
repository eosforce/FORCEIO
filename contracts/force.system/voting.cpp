#include "force.system.hpp"

namespace eosiosystem {
    void system_contract::vote( const account_name voter, const account_name bpname, const asset stake ) {
      require_auth(voter);

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      eosio_assert(stake.symbol == CORE_SYMBOL, "only support CORE SYMBOL token");
      eosio_assert(0 <= stake.amount && stake.amount % 10000 == 0,
                   "need stake quantity >= 0 and quantity is integer");

      int64_t change = 0;
      votes_table votes_tbl(_self, voter);
      auto vts = votes_tbl.find(bpname);
      if( vts == votes_tbl.end()) {
         change = stake.amount;
         votes_tbl.emplace(voter, [&]( vote_info& v ) {
            v.bpname = bpname;
            v.staked = stake;
         });
      } else {
         change = stake.amount - vts->staked.amount;
         votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
            v.voteage += v.staked.amount / 10000 * ( current_block_num() - v.voteage_update_height );
            v.voteage_update_height = current_block_num();
            v.staked = stake;
            if( change < 0 ) {
               v.unstaking.amount += -change;
               v.unstake_height = current_block_num();
            }
         });
      }
      eosio_assert(bp.isactive || (!bp.isactive && change < 0), "bp is not active");
      if( change > 0 ) {
         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {voter, N(active)},
                                                       { voter, N(eosio), asset(change), std::string("vote") } );
      }

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.total_voteage += b.total_staked * ( current_block_num() - b.voteage_update_height );
         b.voteage_update_height = current_block_num();
         b.total_staked += change / 10000;
      });
   }
}