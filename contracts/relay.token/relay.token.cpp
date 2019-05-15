//
// Created by fy on 2019-01-29.
//

#include "relay.token.hpp"
#include "force.relay/force.relay.hpp"
#include <eosiolib/action.hpp>
#include <string>
#include <stdlib.h>
#include <boost/algorithm/string.hpp>
//#include "force.system/force.system.hpp"

namespace relay {

// just a test version by contract
void token::on( name chain, const checksum256 block_id, const force::relay::action& act ) {
   require_auth(N(force.relay)); // TODO use config

   // TODO this ACTION should no err

   const auto data = unpack<token::action>(act.data);
   print("map ", name{ data.from }, " ", data.quantity, " ", data.memo, "\n");

   auto sym = data.quantity.symbol;
   stats statstable(_self, chain);
   auto st = statstable.find(sym.name());
   if( st == statstable.end() ){
      // TODO param err processing
      print("no token err");
      return;
   }

   if(    ( !sym.is_valid() )
       || ( data.memo.size() > 256 )
       || ( !data.quantity.is_valid() )
       || ( data.quantity.amount <= 0 )
       || ( data.quantity.symbol != st->supply.symbol )
       || ( data.quantity.amount > st->max_supply.amount - st->supply.amount )
       ) {
      // TODO param err processing
      print("token err");
      return;
   }

   if( data.memo.empty() || data.memo.size() >= 13 ){
      // TODO param err processing
      print("data.memo err");
      return;
   }
   const auto to = string_to_name(data.memo.c_str());
   if( !is_account(to) ) {
      // TODO param err processing
      print("to is no account");
      return;
   }

   SEND_INLINE_ACTION(*this, issue,
         { chain, N(active) }, { chain, to, data.quantity, "from chain" });
}

void token::create( account_name issuer,
                    name chain,
                    account_name side_account,
                    action_name side_action,
                    asset maximum_supply ) {
   require_auth(chain); // TODO if need

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
      s.side_account = side_account;
      s.side_action = side_action;
      s.reward_pool = asset(0);
      s.total_mineage = 0;
      s.total_mineage_update_height = current_block_num();
      reward_mine_info temp_remind;
      temp_remind.total_mineage = 0;
      temp_remind.reward_block_num = current_block_num();
      temp_remind.reward_pool = asset(0);
      s.reward_mine.push_back(temp_remind);
   });
}


