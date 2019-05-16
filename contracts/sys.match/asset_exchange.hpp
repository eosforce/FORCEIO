#ifndef EOSIO_EXCHANGE_TOKEN_H
#define EOSIO_EXCHANGE_TOKEN_H

#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include "force.system/force.system.hpp"
#include "relay.token/relay.token.hpp"

namespace exchange {
   using namespace eosio;

   // TODO to config
   const account_name relay_token_account = N(relay.token);

   // ex_asset for params 
   struct ex_asset {
      // TODO change api to ex_asset
   };

   inline symbol_type get_symbol_in_chain( const name& chain, 
                                           const account_name& contract, 
                                           const symbol_type& sym ) {
      return   chain.value == 0 
             ? eosio::token::get_symbol( contract, sym )
             : relay::token::get_symbol( contract, chain, sym );
   }

   inline symbol_type get_symbol_in_chain( const name& chain, 
                                           const symbol_type& sym ) {
      return   chain.value == 0 
             ? eosio::token::get_symbol( config::token_account_name, sym )
             : relay::token::get_symbol( relay_token_account, chain, sym );
   }

      /*
   convert a to expected_symbol, including symbol name and symbol precision
   */
   inline asset convert_asset_precision( const symbol_type& expected_symbol, const asset& a ) {
      /*
         convert asset from one precision to another
         example:
            (5,AAA) -> (6, AAA)
            500 (5, AAA) -> amount : 500 * 100000 
            500 (6, AAA) -> amount` : 500 * 1000000 
            convert need amount` = amount * (10 ^ ( 6 - 5 ))

            (5,AAA) -> (2, AAA)
            500 (5, AAA) -> amount : 500 * 100000 
            500 (2, AAA) -> amount` : 500 * 100 
            convert need amount` = amount / (10 ^ ( 5 - 2 ))
      */
      const auto pre_diff = expected_symbol.precision() - a.symbol.precision();
      const auto factor   = precision( pre_diff > 0 ? pre_diff : -pre_diff );

      const auto amount_n =   pre_diff >= 0
                            ? a.amount * factor
                            : a.amount / factor;
      return asset( amount_n, expected_symbol );
   }

   // asset_exchange a general class for relay.token and system token contract token to exchange
   class asset_exchange {
   public:
      asset_exchange() {}

      asset_exchange( const name& chain, const symbol_type& sym )
         : chain(chain)
         , contract( chain.value == 0 ? config::token_account_name : relay_token_account )
         , asset_data( 0, get_symbol_in_chain( chain, this->contract, sym ) ) {}

      asset_exchange( const name& chain, const symbol_type& sym, const asset& a )
         : chain(chain)
         , contract( chain.value == 0 ? config::token_account_name : relay_token_account ) {
         const auto expected_symbol = get_symbol_in_chain( chain, this->contract, sym );
         if( expected_symbol.value != a.symbol.value ){
            asset_data = convert_asset_precision( expected_symbol, a );
         } else {
            asset_data = a;
         }
      }

      asset_exchange( const name& chain, const asset& a )
         : asset_exchange( chain, a.symbol, a ) {}

      explicit asset_exchange( const asset& a )
         : asset_exchange( name{}, a.symbol, a ) {}

      explicit asset_exchange( const symbol_type& sym )
         : asset_exchange( name{}, sym ) {}

      asset_exchange( const asset_exchange& ) = default;
      ~asset_exchange() = default;

      const asset& data() const {
         return asset_data;
      }

   private:
      account_name contract = 0;
      name         chain;
      asset        asset_data;
   };


}; // namespace exchange

#endif //EOSIO_EXCHANGE_TOKEN_H