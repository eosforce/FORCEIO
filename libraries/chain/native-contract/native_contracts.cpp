#include <fc/log/logger.hpp>
#include <fc/utility.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/apply_context.hpp>
#include <eosio/chain/native-contract/native_contracts.hpp>

namespace eosio { namespace chain {

void hello::apply( apply_context& context ) const {
   ilog("apply_native_hello ${h}", ("h", actor));
}

} }