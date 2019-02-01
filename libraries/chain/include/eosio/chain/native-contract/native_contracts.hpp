#ifndef NATIVE_CONTRACTS_NATIVE_CONTRACTS_H
#define NATIVE_CONTRACTS_NATIVE_CONTRACTS_H

#pragma once

#include <eosio/chain/apply_context.hpp>
#include <eosio/chain/abi_def.hpp>

#include <memory>

#include <iostream>

namespace eosio { namespace chain {

   abi_def native_contract_abi(const abi_def& eosio_system_abi);

   void apply_native_contract( const name& action_name, apply_context& context );


   class hello {
   public:
      static const action_name get_name() {
         return N(hello);
      }

      static const account_name get_account() {
         return config::native_account_name;
      }

   public:
      account_name actor;

      void apply( apply_context& context ) const;
   };

   class stransfer {
   public:
      static const action_name get_name() {
         return N(stransfer);
      }

      static const account_name get_account() {
         return config::native_account_name;
      }

   public:
      name         from;
      name         to;
      int64_t      token;
      string       trxid;

      void apply( apply_context& context ) const;
   };

} }

FC_REFLECT( eosio::chain::hello, (actor) )
FC_REFLECT( eosio::chain::stransfer, (from)(to)(token)(trxid) )

#endif // NATIVE_CONTRACTS_NATIVE_CONTRACTS_H
