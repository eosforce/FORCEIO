#include <eosio/chain/abi_def.hpp>
#include <fc/utility.hpp>

namespace eosio { namespace chain {

vector<type_def> native_contracts_type_defs() {
   vector<type_def> types;
   return types;
}

abi_def native_contract_abi(const abi_def& eosio_system_abi)
{
   abi_def eos_abi(eosio_system_abi);

   if( eos_abi.version.size() == 0 ) {
      eos_abi.version = "eosio::abi/1.0";
   }

   fc::move_append(eos_abi.types, native_contracts_type_defs());

   eos_abi.structs.emplace_back( struct_def {
         "hello", "", {
               {"actor", "name"}
         }
   });

   eos_abi.actions.push_back( action_def{name("hello"), "hello",""} );

   return eos_abi;
}

} } /// eosio::chain
