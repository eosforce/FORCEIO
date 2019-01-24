/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/txfee_manager.hpp>
#include <eosio/chain/controller.hpp>

#if RESOURCE_MODEL == RESOURCE_MODEL_FEE
namespace eosio { namespace chain {

   txfee_manager::txfee_manager(){
      init_native_fee(config::system_account_name, N(newaccount), asset(1000));
      init_native_fee(config::system_account_name, N(updateauth), asset(1000));
      init_native_fee(config::system_account_name, N(deleteauth), asset(1000));

      init_native_fee(config::system_account_name, N(transfer),     asset(100));
      init_native_fee(config::system_account_name, N(vote),         asset(1000));
      init_native_fee(config::system_account_name, N(voteproducer), asset(1000));
      init_native_fee(config::system_account_name, N(freeze),       asset(500));
      init_native_fee(config::system_account_name, N(unfreeze),     asset(100));
      init_native_fee(config::system_account_name, N(vote4ram),     asset(500));
      init_native_fee(config::system_account_name, N(claim),        asset(300));
      init_native_fee(config::system_account_name, N(updatebp),     asset(100*10000));
      init_native_fee(config::system_account_name, N(setemergency), asset(10*10000));

      init_native_fee(config::system_account_name, N(setparams), asset(100*10000));
      init_native_fee(config::system_account_name, N(removebp), asset(100*10000));

      init_native_fee(config::token_account_name, N(transfer), asset(100));
      init_native_fee(config::token_account_name, N(issue),    asset(100));
      init_native_fee(config::token_account_name, N(create),   asset(10*10000));

      init_native_fee(config::system_account_name, N(setabi),  asset(1000));
      init_native_fee(config::system_account_name, config::action::setfee_name,  asset(1000));
      init_native_fee(config::system_account_name, N(setcode), asset(1000));

      init_native_fee(config::system_account_name, config::action::setconfig_name, asset(100));

      init_native_fee(config::system_account_name, N(canceldelay), asset(5000));
      init_native_fee(config::system_account_name, N(linkauth),    asset(5000));
      init_native_fee(config::system_account_name, N(unlinkauth),  asset(5000));

      init_native_fee(config::msig_account_name, N(propose),   asset(15000));
      init_native_fee(config::msig_account_name, N(approve),   asset(10000));
      init_native_fee(config::msig_account_name, N(unapprove), asset(10000));
      init_native_fee(config::msig_account_name, N(cancel),    asset(10000));
      init_native_fee(config::msig_account_name, N(exec),      asset(10000));
   }

   asset txfee_manager::get_required_fee( const controller& ctl, const action& act)const{
      const auto &db = ctl.db();
      const auto block_num = ctl.head_block_num();

      // first check if changed fee
      try{
         const auto fee_in_db = db.find<action_fee_object, by_action_name>(
               boost::make_tuple(act.account, act.name));
         if(    ( fee_in_db != nullptr )
                && ( fee_in_db->fee != asset(0) ) ){
            return fee_in_db->fee;
         }
      } catch (fc::exception &exp){
         elog("catch exp ${e}", ("e", exp.what()));
      } catch (...){
         elog("catch unknown exp in get_required_fee");
      }

      const auto native_fee = get_native_fee(block_num, act.account, act.name);
      if (native_fee != asset(0)) {
         return native_fee;
      }

      // no fee found throw err
      EOS_ASSERT(false, action_validate_exception,
                 "action ${acc} ${act} name not include in feemap or db",
                 ("acc", act.account)("act", act.name));
   }

} } /// namespace eosio::chain
#endif
