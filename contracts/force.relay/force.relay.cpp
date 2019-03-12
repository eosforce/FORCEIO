#include "force.relay.hpp"

namespace force {

void relay::commit( const name chain, const account_name transfer, const relay::block_type& block, const vector<action>& actions ) {
   print("commit ", chain, "\n");

   require_auth(transfer);

   relaystat_table relaystats(_self, chain);
   auto relaystat = relaystats.find(chain);

   eosio_assert(relaystat != relaystats.end(), "no relay stats");

   if(!relaystat->last.is_nil()){
      eosio_assert(block.previous == relaystat->last.id, "previous id no last id");
   }

   bool has_commited = false;
   for( const auto& ucblock : relaystat->unconfirms ){
      if(    ucblock.base.id == block.id
          && ucblock.base.mroot == block.mroot
          && ucblock.base.action_mroot == block.action_mroot ){
         has_commited = true;
         break;
      }
   }
   if(has_commited){
      print("block has commited");
      return;
   }

   relaystats.modify( relaystat, chain, [&]( auto& r ) {
      r.unconfirms.push_back(unconfirm_block{
         block, actions
      });
   });

   // TODO confirm
   onblock(chain, transfer, block, actions);

}

void relay::confirm( const name chain,
                     const account_name checker,
                     const checksum256 id,
                     const checksum256 mroot ) {
   print( "confirm ", chain );
}

void relay::newchannel( const name chain, const checksum256 id ) {
   print( "newchannel ", chain );

   // TODO account
   account_name acc{chain};
   require_auth(acc);

   channels_table channels(_self, acc);

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
                    const account_name account, const bytes data ) {
   print("newmap ", chain, " ", type);

   // TODO account
   account_name acc{ chain };
   require_auth(acc);

   channels_table channels(_self, acc);
   handlers_table handlers(_self, acc);

   eosio_assert(channels.find(chain) != channels.end(), "channel not created");

   auto hh = handlers.find(type);
   if( hh == handlers.end() ) {
      handlers.emplace(chain, [&]( auto& h ) {
         h.chain = chain;
         h.name = type;
         h.actaccount = act_account;
         h.actname = act_name;
         h.account = account;
         h.data = data;
      });
   } else {
      handlers.modify(hh, acc, [&]( auto& h ) {
         h.actaccount = act_account;
         h.actname = act_name;
         h.account = account;
         h.data = data;
      });
   }
}

void relay::new_transfer( name chain, account_name transfer, const asset& deposit ) {
   eosio_assert(deposit.amount > 0 , "deposit should >= 0");
   eosio_assert(deposit.symbol == CORE_SYMBOL, "deposit should core symbol");

   channels_table channels(_self, chain);

   auto channel = channels.find(chain);
   eosio_assert(channel != channels.end(), "channel has created");

   channels.modify(channel, chain, [&](auto& cc){
      cc.power_sum += deposit.amount;
   });

   transfers_table transfers(_self, chain);

   auto it = transfers.find(transfer);
   if( it == transfers.end() ) {
      transfers.emplace(chain, [&]( auto& h ) {
         h.chain = chain;
         h.transfer = transfer;
         h.power = deposit.amount;
      });
   } else {
      transfers.modify(it, transfer, [&]( auto& h ) {
         h.power += deposit.amount;
      });
   }
}

void relay::onblock( const name chain, const account_name transfer, const block_type& block, const vector<action>& actions ){
   print("onblock ", chain, "\n");

   account_name acc{ chain };
   channels_table channels(_self, acc);
   eosio_assert(channels.find(chain) != channels.end(), "channel not created");

   handlers_table handlers(_self, acc);

   std::map<std::pair<account_name, action_name>, map_handler> handler_map;
   for(const auto& h : handlers){
      handler_map[std::make_pair(h.actaccount, h.actname)] = h;
   }

   for(const auto& act : actions){
      print("check act ", act.account, " ", act.name, "\n");
      const auto& h = handler_map.find(std::make_pair(act.account, act.name));
      if(h != handler_map.end()){
         onaction(transfer, block, act, h->second);
      }
   }

   relaystat_table relaystats(_self, chain);
   auto relaystat = relaystats.find(chain);

   eosio_assert(relaystat != relaystats.end(), "no relay stats");

   relaystats.modify( relaystat, chain, [&]( auto& r ) {
      r.last = block;
      r.unconfirms.clear();
   });
}

void relay::onaction( const account_name transfer, const block_type& block, const action& act, const map_handler& handler ){
   //print("onaction ", act.account, " ", act.name, "\n");
   eosio::action{
         vector<eosio::permission_level>{},
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

   new_transfer(name{string_to_name(memo.c_str())}, from, quantity);
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
            EOSIO_API(  force::relay, (commit)(confirm)(newchannel)(newmap) )
         }
      }
   }
}