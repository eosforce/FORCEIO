#ifndef EOSIO_EXCHANGE_PAIR_H
#define EOSIO_EXCHANGE_PAIR_H

#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/types.h>

#include "exchange_asset.hpp"

#include <string>


namespace exchange {
   // ex_symbol_type exchange symbol_type
   struct ex_symbol_type {
      symbol_type  typ; // TODO if useless
      name         chain;
      symbol_type  sym;

      ex_symbol_type() = default;
      ex_symbol_type( const name& chain, const symbol_type& typ, const symbol_type& sym )
         : chain( chain )
         , sym( sym )
         , typ( ( typ.name() << 8 ) | sym.precision() ) {}

      ex_symbol_type( const name& chain, const symbol_type& sym )
         : chain( chain ), sym( sym ), typ( sym ) {}

      ex_symbol_type( const ex_symbol_type& ) = default;
      ~ex_symbol_type() = default;

      uint64_t name()const { typ.name(); }
   };

   template< typename T >
   class trading_pairs_t {
   public:
      trading_pairs_t( const account_name& contract )
         : _table(contract, contract) {}

      using table_t        = T;
      using const_iterator = typename table_t::const_iterator;
      using item_type      = typename const_iterator::value_type;
      
      inline const_iterator end() const { return _table.end(); }

      // TODO use idx to find item
      inline const_iterator find( const uint32_t id ) const {
         return _table.find( static_cast<uint64_t>(id) );
      }

      inline const_iterator find( const ex_symbol_type& base, const ex_symbol_type& quote ) const {
         const auto lower_key = std::numeric_limits<uint64_t>::lowest();
         const auto lower     = _table.lower_bound(lower_key);
         const auto upper_key = std::numeric_limits<uint64_t>::max();
         const auto upper     = _table.upper_bound(upper_key);

         for( auto itr = lower; itr != upper; ++itr ) {
            if(   itr->base_chain      == base.chain 
               && itr->base_sym.name()  == base.sym.name() 
               && itr->quote_chain      == quote.chain 
               && itr->quote_sym.name() == quote.sym.name() ) {
               return itr;
            }
         }

         return _table.end();
      }

      inline const_iterator find( const name& base_chain, const symbol_type& base_sym, 
                                  const name& quote_chain, const symbol_type& quote_sym ) const {
         return find( ex_symbol_type{ base_chain, base_sym }, ex_symbol_type{ quote_chain, quote_sym } );
      }

      inline const item_type& get( const uint32_t id, const char *msg = "unable to find pair by key"  ) const {
         return _table.get(id, msg);
      }

      inline const item_type& get( const ex_symbol_type& base, const ex_symbol_type& quote, 
                            const char *msg = "unable to find pair by key"  ) const {
         auto result = find( base, quote );
         eosio_assert( result != end(), msg );
         return *result;
      }

      inline uint32_t get_pair_id( const ex_symbol_type& base, const ex_symbol_type& quote ) const {
         return get( base, quote ).id;
      }

      inline uint32_t get_pair_id( const name& base_chain, const symbol_type& base_sym, 
                                   const name& quote_chain, const symbol_type& quote_sym ) const {
         return get( ex_symbol_type{ base_chain, base_sym }, 
                     ex_symbol_type{ quote_chain, quote_sym } ).id;
      }

      inline symbol_type get_pair_base( const uint32_t pair_id ) const {
         return get( pair_id ).base;
      }

      inline symbol_type get_pair_quote( const uint32_t pair_id ) const {
         return get( pair_id ).quote;
      }

      inline bool empty() const {
         return _table.lower_bound(std::numeric_limits<uint64_t>::lowest()) 
             == _table.upper_bound(std::numeric_limits<uint64_t>::max());
      }

      table_t _table;
   };
   
}

#endif // EOSIO_EXCHANGE_PAIR_H