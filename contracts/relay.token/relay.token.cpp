//
// Created by fy on 2019-01-29.
//

#include "relay.token.hpp"
#include "force.relay/force.relay.hpp"
#include "sys.match/sys.match.hpp"
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
   });
}


void token::issue( name chain, account_name to, asset quantity, string memo ) {
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

   auto current_block = current_block_num();
   auto last_devidend_num = current_block - current_block % UPDATE_CYCLE;

   statstable.modify(st, 0, [&]( auto& s ) {
      if (s.total_mineage_update_height < last_devidend_num) {
         s.total_mineage += get_current_age(s.chain,s.supply,s.total_mineage_update_height,last_devidend_num) + s.total_pending_mineage;
         s.total_pending_mineage = get_current_age(s.chain,s.supply,last_devidend_num,current_block);
      }
      else {
         s.total_pending_mineage += get_current_age(s.chain,s.supply,s.total_mineage_update_height,current_block);
      }
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
      if (s.total_mineage_update_height < last_devidend_num) {
         s.total_mineage += get_current_age(s.chain,s.supply,s.total_mineage_update_height,last_devidend_num) + s.total_pending_mineage;
         s.total_pending_mineage = get_current_age(s.chain,s.supply,last_devidend_num,current_block);
      }
      else {
         s.total_pending_mineage += get_current_age(s.chain,s.supply,s.total_mineage_update_height,current_block);
      }
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

int64_t token::get_current_age(name chain,asset balance,int64_t first,int64_t last) {
   eosio_assert(first <= last,"wrong entering");
   if (first == last) return 0;

   exchange::exchange t(SYS_MATCH);
   auto interval_block = exchange::INTERVAL_BLOCKS;
   uint32_t first_index = first / interval_block,last_index = last / interval_block;
   if (first_index == last_index) {
      asset price = t.get_avg_price(last_index * interval_block + 1, chain,balance.symbol);
      return balance.amount * OTHER_COIN_WEIGHT / 10000 * (last - first) * price.amount / 10000;
   }
   else
   {
      auto temp_start = first;
      int64_t result = 0;
      for(uint32_t i = first_index +1;i!=last_index + 1;++i) {
         asset price = t.get_avg_price(last_index * interval_block + 1, chain,balance.symbol);
         result += balance.amount * OTHER_COIN_WEIGHT / 10000 * (i*interval_block - temp_start) * price.amount / 10000;
         temp_start = i*interval_block;
      }
      return result;
   }
   
}

void token::sub_balance( account_name owner, name chain, asset value ) {
   accounts from_acnts(_self, owner);

   auto idx = from_acnts.get_index<N(bychain)>();

   const auto& from = idx.get(get_account_idx(chain, value), "no balance object found");
   eosio_assert(from.balance.amount >= value.amount, "overdrawn balance");
   eosio_assert(from.chain == chain, "symbol chain mismatch");
   auto current_block = current_block_num();
   auto last_devidend_num = current_block - current_block % UPDATE_CYCLE;

   from_acnts.modify(from, owner, [&]( auto& a ) {
      if (a.mineage_update_height < last_devidend_num) {
         a.mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,last_devidend_num) + a.pending_mineage;
         a.pending_mineage = get_current_age(a.chain,a.balance,last_devidend_num,current_block);
      }
      else {
         a.pending_mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,current_block);
      }
      a.mineage_update_height = current_block;
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
         a.pending_mineage = 0;
         a.mineage_update_height = current_block_num();
      });
   } else { 
      idx.modify(to, 0, [&]( auto& a ) {
         if (a.mineage_update_height < last_devidend_num) {
            a.mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,last_devidend_num) + a.pending_mineage;
            a.pending_mineage = get_current_age(a.chain,a.balance,last_devidend_num,current_block);
         }
         else {
            a.pending_mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,current_block);
         }
         a.mineage_update_height = current_block_num();
         a.balance += value;
      });
   }
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
      SEND_INLINE_ACTION(*this, transfer, { from, N(active) }, { from, to, chain, quantity, memo });
   }
   else {
      eosio_assert(false,"invalid trade type");
   }
   
}