void token::issue( name chain, account_name to, asset quantity, string memo ) {
   auto sym = quantity.symbol;
   eosio_assert(sym.is_valid(), "invalid symbol name");
   eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

   auto sym_name = sym.name();
   stats statstable(_self, chain);
   auto existing = statstable.find(sym.name());
   eosio_assert(existing != statstable.end(), "token with symbol does not exist, create token before issue");
   const auto& st = *existing;

   // TODO auth
   require_auth(st.issuer);

   eosio_assert(quantity.is_valid(), "invalid quantity");
   eosio_assert(quantity.amount > 0, "must issue positive quantity");

   eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
   eosio_assert(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

   auto current_block = current_block_num();

   statstable.modify(st, 0, [&]( auto& s ) {
      s.total_mineage += s.supply.amount * (current_block - s.total_mineage_update_height);
      s.total_mineage_update_height = current_block_num();
      s.supply += quantity;
   });
   add_balance(st.issuer, chain, quantity, st.issuer);

   if( to != st.issuer ) {
      SEND_INLINE_ACTION(*this, transfer, { st.issuer, N(active) }, { st.issuer, to, chain, quantity, memo });
   }
}

void token::destroy( name chain, account_name from, asset quantity, string memo ) {
   require_auth(from);

   auto sym = quantity.symbol;
   eosio_assert(sym.is_valid(), "invalid symbol name");
   eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

   auto sym_name = sym.name();
   stats statstable(_self, chain);
   auto existing = statstable.find(sym_name);
   eosio_assert(existing != statstable.end(), "token with symbol does not exist, create token before issue");
   const auto& st = *existing;

   eosio_assert(quantity.is_valid(), "invalid quantity");
   eosio_assert(quantity.amount > 0, "must issue positive quantity");

   eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
   eosio_assert(quantity.amount <= st.supply.amount, "quantity exceeds available supply");

   auto current_block = current_block_num();
   auto last_devidend_num = current_block - current_block % UPDATE_CYCLE;
   statstable.modify(st, 0, [&]( auto& s ) {
      s.total_mineage += s.supply.amount * (current_block - s.total_mineage_update_height);
      s.total_mineage_update_height = current_block;
      s.supply -= quantity;
   });

   sub_balance(from, chain, quantity);
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
   settle_user(owner,chain,value);
   accounts from_acnts(_self, owner);
   auto idx = from_acnts.get_index<N(bychain)>();
   auto from = idx.get(get_account_idx(chain, value), "no balance object found");
   eosio_assert(from.balance.amount >= value.amount, "overdrawn balance");
   eosio_assert(from.chain == chain, "symbol chain mismatch");
   auto current_block = current_block_num();
   auto last_devidend_num = current_block - current_block % UPDATE_CYCLE;

   from = idx.get(get_account_idx(chain, value), "no balance object found");
   from_acnts.modify(from, owner, [&]( auto& a ) {
      a.balance -= value;
   });

}

void token::add_balance( account_name owner, name chain, asset value, account_name ram_payer ) {
   accounts to_acnts(_self, owner);
   account_next_ids acntids(_self, owner);

   auto idx = to_acnts.get_index<N(bychain)>();

   auto to = idx.find(get_account_idx(chain, value));
   auto current_block = current_block_num();
   auto last_devidend_num = current_block - current_block % UPDATE_CYCLE;
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
         a.mineage = 0;
         a.mineage_update_height = current_block_num();
      });
   } else { 
      settle_user(owner,chain,value);
      accounts to_acnts_temp(_self, owner);
      idx = to_acnts_temp.get_index<N(bychain)>();
      to = idx.find(get_account_idx(chain, value));
      idx.modify(to, 0, [&]( auto& a ) {
         a.balance += value;
      });
      
   }
}


void token::settlemine(account_name system_account) {
   require_auth(::config::system_account_name);
   rewards rewardtable(_self, _self);
   auto current_block = current_block_num();
   for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
      stats statstable(_self, it->chain);
      auto existing = statstable.find(it->supply.symbol.name());
      if (existing != statstable.end()) {
         reward_mine_info temp_remind;
         temp_remind.total_mineage = existing->total_mineage + static_cast<int128_t>(existing->supply.amount) * (current_block - existing->total_mineage_update_height);
         temp_remind.reward_block_num = current_block;
         statstable.modify(*existing, 0, [&]( auto& s ) {
            s.reward_mine.push_back(temp_remind);
            s.total_mineage = 0;
            s.total_mineage_update_height = current_block;
         });
      }
   }
}

