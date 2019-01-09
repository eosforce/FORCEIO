/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include "foce.system.hpp"

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.h>
#include <eosiolib/transaction.hpp>

#include <eosio.token/eosio.token.hpp>


#include <cmath>
#include <map>

namespace eosiosystem {
   using eosio::asset;
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::bytes;
   using eosio::print;
   using eosio::permission_level;
   using std::map;
   using std::pair;

   static constexpr time refund_delay = 3*24*3600;
   static constexpr time refund_expiration_time = 3600;

   struct user_resources {
      account_name  owner;
      asset         net_weight;
      asset         cpu_weight;
      int64_t       ram_bytes = 0;

      uint64_t primary_key()const { return owner; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes) )
   };


   /**
    *  Every user 'from' has a scope/table that uses every receipient 'to' as the primary key.
    */
   struct delegated_bandwidth {
      account_name  from;
      account_name  to;
      asset         net_weight;
      asset         cpu_weight;

      uint64_t  primary_key()const { return to; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight) )

   };

   struct refund_request {
      account_name  owner;
      time          request_time;
      eosio::asset  net_amount;
      eosio::asset  cpu_amount;

      uint64_t  primary_key()const { return owner; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( refund_request, (owner)(request_time)(net_amount)(cpu_amount) )
   };

   /**
    *  These tables are designed to be constructed in the scope of the relevant user, this
    *  facilitates simpler API for per-user queries
    */
   typedef eosio::multi_index< N(userres), user_resources>      user_resources_table;
   typedef eosio::multi_index< N(delband), delegated_bandwidth> del_bandwidth_table;
   typedef eosio::multi_index< N(refunds), refund_request>      refunds_table;


   void system_contract::changebw( account_name from, account_name receiver,
                                   const asset stake_net_delta, const asset stake_cpu_delta, bool transfer )
   {
      require_auth( from );
      eosio_assert( stake_net_delta != asset(0) || stake_cpu_delta != asset(0), "should stake non-zero amount" );
      eosio_assert( std::abs( (stake_net_delta + stake_cpu_delta).amount )
                     >= std::max( std::abs( stake_net_delta.amount ), std::abs( stake_cpu_delta.amount ) ),
                    "net and cpu deltas cannot be opposite signs" );

      account_name source_stake_from = from;
      if ( transfer ) {
         from = receiver;
      }

      // update stake delegated from "from" to "receiver"
      {
         del_bandwidth_table     del_tbl( _self, from);
         auto itr = del_tbl.find( receiver );
         if( itr == del_tbl.end() ) {
            itr = del_tbl.emplace( from, [&]( auto& dbo ){
                  dbo.from          = from;
                  dbo.to            = receiver;
                  dbo.net_weight    = stake_net_delta;
                  dbo.cpu_weight    = stake_cpu_delta;
               });
         }
         else {
            del_tbl.modify( itr, 0, [&]( auto& dbo ){
                  dbo.net_weight    += stake_net_delta;
                  dbo.cpu_weight    += stake_cpu_delta;
               });
         }
         eosio_assert( asset(0) <= itr->net_weight, "insufficient staked net bandwidth" );
         eosio_assert( asset(0) <= itr->cpu_weight, "insufficient staked cpu bandwidth" );
         if ( itr->net_weight == asset(0) && itr->cpu_weight == asset(0) ) {
            del_tbl.erase( itr );
         }
      } // itr can be invalid, should go out of scope

      // update totals of "receiver"
      {
         user_resources_table   totals_tbl( _self, receiver );
         auto tot_itr = totals_tbl.find( receiver );
         if( tot_itr ==  totals_tbl.end() ) {
            tot_itr = totals_tbl.emplace( from, [&]( auto& tot ) {
                  tot.owner = receiver;
                  tot.net_weight    = stake_net_delta;
                  tot.cpu_weight    = stake_cpu_delta;
               });
         } else {
            totals_tbl.modify( tot_itr, from == receiver ? from : 0, [&]( auto& tot ) {
                  tot.net_weight    += stake_net_delta;
                  tot.cpu_weight    += stake_cpu_delta;
               });
         }
         eosio_assert( asset(0) <= tot_itr->net_weight, "insufficient staked total net bandwidth" );
         eosio_assert( asset(0) <= tot_itr->cpu_weight, "insufficient staked total cpu bandwidth" );

         set_resource_limits( receiver, tot_itr->ram_bytes, tot_itr->net_weight.amount, tot_itr->cpu_weight.amount );

         if ( tot_itr->net_weight == asset(0) && tot_itr->cpu_weight == asset(0)  && tot_itr->ram_bytes == 0 ) {
            totals_tbl.erase( tot_itr );
         }
      } // tot_itr can be invalid, should go out of scope

      // create refund or update from existing refund
      if ( N(eosio) != source_stake_from ) { //for eosio both transfer and refund make no sense
         refunds_table refunds_tbl( _self, from );
         auto req = refunds_tbl.find( from );

         //create/update/delete refund
         auto net_balance = stake_net_delta;
         auto cpu_balance = stake_cpu_delta;

         // net and cpu are same sign by assertions in delegatebw and undelegatebw
         // redundant assertion also at start of changebw to protect against misuse of changebw
         bool is_undelegating = (net_balance.amount + cpu_balance.amount ) < 0;
         bool is_delegating_to_self = (!transfer && from == receiver);

         if( is_delegating_to_self || is_undelegating ) {
            if ( req != refunds_tbl.end() ) { //need to update refund
               refunds_tbl.modify( req, 0, [&]( refund_request& r ) {
                  if ( net_balance < asset(0) || cpu_balance < asset(0) ) {
                     r.request_time = now();
                  }
                  r.net_amount -= net_balance;
                  if ( r.net_amount < asset(0) ) {
                     net_balance = -r.net_amount;
                     r.net_amount = asset(0);
                  } else {
                     net_balance = asset(0);
                  }
                  r.cpu_amount -= cpu_balance;
                  if ( r.cpu_amount < asset(0) ){
                     cpu_balance = -r.cpu_amount;
                     r.cpu_amount = asset(0);
                  } else {
                     cpu_balance = asset(0);
                  }
               });

               eosio_assert( asset(0) <= req->net_amount, "negative net refund amount" ); //should never happen
               eosio_assert( asset(0) <= req->cpu_amount, "negative cpu refund amount" ); //should never happen

               if ( req->net_amount == asset(0) && req->cpu_amount == asset(0) ) {
                  refunds_tbl.erase( req );
               }

            } else if ( net_balance < asset(0) || cpu_balance < asset(0) ) { //need to create refund
               refunds_tbl.emplace( from, [&]( refund_request& r ) {
                  r.owner = from;
                  if ( net_balance < asset(0) ) {
                     r.net_amount = -net_balance;
                     net_balance = asset(0);
                  } // else r.net_amount = 0 by default constructor
                  if ( cpu_balance < asset(0) ) {
                     r.cpu_amount = -cpu_balance;
                     cpu_balance = asset(0);
                  } // else r.cpu_amount = 0 by default constructor
                  r.request_time = now();
               });
            } // else stake increase requested with no existing row in refunds_tbl -> nothing to do with refunds_tbl
         } /// end if is_delegating_to_self || is_undelegating


         auto transfer_amount = net_balance + cpu_balance;
         if ( asset(0) < transfer_amount ) {
            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {source_stake_from, N(active)},
               { source_stake_from, N(eosio), asset(transfer_amount), std::string("stake bandwidth") } );
         }
      }
   }

   void system_contract::delegatebw( account_name from, account_name receiver,
                                     asset stake_net_quantity,
                                     asset stake_cpu_quantity, bool transfer )
   {
      eosio_assert( stake_cpu_quantity >= asset(0), "must stake a positive amount" );
      eosio_assert( stake_net_quantity >= asset(0), "must stake a positive amount" );
      eosio_assert( stake_net_quantity + stake_cpu_quantity > asset(0), "must stake a positive amount" );
      eosio_assert( !transfer || from != receiver, "cannot use transfer flag if delegating to self" );

      changebw( from, receiver, stake_net_quantity, stake_cpu_quantity, transfer);
   } // delegatebw

   void system_contract::undelegatebw( account_name from, account_name receiver,
                                       asset unstake_net_quantity, asset unstake_cpu_quantity )
   {
      eosio_assert( asset() <= unstake_cpu_quantity, "must unstake a positive amount" );
      eosio_assert( asset() <= unstake_net_quantity, "must unstake a positive amount" );
      eosio_assert( asset() < unstake_cpu_quantity + unstake_net_quantity, "must unstake a positive amount" );

      changebw( from, receiver, -unstake_net_quantity, -unstake_cpu_quantity, false);
   } // undelegatebw


   void system_contract::refund( const account_name owner ) {
      require_auth( owner );

      refunds_table refunds_tbl( _self, owner );
      auto req = refunds_tbl.find( owner );
      eosio_assert( req != refunds_tbl.end(), "refund request not found" );
      eosio_assert( req->request_time + refund_delay <= now(), "refund is not available yet" );
      // Until now() becomes NOW, the fact that now() is the timestamp of the previous block could in theory
      // allow people to get their tokens earlier than the 3 day delay if the unstake happened immediately after many
      // consecutive missed blocks.

      INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                    { N(eosio), req->owner, req->net_amount + req->cpu_amount, std::string("unstake") } );

      refunds_tbl.erase( req );
   }


} //namespace eosiosystem
