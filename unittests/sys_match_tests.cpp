#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester_network.hpp>
#include <eosio/chain/producer_object.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/generated_transaction_object.hpp>
#include <relay.token/relay.token.wast.hpp>
#include <relay.token/relay.token.abi.hpp>
#include <sys.match/sys.match.wast.hpp>
#include <sys.match/sys.match.abi.hpp>


#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;

struct sys_match : public TESTER {
   
public:
   sys_match() : sys_match_acc("sys.match"), exc_acc("biosbpa"), buyer("buyer"), 
      seller("seller"), escrow("sys.match"), rel_token_acc("relay.token"),
      pair1_base(4, "BTC"), pair1_quote(2, "USDT"), pair2_base(4, "BTCC"), pair2_quote(2, "USDTC") {
      produce_blocks(2);
      
      // deploy relay.token contract
      create_account(rel_token_acc);
      push_action(config::system_account_name, name("freeze"), rel_token_acc, fc::mutable_variant_object()
         ("voter", rel_token_acc)
         ("stake", "10000.0000 SYS"));
      push_action(config::system_account_name, name("vote4ram"), rel_token_acc, fc::mutable_variant_object()
         ("voter", rel_token_acc)
         ("bpname", "biosbpa")
         ("stake", "10000.0000 SYS"));
      set_code(rel_token_acc, relay_token_wast);
      set_abi(rel_token_acc, relay_token_abi);
      
      // first, deploy sys.match contract on eosforce contract
      create_account(sys_match_acc);
      push_action(config::system_account_name, name("freeze"), sys_match_acc, fc::mutable_variant_object()
         ("voter", sys_match_acc)
         ("stake", "10000.0000 SYS"));
      push_action(config::system_account_name, name("vote4ram"), sys_match_acc, fc::mutable_variant_object()
         ("voter", sys_match_acc)
         ("bpname", "biosbpa")
         ("stake", "10000.0000 SYS"));
      set_code(sys_match_acc, sys_match_wast);
      set_abi(sys_match_acc, sys_match_abi);
      
      // second, setfee
      set_fee(sys_match_acc, N(create), asset(100), 100000, 1000000, 1000);
      set_fee(sys_match_acc, N(match), asset(100), 100000, 1000000, 1000);
      set_fee(sys_match_acc, N(cancel), asset(100), 100000, 1000000, 1000);
      set_fee(sys_match_acc, N(done), asset(100), 100000, 1000000, 1000);
      set_fee(rel_token_acc, N(trade), asset(100), 100000, 1000000, 1000);
      set_fee(rel_token_acc, N(create), asset(100), 100000, 1000000, 1000);
      set_fee(rel_token_acc, N(issue), asset(100), 100000, 1000000, 1000);
      set_fee(rel_token_acc, N(transfer), asset(100), 100000, 1000000, 1000);
      
      // third, issue BTC、 USDT tokens
      create_account(seller);// sell
      create_account(buyer);// buy
      push_action(rel_token_acc, name("create"), N(eosforce), fc::mutable_variant_object()
         ("issuer", seller)
         ("chain", token1_chain)
         ("maximum_supply", token1_max_supply)
      );
      push_action(rel_token_acc, name("create"), N(eosforce), fc::mutable_variant_object()
         ("issuer", buyer)
         ("chain", token2_chain)
         ("maximum_supply", token2_max_supply)
      );
      
      push_action(rel_token_acc, name("issue"), seller, fc::mutable_variant_object()
         ("chain", token1_chain)
         ("to", seller)
         ("quantity", token1_max_supply)
         ("memo", "issue")
      );
      push_action(rel_token_acc, name("issue"), buyer, fc::mutable_variant_object()
         ("chain", token2_chain)
         ("to", buyer)
         ("quantity", token2_max_supply)
         ("memo", "issue")
      );
         
      // fourth, authorization
      create_account(exc_acc);
      //cleos push action eosio updateauth '{"account": "eosio", "permission": "owner", "parent": "", "auth": {"threshold": 1, "keys": [], "waits": [], "accounts": [{"weight": 1, "permission": {"actor": "eosio.prods", "permission": "active"}}]}}' -p eosio@owner
      auto auth = authority(get_public_key(exc_acc, "active"), 0);
      auth.accounts.push_back( permission_level_weight{{rel_token_acc, config::eosio_code_name}, 1} );
      push_action(config::system_account_name, updateauth::get_name(), exc_acc, fc::mutable_variant_object()
         ("account", exc_acc)
         ("permission", "active")
         ("parent", "owner")
         ("auth",  auth)
      );
      
      auth = authority(get_public_key(sys_match_acc, "active"), 0);
      auth.accounts.push_back( permission_level_weight{{sys_match_acc, config::eosio_code_name}, 1} );
      push_action(config::system_account_name, updateauth::get_name(), sys_match_acc, fc::mutable_variant_object()
         ("account", sys_match_acc)
         ("permission", "active")
         ("parent", "owner")
         ("auth",  auth)
      );
      

//      efc push action sys.match create '["4,BTC", "btc1", "4,CBTC", "2,USDT", "usdt1", "2,CUSDT", "0", "biosbpa"]' -p biosbpa
//efc push action sys.match create '["4,BTCC", "btc1", "4,CBTC", "2,USDT", "usdt1", "2,CUSDT", "0", "biosbpa"]' -p biosbpa

      // fifth, create trading pair
      push_action(sys_match_acc, name("create"), exc_acc, fc::mutable_variant_object()
         ("base", pair1_base)
         ("base_chain", token1_chain)
         ("base_sym", asset::from_string(string(token1_max_supply)).get_symbol())
         ("quote", pair1_quote)
         ("quote_chain", token2_chain)
         ("quote_sym", asset::from_string(string(token2_max_supply)).get_symbol())
         ("fee_rate", pair1_fee_rate)
         ("exc_acc", exc_acc)
      );
      push_action(sys_match_acc, name("create"), exc_acc, fc::mutable_variant_object()
         ("base", pair2_base)
         ("base_chain", token1_chain)
         ("base_sym", asset::from_string(string(token1_max_supply)).get_symbol())
         ("quote", pair2_quote)
         ("quote_chain", token2_chain)
         ("quote_sym", asset::from_string(string(token2_max_supply)).get_symbol())
         ("fee_rate", pair2_fee_rate)
         ("exc_acc", exc_acc)
      );
   }
   ~sys_match() {}
   
