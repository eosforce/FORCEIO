/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   struct sys_bridge_addmort {
      name trade_name;
      account_name trade_maker;
      uint64_t type;
      void parse(const string memo);
   };

   struct sys_bridge_exchange {
      name trade_name;
      account_name trade_maker;
      account_name recv;
      uint64_t type;
      void parse(const string memo);
   };

   enum  class func_type:uint64_t {
      match=1,
      bridge_addmortgage,
      bridge_exchange,
      trade_type_count
   };

   const account_name SYS_BRIDGE = N(sys.bridge);
   static constexpr uint32_t UPDATE_CYCLE = 42;
   static constexpr uint32_t PRE_CAST_NUM = 5184000;
   static constexpr uint32_t STABLE_CAST_NUM = 1209600;
   static constexpr uint32_t WEAKEN_CAST_NUM = 161;

   class token : public contract {
      public:
         token( account_name self ):contract(self){}

         void create( account_name issuer,
                      asset        maximum_supply);

         void issue( account_name to, asset quantity, string memo );

         void transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo );

         void fee( account_name payer, asset quantity );
      
      
         inline asset get_supply( symbol_name sym )const;
         
         inline asset get_balance( account_name owner, symbol_name sym )const;

         void trade( account_name   from,
                     account_name   to,
                     asset          quantity,
                     func_type      type,
                     string           memo);
         //进入铸币池
         void castcoin(account_name from,account_name to,asset quantity);
         //领取铸币池中的币
         void takecoin(account_name to);

      private:
         struct account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.name(); }
         };

         struct currency_stats {
            asset          supply;
            asset          max_supply;
            account_name   issuer;

            uint64_t primary_key()const { return supply.symbol.name(); }
         };

         struct coin_cast {
            asset    balance = asset(0);
            uint32_t   finish_block = 0;

            uint64_t primary_key()const { return static_cast<uint64_t>(finish_block); }
         };

         typedef eosio::multi_index<N(accounts), account> accounts;
         typedef eosio::multi_index<N(stat), currency_stats> stats;
         typedef eosio::multi_index<N(coincast), coin_cast> coincasts;

         void sub_balance( account_name owner, asset value );
         void add_balance( account_name owner, asset value, account_name ram_payer );

      public:
         struct transfer_args {
            account_name  from;
            account_name  to;
            asset         quantity;
            string        memo;
         };
   };

   asset token::get_supply( symbol_name sym )const
   {
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );
      return st.supply;
   }

   asset token::get_balance( account_name owner, symbol_name sym )const
   {
      accounts accountstable( _self, owner );
      const auto& ac = accountstable.get( sym );
      return ac.balance;
   }

} /// namespace eosio
