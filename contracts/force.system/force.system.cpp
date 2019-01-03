#include "force.system.hpp"
#include <eosio.token/eosio.token.hpp>
//#include "native.cpp"
#include "producer.cpp"
#include "voting.cpp"

namespace eosiosystem {

   

   void system_contract::unfreeze( const account_name voter, const account_name bpname ) {
      require_auth(voter);

      votes_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      eosio_assert(vts.unstake_height + FROZEN_DELAY < current_block_num(), "unfreeze is not available yet");
      eosio_assert(0 < vts.unstaking.amount, "need unstaking quantity > 0");

      INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio), N(active)},
                                                    { N(eosio), voter, vts.unstaking, std::string("unfreeze") } );

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.unstaking.set_amount(0);
      });
   }

   void system_contract::vote4ram( const account_name voter, const account_name bpname, const asset stake ) {
      require_auth(voter);

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      eosio_assert(stake.symbol == CORE_SYMBOL, "only support CORE SYMBOL token");
      eosio_assert(0 <= stake.amount && stake.amount % 10000 == 0,
                   "need stake quantity >= 0 and quantity is integer");

      int64_t change = 0;
      votes4ram_table votes_tbl(_self, voter);
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
                                                       { voter, N(eosio), asset(change), std::string("vote4ram") } );
      }

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.total_voteage += b.total_staked * ( current_block_num() - b.voteage_update_height );
         b.voteage_update_height = current_block_num();
         b.total_staked += change / 10000;
      });

      vote4ramsum_table vote4ramsum_tbl(_self, _self);
      auto vtss = vote4ramsum_tbl.find(voter);
      if(vtss == vote4ramsum_tbl.end()){
         vote4ramsum_tbl.emplace(voter, [&]( vote4ram_info& v ) {
            v.voter = voter;
            v.staked = stake; // for first vote all staked is stake
         });
      }else{
         vote4ramsum_tbl.modify(vtss, 0, [&]( vote4ram_info& v ) {
            v.staked += asset{change};
         });
      }

      set_need_check_ram_limit( voter );
   }

   void system_contract::unfreezeram( const account_name voter, const account_name bpname ) {
      require_auth(voter);

      votes4ram_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      eosio_assert(vts.unstake_height + FROZEN_DELAY < current_block_num(), "unfreeze is not available yet");
      eosio_assert(0 < vts.unstaking.amount, "need unstaking quantity > 0");

      INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio), N(active)},
                                                    { N(eosio), voter, vts.unstaking, std::string("unfreezeram") } );

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.unstaking.set_amount(0);
      });
   }

   void system_contract::claim( const account_name voter, const account_name bpname ) {
      require_auth(voter);

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      votes_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      int64_t newest_voteage =
            vts.voteage + vts.staked.amount / 10000 * ( current_block_num() - vts.voteage_update_height );
      int64_t newest_total_voteage =
            bp.total_voteage + bp.total_staked * ( current_block_num() - bp.voteage_update_height );
      eosio_assert(0 < newest_total_voteage, "claim is not available yet");

      int128_t amount_voteage = ( int128_t ) bp.rewards_pool.amount * ( int128_t ) newest_voteage;
      asset reward = asset(static_cast<int64_t>(( int128_t ) amount_voteage / ( int128_t ) newest_total_voteage ));
      eosio_assert(0 <= reward.amount && reward.amount <= bp.rewards_pool.amount,
                   "need 0 <= claim reward quantity <= rewards_pool");

      asset reward_block;
      if(voter == bpname){
         reward_block = bp.rewards_block;
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
             b.rewards_block.set_amount(0);
         });
      }

      eosio_assert(reward + reward_block > asset(0), "no any reward!");
      INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio), N(active)},
                                                    { N(eosio), voter, reward + reward_block, std::string("claim") } );

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.voteage = 0;
         v.voteage_update_height = current_block_num();
      });

      bps_tbl.modify(bp, 0, [&](bp_info &b) {
         b.rewards_pool -= reward;
         if (voter == bpname) {
            b.rewards_block.set_amount(0);
         }
         b.total_voteage = newest_total_voteage - newest_voteage;
         b.voteage_update_height = current_block_num();
      });
   }
//******** private ********//

   

   void system_contract::setparams( const eosio::blockchain_parameters& params ) {
      require_auth( _self );
      eosio_assert( 3 <= params.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

   void system_contract::rmvproducer( account_name bpname ) {
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

   
}
