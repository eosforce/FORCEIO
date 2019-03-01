

#include "sys.match.hpp"
#include <cmath>

namespace exchange {
    using namespace eosio;
    const account_name relay_token_acc = N(relay.token);
    const account_name escrow = N(eosfund1);

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
    void exchange::create(symbol_type base, name base_chain, symbol_type base_sym, symbol_type quote, name quote_chain, symbol_type quote_sym)
    {
        require_auth( _self );

        eosio_assert(base.is_valid(), "invalid base symbol");
        eosio_assert(quote.is_valid(), "invalid quote symbol");

        trading_pairs trading_pairs_table(_self,_self);

        //uint128_t idxkey = compute_pair_index(base, quote);
        uint128_t idxkey = (uint128_t(base.name()) << 64) | quote.name();
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
            
        });
        
        // test
        {
         auto itr2 = trading_pairs_table.find(pk);
         eosio_assert(itr2 != trading_pairs_table.end(), "trading pair does not exist");
         print("\n pair created\n");
        }
    }

    asset exchange::to_asset( account_name code, name chain, const asset& a ) {
        asset b;
        relay::token t(code);
        
        symbol_type expected_symbol = t.get_supply(chain, a.symbol.name()).symbol ;

        b = convert(expected_symbol, a);
        return b;
    }
    
    asset exchange::convert( symbol_type expected_symbol, const asset& a ) {
        //print("\n exchange::convert   expected_symbol=", expected_symbol, ", a=", a);
        eosio_assert(expected_symbol.precision() >= a.symbol.precision(), "converted precision must be greater or equal");
         //print("\n exchange::convert   &7777777777=", expected_symbol, ", a=", a);
        auto factor = precision( expected_symbol.precision() ) / precision( a.symbol.precision() );
         //print("\n exchange::convert   &000000000000=", expected_symbol, ", a=", a);
        auto b = asset( a.amount * factor, expected_symbol );
        //print("\n exchange::convert   &555555555 b=", b);
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
        
        print("\n--------------enter exchange::match: payer=", payer, "receiver=", receiver,"base=", base, "price=", price,"bid_or_ask=", bid_or_ask,"\n");
        require_auth( _self );

        //eosio_assert(eos_quant.symbol == S(4, EOS), "eos_quant symbol must be EOS");
        //eosio_assert(token_symbol.is_valid(), "invalid token_symbol");
        //eosio_assert(fee_rate >= 0 && fee_rate < max_fee_rate, "invalid fee_rate, 0<= fee_rate < 10000");
        
        trading_pairs   trading_pairs_table(_self,_self);
        orderbook       orders(_self,_self);
        asset           quant_after_fee;

         

        uint128_t idxkey = (uint128_t(base.symbol.name()) << 64) | price.symbol.name();

        //print("idxkey=",idxkey,",base_sym=",base.symbol.name(),",price.symbol=",price.symbol.name());

        auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
        auto itr1 = idx_pair.find(idxkey);
        eosio_assert(itr1 != idx_pair.end(), "trading pair does not exist");

        base    = convert(itr1->base, base);
        price   = convert(itr1->quote, price);
        
        //print("after convert: base=",base,",price=",price);
        print("\n--------------exchange::match: base=", itr1->base, "base_chain=", itr1->base_sym,"base_sym=", itr1->base_sym, "quote=", itr1->quote,"quote_chain=", itr1->quote_chain,"quote_sym", itr1->quote_sym,"\n");

        /*auto quant_after_fee = eos_quant;

        if(fee_rate > 0 && payer != fee_account){
            auto fee = calcfee(eos_quant, fee_rate);
            quant_after_fee -= fee;

            action(
                    permission_level{ payer, N(active) },
                    market->quote.contract, N(transfer),
                    std::make_tuple(payer, fee_account, fee, std::string("send EOS fee to fee_account:" + to_string(fee)))
            ).send();
        }
        */
        
        auto idx_orderbook = orders.template get_index<N(idxkey)>();
        // if bid, traverse ask orders, other traverse bid orders
        // auto lookup_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | price.amount;
        auto lookup_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, (uint32_t)price.amount);
        // test. walk through orderbook
        {
            print("\n ---------------- begin to walk through orderbook: ----------------");
            auto walk_table_range = [&]( auto itr, auto end_itr ) {
                for( ; itr != end_itr; ++itr ) {
                    print("\n bid: order: id=", itr->id, ", pair_id=", itr->pair_id, ", bid_or_ask=", itr->bid_or_ask,", base=", itr->base,", price=", itr->price,", maker=", itr->maker);
                }
            };
            //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
            auto lower_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::lowest());
            auto lower = idx_orderbook.lower_bound( lower_key );
            //auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
            auto upper_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::max());
            auto upper = idx_orderbook.upper_bound( upper_key );
            walk_table_range(lower, upper);
            print("\n -------------------- walk through orderbook ends ----------------:");
        }
        name from,to;
        
        if (bid_or_ask) {
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
                        auto pk = orders.available_primary_key();
                        orders.emplace( _self, [&]( auto& o ) {
                            o.id            = pk;
                            o.pair_id       = itr1->id;
                            o.bid_or_ask    = bid_or_ask;
                            o.base          = convert(itr1->base, base);
                            o.price         = convert(itr1->quote, price);
                            o.maker         = payer;
                            o.receiver      = receiver;
                        });
                        return;
                    }
                    if( base < itr->base ) { // full match
                        // eat the order
                        base = convert_asset(itr1->base_sym, base);
                        quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, base);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, receiver, itr1->base_chain, quant_after_fee, "");
                        quant_after_fee = convert(itr1->quote_sym, itr->price) * base.amount / precision(base.symbol.precision());
                        quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, itr->maker, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
                        idx_orderbook.modify(itr, _self, [&]( auto& o ) {
                            o.base          -= convert(itr1->base, base);
                        });
                        // refund the difference to payer
                        if ( price > itr->price) {
                            quant_after_fee = convert(itr1->quote_sym, price - itr->price) * base.amount / precision(base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            /*action(
                                    permission_level{ escrow, N(active) },
                                    relay_token_acc, N(transfer),
                                    std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();*/
                            inline_transfer(escrow, payer, itr1->quote_chain, quant_after_fee, "");
                        }
                        return;
                    } else if ( base == itr->base ) { // full match
                        base = convert_asset(itr1->base_sym, base);
                        quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, base);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, receiver, itr1->base_chain, quant_after_fee, "");
                        quant_after_fee = convert(itr1->quote_sym, itr->price) * base.amount / precision(base.symbol.precision());
                        quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, itr->maker, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
                        // refund the difference to payer
                        if ( price > itr->price) {
                            quant_after_fee = convert(itr1->quote_sym, price - itr->price) * base.amount / precision(base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            /*action(
                                    permission_level{ escrow, N(active) },
                                    relay_token_acc, N(transfer),
                                    std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();*/
                            inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
                        }
                        idx_orderbook.erase(itr);
                        return;
                    } else { // partial match
                        quant_after_fee = convert_asset(itr1->base_sym, itr->base);
                        quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, receiver, itr1->base_chain, quant_after_fee, "");
                        quant_after_fee = convert(itr1->quote_sym, itr->price) * itr->base.amount / precision(itr->base.symbol.precision()) ;
                        quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, itr->maker, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
                        base -= convert(itr1->base, itr->base);
                        // refund the difference to payer
                        if ( price > itr->price) {
                            quant_after_fee = convert(itr1->quote_sym, price - itr->price) * itr->base.amount / precision(itr->base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            /*action(
                                    permission_level{ escrow, N(active) },
                                    relay_token_acc, N(transfer),
                                    std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();*/
                            inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
                        }
                        idx_orderbook.erase(itr++);
                    }
                }
            };
            
            //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
            auto lower_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::lowest());    
            auto lower = idx_orderbook.lower_bound( lower_key );
            auto upper = idx_orderbook.upper_bound( lookup_key );
 
            if (lower == upper) {
                prints("\n buy: sell orderbook empty");
                auto pk = orders.available_primary_key();
                orders.emplace( _self, [&]( auto& o ) {
                    o.id            = pk;
                    o.pair_id       = itr1->id;
                    o.bid_or_ask    = bid_or_ask;
                    o.base          = convert(itr1->base, base);
                    o.price         = convert(itr1->quote, price);
                    o.maker         = payer;
                    o.receiver      = receiver;
                });
            } else {
                walk_table_range(lower, upper);
            }
        } else {
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
                        auto pk = orders.available_primary_key();
                        orders.emplace( _self, [&]( auto& o ) {
                            o.id            = pk;
                            o.pair_id       = itr1->id;
                            o.bid_or_ask    = bid_or_ask;
                            o.base          = convert(itr1->base, base);
                            o.price         = convert(itr1->quote, price);
                            o.maker         = payer;
                            o.receiver      = receiver;
                        });
                        return;
                    }
                    
                    if( base < end_itr->base ) { // full match
                        // eat the order
                        quant_after_fee = convert(itr1->quote_sym, price) * base.amount / precision(base.symbol.precision()) ;
                        quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                        from.value = escrow; to.value = payer;
                        print("\n 1: from=", from.to_string().c_str(), ", to=", to.to_string().c_str(),  ", amount=", quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
                        quant_after_fee = convert_asset(itr1->base_sym, base);
                        quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
                            from.value = escrow; to.value = end_itr->maker;
                        print("\n 2: from=", from.to_string().c_str(), ", to=", to.to_string().c_str(),  ", amount=", quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, end_itr->maker, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
                        idx_orderbook.modify(end_itr, _self, [&]( auto& o ) {
                            o.base          -= convert(itr1->base, base);
                        });
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
                            inline_transfer(escrow, end_itr->receiver, itr1->quote_chain, quant_after_fee, "");
                        }
                        
                        return;
                    } else if ( base == end_itr->base ) { // full match
                        quant_after_fee = convert(itr1->quote_sym, price) * base.amount / precision(base.symbol.precision());
                        quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
                        quant_after_fee = convert_asset(itr1->base_sym, base);
                        quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, end_itr->maker, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
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
                            inline_transfer(escrow, end_itr->receiver, itr1->quote_chain, quant_after_fee, "");
                        }
                        idx_orderbook.erase(end_itr);
                        return;
                    } else { // partial match
                        quant_after_fee = convert(itr1->quote_sym, price) * end_itr->base.amount / precision(end_itr->base.symbol.precision());
                        quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, receiver, itr1->quote_chain, quant_after_fee, "");
                        quant_after_fee = convert_asset(itr1->base_sym, end_itr->base);
                        quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
                        /*action(
                                permission_level{ escrow, N(active) },
                                relay_token_acc, N(transfer),
                                std::make_tuple(escrow, end_itr->maker, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();*/
                        inline_transfer(escrow, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
                        base -= convert(itr1->base, end_itr->base);
                        // refund the difference
                        if ( end_itr->price > price) {
                            quant_after_fee = convert(itr1->quote_sym, end_itr->price - price) * end_itr->base.amount / precision(end_itr->base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            /*action(
                                    permission_level{ escrow, N(active) },
                                    relay_token_acc, N(transfer),
                                    std::make_tuple(escrow, end_itr->maker, quant_after_fee.symbol.name(), quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();*/
                            inline_transfer(escrow, end_itr->receiver, itr1->quote_chain, quant_after_fee, "");
                        }
                        if (exit_on_done)
                            idx_orderbook.erase(end_itr);
                        else
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
                auto pk = orders.available_primary_key();
                orders.emplace( _self, [&]( auto& o ) {
                    o.id            = pk;
                    o.pair_id       = itr1->id;
                    o.bid_or_ask    = bid_or_ask;
                    o.base          = base;
                    o.price         = price;
                    o.maker         = payer;
                    o.receiver      = receiver;
                });
            } else {
                
                if (upper == idx_orderbook.cend()) {
                    print("\n 8888888888888");
                    upper--;
                }
                    
                 print("\n ^^^^^^^^^^^^ ask: order: id=", upper->id, ", pair_id=", upper->pair_id, ", bid_or_ask=", upper->bid_or_ask,", base=", upper->base,", price=", upper->price,", maker=", upper->maker);
                walk_table_range(lower, upper);
            }
        }
