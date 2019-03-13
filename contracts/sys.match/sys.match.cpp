

#include "sys.match.hpp"
#include <cmath>

namespace exchange {
   using namespace eosio;
   const int64_t max_fee_rate = 10000;
   const account_name relay_token_acc = N(relay.token);
   const account_name escrow = N(sys.match);

   uint128_t compute_pair_index(symbol_type base, symbol_type quote)
   {
      uint128_t idxkey = (uint128_t(base.name()) << 64) | quote.name();
      return idxkey;
   }
   
   /*  create trade pairt
    *  payer:			
    *  base:	        
    *  quote:		    
   */
   void exchange::create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym, uint32_t fee_rate, account_name exc_acc)
   {
      require_auth( exc_acc );

      eosio_assert(base.is_valid(), "invalid base symbol");
      eosio_assert(quote.is_valid(), "invalid quote symbol");

      trading_pairs trading_pairs_table(_self,_self);

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
         
         p.base         = base;
         p.base_chain   = base_chain;
         p.base_sym     = base_sym.value | (base.value & 0xff);
         p.quote        = quote;
         p.quote_chain  = quote_chain;
         p.quote_sym    = quote_sym.value | (quote.value & 0xff);
         p.fee_rate     = fee_rate;
         p.exc_acc      = exc_acc;
          
      });
   }

   asset exchange::to_asset( account_name code, name chain, const asset& a ) {
      asset b;
      relay::token t(code);
      
      symbol_type expected_symbol = t.get_supply(chain, a.symbol.name()).symbol ;

      b = convert(expected_symbol, a);
      return b;
   }
   
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
   
   asset exchange::convert_asset( symbol_type expected_symbol, const asset& a ) {
      auto b = asset( a.amount, expected_symbol );
      return b;
   }

   uint128_t compute_orderbook_lookupkey(uint32_t pair_id, uint32_t bid_or_ask, uint64_t value) {
      auto lookup_key = (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | value;
      return lookup_key;
   }
   
   void inline_transfer(account_name from, account_name to, name chain, asset quantity, string memo ) {
      action(
              permission_level{ from, N(active) },
              relay_token_acc, N(transfer),
              std::make_tuple(from, to, chain, quantity, memo)
      ).send();
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
      asset           fee;
      uint32_t        bid_or_ask = 1;

      uint128_t idxkey = compute_pair_index(base.symbol, price.symbol);
      //uint128_t idxkey = (uint128_t(base.symbol.name()) << 64) | price.symbol.name();

      //print("idxkey=",idxkey,",base_sym=",base.symbol.name(),",price.symbol=",price.symbol.name());

      auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end(), "trading pair does not exist");
      require_auth( itr1->exc_acc );

      base    = convert(itr1->base, base);
      price   = convert(itr1->quote, price);
      
      //print("after convert: base=",base,",price=",price);
      print("\n---exchange::match: base=", itr1->base, ", base_chain=", itr1->base_sym,", base_sym=", itr1->base_sym, ", quote=", itr1->quote,", quote_chain=", itr1->quote_chain,", quote_sym", itr1->quote_sym,"\n");
      
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
            if( base <= itr->base ) { // full match
               // eat the order
               quant_after_fee = convert_asset(itr1->base_sym, base);
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
               done_helper(itr1->exc_acc, itr->price, base, bid_or_ask);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, receiver, itr1->base_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0)
                  inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
               quant_after_fee = convert(itr1->quote_sym, itr->price) * base.amount / precision(base.symbol.precision());
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
                 // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0)
                  inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
               // refund the difference to payer
               if ( price > itr->price) {
                   quant_after_fee = convert(itr1->quote_sym, price - itr->price) * base.amount / precision(base.symbol.precision());
                   //print("\n bid step1: quant_after_fee=",quant_after_fee);
                   quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                   //print("\n bid step2: quant_after_fee=",quant_after_fee);
                   inline_transfer(escrow, payer, itr1->quote_chain, quant_after_fee, "");
               }
               if (base < itr->base) {
                  idx_orderbook.modify(itr, _self, [&]( auto& o ) {
                     o.base -= convert(itr1->base, base);
                  });
               } else {
                  idx_orderbook.erase(itr);
               }
               return;
            } else { // partial match
               quant_after_fee = convert_asset(itr1->base_sym, itr->base);
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
               done_helper(itr1->exc_acc, itr->price, itr->base, bid_or_ask);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, receiver, itr1->base_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0)
                  inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
               quant_after_fee = convert(itr1->quote_sym, itr->price) * itr->base.amount / precision(itr->base.symbol.precision()) ;
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0)
                  inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
               base -= convert(itr1->base, itr->base);
               // refund the difference to payer
               if ( price > itr->price) {
                   quant_after_fee = convert(itr1->quote_sym, price - itr->price) * itr->base.amount / precision(itr->base.symbol.precision());
                   //print("bid step1: quant_after_fee=",quant_after_fee);
                   quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
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
      asset          fee;
      uint32_t       bid_or_ask = 0;

      uint128_t idxkey = compute_pair_index(base.symbol, price.symbol);
      //uint128_t idxkey = (uint128_t(base.symbol.name()) << 64) | price.symbol.name();

      //print("idxkey=",idxkey,",base_sym=",base.symbol.name(),",price.symbol=",price.symbol.name());

      auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end(), "trading pair does not exist");
      require_auth( itr1->exc_acc );

      base    = convert(itr1->base, base);
      price   = convert(itr1->quote, price);
      
      //print("after convert: base=",base,",price=",price);
      print("\n---exchange::match: base=", itr1->base, ", base_chain=", itr1->base_sym,", base_sym=", itr1->base_sym, ", quote=", itr1->quote,", quote_chain=", itr1->quote_chain,", quote_sym", itr1->quote_sym,"\n");
      
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
               quant_after_fee = convert(itr1->quote_sym, price) * base.amount / precision(base.symbol.precision()) ;
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
               done_helper(itr1->exc_acc, price, base, bid_or_ask);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               from.value = escrow; to.value = payer;
               print("\n 1: from=", from.to_string().c_str(), ", to=", to.to_string().c_str(),  ", amount=", quant_after_fee);
               inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0)
                  inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
               quant_after_fee = convert_asset(itr1->base_sym, base);
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
                   from.value = escrow; to.value = end_itr->maker;
               print("\n 2: from=", from.to_string().c_str(), ", to=", to.to_string().c_str(),  ", amount=", quant_after_fee);
               inline_transfer(escrow, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0)
                  inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
               
               // refund the difference
               if ( end_itr->price > price) {
                   quant_after_fee = convert(itr1->quote_sym, end_itr->price - price) * base.amount / precision(base.symbol.precision());
                   //print("bid step1: quant_after_fee=",quant_after_fee);
                   quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                   //print("bid step2: quant_after_fee=",quant_after_fee);
                   /*action(
                           permission_level{ escrow, N(active) },
                           relay_token_acc, N(transfer),
                           std::make_tuple(escrow, end_itr->maker, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                   ).send();*/
                   inline_transfer(escrow, end_itr->maker, itr1->quote_chain, quant_after_fee, "");
               }
               if( base < end_itr->base ) {
                  idx_orderbook.modify(end_itr, _self, [&]( auto& o ) {
                     o.base -= convert(itr1->base, base);
                  });
               } else {
                  idx_orderbook.erase(end_itr);
               }
               
               return;
            } else { // partial match
               quant_after_fee = convert(itr1->quote_sym, price) * end_itr->base.amount / precision(end_itr->base.symbol.precision());
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
               done_helper(itr1->exc_acc, price, end_itr->base, bid_or_ask);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0)
                  inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
               quant_after_fee = convert_asset(itr1->base_sym, end_itr->base);
               quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
               fee = calcfee(quant_after_fee, itr1->fee_rate);
               quant_after_fee -= fee;
               inline_transfer(escrow, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
               // transfer fee to exchange
               if (escrow != itr1->exc_acc && fee.amount > 0)
                  inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
               base -= convert(itr1->base, end_itr->base);
               // refund the difference
               if ( end_itr->price > price) {
                  quant_after_fee = convert(itr1->quote_sym, end_itr->price - price) * end_itr->base.amount / precision(end_itr->base.symbol.precision());
                  //print("bid step1: quant_after_fee=",quant_after_fee);
                  quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                  //print("bid step2: quant_after_fee=",quant_after_fee);
                  inline_transfer(escrow, end_itr->maker, itr1->quote_chain, quant_after_fee, "");
               }
               if (exit_on_done) {
                  idx_orderbook.erase(end_itr);
                  insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
                  /*auto pk = orders.available_primary_key();
                  orders.emplace( _self, [&]( auto& o ) {
                      o.id            = pk;
                      o.pair_id       = itr1->id;
                      o.bid_or_ask    = bid_or_ask;
                      o.base          = base;
                      o.price         = price;
                      o.maker         = payer;
                      o.receiver      = receiver;
                      o.timestamp     = time_point_sec(uint32_t(current_time() / 1000000ll));
                  });*/
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
         /*auto pk = orders.available_primary_key();
         orders.emplace( _self, [&]( auto& o ) {
             o.id            = pk;
             o.pair_id       = itr1->id;
             o.bid_or_ask    = bid_or_ask;
             o.base          = base;
             o.price         = price;
             o.maker         = payer;
             o.receiver      = receiver;
             o.timestamp     = time_point_sec(uint32_t(current_time() / 1000000ll));
         });*/
      } else {
         if (upper == idx_orderbook.cend()) {
            print("\n 8888888888888");
            upper--;
         }
             
         print("\n asi orderbook not empty, ask: order: id=", upper->id, ", pair_id=", upper->pair_id, ", bid_or_ask=", upper->bid_or_ask,", base=", upper->base,", price=", upper->price,", maker=", upper->maker);
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
   
   void exchange::done_helper(account_name exc_acc, asset price, asset quantity, uint32_t bid_or_ask) {
      auto timestamp = time_point_sec(uint32_t(current_time() / 1000000ll));
      action(
            permission_level{ exc_acc, N(active) },
            N(sys.match), N(done),
            std::make_tuple(price, quantity, bid_or_ask, timestamp)
      ).send();
   }
   
   // now do nothing, only for action capture
   void exchange::done(asset price, asset quantity, uint32_t bid_or_ask, time_point_sec timestamp) {
      
   }

   void exchange::cancel(uint64_t order_id) {
      trading_pairs   trading_pairs_table(_self,_self);
      orderbook       orders(_self,_self);
      asset           quant_after_fee;
      
      auto itr = orders.find(order_id);
      eosio_assert(itr != orders.cend(), "order record not found");
      require_auth( itr->maker );
      
      uint128_t idxkey = (uint128_t(itr->base.symbol.name()) << 64) | itr->price.symbol.name();

      //print("idxkey=",idxkey,",contract=",token_contract,",symbol=",token_symbol.value);

      auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end(), "trading pair does not exist");
      
      // refund the escrow
      if (itr->bid_or_ask) {
         quant_after_fee = convert(itr1->quote_sym, itr->price) * itr->base.amount / precision(itr->base.symbol.precision());
         //print("bid step1: quant_after_fee=",quant_after_fee);
         quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
         //print("bid step2: quant_after_fee=",quant_after_fee);
         inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
      } else {
         quant_after_fee = convert_asset(itr1->base_sym, itr->base);
         quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
         //print("bid step2: quant_after_fee=",quant_after_fee);
         inline_transfer(escrow, itr->receiver, itr1->base_chain, quant_after_fee, "");
      }
      
      orders.erase(itr);

      return;
   }
   
   asset exchange::calcfee(asset quant, uint64_t fee_rate) {
      asset fee = quant * fee_rate / max_fee_rate; // 万分之fee_rate,fee 是 asset已经防止溢出
      if(fee_rate > 0 && fee.amount < 1) {
          fee.amount = 1;//最少万分之一
      }

      return fee;
   }   
}


EOSIO_ABI( exchange::exchange, (create)(match)(cancel))
