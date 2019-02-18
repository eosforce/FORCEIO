//
// Created by fy on 2019-01-29.
//

#include "relay.token.hpp"
#include "force.relay/force.relay.hpp"
#include <eosiolib/action.hpp>
#include "force.token/force.token.hpp"

namespace relay{

// just a test version by contract
void token::on( const name chain, const checksum256 block_id, const force::relay::action& act ) {
   // TODO check account

   // TODO create accounts from diff chain

   // Just send account
   print("on ", name{act.account}, " ", name{act.name}, "\n");
   const auto data = unpack<token::action>(act.data);
   print("map ", name{data.from}, " ", data.quantity, " ", data.memo, "\n");


}

};

EOSIO_ABI( relay::token, (on) )