void token::addreward(name chain,asset supply) {
   require_auth(_self);

   auto sym = supply.symbol;
   eosio_assert(sym.is_valid(), "invalid symbol name");
   
   rewards rewardtable(_self, _self);
   auto idx = rewardtable.get_index<N(bychain)>();
   auto con = idx.find(get_account_idx(chain, supply));

   eosio_assert(con == idx.end(), "token with symbol already exists");

   rewardtable.emplace(_self, [&]( auto& s ) {
      s.id = rewardtable.available_primary_key();
      s.supply = supply;
      s.chain = chain;
   });
}

void token::rewardmine(asset quantity) {
   require_auth(::config::system_account_name);
   rewards rewardtable(_self, _self);
   uint64_t total_power = 0;
   for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
      stats statstable(_self, it->chain);
      auto existing = statstable.find(it->supply.symbol.name());
      eosio_assert(existing != statstable.end(), "token with symbol already exists");
      total_power += existing->supply.amount * OTHER_COIN_WEIGHT / 10000 ;
   }

   if (total_power == 0) return ;
   for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
      stats statstable(_self, it->chain);
      auto existing = statstable.find(it->supply.symbol.name());
      eosio_assert(existing != statstable.end(), "token with symbol already exists");
      uint64_t devide_amount = quantity.amount * existing->supply.amount * OTHER_COIN_WEIGHT / 10000 / total_power;
      statstable.modify(*existing, 0, [&]( auto& s ) {
         s.reward_pool += asset(devide_amount);
      });
   }
}

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

   auto reward_pool = existing->reward_pool;
   auto total_mineage = existing->total_mineage;
   auto total_mineage_update_height = existing->total_mineage_update_height;
   auto total_pending_mineage = existing->total_pending_mineage;
   auto supply = existing->supply;

   accounts to_acnts(_self, receiver);
   auto idxx = to_acnts.get_index<N(bychain)>();
   const auto& to = idxx.get(get_account_idx(chain, quantity), "no balance object found");
   eosio_assert(to.chain == chain, "symbol chain mismatch");
   auto current_block = current_block_num();
   auto last_devidend_num = current_block - current_block % UPDATE_CYCLE;

   auto power = to.mineage;
   if (to.mineage_update_height < last_devidend_num) {
      power = to.mineage + get_current_age(to.chain,to.balance,to.mineage_update_height,last_devidend_num) + to.pending_mineage ;
   }

   eosio_assert(power != 0, "the devident is zreo");

   auto total_power = total_mineage;
   if (total_mineage_update_height < last_devidend_num) {
      total_power = total_mineage + get_current_age(existing->chain,supply,total_mineage_update_height,last_devidend_num) + total_pending_mineage;
   }
   int128_t reward_temp = static_cast<int128_t>(reward_pool.amount) * power;
   auto total_reward = asset(static_cast<int64_t>(reward_temp / total_power));
   statstable.modify( existing,0,[&](auto &st) {
      st.reward_pool -= total_reward;
      st.total_mineage = total_power - power;
      
      if (st.total_mineage_update_height < last_devidend_num) {
         st.total_mineage += st.total_pending_mineage;
         st.total_pending_mineage = 0;
      }
      st.total_mineage_update_height = last_devidend_num;
   });

   to_acnts.modify(to, receiver, [&]( auto& a ) {
      a.mineage = 0;
      if (a.mineage_update_height < last_devidend_num) {
         a.pending_mineage = get_current_age(a.chain,a.balance,last_devidend_num,current_block);
      }
      else {
         a.pending_mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,current_block);
      }
      a.mineage_update_height = current_block;
   });

   eosio_assert(total_reward > asset(100000),"claim amount must > 10");
   eosio::action(
           permission_level{ ::config::reward_account_name, N(active) },
           N(force.token), N(castcoin),
           std::make_tuple(::config::reward_account_name, receiver,total_reward)
   ).send();
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

};

EOSIO_ABI(relay::token, (on)(create)(issue)(destroy)(transfer)(trade)(rewardmine)(addreward)(claim))
