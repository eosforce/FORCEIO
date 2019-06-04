#include <force.token/force.token.hpp>
#include "force.system.hpp"

#include "native.cpp"
#include "producer.cpp"
#include "voting.cpp"
#include "vote4ram.cpp"

#if CONTRACT_RESOURCE_MODEL == RESOURCE_MODEL_DELEGATE
#include "delegate_bandwidth.cpp"
#endif
#include "multiple_vote.cpp"

namespace eosiosystem {
   void system_contract::setparams( const eosio::blockchain_parameters& params ) {
      require_auth( _self );
      eosio_assert( 3 <= params.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

    void system_contract::newaccount(account_name creator,account_name name,authority owner,authority active) {
#if CONTRACT_RESOURCE_MODEL == RESOURCE_MODEL_DELEGATE

       user_resources_table  userres( _self, name);

       userres.emplace( name, [&]( auto& res ) {
           res.owner = name;
       });

       set_resource_limits( name, 0, 0, 0 );
#endif
    }
}

EOSIO_ABI( eosiosystem::system_contract,
      (updatebp)
      (freeze)(unfreeze)
      (vote)(vote4ram)(voteproducer)(fee)
      //(claim)(claimdevelop)
      (onblock)
      (setparams)(removebp)
      (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)
      (onerror)(addmortgage)(claimmortgage)(claimbp)(claimvote)
      (setconfig)(setcode)(setfee)(setabi)
#if CONTRACT_RESOURCE_MODEL == RESOURCE_MODEL_DELEGATE
      (delegatebw)(undelegatebw)(refund)
#endif
      (punishbp)(canclepunish)(apppunish)(unapppunish)(bailpunish)(rewardmine)
)