/*
        eosio_assert( quant_after_fee.amount > 0, "quant_after_fee must a positive amount" );

        asset market_fee{0, eos_quant.symbol};
        if(market->buy_fee_rate > 0){//减去交易所手续费
            market_fee = calcfee(eos_quant, market->buy_fee_rate);
        }

        action(//给交易所账户转入EOS
                permission_level{ payer, N(active) },
                market->quote.contract, N(transfer),
                std::make_tuple(payer, market->exchange_account, quant_after_fee, std::string("send EOS to ET included fee:" + to_string(market_fee)))
        ).send();

        quant_after_fee -= market_fee;
        eosio_assert( quant_after_fee.amount > 0, "quant_after_fee2 must a positive amount " );

        print("\nquant_after_fee:");
        quant_after_fee.print();

        asset token_out{0,token_symbol};
        _market.modify( *market, 0, [&]( auto& es ) {
            token_out = es.convert( quant_after_fee,  token_symbol);
            es.quote.balance += market_fee;

            statsfee(es, market_fee, asset{0, token_symbol});
        });
        eosio_assert( token_out.amount > 0, "must reserve a positive amount" );

        action(//交易所账户转出代币
                permission_level{ market->exchange_account, N(active) },
                market->base.contract, N(transfer),
                std::make_tuple(market->exchange_account, payer, token_out, std::string("receive token from ET"))
        ).send();
    }




    asset exchange::calcfee(asset quant, uint64_t fee_rate){
        asset fee = quant * fee_rate / max_fee_rate; // 万分之fee_rate,fee 是 asset已经防止溢出
        if(fee.amount < 1){
            fee.amount = 1;//最少万分之一
        }*/

        return;
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
            /*action(
                    permission_level{ escrow, N(active) },
                    relay_token_acc, N(transfer),
                    std::make_tuple(escrow, itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
            ).send();*/
            inline_transfer(escrow, itr->receiver, itr1->quote_chain, quant_after_fee, "");
        } else {
            quant_after_fee = convert_asset(itr1->base_sym, itr->base);
            quant_after_fee = to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
            //print("bid step2: quant_after_fee=",quant_after_fee);
            /*action(
                    permission_level{ escrow, N(active) },
                    relay_token_acc, N(transfer),
                    std::make_tuple(escrow, itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
            ).send();*/
            inline_transfer(escrow, itr->receiver, itr1->base_chain, quant_after_fee, "");
        }
        
        orders.erase(itr);

        return;
    }
        
}


EOSIO_ABI( exchange::exchange, (create)(match)(cancel))
