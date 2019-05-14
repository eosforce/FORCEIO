#include "force.relay.hpp"

#include <algorithm>

namespace force {

void relay::commit( const name chain, const account_name transfer, const relay::block_type& block, const vector<action>& actions ) {
   //print("commit ", chain, " ", name{transfer}, " in ", block.num, "\n");
   require_auth(transfer);

   transfers_table transfers(_self, chain);
   auto it = transfers.find(transfer);
   eosio_assert(it != transfers.end(), "no transfers");
   eosio_assert(it->deposit > asset{ 0 }, "no deposit");

   channels_table channels(_self, chain);
   auto ich = channels.find(chain);
   eosio_assert(ich != channels.end(), "no channel");
   eosio_assert(ich->deposit_sum > asset{ 0 }, "no deposit");

   relaystat_table relaystats(_self, chain);
   auto relaystat = relaystats.find(chain);
   eosio_assert(relaystat != relaystats.end(), "no relay stats");

   if( !relaystat->last.is_nil() && relaystat->last.num >= block.num ){
      // no need do any more
      return;
   }

   bool has_commited = false;
   auto new_confirm = it->deposit;
   for( const auto& ucblock : relaystat->unconfirms ) {
      if( ucblock.base == block) {
         has_commited = true;
         new_confirm += ucblock.confirm;
         break;
      }
   }

   if( has_commited ) {
      relaystats.modify(relaystat, transfer, [&]( auto& r ) {
         for( auto& ucblock : r.unconfirms ) {
            if( ucblock.base == block ) {
               ucblock.confirm = new_confirm;
               ucblock.confirm_by( transfer );
               break;
            }
         }
      });
   } else {
      relaystats.modify(relaystat, transfer, [&]( auto& r ) {
         unconfirm_block ub;
         ub.base = block;
         ub.actions = actions;
         ub.confirm = new_confirm;
         ub.confirm_by( transfer );
         r.unconfirms.push_back(ub);
         std::sort( r.unconfirms.begin(), r.unconfirms.end() );
      });
   }

   const auto currrelaystat = relaystats.find(chain);
   for(int i = 0; i < 8 && i < currrelaystat->unconfirms.size(); i++) {
      const auto& unconfirm = currrelaystat->unconfirms[i];
      if( unconfirm.confirm * 3 >= ich->deposit_sum * 2 ){
         //print("send inline ", unconfirm.base.num, " ", unconfirm.confirm, " ", ich->deposit_sum, "\n");
         eosio::action{
               vector<eosio::permission_level>{
                  { _self, N(active)  }
               },
               _self,
               N(onblock),
               std::make_tuple(chain, unconfirm.base, unconfirm.actions)
         }.send();
      }else{
         // just do onblock for first N confirmed block
         break;
      }
   }

}

void relay::onblock( const name chain, const block_type& block ) {
   require_auth(_self);

   relaystat_table relaystats(_self, chain);
   const auto currrelaystat = relaystats.find(chain);
   if( currrelaystat->last.num >= block.num ){
      return;
   }

   channels_table channels(_self, chain);
   auto ich = channels.find(chain);
   eosio_assert(ich != channels.end(), "no channel");
   eosio_assert(ich->deposit_sum > asset{ 0 }, "no deposit");

   for( const auto& uc : currrelaystat->unconfirms ) {
      if( uc.base == block && (uc.confirm * 3 >= ich->deposit_sum * 2) ) {
         //print("onblock ", chain, " in ", block.num, "\n");
         onblockimp( chain, block, uc.actions );
         break;
      }
   }

}

void relay::newchannel( const name chain, const checksum256 id ) {
   require_auth(chain);

   channels_table channels(_self, chain);

   eosio_assert(chain != 0 , "chain name cannot be zero");
   eosio_assert(channels.find(chain) == channels.end(), "channel has created");

   channels.emplace(chain, [&](auto& cc){
      cc.chain = chain;
      cc.id = id;
   });

   relaystat_table relaystats(_self, chain);
   relaystats.emplace(chain, [&](auto& cc){
      cc.chain = chain;
      cc.last.producer = name{0};
   });
}

void relay::newmap( const name chain, const name type,
                    const account_name act_account, const action_name act_name,
                    const account_name account, const account_name relayacc, const std::string& data ) {
   require_auth(chain); // if everyone can create map?

   channels_table channels(_self, chain);
   eosio_assert(channels.find(chain) != channels.end(), "channel not created");

   handlers_table handlers(_self, chain);
   for(const auto& h : handlers){
      // handler num will <= 10, so just search by for
      eosio_assert( ((h.actaccount != act_account) || (h.actname != act_name)), "one action can only one handler" );
   }

   auto hh = handlers.find(type);
   if( hh == handlers.end() ) {
      handlers.emplace(chain, [&]( auto& h ) {
         h.chain = chain;
         h.name = type;
         h.actaccount = act_account;
         h.actname = act_name;
         h.account = account;
         h.relayacc = relayacc;
         h.data = data;
      });
   } else {
      handlers.modify(hh, chain, [&]( auto& h ) {
         h.actaccount = act_account;
         h.actname = act_name;
         h.account = account;
         h.relayacc = relayacc;
         h.data = data;
      });
   }
}

void relay::new_transfer( name chain, account_name transfer, const asset& deposit ) {
   eosio_assert(deposit.symbol == CORE_SYMBOL, "deposit should core symbol");
   eosio_assert(deposit >= asset{0} , "deposit should > 0");

   channels_table channels(_self, chain);
   auto channel = channels.find(chain);
   eosio_assert(channel != channels.end(), "channel has created");

   transfers_table transfers(_self, chain);
   auto it = transfers.find(transfer);
   if( it == transfers.end() ) {
      channels.modify(channel, transfer, [&](auto& cc){
         cc.deposit_sum += deposit;
      });
      transfers.emplace(transfer, [&]( auto& h ) {
         h.chain = chain;
         h.transfer = transfer;
         h.deposit = deposit;
      });
   } else {
      const auto old = it->deposit;
      eosio_assert(old <= channel->deposit_sum, "old deposit should <= sum");
      channels.modify(channel, transfer, [&](auto& cc){
         cc.deposit_sum -= old;
         cc.deposit_sum += deposit;
      });
      transfers.modify(it, transfer, [&]( auto& h ) {
         h.deposit = deposit;
      });
   }
}

void relay::onblockimp( const name chain, const block_type& block, const vector<action>& actions ) {
   channels_table channels(_self, chain);
   eosio_assert(channels.find(chain) != channels.end(), "channel not created");

   handlers_table handlers(_self, chain);

   for( const auto& act : actions ) {
      //print("check act ", act.account, " ", act.name, "\n");
      for( const auto& h : handlers ) {
         // handler num will <= 10, so just search by for
         if( (h.actaccount == act.account) && (h.actname == act.name) ) {
            onaction(block, act, h);
         }
      }
   }

   relaystat_table relaystats(_self, chain);
   auto relaystat = relaystats.find(chain);

   eosio_assert(relaystat != relaystats.end(), "no relay stats");

   relaystats.modify( relaystat, _self, [&]( auto& r ) {
      r.last = block;
      r.unconfirms.erase(
         std::remove_if( r.unconfirms.begin(), 
                         r.unconfirms.end(), 
                         [&](const unconfirm_block& ub){return ub.base.num == r.last.num;}), 
         r.unconfirms.end());
   });
}

void relay::onaction( const block_type& block, const action& act, const map_handler& handler ){
   //print("onaction ", act.account, " ", act.name, "\n");
   eosio::action{
         {{ _self, N(active) }},
         handler.account,
         N(on),
         handler_action{
               handler.chain,
               block.id,
               act
         }
   }.send();
}

void relay::ontransfer( const account_name from,
                        const account_name to,
                        const asset& quantity,
                        const std::string& memo) {
   if (from == _self || to != _self) {
      return;
   }
   if ("NoProcessMemo" == memo) {
      return;
   }

   new_transfer( name{string_to_name(memo.c_str())}, from, quantity );
}

} // namespace force

extern "C" {
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
      auto self = receiver;
      if( action == N(onerror)) {
         eosio_assert(code == ::config::system_account_name, "onerror action's are only valid from the \"eosio\" system account");
      }

      if ((code == config::token_account_name) && (action == N(transfer))) {
         force::relay thiscontract( self );
         execute_action(&thiscontract, &force::relay::ontransfer);
         return;
      }

      if( code == self || action == N(onerror) ) {
         force::relay thiscontract( self );
         switch( action ) {
            EOSIO_API(  force::relay, (commit)(newchannel)(newmap)(onblock) )
         }
      }
   }
}