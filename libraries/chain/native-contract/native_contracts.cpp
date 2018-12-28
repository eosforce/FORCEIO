#include <fc/log/logger.hpp>
#include <fc/utility.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/apply_context.hpp>
#include <eosio/chain/native-contract/native_contracts.hpp>

namespace eosio { namespace chain {

struct native_hello_param {
   account_name actor;

   static account_name get_account() {
      return N(force.native);
   }

   static action_name get_name() {
      return N(hello);
   }
};

void apply_native_hello( apply_context& context ) {
   const auto data = context.act.data_as<native_hello_param>();
   ilog("apply_native_hello ${h}", ("h", data.actor));
}

} }

FC_REFLECT( eosio::chain::native_hello_param, (actor) )