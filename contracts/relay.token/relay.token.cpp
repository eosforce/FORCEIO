//
// Created by fy on 2019-01-29.
//

#include "relay.token.hpp"
#include "force.relay/force.relay.hpp"
#include <eosiolib/action.hpp>



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

void token::trade( account_name from,
                  account_name to,
                  name chain,
                  asset quantity,
                  trade_type type,
                  string memo ) {
   eosio_assert(from != to, "cannot trade to self");
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
   //eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");
   //解析memo 调用market
   if (type == trade_type::bridge_addmortgage && to == SYS_BRIDGE) {
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

   }
   else {
      eosio_assert(false,"invalid type");
   }
   
   sub_balance(from, chain, quantity);
   add_balance(to, chain, quantity, from);
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

EOSIO_ABI(relay::token, (on)(create)(issue)(transfer)(trade))