   account_name sys_match_acc;
   account_name exc_acc;
   account_name buyer;
   account_name seller;
   account_name escrow;
   account_name rel_token_acc;
   const name token1_chain = "btc1";
   const char *token1_max_supply = "1000000000.0000 CBTC";
   const name token2_chain = "usdt1";
   const char *token2_max_supply = "1000000000.0000 CUSDT";
   const symbol pair1_base;
   const symbol pair1_quote;
   const symbol pair2_base;
   const symbol pair2_quote;
   const uint32_t trade_type = 1;
   const uint32_t pair1_fee_rate = 0;
   const uint32_t pair2_fee_rate = 10;
};

BOOST_FIXTURE_TEST_SUITE(sys_match_tests, sys_match)

// partial match / full match when test price equals order price
BOOST_AUTO_TEST_CASE( test1 ) { try {

   produce_blocks(2);
   asset quantity;
   asset seller_before;
   asset seller_now;
   asset buyer_before;
   asset buyer_now;
   asset escrow_before;
   asset escrow_now;
   symbol pair1_base_sym = asset::from_string(string(token1_max_supply)).get_symbol();
   symbol pair1_quote_sym = asset::from_string(string(token2_max_supply)).get_symbol();

   // make an ask order
   //efc push action relay.token trade '["testa", "sys.match", "btc1", "4.0000 CBTC", "1", "testa;testa;0;4000.00 CUSDT;0"]' -p testa
   
   quantity = asset::from_string( "4.0000 CBTC" );
   seller_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), seller);
   escrow_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), escrow);
   push_action(rel_token_acc, name("trade"), seller, fc::mutable_variant_object()
           ("from", seller)
           ("to", escrow)
           ("chain", token1_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "seller;seller;0;4000.00 CUSDT;0")
   );
   seller_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), seller);  
   escrow_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), escrow);
   BOOST_REQUIRE_EQUAL((seller_before - quantity), seller_now);
   BOOST_REQUIRE_EQUAL((escrow_before + quantity), escrow_now);

   // make a bid order
   // efc push action relay.token trade '["testb", "sys.match", "usdt1", "39500.0000 CUSDT", "1", "testb;testb;0;3950.00 CUSDT;1"]' -p testb
   quantity = asset::from_string( "39500.0000 CUSDT" );
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), buyer);
   escrow_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), escrow);
   push_action(rel_token_acc, name("trade"), buyer, fc::mutable_variant_object()
           ("from", buyer)
           ("to", escrow)
           ("chain", token2_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "buyer;buyer;0;3950.00 CUSDT;1")
   );
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), buyer);
   escrow_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), escrow);
   BOOST_REQUIRE_EQUAL(buyer_before - quantity, buyer_now);
   BOOST_REQUIRE_EQUAL(escrow_before + quantity, escrow_now);

   // partially match bid orders (price equals buy first price)
   //efc push action relay.token trade '["testa", "sys.match", "btc1", "1.0000 CBTC", "1", "testa;testa;0;3950.00 CUSDT;0"]' -p testa
   quantity = asset::from_string( "1.0000 CBTC" );
   seller_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), buyer);
   push_action(rel_token_acc, name("trade"), seller, fc::mutable_variant_object()
           ("from", seller)
           ("to", escrow)
           ("chain", token1_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "seller;seller;0;3950.00 CUSDT;0")
   );
   seller_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), buyer);
   BOOST_REQUIRE_EQUAL(seller_before + asset::from_string( "3950.0000 CUSDT" ), seller_now);
   BOOST_REQUIRE_EQUAL(buyer_before + quantity, buyer_now);

   // 完全成交买单(价格等于买一价, 数量等于挂单数量)
   // fully match bid orders (price equals buy first price, bid quantity equals order quantity)
   //efc push action relay.token trade '["testa", "sys.match", "btc1", "9.0000 CBTC", "1", "testa;testa;0;3950.00 CUSDT;0"]' -p testa
   quantity = asset::from_string( "9.0000 CBTC" );
   seller_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), buyer);
   push_action(rel_token_acc, name("trade"), seller, fc::mutable_variant_object()
           ("from", seller)
           ("to", escrow)
           ("chain", token1_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "seller;seller;0;3950.00 CUSDT;0")
   );
   seller_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), buyer);
   BOOST_REQUIRE_EQUAL(seller_before + asset::from_string( "35550.0000 CUSDT" ), seller_now);
   BOOST_REQUIRE_EQUAL(buyer_before + quantity, buyer_now);

   //部分成交卖单(价格等于买一价)
   // partially match ask order (price equals sell first price)
   //efc push action relay.token trade '["testb", "sys.match", "usdt1", "4000.0000 CUSDT", "1", "testb;testb;0;4000.0000 CUSDT;1"]' -p testb
   quantity = asset::from_string( "4000.0000 CUSDT" );
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, pair1_base_sym, buyer);
   seller_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), seller);
   push_action(rel_token_acc, name("trade"), buyer, fc::mutable_variant_object()
           ("from", buyer)
           ("to", escrow)
           ("chain", token2_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "buyer;buyer;0;4000.00 CUSDT;1")
   );
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, pair1_base_sym, buyer);
   seller_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), seller);
   BOOST_REQUIRE_EQUAL(buyer_before + asset::from_string( "1.0000 CBTC" ), buyer_now);
   BOOST_REQUIRE_EQUAL(seller_before + quantity, seller_now);

   // 完全成交卖单(价格等于买一价, 数量等于挂单数量)
   // efc push action relay.token trade '["testb", "sys.match", "usdt1", "12000.0000 CUSDT", "1", "testb;testb;0;4000.0000 CUSDT;1"]' -p testb
   quantity = asset::from_string( "12000.0000 CUSDT" );
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, pair1_base_sym, buyer);
   seller_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), seller);
   push_action(rel_token_acc, name("trade"), buyer, fc::mutable_variant_object()
           ("from", buyer)
           ("to", escrow)
           ("chain", token2_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "buyer;buyer;0;4000.00 CUSDT;1")
   );
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, pair1_base_sym, buyer);
   seller_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), seller);
   BOOST_REQUIRE_EQUAL(buyer_before + asset::from_string( "3.0000 CBTC" ), buyer_now);
   BOOST_REQUIRE_EQUAL(seller_before + quantity, seller_now);

   produce_blocks(8);

} FC_LOG_AND_RETHROW() }

