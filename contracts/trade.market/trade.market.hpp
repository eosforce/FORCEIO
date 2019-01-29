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

   const int64_t PROPORTION_CARD = 10000;  

   enum  class fee_type:int64_t {
      fixed=1,                //fixed
      proportion,             // proportion
      proportion_mincost,      // proportion and have a Minimum cost
      fee_type_count
   };

   enum  class trade_type:int64_t {
      equal_ratio=1,
      bancor,
      trade_type_count
   };

   enum  class coin_type:int64_t {
      coin_base=1,
      coin_market,
      trade_type_count
   };

   class market : public contract {
      public:
         market( account_name self ):contract(self){}
         /**
          * add a trade market 
          * trade : the name of the trade market
          * trade_maker : the account make the trade market
          * type : the trade type 1 for Fixed ratio  2 for bancor(Not yet implemented)
          * base_amount : the base coin amount 
          * base_account : the account to recv the basecoin when you claim from the trade market (when you add a trade market you should pay some base coin from this account)
          * base_weight : the base coin weight to calculate exchange rate
          * market_amount : the market coin amount 
          * market_account : the account to recv the marketcoin when you claim from the trade market (when you add a trade market you should pay some market coin from this account)
          * market_weight : the market coin weight to calculate exchange rate
          */
         void addmarket(name trade,account_name trade_maker,trade_type type,asset base_amount,account_name base_account,uint64_t base_weight,
               asset market_amount,account_name market_account,uint64_t market_weight);
         /**
          * add mortgage
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          * recharge_account : the account to pay the coin 
          * recharge_amount ： the amount you add mortgage
          * type : to distinguish the base coin or the market coin 1 for base coin 2 for market coin
          */
         void addmortgage(name trade,account_name trade_maker,account_name recharge_account,asset recharge_amount,coin_type type);
         /**
          * claim mortgage
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          * claim_amount ： the amount you claim the mortgage
          * type : to distinguish the base coin or the market coin 1 for base coin 2 for market coin
          */
         void claimmortgage(name trade,account_name market_maker,asset claim_amount,coin_type type);
          /**
          * exchange the client use this function for exchange two coins
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          * account_covert ： the account to pay the coin
          * account_recv : the account to recv the coin
          * amount : the amount you pay the coin 
          * type : to distinguish the exchange 1 for you pay basecoin and recv the market coin  AND 2 for you pay the market coin and recv the base coin
          */
         void exchange(name trade,account_name trade_maker,account_name account_covert,account_name account_recv,asset amount,coin_type type);
         /**
          * forzen market : for the trade maker to frozen the trade market . if the market is frozened it could not use exchage function
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          */
         void frozenmarket(name trade,account_name trade_maker);
         /**
          * thaw market : for the trade maker to thaw the trade market 
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          */
         void trawmarket(name trade,account_name trade_maker);
         /**
          * set fixed fee
          */
         void setfixedfee(name trade,account_name trade_maker,asset base,asset market);
         /**
          * set proportion fee
          */
         void setprofee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio);
         /**
          * set proportion with a Minimum fee
          */
         void setprominfee(name trade,account_name trade_maker,uint64_t base_ratio,uint64_t market_ratio,asset base,asset market);
      private:
         //fixed cost      think about the Proportionate fee
         struct trade_fee {
            asset base;             //fixed
            asset market;
            
            uint64_t    base_ratio;    //Proportionate
            uint64_t    market_ratio;

            fee_type  fee_type;
            
         };

         struct coin {
            asset  amount;             //the coin amont
            account_name   coin_maker; //the account to recv the coin when claim mortgage
         };

         struct trade_pair {          
            name trade_name;          //the name of the trade market
            trade_type type;          // the trade type 1 for Fixed ratio  2 for bancor(Not yet implemented)
            coin  base;               //the base coin 
            coin  market;             //the market coin
            account_name   trade_maker;//the account to pay for the trade market
            uint64_t  base_weight;      //coin_base weight 
            uint64_t  market_weight;      //coin_base weight 
            bool  isactive =true;      //the sattus of the trade market when isactive is false ,it can exchange
            trade_fee fee;

            uint64_t primary_key()const { return trade_name; }
         };
         
         typedef eosio::multi_index<N(tradepairs), trade_pair> tradepairs;
   };

   EOSIO_ABI( market, (addmarket)(addmortgage)(claimmortgage)(exchange)(frozenmarket)(trawmarket)(setfixedfee)(setprofee)(setprominfee) ) 
} /// namespace eosio
