/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "force.token.hpp"

namespace eosio {

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");
    eosio_assert( sym != CORE_SYMBOL, "not allow create core symbol token by token contract");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    sub_balance( from, quantity );
    add_balance( to, quantity, from );
}

void token::fee( account_name payer, asset quantity ){
   // account to get fee, TODO By FanYang : need use conf
   const auto fee_account = N(force.fee);

   require_auth( payer );

   auto sym = quantity.symbol.name();
   stats statstable( _self, sym );
   const auto& st = statstable.get( sym );

   eosio_assert( quantity.is_valid(), "invalid quantity" );
   eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
   eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

   sub_balance( payer, quantity );
   add_balance( fee_account, quantity, payer );
}

void token::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void token::add_balance( account_name owner, asset value, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::trade( account_name from,
                  account_name to,
                  asset quantity,
                  func_type type,
                  string memo ) {
   //eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");
   if (type == func_type::bridge_addmortgage && to == SYS_BRIDGE) {
      transfer(from, to,  quantity, memo);
      
      sys_bridge_addmort bri_add;
      bri_add.parse(memo);
      
      eosio::action(
            vector<eosio::permission_level>{{SYS_BRIDGE,N(active)}},
            SYS_BRIDGE,
            N(addmortgage),
            std::make_tuple(
                  bri_add.trade_name.value,bri_add.trade_maker,from,N(self),quantity,bri_add.type
            )
      ).send();
   }
   else if (type == func_type::bridge_exchange && to == SYS_BRIDGE) {
      transfer(from, to, quantity, memo);

      sys_bridge_exchange bri_exchange;
      bri_exchange.parse(memo);

      eosio::action(
            vector<eosio::permission_level>{{SYS_BRIDGE,N(active)}},
            SYS_BRIDGE,
            N(exchange),
            std::make_tuple(
                  bri_exchange.trade_name.value,bri_exchange.trade_maker,from,bri_exchange.recv,N(self),quantity,bri_exchange.type
            )
      ).send();
   }
   else if(type == func_type::match) {
      // account_name    escrow  = N(eosfund1);
      // eosio_assert(to == escrow, "to must be escrow");
      // transfer(from, to, chain, quantity, memo);
      // sys_match_match smm;
      // smm.parse(memo);
      // trade_imp(smm.payer, smm.receiver, smm.pair_id, quantity, smm.price, smm.bid_or_ask);
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

} /// namespace eosio

EOSIO_ABI( eosio::token, (create)(issue)(transfer)(fee)(trade) )
