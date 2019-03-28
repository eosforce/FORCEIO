

#include "sys.match.hpp"
#include <cmath>
#include "eosiolib/transaction.hpp"

namespace exchange {
   using namespace eosio;
   const int64_t max_fee_rate = 10000;
  
   const account_name escrow = N(sys.match);

   uint128_t compute_pair_index(symbol_type base, symbol_type quote)
   {
      uint128_t idxkey = (uint128_t(base.name()) << 64) | quote.name();
      return idxkey;
   }
   
   /*  create trade pair
    *  payer:			
    *  base:	        
    *  quote:		    
   */
   void exchange::create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym, uint32_t fee_rate, account_name exc_acc)
   {
      require_auth( exc_acc );

      eosio_assert(base.is_valid(), "invalid base symbol");
      eosio_assert(quote.is_valid(), "invalid quote symbol");
      
      symbol_type expected_symbol;
      
      if (base_chain.value == 0) {
         eosio::token t(config::token_account_name);
         expected_symbol = t.get_supply(base_sym.name()).symbol;
      } else {
         relay::token t(relay_token_acc);
         expected_symbol = t.get_supply(base_chain, base_sym.name()).symbol;
      }
      eosio_assert(base.precision() <= expected_symbol.precision(), "invalid base symbol precision");
      
      if (quote_chain.value == 0) {
         eosio::token t(config::token_account_name);
         expected_symbol = t.get_supply(quote_sym.name()).symbol;
      } else {
         relay::token t(relay_token_acc);
         expected_symbol = t.get_supply(quote_chain, quote_sym.name()).symbol;
      }
      eosio_assert(quote.precision() <= expected_symbol.precision(), "invalid quote symbol precision");

      trading_pairs trading_pairs_table(_self,_self);

      uint32_t pair_id = get_pair(base_chain, base_sym, quote_chain, quote_sym);
      uint128_t idxkey = compute_pair_index(base, quote);
      //uint128_t idxkey = (uint128_t(base.name()) << 64) | quote.name();
      //print("idxkey=",idxkey,",base_sym=",base.name(),",price.symbol=",quote.name());
      print("\n base=",base,",base_chain=",base_chain,",base_sym=",base_sym,"base=",quote,",base_chain=",quote_chain,",base_sym=",quote_sym,"\n");

      auto idx = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr = idx.find(idxkey);

      eosio_assert(itr == idx.end(), "trading pair already created");

      auto pk = trading_pairs_table.available_primary_key();
      trading_pairs_table.emplace( _self, [&]( auto& p ) {
         p.id = (uint32_t)pk;
         p.pair_id      = pair_id;
         p.base         = base;
         p.base_chain   = base_chain;
         p.base_sym     = base_sym.value | (base.value & 0xff);
         p.quote        = quote;
         p.quote_chain  = quote_chain;
         p.quote_sym    = quote_sym.value | (quote.value & 0xff);
         p.fee_rate     = fee_rate;
         p.exc_acc      = exc_acc;
         p.fees_base    = to_asset(relay_token_acc, base_chain, base_sym, asset(0, base_sym));
         p.fees_quote   = to_asset(relay_token_acc, quote_chain, quote_sym, asset(0, quote_sym));
         p.frozen       = 0;
      });
   }

   asset exchange::to_asset( account_name code, name chain, symbol_type sym, const asset& a ) {
      asset b;
      symbol_type expected_symbol;
      
      if (chain.value == 0) {
         eosio::token t(config::token_account_name);
      
         expected_symbol = t.get_supply(sym.name()).symbol ;
      } else {
         relay::token t(relay_token_acc);
      
         expected_symbol = t.get_supply(chain, sym.name()).symbol ;
      }

      b = convert(expected_symbol, a);
      return b;
   }
   
   /*
   convert a to expected_symbol, including symbol name and symbol precision
   */
   asset exchange::convert( symbol_type expected_symbol, const asset& a ) {
      int64_t factor;
      asset b;
      
      if (expected_symbol.precision() >= a.symbol.precision()) {
         factor = precision( expected_symbol.precision() ) / precision( a.symbol.precision() );
         b = asset( a.amount * factor, expected_symbol );
      }
      else {
         factor =  precision( a.symbol.precision() ) / precision( expected_symbol.precision() );
         b = asset( a.amount / factor, expected_symbol );
         
      }
      return b;
   }

   uint128_t compute_orderbook_lookupkey(uint32_t pair_id, uint32_t bid_or_ask, uint64_t value) {
      auto lookup_key = (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | value;
      return lookup_key;
   }
   
   void inline_transfer(account_name from, account_name to, name chain, asset quantity, string memo ) {
      if (chain.value == 0) {
         action(
                 permission_level{ from, N(active) },
                 config::token_account_name, N(transfer),
                 std::make_tuple(from, to, quantity, memo)
         ).send();
      } else {
         action(
                 permission_level{ from, N(active) },
                 relay_token_acc, N(transfer),
                 std::make_tuple(from, to, chain, quantity, memo)
         ).send();
      }
   }
   
   void exchange::insert_order(
                              orderbook &orders, 
                              uint32_t pair_id, 
                              uint32_t bid_or_ask, 
                              asset base, 
                              asset price, 
                              account_name payer, 
                              account_name receiver) {
      // insert the order
      auto pk = orders.available_primary_key();
      orders.emplace( _self, [&]( auto& o ) {
          o.id            = pk;
          o.pair_id       = pair_id;
          o.bid_or_ask    = bid_or_ask;
          o.base          = base;
          o.price         = price;
          o.maker         = payer;
          o.receiver      = receiver;
          o.timestamp     = time_point_sec(uint32_t(current_time() / 1000000ll));
      });
   }

   void exchange::match_for_bid( account_name payer, account_name receiver, asset base, asset price) {
      trading_pairs   trading_pairs_table(_self,_self);
      orderbook       orders(_self,_self);
      asset           quant_after_fee;
      asset           converted_price;
      asset           fee;
      uint32_t        bid_or_ask = 1;

      uint128_t idxkey = compute_pair_index(base.symbol, price.symbol);
      //uint128_t idxkey = (uint128_t(base.symbol.name()) << 64) | price.symbol.name();

      //print("idxkey=",idxkey,",base_sym=",base.symbol.name(),",price.symbol=",price.symbol.name());

      auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");
      require_auth( itr1->exc_acc );

      base    = convert(itr1->base, base);
      price   = convert(itr1->quote, price);
      
      //print("after convert: base=",base,",price=",price);
      print("\n---exchange::match_for_bid: base=", itr1->base, ", base_chain=", itr1->base_sym,", base_sym=", itr1->base_sym, ", quote=", itr1->quote,", quote_chain=", itr1->quote_chain,", quote_sym=", itr1->quote_sym,"\n");
      
      auto idx_orderbook = orders.template get_index<N(idxkey)>();
      // if bid, traverse ask orders, other traverse bid orders
      // auto lookup_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | price.amount;
      auto lookup_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, (uint32_t)price.amount);
      
      name from,to;
      
      auto walk_table_range = [&]( auto itr, auto end_itr ) {
         for( ; itr != end_itr; ) {
            print("\n bid: order: id=", itr->id, ", pair_id=", itr->pair_id, ", bid_or_ask=", itr->bid_or_ask,", base=", itr->base,", price=", itr->price,", maker=", itr->maker);
            // only traverse ask orders
            if (itr->bid_or_ask) {
                itr++;
                continue;
            }
            // no matching sell order
            if (price < itr->price) {
               // insert the order
               insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
               return;
            }
            if( base <= itr->base ) { // full match
               // eat the order
               //quant_after_fee = convert_asset(itr1->base_sym, base);
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, base);
               converted_price = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, itr->price);   
               done_helper(itr1->exc_acc, itr1->quote_chain, converted_price, itr1->base_chain, quant_after_fee, bid_or_ask);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, receiver, itr1->base_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0) {
                  //inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
                  idx_pair.modify(itr1, _self, [&]( auto& p ) {
                     p.fees_base += fee;
                  });
               }
                  
               //quant_after_fee = convert(itr1->quote_sym, itr->price) * base.amount / precision(base.symbol.precision());
               if (itr->price.symbol.precision() >= base.symbol.precision())
                  quant_after_fee = itr->price * base.amount / precision(base.symbol.precision());
               else
                  quant_after_fee = base * itr->price.amount / precision(itr->price.symbol.precision());
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0) {
                  //inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
                  idx_pair.modify(itr1, _self, [&]( auto& p ) {
                     p.fees_quote += base;
                  });
               }
               // refund the difference to payer
               if ( price > itr->price) {
                  auto diff = price - itr->price;
                  if (itr->price.symbol.precision() >= base.symbol.precision())
                     quant_after_fee = diff * base.amount / precision(base.symbol.precision());
                  else
                     quant_after_fee = base * diff.amount / precision(diff.symbol.precision());
                  //print("\n bid step1: quant_after_fee=",quant_after_fee);
                  quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
                  //print("\n bid step2: quant_after_fee=",quant_after_fee);
                  inline_transfer(escrow, payer, itr1->quote_chain, quant_after_fee, "");
               }
               if (base < itr->base) {
                  idx_orderbook.modify(itr, _self, [&]( auto& o ) {
                     o.base -= base;
                  });
               } else {
                  idx_orderbook.erase(itr);
               }
               return;
            } else { // partial match
               //quant_after_fee = convert_asset(itr1->base_sym, itr->base);
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, itr->base);
               converted_price = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, itr->price);
               done_helper(itr1->exc_acc, itr1->quote_chain, converted_price, itr1->base_chain, quant_after_fee, bid_or_ask);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, receiver, itr1->base_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0) {
                  //inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
                  idx_pair.modify(itr1, _self, [&]( auto& p ) {
                     p.fees_base += fee;
                  });
               }
                  
               if (itr->price.symbol.precision() >= itr->base.symbol.precision())
                  quant_after_fee = itr->price * itr->base.amount / precision(itr->base.symbol.precision());
               else
                  quant_after_fee = itr->base * itr->price.amount / precision(itr->price.symbol.precision());
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0) {
                  //inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
                  idx_pair.modify(itr1, _self, [&]( auto& p ) {
                     p.fees_quote += fee;
                  });
               }
                  
               base -= itr->base;
               // refund the difference to payer
               if ( price > itr->price) {
                  auto diff = price - itr->price;
                  if (itr->price.symbol.precision() >= itr->base.symbol.precision())
                     quant_after_fee = diff * itr->base.amount / precision(itr->base.symbol.precision());
                  else
                     quant_after_fee = itr->base * diff.amount / precision(diff.symbol.precision());
                  //print("bid step1: quant_after_fee=",quant_after_fee);
                  quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
                  //print("bid step2: quant_after_fee=",quant_after_fee);
                  inline_transfer(escrow, payer, itr1->quote_chain, quant_after_fee, "");
               }
               idx_orderbook.erase(itr++);
               if (itr == end_itr) {
                  insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
                  /*auto pk = orders.available_primary_key();
                  orders.emplace( _self, [&]( auto& o ) {
                      o.id            = pk;
                      o.pair_id       = itr1->id;
                      o.bid_or_ask    = bid_or_ask;
                      o.base          = convert(itr1->base, base);
                      o.price         = convert(itr1->quote, price);
                      o.maker         = payer;
                      o.receiver      = receiver;
                      o.timestamp     = time_point_sec(uint32_t(current_time() / 1000000ll));
                  });*/
               }
            }
         }
      };
      
      //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
      auto lower_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::lowest());    
      auto lower = idx_orderbook.lower_bound( lower_key );
      auto upper = idx_orderbook.upper_bound( lookup_key );
 
      if (lower == upper) {
         prints("\n buy: sell orderbook empty");
         insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
         /*auto pk = orders.available_primary_key();
         orders.emplace( _self, [&]( auto& o ) {
             o.id            = pk;
             o.pair_id       = itr1->id;
             o.bid_or_ask    = bid_or_ask;
             o.base          = convert(itr1->base, base);
             o.price         = convert(itr1->quote, price);
             o.maker         = payer;
             o.receiver      = receiver;
             o.timestamp     = time_point_sec(uint32_t(current_time() / 1000000ll));
         });*/
      } else {
         walk_table_range(lower, upper);
      }
   }

   void exchange::match_for_ask( account_name payer, account_name receiver, asset base, asset price) {
      trading_pairs  trading_pairs_table(_self,_self);
      orderbook      orders(_self,_self);
      asset          quant_after_fee;
      asset          converted_price;
      asset          converted_base;
      asset          fee;
      uint32_t       bid_or_ask = 0;

      uint128_t idxkey = compute_pair_index(base.symbol, price.symbol);
      //uint128_t idxkey = (uint128_t(base.symbol.name()) << 64) | price.symbol.name();

      //print("idxkey=",idxkey,",base_sym=",base.symbol.name(),",price.symbol=",price.symbol.name());

      auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");
      require_auth( itr1->exc_acc );

      base    = convert(itr1->base, base);
      price   = convert(itr1->quote, price);
      
      //print("after convert: base=",base,",price=",price);
      print("\n---exchange::match_for_ask: base=", itr1->base, ", base_chain=", itr1->base_sym,", base_sym=", itr1->base_sym, ", quote=", itr1->quote,", quote_chain=", itr1->quote_chain,", quote_sym=", itr1->quote_sym,"\n");
      
      auto idx_orderbook = orders.template get_index<N(idxkey)>();
      // if bid, traverse ask orders, other traverse bid orders
      // auto lookup_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | price.amount;
      auto lookup_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, (uint32_t)price.amount);
      
      name from,to;
      
      auto walk_table_range = [&]( auto itr, auto end_itr ) {
         for ( auto exit_on_done = false; !exit_on_done; ) {
            print("\n ask: order: id=", end_itr->id, ", pair_id=", end_itr->pair_id, ", bid_or_ask=", end_itr->bid_or_ask,", base=", end_itr->base,", price=", end_itr->price,", maker=", end_itr->maker);
            // only traverse bid orders
            if (end_itr == itr) exit_on_done = true;
            if (!end_itr->bid_or_ask) {
               if (!exit_on_done) end_itr--;
               continue;
            }
            // no matching sell order
            if (price > end_itr->price) {
               // insert the order
               insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
               /*auto pk = orders.available_primary_key();
               orders.emplace( _self, [&]( auto& o ) {
                   o.id            = pk;
                   o.pair_id       = itr1->id;
                   o.bid_or_ask    = bid_or_ask;
                   o.base          = convert(itr1->base, base);
                   o.price         = convert(itr1->quote, price);
                   o.maker         = payer;
                   o.receiver      = receiver;
                   o.timestamp     = time_point_sec(uint32_t(current_time() / 1000000ll));
               });*/
               return;
            }
            
            if ( base <= end_itr->base ) { // full match
               // eat the order
               if (price.symbol.precision() >= base.symbol.precision())
                  quant_after_fee = price * base.amount / precision(base.symbol.precision()) ;
               else
                  quant_after_fee = base * price.amount / precision(price.symbol.precision()) ;
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
               converted_price = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, price);
               converted_base = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, base);
               done_helper(itr1->exc_acc, itr1->quote_chain, converted_price, itr1->base_chain, converted_base, bid_or_ask);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               from.value = escrow; to.value = payer;
               print("\n 1: from=", from.to_string().c_str(), ", to=", to.to_string().c_str(),  ", amount=", quant_after_fee);
               inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0) {
                  //inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
                  idx_pair.modify(itr1, _self, [&]( auto& p ) {
                     p.fees_quote += fee;
                  });
               }
                  
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, base);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               from.value = escrow; to.value = end_itr->maker;
               print("\n 2: from=", from.to_string().c_str(), ", to=", to.to_string().c_str(),  ", amount=", quant_after_fee);
               inline_transfer(escrow, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0) {
                  //inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
                  idx_pair.modify(itr1, _self, [&]( auto& p ) {
                     p.fees_base += fee;
                  });
               }
                  
               // refund the difference
               if ( end_itr->price > price) {
                  auto diff = end_itr->price - price;
                  if (end_itr->price.symbol.precision() >= base.symbol.precision())
                     quant_after_fee = diff * base.amount / precision(base.symbol.precision());
                  else
                     quant_after_fee = base * diff.amount / precision(diff.symbol.precision());
                  //print("bid step1: quant_after_fee=",quant_after_fee);
                  quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
                  //print("bid step2: quant_after_fee=",quant_after_fee);
                  inline_transfer(escrow, end_itr->maker, itr1->quote_chain, quant_after_fee, "");
               }
               if( base < end_itr->base ) {
                  idx_orderbook.modify(end_itr, _self, [&]( auto& o ) {
                     o.base -= base;
                  });
               } else {
                  idx_orderbook.erase(end_itr);
               }
               
               return;
            } else { // partial match
               if (price.symbol.precision() >= end_itr->base.symbol.precision())
                  quant_after_fee = price * end_itr->base.amount / precision(end_itr->base.symbol.precision());
               else
                  quant_after_fee = end_itr->base * price.amount / precision(price.symbol.precision());
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
               converted_price = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, price);
               converted_base = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, end_itr->base);
               done_helper(itr1->exc_acc, itr1->quote_chain, converted_price, itr1->base_chain, converted_base, bid_or_ask);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0) {
                  //inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
                  idx_pair.modify(itr1, _self, [&]( auto& p ) {
                     p.fees_quote += fee;
                  });
               }
                  
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, end_itr->base);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0) {
                  //inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
                  idx_pair.modify(itr1, _self, [&]( auto& p ) {
                     p.fees_base += fee;
                  });
               }
                  
               base -= end_itr->base;
               // refund the difference
               if ( end_itr->price > price ) {
                  auto diff = end_itr->price - price;
                  if (price.symbol.precision() >= end_itr->base.symbol.precision())
                     quant_after_fee = diff * end_itr->base.amount / precision(end_itr->base.symbol.precision());
                  else
                     quant_after_fee = end_itr->base * diff.amount / precision(diff.symbol.precision());
                  //print("bid step1: quant_after_fee=",quant_after_fee);
                  quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
                  //print("bid step2: quant_after_fee=",quant_after_fee);
                  inline_transfer(escrow, end_itr->maker, itr1->quote_chain, quant_after_fee, "");
               }
               if (exit_on_done) {
                  idx_orderbook.erase(end_itr);
                  insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
               } else
                  idx_orderbook.erase(end_itr--);
            }
         }
      };
      
      auto lower = idx_orderbook.lower_bound( lookup_key );
      //auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
      auto upper_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::max());
      auto upper = idx_orderbook.lower_bound( upper_key );
      
      if (lower == idx_orderbook.cend()) {
         print("\n sell: buy orderbook empty, lookup_key=", lookup_key);
         insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
      } else {
         if (upper == idx_orderbook.cend()) {
            print("\n 8888888888888");
            upper--;
         }
             
         print("\n ask orderbook not empty, ask: order: id=", upper->id, ", pair_id=", upper->pair_id, ", bid_or_ask=", upper->bid_or_ask,", base=", upper->base,", price=", upper->price,", maker=", upper->maker);
         walk_table_range(lower, upper);
      }
   }

   /*  
    *  payer: 	            paying account
    *  base:		        target token symbol and amount
    *  price: 	            quoting token symbol and price
    *  bid_or_ask:         1: buy, 0: sell
    *  fee_account:		fee account,payer==fee_account means no fee
    *  fee_rate:		    fee rate:[0,10000), for example:50 means 50 of ten thousandths ; 0 means no fee
    * */
   void exchange::match( account_name payer, account_name receiver, asset base, asset price, uint32_t bid_or_ask) {
      //require_auth( payer );
      
      print("\n--------------enter exchange::match: payer=", payer, ", receiver=", receiver,", base=", base, ", price=", price,", bid_or_ask=", bid_or_ask,"\n");

      if (bid_or_ask) {
         match_for_bid(payer, receiver, base, price);
      } else {
         match_for_ask(payer, receiver, base, price);
      }

      return;
   }
   
   void exchange::done_helper(account_name exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask) {
      auto timestamp = time_point_sec(uint32_t(current_time() / 1000000ll));
      action(
            permission_level{ exc_acc, N(active) },
            N(sys.match), N(done),
            std::make_tuple(exc_acc, quote_chain, price, base_chain, quantity, bid_or_ask, timestamp)
      ).send();
      /*transaction trx;
      
      trx.actions.emplace_back(action(
            permission_level{ exc_acc, N(active) },
            N(sys.match), N(done),
            std::make_tuple(exc_acc, quote_chain, price, base_chain, quantity, bid_or_ask, timestamp)
      ));
      trx.send(current_time(), exc_acc);*/
   }
   
   // now do nothing, only for action capture
   void exchange::done(account_name exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask, time_point_sec timestamp) {
      require_auth(exc_acc);
      
      asset quant_after_fee;
      
      print("exchange::done: exc_acc=", exc_acc, ", price=", price,", quantity=", quantity, ", bid_or_ask=", bid_or_ask, "\n");
      if (price.symbol.precision() >= quantity.symbol.precision())
         quant_after_fee = price * quantity.amount / precision(quantity.symbol.precision());
      else
         quant_after_fee = quantity * price.amount / precision(price.symbol.precision());
      quant_after_fee = to_asset(relay_token_acc, quote_chain, price.symbol, quant_after_fee);
      upd_mark( base_chain, quantity.symbol, quote_chain, price.symbol, quant_after_fee, quantity );
   }

   /*
   type: 0 - cancel designated order, 1 - cancel designated pairs' order, 2 - cancel all orders
   */
   void exchange::cancel(account_name maker, uint32_t type, uint64_t order_id, uint32_t pair_id) {
      eosio_assert(type == 0 || type == 1 || type == 2, "invalid cancel type");
      trading_pairs   trading_pairs_table(_self,_self);
      orderbook       orders(_self,_self);
      asset           quant_after_fee;
      
      require_auth( maker );
      
      if (type == 0) {
         auto itr = orders.find(order_id);
         eosio_assert(itr != orders.cend(), "order record not found");
         eosio_assert(maker == itr->maker, "not the maker");
         
         uint128_t idxkey = (uint128_t(itr->base.symbol.name()) << 64) | itr->price.symbol.name();
         
         //print("idxkey=",idxkey,",contract=",token_contract,",symbol=",token_symbol.value);
         
         auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
         auto itr1 = idx_pair.find(idxkey);
         eosio_assert(itr1 != idx_pair.end(), "trading pair does not exist");
         
         // refund the escrow
         if (itr->bid_or_ask) {
            if (itr->price.symbol.precision() >= itr->base.symbol.precision())
               quant_after_fee = itr->price * itr->base.amount / precision(itr->base.symbol.precision());
            else
               quant_after_fee = itr->base * itr->price.amount / precision(itr->price.symbol.precision());
            //print("bid step1: quant_after_fee=",quant_after_fee);
            quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
            //print("bid step2: quant_after_fee=",quant_after_fee);
            inline_transfer(escrow, itr->maker, itr1->quote_chain, quant_after_fee, "");
         } else {
            //quant_after_fee = convert_asset(itr1->base_sym, itr->base);
            quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, itr->base);
            //print("bid step2: quant_after_fee=",quant_after_fee);
            inline_transfer(escrow, itr->maker, itr1->base_chain, quant_after_fee, "");
         }
         
         orders.erase(itr);
      } else {
         auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();

         auto lower_key = std::numeric_limits<uint64_t>::lowest();
         auto lower = orders.lower_bound( lower_key );
         auto upper_key = std::numeric_limits<uint64_t>::max();
         auto upper = orders.lower_bound( upper_key );
         
         if ( lower == orders.cend() ) {
            eosio_assert(false, "orderbook empty");
         }
         
         for ( ; lower != upper; ) {
            if (maker != lower->maker) {
               lower++;
               continue;
            }
            if (type == 1 && pair_id != lower->pair_id) {
               lower++;
               continue;
            }
            
            auto itr = lower++;
            uint128_t idxkey = compute_pair_index(itr->base.symbol, itr->price.symbol);
            auto itr1 = idx_pair.find(idxkey);
            // refund the escrow
            if (itr->bid_or_ask) {
               if (itr->price.symbol.precision() >= itr->base.symbol.precision())
                  quant_after_fee = itr->price * itr->base.amount / precision(itr->base.symbol.precision());
               else
                  quant_after_fee = itr->base * itr->price.amount / precision(itr->price.symbol.precision());
               //print("bid step1: quant_after_fee=",quant_after_fee);
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
               //print("bid step2: quant_after_fee=",quant_after_fee);
               inline_transfer(escrow, itr->maker, itr1->quote_chain, quant_after_fee, "");
            } else {
               //quant_after_fee = convert_asset(itr1->base_sym, itr->base);
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, itr->base);
               //print("bid step2: quant_after_fee=",quant_after_fee);
               inline_transfer(escrow, itr->maker, itr1->base_chain, quant_after_fee, "");
            }
            
            orders.erase(itr);
         }
      }

      return;
   }
   
   void exchange::mark(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym) {
      require_auth(_self);
      
      deals   deals_table(_self, _self);
     
      auto pair_id = get_pair_id(base_chain, base_sym, quote_chain, quote_sym);
      auto lower_key = ((uint64_t)pair_id << 32) | 0;
      auto idx_deals = deals_table.template get_index<N(idxkey)>();
      auto itr1 = idx_deals.lower_bound(lower_key);
      eosio_assert(!(itr1 != idx_deals.end() && itr1->pair_id == pair_id), "trading pair already marked");
      
      auto start_block = (current_block_num() - 1) / INTERVAL_BLOCKS * INTERVAL_BLOCKS + 1;
      auto pk = deals_table.available_primary_key();
      deals_table.emplace( _self, [&]( auto& d ) {
         d.id = (uint32_t)pk;
         d.pair_id      = pair_id;
         d.base_chain   = base_chain;
         d.base_sym     = base_sym;
         d.quote_chain  = quote_chain;
         d.quote_sym    = quote_sym;
         d.sum          = to_asset(relay_token_acc, quote_chain, quote_sym, asset(0, quote_sym));
         d.vol          = to_asset(relay_token_acc, base_chain, base_sym, asset(0, base_sym));
         d.reset_block_height = start_block;
         d.block_height_end = start_block + INTERVAL_BLOCKS - 1;
      });
   }
   
   void exchange::claim(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc, account_name fee_acc) {
      require_auth(exc_acc);
      
      trading_pairs   trading_pairs_table(_self, _self);
      bool claimed = false;
     
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = trading_pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = trading_pairs_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         if (itr->base_chain == base_chain && itr->base_sym == base_sym && 
               itr->quote_chain == quote_chain && itr->quote_sym == quote_sym && itr->exc_acc == exc_acc) {
            print("exchange::claim -- pair: id=", itr->id, "\n");
            if (escrow != fee_acc && itr->fees_base.amount > 0)
            {
               inline_transfer(escrow, fee_acc, itr->base_chain, itr->fees_base, "");
               trading_pairs_table.modify(itr, _self, [&]( auto& p ) {
                  p.fees_base = to_asset(relay_token_acc, itr->base_chain, itr->base_sym, asset(0, itr->base_sym));
               });
               claimed = true;
            }
            if (escrow != fee_acc && itr->fees_quote.amount > 0)
            {
               inline_transfer(escrow, fee_acc, itr->quote_chain, itr->fees_quote, "");
               trading_pairs_table.modify(itr, _self, [&]( auto& p ) {
                  p.fees_quote = to_asset(relay_token_acc, itr->quote_chain, itr->quote_sym, asset(0, itr->quote_sym));
               });
               claimed = true;
            }   
         }
         eosio_assert(claimed, "no fees or fee_acc is escrow account");

         return;
      }

      eosio_assert(false, "trading pair does not exist");
      return;
   }
   
   void exchange::freeze(uint32_t id) {
      trading_pairs   trading_pairs_table(_self, _self);
      
      auto itr = trading_pairs_table.find(id);
      eosio_assert(itr != trading_pairs_table.cend(), "trading pair not found");
      
      require_auth(itr->exc_acc);
      
      trading_pairs_table.modify(itr, _self, [&]( auto& p ) {
        p.frozen = 1;
      });
   }
   
   void exchange::unfreeze(uint32_t id) {
      trading_pairs   trading_pairs_table(_self, _self);
      
      auto itr = trading_pairs_table.find(id);
      eosio_assert(itr != trading_pairs_table.cend(), "trading pair not found");
      
      require_auth(itr->exc_acc);
      
      trading_pairs_table.modify(itr, _self, [&]( auto& p ) {
        p.frozen = 0;
      });
   }
   
   asset exchange::calcfee(asset quant, uint64_t fee_rate) {
      asset fee = quant * fee_rate / max_fee_rate;
      if(fee_rate > 0 && fee.amount < 1) {
          fee.amount = 1;
      }

      return fee;
   }   
}

EOSIO_ABI( exchange::exchange, (create)(match)(cancel)(done)(mark)(claim)(freeze)(unfreeze))
