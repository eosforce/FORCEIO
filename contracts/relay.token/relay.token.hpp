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

   struct action {
      account_name from;
      account_name to;
      asset        quantity;
      std::string  memo;

      EOSLIB_SERIALIZE( action, (from)(to)(quantity)(memo) )
   };

   /// @abi action
   void on( const name chain, const checksum256 block_id, const force::relay::action& act );
};


};

#endif //FORCEIO_CONTRACT_RELAY_TOKEN_HPP
