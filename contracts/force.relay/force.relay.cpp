#include "force.relay.hpp"
namespace force {

void relay::commit( const name chain, const account_name transfer, const relay::block_type& block ) {
   print("commit ", chain);

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
   print("newmap ", chain);

   // TODO account
   account_name acc{ chain };
   require_auth(acc);

   channels_table channels(_self, acc);
   handlers_table handlers(_self, acc);

   eosio_assert(channels.find(chain) != channels.end(), "channel has created");

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


} // namespace force

EOSIO_ABI( force::relay, (commit)(confirm)(newchannel)(newmap) )