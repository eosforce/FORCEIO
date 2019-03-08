//
// Created by fy on 2019-01-29.
//

#include "relay.token.hpp"
#include "force.relay/force.relay.hpp"
#include "sys.match/sys.match.hpp"
#include <eosiolib/action.hpp>
#include <string>
#include <stdlib.h>

namespace relay {

// just a test version by contract
void token::on( const name chain, const checksum256 block_id, const force::relay::action& act ) {
   // TODO check account

   // TODO create accounts from diff chain

   // Just send account
   print("on ", name{ act.account }, " ", name{ act.name }, "\n");
   const auto data = unpack<token::action>(act.data);

   print("map ", name{ data.from }, " ", data.quantity, " ", data.memo, "\n");

   SEND_INLINE_ACTION(*this, issue,
         { N(eosforce), N(active) },
         { chain, data.from, data.quantity, data.memo });
}

void token::create( account_name issuer,
                    name chain,
                    asset maximum_supply ) {
   require_auth(N(eosforce));

   auto sym = maximum_supply.symbol;
   eosio_assert(sym.is_valid(), "invalid symbol name");
   eosio_assert(maximum_supply.is_valid(), "invalid supply");
   eosio_assert(maximum_supply.amount > 0, "max-supply must be positive");

   stats statstable(_self, chain);
   auto existing = statstable.find(sym.name());
   eosio_assert(existing == statstable.end(), "token with symbol already exists");

   statstable.emplace(_self, [&]( auto& s ) {
      s.supply.symbol = maximum_supply.symbol;
      s.max_supply = maximum_supply;
      s.issuer = issuer;
      s.chain = chain;
   });
}


void token::issue( const name chain, account_name to, asset quantity, string memo ) {
   auto sym = quantity.symbol;
   eosio_assert(sym.is_valid(), "invalid symbol name");
   eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

   auto sym_name = sym.name();
   stats statstable(_self, chain);
   auto existing = statstable.find(sym_name);
   eosio_assert(existing != statstable.end(), "token with symbol does not exist, create token before issue");
   const auto& st = *existing;

   // TODO auth
   require_auth(st.issuer);

   eosio_assert(quantity.is_valid(), "invalid quantity");
   eosio_assert(quantity.amount > 0, "must issue positive quantity");

   eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
   eosio_assert(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

   statstable.modify(st, 0, [&]( auto& s ) {
      s.supply += quantity;
   });

   add_balance(st.issuer, chain, quantity, st.issuer);

   if( to != st.issuer ) {
      SEND_INLINE_ACTION(*this, transfer, { st.issuer, N(active) }, { st.issuer, to, chain, quantity, memo });
   }
}

void token::transfer( account_name from,
                      account_name to,
                      name chain,
                      asset quantity,
                      string memo ) {
   eosio_assert(from != to, "cannot transfer to self");
   require_auth(from);
   eosio_assert(is_account(to), "to account does not exist");
   auto sym = quantity.symbol.name();
   stats statstable(_self, chain);
   const auto& st = statstable.get(sym);

   require_recipient(from);
   require_recipient(to);

   eosio_assert(quantity.is_valid(), "invalid quantity");
   eosio_assert(quantity.amount > 0, "must transfer positive quantity");
   eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
   eosio_assert(chain == st.chain, "symbol chain mismatch");
   eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");


   sub_balance(from, chain, quantity);
   add_balance(to, chain, quantity, from);
}

void token::sub_balance( account_name owner, name chain, asset value ) {
   accounts from_acnts(_self, owner);

   auto idx = from_acnts.get_index<N(bychain)>();

   const auto& from = idx.get(get_account_idx(chain, value), "no balance object found");
   eosio_assert(from.balance.amount >= value.amount, "overdrawn balance");
   eosio_assert(from.chain == chain, "symbol chain mismatch");

   if( from.balance.amount == value.amount ) {
      from_acnts.erase(from);
   } else {
      from_acnts.modify(from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
   }
}

void token::add_balance( account_name owner, name chain, asset value, account_name ram_payer ) {
   accounts to_acnts(_self, owner);
   account_next_ids acntids(_self, owner);

   auto idx = to_acnts.get_index<N(bychain)>();

   auto to = idx.find(get_account_idx(chain, value));
   if( to == idx.end() ) {
      uint64_t id = 1;
      auto ids = acntids.find(owner);
      if(ids == acntids.end()){
         acntids.emplace(ram_payer, [&]( auto& a ){
            a.id = 2;
            a.account = owner;
         });
      }else{
         id = ids->id;
         acntids.modify(ids, 0, [&]( auto& a ) {
            a.id++;
         });
      }

      to_acnts.emplace(ram_payer, [&]( auto& a ) {
         a.id = id;
         a.balance = value;
         a.chain = chain;
      });
   } else {
      idx.modify(to, 0, [&]( auto& a ) {
         a.balance += value;
      });
   }
}

int64_t precision(uint64_t decimals)
{
   int64_t p10 = 1;
   int64_t p = (int64_t)decimals;
   while( p > 0  ) {
      p10 *= 10; --p;
   }
   return p10;
}

asset convert( symbol_type expected_symbol, const asset& a ) {
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

asset to_asset( account_name code, name chain, const asset& a ) {
   asset b;
   relay::token t(code);
   
   symbol_type expected_symbol = t.get_supply(chain, a.symbol.name()).symbol ;

   b = convert(expected_symbol, a);
   return b;
}
    
asset convert_asset( symbol_type expected_symbol, const asset& a ) {
   auto b = asset( a.amount, expected_symbol );
   return b;
}

void token::trade_imp( account_name payer, account_name receiver, uint32_t pair_id, asset quantity, asset price2, uint32_t bid_or_ask ) {
    //account_name    exc_acc;// = N(sys.match);
    //account_name    escrow  = N(eosfund1);
    //const account_name relay_token_acc = N(relay.token);
    
    //exchange::exchange::trading_pairs   trading_pairs_table(exc_acc, exc_acc);
    asset           quant_after_fee;
    asset            base;
    asset           price;

    // test. walk through trading_pairs table
     /*   {
            print("\n ---------------- begin to trading_pairs table: ----------------");
            auto walk_table_range = [&]( auto itr, auto end_itr ) {
                for( ; itr != end_itr; ++itr ) {
                    print("\n pair: id=", itr->id);
                }
            };
            //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
            auto lower_key = std::numeric_limits<uint64_t>::lowest();
            auto lower = trading_pairs_table.lower_bound( lower_key );
            //auto upper_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::max();
            auto upper_key = std::numeric_limits<uint64_t>::max();
            auto upper = trading_pairs_table.upper_bound( upper_key );
            walk_table_range(lower, upper);
            print("\n -------------------- walk through trading_pairs table ends ----------------:");
        }*/

    //uint128_t idxkey = (uint128_t(base_sym.name()) << 64) | price.symbol.name();

    

    //auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
    //auto itr1 = trading_pairs_table.find(pair_id);
    //eosio_assert(itr1 != trading_pairs_table.end(), "trading pair does not exist");

    exchange::exchange e(N(sys.match));
    auto base_sym = e.get_pair_base(pair_id);
    auto quote_sym = e.get_pair_quote(pair_id);
    auto exc_acc = e.get_exchange_account(pair_id);
    
    //price   = convert(itr1->quote, price);
    if (bid_or_ask) {
        int64_t amount = (quantity.amount / precision(quantity.symbol.precision())) / (price2.amount / precision(price2.symbol.precision()));
        // first, transfer the quote currency to escrow account
        //print("bid step0: quant_after_fee=",convert(itr1->quote, price)," base.amount =",base.amount, " precision =",precision(base.symbol.precision()));
        //quant_after_fee = exchange::exchange::convert(itr1->quote_sym, price) * base.amount / exchange::exchange::precision(base.symbol.precision());
         print("\n2222222 quantity.amount=", quantity.amount, ", quantity.symbol.precision=", precision(quantity.symbol.precision()));
        print("\n22222221111 price2.amount=", price2.amount, ", price2.symbol.precision()=", precision(price2.symbol.precision()));
        print("\n33333333 base_sym=", base_sym, ", amount=", amount);
        symbol_type sym;
        sym.value = base_sym.value & 0XFFFFFFFFFFFFFF00;
        quant_after_fee = asset(amount, sym);
        //quant_after_fee = convert(itr1->quote_sym, quant_after_fee);
        base = convert(base_sym, quant_after_fee);
        print("\n4444444 quant_after_fee=", quant_after_fee, ", base=", base);
       
        //print("bid step1: quant_after_fee=",quant_after_fee);
        //quant_after_fee = to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
        //print("bid step2: quant_after_fee=",quant_after_fee);
    } else {
        base = convert(base_sym, quantity);
    }
    price = convert(quote_sym, price2);
    
    print("\n before inline call sys.match --payer=",payer,", receiver=",receiver,", pair_id=",pair_id,", quantity=",quantity,", price=",price,", bid_or_ask=",bid_or_ask, ", base=",base);
    
    eosio::action(
            permission_level{ exc_acc, N(active) },
            N(sys.match), N(match),
            std::make_tuple(payer, receiver, base, price, bid_or_ask)
    ).send();
}

void token::trade( account_name from,
                  account_name to,
                  name chain,
                  asset quantity,
                  trade_type type,
                  string memo ) {
   //eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");
   if (type == trade_type::bridge_addmortgage && to == SYS_BRIDGE) {
      transfer(from, to, chain, quantity, memo);
      
      sys_bridge_addmort bri_add;
      bri_add.parse(memo);
      
      eosio::action(
            vector<eosio::permission_level>{{SYS_BRIDGE,N(active)}},
            SYS_BRIDGE,
            N(addmortgage),
            std::make_tuple(
                  bri_add.trade_name.value,bri_add.trade_maker,from,chain,quantity,bri_add.type
            )
      ).send();
   }
   else if (type == trade_type::bridge_exchange && to == SYS_BRIDGE) {
      transfer(from, to, chain, quantity, memo);

      sys_bridge_exchange bri_exchange;
      bri_exchange.parse(memo);

      eosio::action(
            vector<eosio::permission_level>{{SYS_BRIDGE,N(active)}},
            SYS_BRIDGE,
            N(exchange),
            std::make_tuple(
                  bri_exchange.trade_name.value,bri_exchange.trade_maker,from,bri_exchange.recv,chain,quantity,bri_exchange.type
            )
      ).send();
   }
   else if(type == trade_type::match && to == N(sys.match)) {
      transfer(from, to, chain, quantity, memo);
      sys_match_match smm;
      smm.parse(memo);
      trade_imp(smm.payer, smm.receiver, smm.pair_id, quantity, smm.price, smm.bid_or_ask);
   }
   else {
      eosio_assert(false,"invalid trade type");
   }
   
   
}

void splitMemo(std::vector<std::string>& results, const std::string& memo,char separator) {
   auto start = memo.cbegin();
   auto end = memo.cend();

   for (auto it = start; it != end; ++it) {
     if (*it == separator) {
       results.emplace_back(start, it);
       start = it + 1;
     }
   }
   if (start != end) results.emplace_back(start, end);
}
void sys_bridge_addmort::parse(const string memo) {
   std::vector<std::string> memoParts;
   splitMemo(memoParts, memo, ';');
   eosio_assert(memoParts.size() == 3,"memo is not adapted with bridge_addmortgage");
   this->trade_name.value = ::eosio::string_to_name(memoParts[0].c_str());
   this->trade_maker = ::eosio::string_to_name(memoParts[1].c_str());
   this->type = atoi(memoParts[2].c_str());
   eosio_assert(this->type == 1 || this->type == 2,"type is not adapted with bridge_addmortgage");
}

void sys_bridge_exchange::parse(const string memo) {
   std::vector<std::string> memoParts;
   splitMemo(memoParts, memo, ';');
   eosio_assert(memoParts.size() == 4,"memo is not adapted with bridge_addmortgage");
   this->trade_name.value = ::eosio::string_to_name(memoParts[0].c_str());
   this->trade_maker = ::eosio::string_to_name(memoParts[1].c_str());
   this->recv = ::eosio::string_to_name(memoParts[2].c_str());
   this->type = atoi(memoParts[3].c_str());
   eosio_assert(this->type == 1 || this->type == 2,"type is not adapted with bridge_addmortgage");
}

inline std::string ltrim(std::string str) {
    auto str1 = str;
    std::string::iterator p = std::find_if(str1.begin(), str1.end(), not1(std::ptr_fun<int, int>(isspace)));
    str.erase(str1.begin(), p);
    return str1;
}
 
inline std::string rtrim(std::string str) {
    auto str1 = str;
    std::string::reverse_iterator p = std::find_if(str1.rbegin(), str1.rend(), not1(std::ptr_fun<int , int>(isspace)));
    str.erase(p.base(), str1.end());
    return str1;
}

inline std::string trim(const std::string str) {
    auto str1 = str;
    ltrim(rtrim(str1));
    return str1;
}
/*
asset asset_from_string(const std::string& from)
{
    std::string s = trim(from);

    // Find space in order to split amount and symbol
    auto space_pos = s.find(' ');
    eosio_assert((space_pos != std::string::npos), "Asset's amount and symbol should be separated with space");
    auto symbol_str = trim(s.substr(space_pos + 1));
    auto amount_str = s.substr(0, space_pos);
    eosio_assert((amount_str[0] != '-'), "now do not support negetive asset");

    // Ensure that if decimal point is used (.), decimal fraction is specified
    auto dot_pos = amount_str.find('.');
    if (dot_pos != std::string::npos) {
       eosio_assert((dot_pos != amount_str.size() - 1), "Missing decimal fraction after decimal point");
    }

    // Parse symbol
    uint32_t precision_digits;
    if (dot_pos != std::string::npos) {
       precision_digits = amount_str.size() - dot_pos - 1;
    } else {
       precision_digits = 0;
    }

    symbol_type sym;
    sym.value = (::eosio::string_to_name(symbol_str.c_str()) << 8) | (uint8_t)precision_digits;

    // Parse amount
    int64_t int_part, fract_part;
    if (dot_pos != string::npos) {
       int_part = ::atoll(amount_str.substr(0, dot_pos).c_str());
       fract_part = ::atoll(amount_str.substr(dot_pos + 1).c_str());
    } else {
       int_part = ::atoll(amount_str.c_str());
       fract_part = 0;
    }

    int64_t amount = int_part * exchange::exchange::precision(precision_digits);
    amount += fract_part;

    return asset(amount, sym);
}
*/

asset asset_from_string(const std::string& from)
{
    //std::string s = trim(from);
    std::string s = from;
    const char * str1 = s.c_str();

    // Find space in order to split amount and symbol
    const char * pos = strchr(str1, ' ');
    eosio_assert((pos != NULL), "Asset's amount and symbol should be separated with space");
    auto space_pos = pos - str1;
    //auto symbol_str = trim(s.substr(space_pos + 1));
    auto symbol_str = s.substr(space_pos + 1);
    auto amount_str = s.substr(0, space_pos);
    eosio_assert((amount_str[0] != '-'), "now do not support negetive asset");

    // Ensure that if decimal point is used (.), decimal fraction is specified
    const char * str2 = amount_str.c_str();
    const char *dot_pos = strchr(str2, '.');
    if (dot_pos != NULL) {
       eosio_assert((dot_pos - str2 != amount_str.size() - 1), "Missing decimal fraction after decimal point");
    }
 print("------asset_from_string: symbol_str=",symbol_str, ", amount_str=",amount_str, "\n");
    // Parse symbol
    uint32_t precision_digits;
    if (dot_pos != NULL) {
       precision_digits = amount_str.size() - (dot_pos - str2 + 1);
    } else {
       precision_digits = 0;
    }

    symbol_type sym;
    sym.value = ::eosio::string_to_symbol((uint8_t)precision_digits, symbol_str.c_str());
 print("----333--asset_from_string: sym=", sym, "\n");
    // Parse amount
    int64_t int_part, fract_part;
    if (dot_pos != NULL) {
       int_part = ::atoll(amount_str.substr(0, dot_pos - str2).c_str());
       fract_part = ::atoll(amount_str.substr(dot_pos - str2 + 1).c_str());
    } else {
       int_part = ::atoll(amount_str.c_str());
       fract_part = 0;
    }
 print("----444--asset_from_string: int_part=", int_part, "precision_digits=", precision_digits, "\n");
    int64_t amount = int_part * precision(precision_digits);
    amount += fract_part;

    return asset(amount, sym);
}

void sys_match_match::parse(const string memo) {
   std::vector<std::string> memoParts;
   splitMemo(memoParts, memo, ';');
   eosio_assert(memoParts.size() == 5,"memo is not adapted with sys_match_match");
   payer = ::eosio::string_to_name(memoParts[0].c_str());
   receiver = ::eosio::string_to_name(memoParts[1].c_str());
   pair_id = (uint32_t)atoi(memoParts[2].c_str());
      print("------1111");
   price = asset_from_string(memoParts[3]);
   print("------2222");
   bid_or_ask = (uint32_t)atoi(memoParts[4].c_str());
   eosio_assert(bid_or_ask == 0 || bid_or_ask == 1,"type is not adapted with sys_match_match");
   print("-------sys_match_match::parse payer=", payer, ", receiver=", receiver, ", pair_id=", pair_id, ", price=", price, " bid_or_ask=", bid_or_ask, "\n");
}

};

EOSIO_ABI(relay::token, (on)(create)(issue)(transfer)(trade))
