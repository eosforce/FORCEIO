

#include "exchange.h"
#include <cmath>

namespace exchange {
    using namespace eosio;
    //const account_name escrow = N(exc_escrow);
    const account_name escrow = N(eosfund1);

    /*  create trade pairt
     *  payer:			
     *  base:	        
     *  quote:		    
    */
    void exchange::create(symbol_type base, symbol_type quote)
    {
        require_auth( _self );

        eosio_assert(base.is_valid(), "invalid base symbol");
        eosio_assert(quote.is_valid(), "invalid quote symbol");

        trading_pairs trading_pairs_table(_self,_self);

        uint128_t idxkey = (uint128_t(base.name()) << 64) | quote.name();

        auto idx = trading_pairs_table.template get_index<N(idxkey)>();
        auto itr = idx.find(idxkey);

        eosio_assert(itr == idx.end(), "trading pair already created");

        auto pk = trading_pairs_table.available_primary_key();
        trading_pairs_table.emplace( _self, [&]( auto& p ) {
            p.id = (uint32_t)pk;
            
            p.base = base;
            p.quote = quote;
            
        });
    }

    asset exchange::to_asset( account_name code, const asset& a ) {
        asset b;
        token t(code);
        
        symbol_type expected_symbol = t.get_supply(a.symbol.name()).symbol ;

        b = convert(expected_symbol, a);
        return b;
    }
    
    asset exchange::convert( symbol_type expected_symbol, const asset& a ) {
        eosio_assert(expected_symbol.precision() >= a.symbol.precision(), "converted precision must be greater or equal");
        auto factor = precision( expected_symbol.precision() ) / precision( a.symbol.precision() );
        auto b = asset( a.amount * factor, expected_symbol.value );
        return b;
    }

