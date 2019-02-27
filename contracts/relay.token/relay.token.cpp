//
// Created by fy on 2019-01-29.
//

#include "relay.token.hpp"
#include "force.relay/force.relay.hpp"
#include "sys.match/exchange.h"
#include <eosiolib/action.hpp>
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

void token::trade_imp( account_name payer, account_name receiver, asset base, asset price, uint32_t bid_or_ask ) {
    account_name    exc_acc = N(sys.match);
    account_name    escrow  = N(eosfund1);
    const account_name relay_token_acc = N(relay.token);
    
    exchange::exchange::trading_pairs   trading_pairs_table(exc_acc, exc_acc);
    asset           quant_after_fee;

    uint128_t idxkey = (uint128_t(base.symbol.name()) << 64) | price.symbol.name();

    //print("idxkey=",idxkey,",contract=",token_contract,",symbol=",token_symbol.value);

    auto idx_pair = trading_pairs_table.template get_index<N(idxkey)>();
    auto itr1 = idx_pair.find(idxkey);
    eosio_assert(itr1 != idx_pair.end(), "trading pair does not exist");

    base    = exchange::exchange::convert(itr1->base, base);
    price   = exchange::exchange::convert(itr1->quote, price);
    if (bid_or_ask) {
        // first, transfer the quote currency to escrow account
        //print("bid step0: quant_after_fee=",convert(itr1->quote, price)," base.amount =",base.amount, " precision =",precision(base.symbol.precision()));
        quant_after_fee = exchange::exchange::convert(itr1->quote_sym, price) * base.amount / exchange::exchange::precision(base.symbol.precision());
        //print("bid step1: quant_after_fee=",quant_after_fee);
        quant_after_fee = exchange::exchange::to_asset(relay_token_acc, itr1->quote_chain, quant_after_fee);
        //print("bid step2: quant_after_fee=",quant_after_fee);
        transfer( payer, escrow, itr1->quote_chain, quant_after_fee, "");
    } else {
        quant_after_fee = exchange::exchange::convert_asset(itr1->base_sym, base);
        quant_after_fee = exchange::exchange::to_asset(relay_token_acc, itr1->base_chain, quant_after_fee);
        transfer( payer, escrow, itr1->base_chain, quant_after_fee, "");
    }
    
    eosio::action(
            permission_level{ payer, N(active) },
            exc_acc, N(match),
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
                  bri_add.trade_name.value,bri_add.trade_maker,from,quantity,bri_add.type
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
                  bri_exchange.trade_name.value,bri_exchange.trade_maker,from,bri_exchange.recv,quantity,bri_exchange.type
            )
      ).send();
   }
   else if(type == trade_type::match && to == N(sys.match)) {
      sys_match_match smm;
      smm.parse(memo);
      trade_imp(smm.payer, smm.receiver, smm.base, smm.price, smm.bid_or_ask);
   }
   else {
      eosio_assert(false,"invalid type");
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

inline string ltrim(string str) {
    auto str1 = str;
    string::iterator p = find_if(str1.begin(), str1.end(), not1(std::ptr_fun<int, int>(isspace)));
    str.erase(str1.begin(), p);
    return str1;
}
 
inline string rtrim(string str) {
    auto str1 = str;
    string::reverse_iterator p = find_if(str1.rbegin(), str1.rend(), not1(std::ptr_fun<int , int>(isspace)));
    str.erase(p.base(), str1.end());
    return str1;
}

inline string trim(const string str) {
    auto str1 = str;
    ltrim(rtrim(str1));
    return str1;
}

asset asset_from_string(const string& from)
{
    string s = trim(from);

    // Find space in order to split amount and symbol
    auto space_pos = s.find(' ');
    eosio_assert((space_pos != string::npos), "Asset's amount and symbol should be separated with space");
    auto symbol_str = trim(s.substr(space_pos + 1));
    auto amount_str = s.substr(0, space_pos);
    eosio_assert((amount_str[0] != '-'), "now do not support negetive asset");

    // Ensure that if decimal point is used (.), decimal fraction is specified
    auto dot_pos = amount_str.find('.');
    if (dot_pos != string::npos) {
       eosio_assert((dot_pos != amount_str.size() - 1), "Missing decimal fraction after decimal point");
    }

    // Parse symbol
    uint32_t precision_digits;
    if (dot_pos != string::npos) {
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

void sys_match_match::parse(const string memo) {
   std::vector<std::string> memoParts;
   splitMemo(memoParts, memo, ';');
   eosio_assert(memoParts.size() == 5,"memo is not adapted with bridge_addmortgage");
   payer = ::eosio::string_to_name(memoParts[0].c_str());
   receiver = ::eosio::string_to_name(memoParts[1].c_str());
   base = asset_from_string(memoParts[2]);
   price = asset_from_string(memoParts[3]);
   bid_or_ask = (uint32_t)atoi(memoParts[4].c_str());
   eosio_assert(bid_or_ask == 0 || bid_or_ask == 1,"type is not adapted with sys_match_match");
}

};

EOSIO_ABI(relay::token, (on)(create)(issue)(transfer)(trade))
