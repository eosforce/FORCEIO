/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/chain/types.hpp>
#include <eosio/chain/contract_types.hpp>

namespace eosio { namespace chain {

   class apply_context;

   /**
    * @defgroup native_action_handlers Native Action Handlers
    */
   ///@{
   void apply_system_native_newaccount(apply_context&);
   void apply_system_native_updateauth(apply_context&);
   void apply_system_native_deleteauth(apply_context&);
   void apply_system_native_linkauth(apply_context&);
   void apply_system_native_unlinkauth(apply_context&);

   /*
   void apply_system_native_postrecovery(apply_context&);
   void apply_system_native_passrecovery(apply_context&);
   void apply_system_native_vetorecovery(apply_context&);
   */
   void apply_system_native_setconfig(apply_context&);
   
   void apply_system_native_setcode(apply_context&);
   void apply_system_native_setfee(apply_context&);
   void apply_system_native_setabi(apply_context&);

#if RESOURCE_MODEL == RESOURCE_MODEL_FEE
   void apply_system_native_setfee(apply_context&);
#endif

   void apply_system_native_canceldelay(apply_context&);
   ///@}  end action handlers

} } /// namespace eosio::chain
