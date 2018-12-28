#ifndef NATIVE_CONTRACTS_NATIVE_CONTRACTS_H
#define NATIVE_CONTRACTS_NATIVE_CONTRACTS_H

#pragma once

#include <eosio/chain/apply_context.hpp>
#include <eosio/chain/abi_def.hpp>

namespace eosio { namespace chain {

   abi_def native_contract_abi(const abi_def& eosio_system_abi);

   void apply_native_contract( const name& action_name, apply_context& context );

} }

#endif // NATIVE_CONTRACTS_NATIVE_CONTRACTS_H
