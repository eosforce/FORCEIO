#include "force.relay.hpp"

namespace force {

void relay::commit( const name chain, const account_name transfer, const relay::block_type& block, const vector<action>& actions ) {
   print("commit ", chain, "\n");

   require_auth(transfer);

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

} // namespace force

EOSIO_ABI( force::relay, (commit)(confirm)(newchannel)(newmap) )
