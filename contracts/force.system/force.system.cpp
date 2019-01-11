#include <eosio.token/eosio.token.hpp>
#include "force.system.hpp"

#include "native.cpp"
#include "producer.cpp"
#include "voting.cpp"
#include "vote4ram.cpp"
#include "delegate_bandwidth.cpp"

namespace eosiosystem {
   void system_contract::setparams( const eosio::blockchain_parameters& params ) {
      require_auth( _self );
      eosio_assert( 3 <= params.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }
}