void token::activemine(account_name system_account) {
   require_auth(::config::system_account_name);
   rewards rewardtable(_self, _self);
   for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
      rewardtable.modify(*it, 0, [&]( auto& s ) {
         s.reward_now = true;
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
      
      auto trade_imp = [smm](account_name payer, account_name receiver, uint32_t pair_id, asset quantity, asset price2, uint32_t bid_or_ask, account_name exc_acc, string referer) {
         asset           quant_after_fee;
         asset           base;
         asset           price;
         
         exchange::exchange e(N(sys.match));
         auto base_sym = e.get_pair_base(pair_id);
         auto quote_sym = e.get_pair_quote(pair_id);
         //auto exc_acc = e.get_exchange_account(pair_id);
         
         if (bid_or_ask) {
            // to preserve precision
            quant_after_fee = convert(base_sym, quantity);
            //base = quant_after_fee * precision(price2.symbol.precision()) / price2.amount;
            base = quant_after_fee;
            print("after convert: quant_after_fee=", quant_after_fee, ", base=", base, "\n");
         } else {
            base = convert(base_sym, quantity);
         }
         price = convert(quote_sym, price2);
         
         print("\n before inline call sys.match --payer=",payer,", receiver=",receiver,", pair_id=",pair_id,", quantity=",quantity,", price=",price2,", bid_or_ask=",bid_or_ask, ", base=",quantity);
         
         eosio::action(
                 permission_level{ exc_acc, N(active) },
                 N(sys.match), N(match),
                 std::make_tuple(pair_id, payer, receiver, base, price, bid_or_ask, exc_acc, referer)
         ).send();
      };
      trade_imp(from, smm.receiver, smm.pair_id, quantity, smm.price, smm.bid_or_ask, smm.exc_acc, smm.referer);
   }
   else {
      eosio_assert(false,"invalid trade type");
   }
   
}

void token::addreward(name chain,asset supply,int32_t reward_now) {
   require_auth(_self);

   auto sym = supply.symbol;
   eosio_assert(sym.is_valid(), "invalid symbol name");

   stats statstable(_self, chain);
   auto existing = statstable.find(supply.symbol.name());
   eosio_assert(existing != statstable.end(), "token with symbol do not exists");

   // if (reward_now == 1) {
   //    statstable.modify(*existing, 0, [&]( auto& s ) {
   //       reward_mine_info temp_remind;
   //       temp_remind.total_mineage = 0;
   //       temp_remind.reward_block_num = 0;
   //       temp_remind.reward_pool = asset(0);
   //       s.reward_mine.push_back(temp_remind);
   //    });
   // }

   rewards rewardtable(_self, _self);
   auto idx = rewardtable.get_index<N(bychain)>();
   auto con = idx.find(get_account_idx(chain, supply));

   eosio_assert(con == idx.end(), "token with symbol already exists");

   rewardtable.emplace(_self, [&]( auto& s ) {
      s.id = rewardtable.available_primary_key();
      s.supply = supply;
      s.chain = chain;
      if (reward_now == 1){
         s.reward_now = true;
      }
      else {
         s.reward_now = false;
      }
   });
}

void token::rewardmine(asset quantity) {
   require_auth(::config::system_account_name);
   rewards rewardtable(_self, _self);
   exchange::exchange t(SYS_MATCH);
   uint128_t total_power = 0;
   for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
      stats statstable(_self, it->chain);
      auto existing = statstable.find(it->supply.symbol.name());
      eosio_assert(existing != statstable.end(), "token with symbol already exists");
      auto price = t.get_avg_price(current_block_num(),existing->chain,existing->supply.symbol).amount;
      total_power += existing->reward_mine[existing->reward_mine.size() - 1].total_mineage*price ;
   }

   if (total_power == 0) return ;
   for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
      stats statstable(_self, it->chain);
      auto existing = statstable.find(it->supply.symbol.name());
      eosio_assert(existing != statstable.end(), "token with symbol do not exists");
      auto price = t.get_avg_price(current_block_num(),existing->chain,existing->supply.symbol).amount;
      uint128_t devide_amount =  existing->reward_mine[existing->reward_mine.size() - 1].total_mineage * price * quantity.amount  / total_power;
      statstable.modify(*existing, 0, [&]( auto& s ) {
         s.reward_mine[s.reward_mine.size() - 1].reward_pool = asset(devide_amount);
      });
   }
}

//todo
void token::claim(name chain,asset quantity,account_name receiver) {

   require_auth(receiver);
   auto sym = quantity.symbol;
   eosio_assert(sym.is_valid(), "invalid symbol name");
   
   rewards rewardtable(_self, _self);
   auto idx = rewardtable.get_index<N(bychain)>();
   auto con = idx.find(get_account_idx(chain, quantity));
   eosio_assert(con != idx.end(), "token with symbol donot participate in dividends ");

   stats statstable(_self, chain);
   auto existing = statstable.find(quantity.symbol.name());
   eosio_assert(existing != statstable.end(), "token with symbol already exists");

   settle_user(receiver,chain,quantity);

   accounts to_acnts(_self, receiver);
   auto idxx = to_acnts.get_index<N(bychain)>();
   const auto& to = idxx.get(get_account_idx(chain, quantity), "no balance object found");
   eosio_assert(to.chain == chain, "symbol chain mismatch");
   
   auto total_reward = to.reward;

   to_acnts.modify(to, receiver, [&]( auto& a ) {
      a.reward = asset(0);
   });

   eosio_assert(total_reward > asset(100000),"claim amount must > 10");
   eosio::action(
           permission_level{ ::config::reward_account_name, N(active) },
           N(force.token), N(castcoin),
           std::make_tuple(::config::reward_account_name, receiver,total_reward)
   ).send();
}

