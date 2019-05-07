

#include "sys.match.hpp"
#include <cmath>
#include "eosiolib/transaction.hpp"
#include "force.system/force.system.hpp"

namespace exchange {
   using namespace eosio;
   const int64_t max_fee_rate = 10000;
  
   const account_name escrow = N(sys.match);

   uint128_t compute_pair_index(symbol_type base, symbol_type quote)
   {
      uint128_t idxkey = (uint128_t(base.name()) << 64) | quote.name();
      return idxkey;
   }
   
   void exchange::regex(account_name exc_acc) {
      require_auth( exc_acc );
      
      const asset min_staked(10000000);
      
      // check if exc_acc has freezed 1000 SYS
      eosiosystem::system_contract sys_contract(config::system_account_name);
      eosio_assert(sys_contract.get_freezed(exc_acc) >= min_staked, "must freeze 1000 or more CDX!");
      
      exchanges exc_tbl(_self,_self);
      
      exc_tbl.emplace( exc_acc, [&]( auto& e ) {
         e.exc_acc      = exc_acc;
      });
   }
   
   /*  create trade pair
    *  payer:			
    *  base:	        
    *  quote:		    
   */
   void exchange::create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym, account_name exc_acc)
   {
      require_auth( exc_acc );

      eosio_assert(base.is_valid(), "invalid base symbol");
      eosio_assert(quote.is_valid(), "invalid quote symbol");
      
      exchanges exc_tbl(_self,_self);
      auto itr1 = exc_tbl.find(exc_acc);
      eosio_assert(itr1 != exc_tbl.end(), "exechange account has not been registered!");
      
      trading_pairs trading_pairs_table(_self,_self);

      check_pair(base_chain, base_sym, quote_chain, quote_sym);
      uint128_t idxkey = compute_pair_index(base, quote);
      //uint128_t idxkey = (uint128_t(base.name()) << 64) | quote.name();
      //print("idxkey=",idxkey,",base_sym=",base.name(),",price.symbol=",quote.name());
      print("\n base=", base, ", base_chain=", base_chain,", base_sym=", base_sym, "quote=", quote, ", quote_chain=", quote_chain, ", quote_sym=", quote_sym, "\n");

      auto idx = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr = idx.find(idxkey);

      eosio_assert(itr == idx.end(), "trading pair already created");

      auto pk = trading_pairs_table.available_primary_key();
      trading_pairs_table.emplace( exc_acc, [&]( auto& p ) {
         p.id = (uint32_t)pk;
         p.base_chain   = base_chain;
         p.base_sym     = to_asset(relay_token_acc, base_chain, base_sym, asset(0, base_sym)).symbol;
         p.base         = (base.name() << 8) | (p.base_sym.value & 0xff);
         p.quote_chain  = quote_chain;
         p.quote_sym    = to_asset(relay_token_acc, quote_chain, quote_sym, asset(0, quote_sym)).symbol;
         p.quote        = (quote.name() << 8) | (p.quote_sym.value & 0xff);
         p.exc_acc      = exc_acc;
         p.frozen       = 0;
      });
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

   void exchange::insert_order(
                              orderbook &orders, 
                              uint32_t pair_id, 
                              account_name exc_acc,
                              uint32_t bid_or_ask, 
                              asset base, 
                              asset price, 
                              account_name payer, 
                              account_name receiver) {
      // insert the order
      auto pk = orders.available_primary_key();
      orders.emplace( exc_acc, [&]( auto& o ) {
          o.id            = pk;
          o.pair_id       = pair_id;
          o.exc_acc       = exc_acc;
          o.bid_or_ask    = bid_or_ask;
          o.base          = base;
          o.price         = price;
          o.maker         = payer;
          o.receiver      = receiver;
          o.timestamp     = time_point_sec(uint32_t(current_time() / 1000000ll));
      });
      print("\ninsert_order: pk=",pk,",pair_id=",pair_id,",exc_acc=",eosio::name{.value=exc_acc}, ",bid_or_ask=",bid_or_ask,",base=",base,",price=",price,",payer=",eosio::name{.value=payer},",receiver=",eosio::name{.value=receiver}, "\n");
   }

   void exchange::match_for_bid( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, account_name exc_acc, string referer) {
      require_auth( exc_acc );

      trading_pairs  trading_pairs_table(_self,_self);
      orderbook      orders(_self,_self);
      asset          quant_after_fee;
      asset          converted_price;
      asset          cumulated_refund_quote;
      asset          fee;
      uint32_t       bid_or_ask = 1;

      auto base = quantity * precision(price.symbol.precision()) / price.amount;

      /*uint128_t idxkey = compute_pair_index(base.symbol, price.symbol);

      //print("idxkey=",idxkey,",base_sym=",base.symbol.name(),",price.symbol=",price.symbol.name());

      auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");*/
      auto itr1 = trading_pairs_table.find(pair_id);
      eosio_assert(itr1 != trading_pairs_table.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");
      
      fees fees_tbl(_self,_self);
      auto idx_fees = fees_tbl.template get_index<N(idxkey)>();
      auto idxkey_taker = (uint128_t(exc_acc) << 64) | pair_id;
      auto itr_fee_taker = idx_fees.find(idxkey_taker);
      auto itr_fee_maker = idx_fees.find(idxkey_taker);

      base    = convert(itr1->base, base);
      price   = convert(itr1->quote, price);
      
      if (price.symbol.precision() >= base.symbol.precision())
         quant_after_fee = price * base.amount / precision(base.symbol.precision());
      else
         quant_after_fee = base * price.amount / precision(price.symbol.precision());
      
      cumulated_refund_quote = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quantity) -
                               to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
      print("\n---exchange::match_for_bid: quantity=", quantity, ", quant_after_fee=", quant_after_fee,", cumulated_refund_quote=", cumulated_refund_quote,"\n");
      //print("after convert: base=",base,",price=",price);
      print("\n---exchange::match_for_bid: base=", itr1->base, ", base_chain=", itr1->base_sym,", base_sym=", itr1->base_sym, ", quote=", itr1->quote,", quote_chain=", itr1->quote_chain,", quote_sym=", itr1->quote_sym,"\n");
      
      auto idx_orderbook = orders.template get_index<N(idxkey)>();
      auto lookup_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, (uint32_t)price.amount);
      
      // traverse ask orders
      auto walk_table_range = [&]( auto itr, auto end_itr ) {
         bool  full_match;
         asset deal_base;
         asset cumulated_base          = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, asset(0));

         print("\n---exchange::match_for_bid: quantity=", quantity, ", quant_after_fee=", quant_after_fee,", cumulated_refund_quote=", cumulated_refund_quote,"\n");

         auto send_cumulated = [&]() {
            if (cumulated_base.amount > 0) {
               inline_transfer(escrow, receiver, itr1->base_chain, cumulated_base, "");
               cumulated_base = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, asset(0));
            }
               
            if (cumulated_refund_quote.amount > 0) {
               inline_transfer(escrow, payer, itr1->quote_chain, cumulated_refund_quote, "");
               cumulated_refund_quote  = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, asset(0));
            }
         };

         for( ; itr != end_itr; ) {
            print("\n bid: order: id=", itr->id, ", pair_id=", itr->pair_id, ", bid_or_ask=", itr->bid_or_ask,", base=", itr->base,", price=", itr->price,", maker=", eosio::name{.value = itr->maker});
            // only traverse ask orders
            /*if (itr->bid_or_ask) {
                itr++;
                continue;
            }
            // no matching sell order
            if (price < itr->price) {
               send_cumulated();
               // insert the order
               insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
               return;
            }*/
            
            
            if (base <= itr->base) {
               full_match  = true;
               deal_base = base;
            } else {
               full_match  = false;
               deal_base = itr->base;
            }
            
            quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, deal_base);
            converted_price = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, itr->price);   
            done_helper(exc_acc, itr->exc_acc, itr1->quote_chain, converted_price, itr1->base_chain, quant_after_fee, bid_or_ask);
            if (itr_fee_taker != idx_fees.cend()) {
               if (itr_fee_taker->type == 1) {
                  fee = calcfee(quant_after_fee, itr_fee_taker->rate);
                  quant_after_fee -= fee;
               } else {
                  // now no other type
               }
            } else {
               fee = calcfee(quant_after_fee, 0);
               quant_after_fee -= fee;
            }
            
            cumulated_base += quant_after_fee;
            if (full_match)
               inline_transfer(escrow, receiver, itr1->base_chain, cumulated_base, "");
               
            // transfer fee to exchange
            if (fee.amount > 0 && itr_fee_taker != idx_fees.cend()) {
               //inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
               /*idx_pair.modify(itr1, _self, [&]( auto& p ) {
                  p.fees_base += fee;
               });*/
               idx_fees.modify(itr_fee_taker, exc_acc, [&]( auto& f ) {
                  f.fees_base += fee;
               });
            }
               
            //quant_after_fee = convert(itr1->quote_sym, itr->price) * base.amount / precision(base.symbol.precision());
            if (itr->price.symbol.precision() >= deal_base.symbol.precision())
               quant_after_fee = itr->price * deal_base.amount / precision(deal_base.symbol.precision());
            else
               quant_after_fee = deal_base * itr->price.amount / precision(itr->price.symbol.precision());
            quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
            if (itr->exc_acc == exc_acc) {
               if (itr_fee_taker != idx_fees.cend()) {
                  fee = calcfee(quant_after_fee, itr_fee_taker->rate);
               } else {
                  fee = calcfee(quant_after_fee, 0);
               }
            } else {
               auto idxkey_maker = (uint128_t(itr->exc_acc) << 64) | pair_id;
               itr_fee_maker = idx_fees.find(idxkey_maker);
               if (itr_fee_maker != idx_fees.cend()) {
                  fee = calcfee(quant_after_fee, itr_fee_maker->rate);
               } else {
                  fee = calcfee(quant_after_fee, 0);
               }
            }
            
            quant_after_fee -= fee;
            inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
            // transfer fee to exchange
            if (fee.amount > 0) {
               //inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
               /*idx_pair.modify(itr1, _self, [&]( auto& p ) {
                  p.fees_quote += fee;
               });*/
               if (itr->exc_acc == exc_acc) {
                  idx_fees.modify(itr_fee_taker, exc_acc, [&]( auto& f ) {
                     f.fees_quote += fee;
                  });
               } else if (itr_fee_maker != idx_fees.cend()) {
                  idx_fees.modify(itr_fee_maker, itr->exc_acc, [&]( auto& f ) {
                     f.fees_quote += fee;
                  });
               }
               
            }
            // refund the difference to payer
            if ( price > itr->price) {
               auto diff = price - itr->price;
               if (itr->price.symbol.precision() >= deal_base.symbol.precision())
                  quant_after_fee = diff * deal_base.amount / precision(deal_base.symbol.precision());
               else
                  quant_after_fee = deal_base * diff.amount / precision(diff.symbol.precision());
               //print("\n bid step1: quant_after_fee=",quant_after_fee);
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
               //print("\n bid step2: quant_after_fee=",quant_after_fee);
               cumulated_refund_quote += quant_after_fee;
               if (full_match)
                  inline_transfer(escrow, payer, itr1->quote_chain, cumulated_refund_quote, "");
            } else if (cumulated_refund_quote.amount > 0)
               send_cumulated();

            if (full_match) {
               if (base < itr->base) {
                  idx_orderbook.modify(itr, _self, [&]( auto& o ) {
                     o.base -= deal_base;
                  });
               } else {
                  idx_orderbook.erase(itr);
               }
               return;
            } else {
               base -= itr->base;
               idx_orderbook.erase(itr++);
               if (itr == end_itr) {
                  send_cumulated();
                  insert_order(orders, itr1->id, exc_acc, bid_or_ask, base, price, payer, receiver);
               }
            }
         }
      };
      
      //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
      auto lower_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::lowest());    
      auto lower = idx_orderbook.lower_bound( lower_key );
      auto upper = idx_orderbook.upper_bound( lookup_key );
 
      if (lower == idx_orderbook.cend() // orderbook empty
         || lower->pair_id != itr1->id || lower->bid_or_ask != 0 || lower->price > price
         ) {
         prints("\n buy: qualified ask orderbook empty");
         if (cumulated_refund_quote.amount > 0) {
            inline_transfer(escrow, payer, itr1->quote_chain, cumulated_refund_quote, "");
         }
         insert_order(orders, itr1->id, exc_acc, bid_or_ask, base, price, payer, receiver);
      } else {
         walk_table_range(lower, upper);
      }
   }

   void exchange::match_for_ask( uint32_t pair_id, account_name payer, account_name receiver, asset base, asset price, account_name exc_acc, string referer) {
      require_auth( exc_acc );

      trading_pairs  trading_pairs_table(_self,_self);
      orderbook      orders(_self,_self);
      asset          quant_after_fee;
      asset          converted_price;
      asset          converted_base;
      asset          fee;
      uint32_t       bid_or_ask = 0;

      /*uint128_t idxkey = compute_pair_index(base.symbol, price.symbol);

      //print("idxkey=",idxkey,",base_sym=",base.symbol.name(),",price.symbol=",price.symbol.name());

      auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");*/
      auto itr1 = trading_pairs_table.find(pair_id);
      eosio_assert(itr1 != trading_pairs_table.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");

      fees fees_tbl(_self,_self);
      auto idx_fees = fees_tbl.template get_index<N(idxkey)>();
      auto idxkey_taker = (uint128_t(exc_acc) << 64) | pair_id;
      auto itr_fee_taker = idx_fees.find(idxkey_taker);
      auto itr_fee_maker = idx_fees.find(idxkey_taker);

      base    = convert(itr1->base, base);
      price   = convert(itr1->quote, price);
      
      //print("after convert: base=",base,",price=",price);
      print("\n---exchange::match_for_ask: base=", itr1->base, ", base_chain=", itr1->base_sym,", base_sym=", itr1->base_sym, ", quote=", itr1->quote,", quote_chain=", itr1->quote_chain,", quote_sym=", itr1->quote_sym,"\n");
      
      auto idx_orderbook = orders.template get_index<N(idxkey)>();
      // if bid, traverse ask orders, other traverse bid orders
      // auto lookup_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | price.amount;
      auto lookup_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, (uint32_t)price.amount);
      
      auto walk_table_range = [&]( auto itr, auto end_itr ) {
         bool  full_match;
         asset deal_base;
         asset cumulated_quote         = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, asset(0));
         
         auto send_cumulated = [&]() {
            if (cumulated_quote.amount > 0) {
               inline_transfer(escrow, receiver, itr1->quote_chain, cumulated_quote, "");
               cumulated_quote         = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, asset(0));
            }
         };
         
         for ( auto exit_on_done = false; !exit_on_done; ) {
            print("\n ask: order: id=", end_itr->id, ", pair_id=", end_itr->pair_id, ", bid_or_ask=", end_itr->bid_or_ask,", base=", end_itr->base,", price=", end_itr->price,", maker=", eosio::name{.value = end_itr->maker});
            // only traverse bid orders
            if (end_itr == itr) exit_on_done = true;
            /*if (!end_itr->bid_or_ask) {
               if (!exit_on_done) end_itr--;
               continue;
            }
            // no matching bid order
            if (price > end_itr->price) {
               send_cumulated();
               // insert the order
               insert_order(orders, itr1->id, bid_or_ask, base, price, payer, receiver);
               return;
            }*/
            
            if ( base <= end_itr->base ) {// full match
               full_match = true;
               deal_base = base;
            } else {
               full_match = false;
               deal_base = end_itr->base;
            }
            
            // eat the order
            if (price.symbol.precision() >= deal_base.symbol.precision())
               quant_after_fee = price * deal_base.amount / precision(deal_base.symbol.precision()) ;
            else
               quant_after_fee = deal_base * price.amount / precision(price.symbol.precision()) ;
            quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
            converted_price = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, price);
            converted_base = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, deal_base);
            done_helper(exc_acc, itr->exc_acc, itr1->quote_chain, converted_price, itr1->base_chain, converted_base, bid_or_ask);
            if (itr_fee_taker != idx_fees.cend()) {
               if (itr_fee_taker->type == 1) {
                  fee = calcfee(quant_after_fee, itr_fee_taker->rate);
               }
               else {
                  // now not support other tpes
               }
            } else {
               fee = calcfee(quant_after_fee, 0);
            }
            
            quant_after_fee -= fee;
            print("\n 1: from=", eosio::name{.value = escrow}, ", to=", eosio::name{.value = payer},  ", amount=", quant_after_fee);
            cumulated_quote += quant_after_fee;
            if (full_match) 
               inline_transfer(escrow, receiver, itr1->quote_chain, cumulated_quote, "");
               
            // transfer fee to exchange
            if (fee.amount > 0 && itr_fee_taker != idx_fees.cend()) {
               //inline_transfer(escrow, itr1->exc_acc, itr1->quote_chain, fee, "");
               /*idx_pair.modify(itr1, _self, [&]( auto& p ) {
                  p.fees_quote += fee;
               });*/
               idx_fees.modify(itr_fee_taker, 0, [&]( auto& f ) {
                  f.fees_quote += fee;
               });
            }
               
            quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, itr1->base_sym, deal_base);
            if (itr->exc_acc == exc_acc) {
               if (itr_fee_taker != idx_fees.cend()) {
                  fee = calcfee(quant_after_fee, itr_fee_taker->rate);
               } else {
                  fee = calcfee(quant_after_fee, 0);
               }
            } else {
               auto idxkey_maker = (uint128_t(itr->exc_acc) << 64) | pair_id;
               itr_fee_maker = idx_fees.find(idxkey_maker);
               if (itr_fee_maker != idx_fees.cend()) {
                  fee = calcfee(quant_after_fee, itr_fee_maker->rate);
               } else {
                  fee = calcfee(quant_after_fee, 0);
               }
            }
            quant_after_fee -= fee;
            print("\n 2: from=", eosio::name{.value = escrow}, ", to=", eosio::name{.value = end_itr->maker},  ", amount=", quant_after_fee);
            inline_transfer(escrow, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
            // transfer fee to exchange
            if (fee.amount > 0) {
               //inline_transfer(escrow, itr1->exc_acc, itr1->base_chain, fee, "");
               /*idx_pair.modify(itr1, _self, [&]( auto& p ) {
                  p.fees_base += fee;
               });*/
               if (itr->exc_acc == exc_acc) {
                  idx_fees.modify(itr_fee_taker, exc_acc, [&]( auto& f ) {
                     f.fees_base += fee;
                  });
               } else if (itr_fee_maker != idx_fees.cend()) {
                  idx_fees.modify(itr_fee_maker, itr->exc_acc, [&]( auto& f ) {
                     f.fees_base += fee;
                  });
               }
            }
               
            // refund the difference
            if ( end_itr->price > price) {
               auto diff = end_itr->price - price;
               if (end_itr->price.symbol.precision() >= deal_base.symbol.precision())
                  quant_after_fee = diff * deal_base.amount / precision(deal_base.symbol.precision());
               else
                  quant_after_fee = deal_base * diff.amount / precision(diff.symbol.precision());
               //print("bid step1: quant_after_fee=",quant_after_fee);
               quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
               //print("bid step2: quant_after_fee=",quant_after_fee);
               inline_transfer(escrow, end_itr->maker, itr1->quote_chain, quant_after_fee, "");
            }
            if (full_match) {
               if( deal_base < end_itr->base ) {
                  idx_orderbook.modify(end_itr, _self, [&]( auto& o ) {
                     o.base -= deal_base;
                  });
               } else {
                  idx_orderbook.erase(end_itr);
               }
               
               return;
            } else {
               base -= end_itr->base;
               
               if (exit_on_done) {
                  send_cumulated();
                  idx_orderbook.erase(end_itr);
                  insert_order(orders, itr1->id, exc_acc, bid_or_ask, base, price, payer, receiver);
               } else
                  idx_orderbook.erase(end_itr--);
            }
         }
      };
      
      auto lower = idx_orderbook.lower_bound( lookup_key );
      auto upper_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::max());
      auto upper = idx_orderbook.lower_bound( upper_key );
      
      if (lower == idx_orderbook.cend() // orderbook empty
         || lower->pair_id != itr1->id // not desired pair /* || lower->bid_or_ask != 1 || lower->price < price */ // not at all
         ) {
         print("\n sell: bid orderbook empty, lookup_key=", lookup_key);
         insert_order(orders, itr1->id, exc_acc, bid_or_ask, base, price, payer, receiver);
      } else {
         if (upper == idx_orderbook.cend() || upper->pair_id != itr1->id) {
            print("\n 8888888888888");
            upper--;
         }
             
         print("\n bid orderbook not empty, ask: order: id=", upper->id, ", pair_id=", upper->pair_id, ", bid_or_ask=", upper->bid_or_ask,", base=", upper->base,", price=", upper->price,", maker=", eosio::name{.value = upper->maker});
         walk_table_range(lower, upper);
      }
   }

   /*  
    *  payer: 	            paying account
    *  base:		        target token symbol and amount
    *  price: 	            quoting token symbol and price
    *  bid_or_ask:         1: buy, 0: sell
    *  fee_account:		fee account,payer==fee_account means no fee
    * */
   void exchange::match( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, uint32_t bid_or_ask, account_name exc_acc, string referer) {
      //require_auth( payer );
      
      print("\n--------------enter exchange::match: pair_id=", pair_id, ", payer=", eosio::name{.value = payer}, ", receiver=", eosio::name{.value = receiver},", quantity=", quantity, ", price=", price,", bid_or_ask=", bid_or_ask,"\n");

      if (bid_or_ask) {
         match_for_bid(pair_id, payer, receiver, quantity, price, exc_acc, referer);
      } else {
         match_for_ask(pair_id, payer, receiver, quantity, price, exc_acc, referer);
      }

      return;
   }
   
   void exchange::done_helper(account_name taker_exc_acc, account_name maker_exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask) {
      auto timestamp = time_point_sec(uint32_t(current_time() / 1000000ll));
      action(
            permission_level{ taker_exc_acc, N(active) },
            N(sys.match), N(done),
            std::make_tuple(taker_exc_acc, maker_exc_acc, quote_chain, price, base_chain, quantity, bid_or_ask, timestamp)
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
   void exchange::done(account_name taker_exc_acc, account_name maker_exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask, time_point_sec timestamp) {
      require_auth(taker_exc_acc);
      
      asset quant_after_fee;
      
      print("exchange::done: taker_exc_acc=", eosio::name{.value = taker_exc_acc}, "maker_exc_acc=", eosio::name{.value = maker_exc_acc}, ", price=", price,", quantity=", quantity, ", bid_or_ask=", bid_or_ask, "\n");
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
   void exchange::cancel(account_name maker, uint32_t type, uint64_t order_or_pair_id) {
      eosio_assert(type == 0 || type == 1 || type == 2, "invalid cancel type");
      trading_pairs   trading_pairs_table(_self,_self);
      orderbook       orders(_self,_self);
      asset           quant_after_fee;
      
      require_auth( maker );
      
      if (type == 0) {
         auto itr = orders.find(order_or_pair_id);
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
            if (type == 1 && static_cast<uint32_t>(order_or_pair_id) != lower->pair_id) {
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
         d.sum          = to_asset(relay_token_acc, quote_chain, quote_sym, asset(0, quote_sym));
         d.vol          = to_asset(relay_token_acc, base_chain, base_sym, asset(0, base_sym));
         d.reset_block_height = start_block;
         d.block_height_end = start_block + INTERVAL_BLOCKS - 1;
      });
   }
   
   void exchange::claim(name base_chain, symbol_type base_sym, name quote_chain, symbol_type quote_sym, account_name exc_acc, account_name fee_acc) {
      require_auth(exc_acc);
      
      auto pair_id = get_pair_id(base_chain, base_sym, quote_chain, quote_sym);
      fees fees_tbl(_self,_self);
      auto idx_fees = fees_tbl.template get_index<N(idxkey)>();
      auto idxkey = (uint128_t(exc_acc) << 64) | pair_id;
      auto itr = idx_fees.find(idxkey);
      eosio_assert(itr != idx_fees.cend(), "no fees in fees table");

      bool claimed = false;
     
      if (escrow != fee_acc && itr->fees_base.amount > 0)
      {
         inline_transfer(escrow, fee_acc, base_chain, itr->fees_base, "");
         idx_fees.modify(itr, exc_acc, [&]( auto& f ) {
            f.fees_base = to_asset(relay_token_acc, base_chain, base_sym, asset(0, base_sym));
         });
         claimed = true;
      }
     
      if (escrow != fee_acc && itr->fees_quote.amount > 0)
      {
         inline_transfer(escrow, fee_acc, quote_chain, itr->fees_quote, "");
         idx_fees.modify(itr, exc_acc, [&]( auto& f ) {
            f.fees_quote = to_asset(relay_token_acc, quote_chain, quote_sym, asset(0, quote_sym));
         });
         claimed = true;
      }   
     
      eosio_assert(claimed, "no fees or fee_acc is escrow account");
     
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
   
   void exchange::setfee(account_name exc_acc, uint32_t pair_id, uint32_t type, uint32_t rate, name chain, asset fee) {
      require_auth(exc_acc);
      
      fees   fees_tbl(_self, _self);
      
      auto idx_fee = fees_tbl.template get_index<N(idxkey)>();
      
      auto idxkey = (uint128_t(exc_acc) << 64) | pair_id;
      auto itr = idx_fee.find(idxkey);
      eosio_assert(type == 1, "now only suppert type 1");
      
      if (itr == idx_fee.cend()) {
         name        base_chain;
         symbol_type base_sym;
         name        quote_chain;
         symbol_type quote_sym;
         
         get_pair(pair_id, base_chain, base_sym, quote_chain, quote_sym);
         
         auto pk = fees_tbl.available_primary_key();
         fees_tbl.emplace( exc_acc, [&]( auto& f ) {
            f.id           = (uint32_t)pk;
            f.exc_acc      = exc_acc;
            f.pair_id      = pair_id;
            f.type         = type;
            f.rate         = rate;
            f.chain        = chain;
            f.fee          = fee;
            f.fees_base    = to_asset(relay_token_acc, base_chain, base_sym, asset(0, base_sym));
            f.fees_quote   = to_asset(relay_token_acc, quote_chain, quote_sym, asset(0, quote_sym));
         });
      } else {
         idx_fee.modify(itr, exc_acc, [&]( auto& f ) {
            f.type         = type;
            f.rate         = rate;
            f.chain        = chain;
            f.fee          = fee;
         });
      }
   }
   
   asset exchange::calcfee(asset quant, uint64_t fee_rate) {
      asset fee = quant * fee_rate / max_fee_rate;
      if(fee_rate > 0 && fee.amount < 1) {
          fee.amount = 1;
      }

      return fee;
   }
}

EOSIO_ABI( exchange::exchange, (create)(match)(cancel)(done)(mark)(claim)(freeze)(unfreeze)(regex)(setfee))
