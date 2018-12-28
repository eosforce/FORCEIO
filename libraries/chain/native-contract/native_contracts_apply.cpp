#include <eosio/chain/native-contract/native_contracts.hpp>

namespace eosio {
namespace chain{

void apply_native_hello( apply_context& context );

void apply_native_contract( const name& action_name, apply_context& context ){
   ilog("apply_native_contract ${act}", ("act", action_name));
   switch(action_name.value){
      case N(hello):
         apply_native_hello(context);
         break;
   }
}


} }