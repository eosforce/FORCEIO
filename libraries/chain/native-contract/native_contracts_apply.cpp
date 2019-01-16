#include <eosio/chain/native-contract/native_contracts.hpp>

namespace eosio {
namespace chain{

void apply_native_contract( const name& action_name, apply_context& context ){
   ilog("apply_native_contract ${act}", ("act", action_name));
   if(action_name == N(hello)) {
      context.act.data_as<hello>().apply(context);
      return;
   }

   EOS_THROW(action_not_found_exception, "apply_native_contract");
}


} }