//测试用例二：测试价格大于挂单情况下，部分匹配，完全匹配
BOOST_AUTO_TEST_CASE( test2 ) { try {

   produce_blocks(2);
   asset quantity;
   asset seller_before;
   asset seller_now;
   asset buyer_before;
   asset buyer_now;
   asset escrow_before;
   asset escrow_now;
   symbol pair1_base_sym = asset::from_string(string(token1_max_supply)).get_symbol();
   symbol pair1_quote_sym = asset::from_string(string(token2_max_supply)).get_symbol();

   // make an ask order
   //efc push action relay.token trade '["testa", "sys.match", "btc1", "4.0000 CBTC", "1", "testa;testa;0;4000.00 CUSDT;0"]' -p testa
   
   quantity = asset::from_string( "4.0000 CBTC" );
   seller_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), seller);
   escrow_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), escrow);
   push_action(rel_token_acc, name("trade"), seller, fc::mutable_variant_object()
           ("from", seller)
           ("to", escrow)
           ("chain", token1_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "seller;seller;0;4000.00 CUSDT;0")
   );
   seller_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), seller);  
   escrow_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), escrow);
   BOOST_REQUIRE_EQUAL((seller_before - quantity), seller_now);
   BOOST_REQUIRE_EQUAL((escrow_before + quantity), escrow_now);

   // make a bid order
   // efc push action relay.token trade '["testb", "sys.match", "usdt1", "39500.0000 CUSDT", "1", "testb;testb;0;3950.00 CUSDT;1"]' -p testb
   quantity = asset::from_string( "39500.0000 CUSDT" );
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), buyer);
   escrow_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), escrow);
   push_action(rel_token_acc, name("trade"), buyer, fc::mutable_variant_object()
           ("from", buyer)
           ("to", escrow)
           ("chain", token2_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "buyer;buyer;0;3950.00 CUSDT;1")
   );
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), buyer);
   escrow_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), escrow);
   BOOST_REQUIRE_EQUAL(buyer_before - quantity, buyer_now);
   BOOST_REQUIRE_EQUAL(escrow_before + quantity, escrow_now);

   // partially match bid orders (price less than buy first price)
   //efc push action relay.token trade '["testa", "sys.match", "btc1", "1.0000 CBTC", "1", "testa;testa;0;3900.00 CUSDT;0"]' -p testa
   quantity = asset::from_string( "1.0000 CBTC" );
   seller_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), buyer);
   push_action(rel_token_acc, name("trade"), seller, fc::mutable_variant_object()
           ("from", seller)
           ("to", escrow)
           ("chain", token1_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "seller;seller;0;3900.00 CUSDT;0")
   );
   seller_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), buyer);
   BOOST_REQUIRE_EQUAL(seller_before + asset::from_string( "3900.0000 CUSDT" ), seller_now);
   BOOST_REQUIRE_EQUAL(buyer_before + quantity, buyer_now);

   // 完全成交买单(价格等于买一价, 数量等于挂单数量)
   // fully match bid orders (price equals buy first price, bid quantity equals order quantity)
   //efc push action relay.token trade '["testa", "sys.match", "btc1", "9.0000 CBTC", "1", "testa;testa;0;3950.00 CUSDT;0"]' -p testa
   quantity = asset::from_string( "9.0000 CBTC" );
   seller_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), buyer);
   push_action(rel_token_acc, name("trade"), seller, fc::mutable_variant_object()
           ("from", seller)
           ("to", escrow)
           ("chain", token1_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "seller;seller;0;3950.00 CUSDT;0")
   );
   seller_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, quantity.get_symbol(), buyer);
   BOOST_REQUIRE_EQUAL(seller_before + asset::from_string( "35550.0000 CUSDT" ), seller_now);
   BOOST_REQUIRE_EQUAL(buyer_before + quantity, buyer_now);

   //部分成交卖单(价格等于买一价)
   // partially match ask order (price equals sell first price)
   //efc push action relay.token trade '["testb", "sys.match", "usdt1", "4100.0000 CUSDT", "1", "testb;testb;0;4100.0000 CUSDT;1"]' -p testb
   quantity = asset::from_string( "4100.0000 CUSDT" );
   seller_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, pair1_base_sym, buyer);
   push_action(rel_token_acc, name("trade"), buyer, fc::mutable_variant_object()
           ("from", buyer)
           ("to", escrow)
           ("chain", token2_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "buyer;buyer;0;4100.00 CUSDT;1")
   );
   seller_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, pair1_quote_sym, seller);
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, pair1_base_sym, buyer);
   BOOST_REQUIRE_EQUAL(seller_before + asset::from_string( "4000.0000 CUSDT" ), seller_now);
   BOOST_REQUIRE_EQUAL(buyer_before + asset::from_string( "1.0000 CBTC" ), buyer_now);

   // 完全成交卖单(价格等于买一价, 数量等于挂单数量)
   // efc push action relay.token trade '["testb", "sys.match", "usdt1", "12000.0000 CUSDT", "1", "testb;testb;0;4000.0000 CUSDT;1"]' -p testb
   /*quantity = asset::from_string( "12000.0000 CUSDT" );
   buyer_before = get_relay_token_currency_balance(rel_token_acc, token1_chain, pair1_base_sym, buyer);
   seller_before = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), seller);
   push_action(rel_token_acc, name("trade"), buyer, fc::mutable_variant_object()
           ("from", buyer)
           ("to", escrow)
           ("chain", token2_chain)
           ("quantity", quantity)
           ("type", trade_type)
           ("memo", "buyer;buyer;0;4000.00 CUSDT;1")
   );
   buyer_now = get_relay_token_currency_balance(rel_token_acc, token1_chain, pair1_base_sym, buyer);
   seller_now = get_relay_token_currency_balance(rel_token_acc, token2_chain, quantity.get_symbol(), seller);
   BOOST_REQUIRE_EQUAL(buyer_before + asset::from_string( "3.0000 CBTC" ), buyer_now);
   BOOST_REQUIRE_EQUAL(seller_before + quantity, seller_now);*/

   produce_blocks(8);

} FC_LOG_AND_RETHROW() }



BOOST_AUTO_TEST_SUITE_END()