    /*  
     *  payer: 	            paying account
     *  base:		        target token symbol and amount
     *  price: 	            quoting token symbol and price
     *  bid_or_ask:         1: buy, 0: sell
     *  fee_account:		fee account,payer==fee_account means no fee
     *  fee_rate:		    fee rate:[0,10000), for example:50 means 50 of ten thousandths ; 0 means no fee
     * */
    void exchange::trade( account_name payer, asset base, asset price, uint32_t bid_or_ask) {
        require_auth( payer );

        //eosio_assert(eos_quant.symbol == S(4, EOS), "eos_quant symbol must be EOS");
        //eosio_assert(token_symbol.is_valid(), "invalid token_symbol");
        //eosio_assert(fee_rate >= 0 && fee_rate < max_fee_rate, "invalid fee_rate, 0<= fee_rate < 10000");
        
        trading_pairs   trading_pairs_table(_self,_self);
        orderbook       orders(_self,_self);
        asset           quant_after_fee;

        uint128_t idxkey = (uint128_t(base.symbol.name()) << 64) | price.symbol.name();

        //print("idxkey=",idxkey,",contract=",token_contract,",symbol=",token_symbol.value);

        auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
        auto itr1 = idx_pair.find(idxkey);
        eosio_assert(itr1 != idx_pair.end(), "trading pair does not exist");

        base    = convert(itr1->base, base);
        price   = convert(itr1->quote, price);
        
        //print("after convert: base=",base,",price=",price);

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
        auto lookup_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | price.amount;
        
        // test. walk through orderbook
        {
            print("\n ---------------- begin to walk through orderbook: ----------------");
            auto walk_table_range = [&]( auto itr, auto end_itr ) {
                for( ; itr != end_itr; ++itr ) {
                    print("\n bid: order: id=", itr->id, ", pair_id=", itr->pair_id, ", bid_or_ask=", itr->bid_or_ask,", base=", itr->base,", price=", itr->price,", maker=", itr->maker);
                }
            };
            auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
            auto lower = idx_orderbook.lower_bound( lower_key );
            auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
            auto upper = idx_orderbook.upper_bound( upper_key );
            walk_table_range(lower, upper);
            print("\n -------------------- walk through orderbook ends ----------------:");
        }
        name from,to;
        
        if (bid_or_ask) {
            // first, transfer the quote currency to escrow account
            //print("bid step0: quant_after_fee=",convert(itr1->quote, price)," base.amount =",base.amount, " precision =",precision(base.symbol.precision()));
            quant_after_fee = convert(itr1->quote, price) * base.amount / precision(base.symbol.precision());
            //print("bid step1: quant_after_fee=",quant_after_fee);
            quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
            //print("bid step2: quant_after_fee=",quant_after_fee);
            action(
                    permission_level{ payer, N(active) },
                    config::token_account_name, N(transfer),
                    std::make_tuple(payer, escrow, quant_after_fee, std::string("send EOS fee to fee_account:"))
            ).send();
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
                        });
                        return;
                    }
                    if( base < itr->base ) { // full match
                        // eat the order
                        quant_after_fee = to_asset(config::token_account_name, base);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        quant_after_fee = convert(itr1->quote, itr->price) * base.amount / precision(base.symbol.precision());
                        quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        idx_orderbook.modify(itr, _self, [&]( auto& o ) {
                            o.base          -= convert(itr1->base, base);
                        });
                        // refund the difference to payer
                        if ( price > itr->price) {
                            quant_after_fee = convert(itr1->quote, price - itr->price) * base.amount / precision(base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            action(
                                    permission_level{ escrow, N(active) },
                                    config::token_account_name, N(transfer),
                                    std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();
                        }
                        return;
                    } else if ( base == itr->base ) { // full match
                        quant_after_fee = to_asset(config::token_account_name, base);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        quant_after_fee = convert(itr1->quote, itr->price) * base.amount / precision(base.symbol.precision());
                        quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        
                        // refund the difference to payer
                        if ( price > itr->price) {
                            quant_after_fee = convert(itr1->quote, price - itr->price) * base.amount / precision(base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            action(
                                    permission_level{ escrow, N(active) },
                                    config::token_account_name, N(transfer),
                                    std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();
                        }
                        idx_orderbook.erase(itr);
                        return;
                    } else { // partial match
                        quant_after_fee = to_asset(config::token_account_name, itr->base);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        quant_after_fee = convert(itr1->quote, itr->price) * itr->base.amount / precision(itr->base.symbol.precision()) ;
                        quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        base -= convert(itr1->base, itr->base);
                        // refund the difference to payer
                        if ( price > itr->price) {
                            quant_after_fee = convert(itr1->quote, price - itr->price) * itr->base.amount / precision(itr->base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            action(
                                    permission_level{ escrow, N(active) },
                                    config::token_account_name, N(transfer),
                                    std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();
                        }
                        idx_orderbook.erase(itr++);
                    }
                }
            };
            
            //auto lower = idx_orderbook.lower_bound( lookup_key );
            //auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
            //auto upper = idx_orderbook.upper_bound( upper_key );
            auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
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
                });
            } else {
                walk_table_range(lower, upper);
            }
        } else {
            // first, transfer the base currency to escrow account
            quant_after_fee = to_asset(config::token_account_name, base);
            action(
                    permission_level{ payer, N(active) },
                    config::token_account_name, N(transfer),
                    std::make_tuple(payer, escrow, quant_after_fee, std::string("send EOS fee to fee_account:"))
            ).send();
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
                        });
                        return;
                    }
                    
                    if( base < end_itr->base ) { // full match
                        // eat the order
                        quant_after_fee = convert(itr1->quote, price) * base.amount / precision(base.symbol.precision()) ;
                        quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                        from.value = escrow; to.value = payer;
                        print("\n 1: from=", from.to_string().c_str(), ", to=", to.to_string().c_str(),  ", amount=", quant_after_fee);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        quant_after_fee = to_asset(config::token_account_name, base);
                            from.value = escrow; to.value = end_itr->maker;
                        print("\n 2: from=", from.to_string().c_str(), ", to=", to.to_string().c_str(),  ", amount=", quant_after_fee);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, end_itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        idx_orderbook.modify(end_itr, _self, [&]( auto& o ) {
                            o.base          -= convert(itr1->base, base);
                        });
                        // refund the difference
                        if ( end_itr->price > price) {
                            quant_after_fee = convert(itr1->quote, end_itr->price - price) * base.amount / precision(base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            action(
                                    permission_level{ escrow, N(active) },
                                    config::token_account_name, N(transfer),
                                    std::make_tuple(escrow, end_itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();
                        }
                        
                        return;
                    } else if ( base == end_itr->base ) { // full match
                        quant_after_fee = convert(itr1->quote, price) * base.amount / precision(base.symbol.precision());
                        quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        quant_after_fee = to_asset(config::token_account_name, base);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, end_itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        // refund the difference
                        if ( end_itr->price > price) {
                            quant_after_fee = convert(itr1->quote, end_itr->price - price) * base.amount / precision(base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            action(
                                    permission_level{ escrow, N(active) },
                                    config::token_account_name, N(transfer),
                                    std::make_tuple(escrow, end_itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();
                        }
                        idx_orderbook.erase(end_itr);
                        return;
                    } else { // partial match
                        quant_after_fee = convert(itr1->quote, price) * end_itr->base.amount / precision(end_itr->base.symbol.precision());
                        quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, payer, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        quant_after_fee = to_asset(config::token_account_name, end_itr->base);
                        action(
                                permission_level{ escrow, N(active) },
                                config::token_account_name, N(transfer),
                                std::make_tuple(escrow, end_itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                        ).send();
                        base -= convert(itr1->base, end_itr->base);
                        // refund the difference
                        if ( end_itr->price > price) {
                            quant_after_fee = convert(itr1->quote, end_itr->price - price) * end_itr->base.amount / precision(end_itr->base.symbol.precision());
                            //print("bid step1: quant_after_fee=",quant_after_fee);
                            quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
                            //print("bid step2: quant_after_fee=",quant_after_fee);
                            action(
                                    permission_level{ escrow, N(active) },
                                    config::token_account_name, N(transfer),
                                    std::make_tuple(escrow, end_itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
                            ).send();
                        }
                        if (exit_on_done)
                            idx_orderbook.erase(end_itr);
                        else
                            idx_orderbook.erase(end_itr--);
                    }
                }
            };
            
            
            auto lower = idx_orderbook.lower_bound( lookup_key );
            auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
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
            quant_after_fee = convert(itr1->quote, itr->price) * itr->base.amount / precision(itr->base.symbol.precision());
            //print("bid step1: quant_after_fee=",quant_after_fee);
            quant_after_fee = to_asset(config::token_account_name, quant_after_fee);
            //print("bid step2: quant_after_fee=",quant_after_fee);
            action(
                    permission_level{ escrow, N(active) },
                    config::token_account_name, N(transfer),
                    std::make_tuple(escrow, itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
            ).send();
        } else {
            quant_after_fee = to_asset(config::token_account_name, itr->base);
            //print("bid step2: quant_after_fee=",quant_after_fee);
            action(
                    permission_level{ escrow, N(active) },
                    config::token_account_name, N(transfer),
                    std::make_tuple(escrow, itr->maker, quant_after_fee, std::string("send EOS fee to fee_account:"))
            ).send();
        }
        
        orders.erase(itr);

        return;
    }
        
}


EOSIO_ABI( exchange::exchange, (create)(trade)(cancel))