void token::settle_user(account_name owner, name chain, asset value) {
   accounts from_acnts(_self, owner);
   auto idx = from_acnts.get_index<N(bychain)>();
   const auto& from = idx.get(get_account_idx(chain, value), "no balance object found");
   eosio_assert(from.chain == chain, "symbol chain mismatch");

   stats statstable(_self, chain);
   auto existing = statstable.find(value.symbol.name());
   eosio_assert(existing != statstable.end(), "settle wrong can not find stat");

   auto isize = existing->reward_mine.size();
   auto last_update_height = from.mineage_update_height;
   auto last_mineage = from.mineage;
   auto total_reward = asset(0);
   bool cross_day = false;
   for(int i=0;i!=isize;++i) {
      if (last_update_height < existing->reward_mine[i].reward_block_num) {
         auto mineage = last_mineage + from.balance.amount * (existing->reward_mine[i].reward_block_num - last_update_height);
         auto reward = existing->reward_mine[i].reward_pool * mineage / existing->reward_mine[i].total_mineage;
         
         total_reward += reward;
         statstable.modify(*existing, 0, [&]( auto& s ) {
            s.reward_mine[i].total_mineage -= mineage;
            s.reward_mine[i].reward_pool -= reward;
         });
         last_update_height = existing->reward_mine[i].reward_block_num;
         last_mineage = 0;
         cross_day = true;
      }
   }
   //是否跨天
   from_acnts.modify(from, 0, [&]( auto& a ) {
      a.reward += total_reward;
      if (cross_day){
         a.mineage = a.balance.amount * (current_block_num() - last_update_height);
      }
      else
      {
         a.mineage += a.balance.amount * (current_block_num() - last_update_height);
      }
      
      a.mineage_update_height = current_block_num();
   });

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

inline std::string trim(const std::string str) {
   auto s = str;
   s.erase(s.find_last_not_of(" ") + 1);
   s.erase(0, s.find_first_not_of(" "));
   
   return s;
}

asset asset_from_string(const std::string& from) {
   std::string s = trim(from);
   const char * str1 = s.c_str();

   // Find space in order to split amount and symbol
   const char * pos = strchr(str1, ' ');
   eosio_assert((pos != NULL), "Asset's amount and symbol should be separated with space");
   auto space_pos = pos - str1;
   auto symbol_str = trim(s.substr(space_pos + 1));
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
   // Parse amount
   int64_t int_part, fract_part;
   if (dot_pos != NULL) {
      int_part = ::atoll(amount_str.substr(0, dot_pos - str2).c_str());
      fract_part = ::atoll(amount_str.substr(dot_pos - str2 + 1).c_str());
   } else {
      int_part = ::atoll(amount_str.c_str());
      fract_part = 0;
   }
   int64_t amount = int_part * precision(precision_digits);
   amount += fract_part;

   return asset(amount, sym);
}

void sys_match_match::parse(const string memo) {
   using namespace boost;

   std::vector<std::string> memoParts;
   //splitMemo(memoParts, memo, ';');
   split( memoParts, memo, is_any_of( ";" ) );
   eosio_assert(memoParts.size() == 6,"memo is not adapted with sys_match_match");
   receiver    = ::eosio::string_to_name(memoParts[0].c_str());
   pair_id     = (uint32_t)atoi(memoParts[1].c_str());
   price       = asset_from_string(memoParts[2]);
   bid_or_ask  = (uint32_t)atoi(memoParts[3].c_str());
   exc_acc     = ::eosio::string_to_name(memoParts[4].c_str());
   referer     = memoParts[5];
   eosio_assert(bid_or_ask == 0 || bid_or_ask == 1,"type is not adapted with sys_match_match");
   print("-------sys_match_match::parse receiver=", ::eosio::name{.value=receiver}, ", pair_id=", pair_id, ", price=", price, " bid_or_ask=", bid_or_ask, " exc_acc=", ::eosio::name{.value=exc_acc}, " referer=", referer, "\n");
}

};

EOSIO_ABI(relay::token, (on)(create)(issue)(destroy)(transfer)(trade)(rewardmine)(addreward)(claim)(settlemine)(activemine))
