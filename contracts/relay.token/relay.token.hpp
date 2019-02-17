//
// Created by fy on 2019-01-29.
//

#ifndef FORCEIO_CONTRACT_RELAY_TOKEN_HPP
#define FORCEIO_CONTRACT_RELAY_TOKEN_HPP

#pragma once

#include <eosiolib/eosio.hpp>
#include "force.relay/force.relay.hpp"

using namespace eosio;

namespace relay {

class token : public eosio::contract {
public:
   using contract::contract;

   /// @abi action
   void on( const name chain, const checksum256 block_id, const force::relay::action& act ) {
      print("on, ", chain, " ", act.account, " ", act.name, "\n");
   }
};


};

#endif //FORCEIO_CONTRACT_RELAY_TOKEN_HPP
