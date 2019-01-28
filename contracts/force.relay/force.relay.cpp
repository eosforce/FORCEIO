#include "force.relay.hpp"
namespace force {

/// @abi action
void relay::commit( const name chain, const account_name transfer, const relay::block_type& block ) {
   print("commit ", chain);

}

/// @abi action
void relay::confirm( const name chain,
                     const account_name checker,
                     const checksum256 id,
                    const checksum256 mroot ) {
   print( "confirm ", chain );
}

/// @abi action
void relay::newchannel( const name chain, const checksum256 id ) {
   print( "newchannel ", chain );
}

/// @abi action
void relay::newmap( const name chain, const name type,
                    const account_name act_account, const action_name act_name,
                    const account_name account ) {
   print( "newchannel ", chain );
}


} // namespace force

EOSIO_ABI( force::relay, (commit)(confirm)(newchannel)(newmap) )