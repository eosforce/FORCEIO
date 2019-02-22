#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester_network.hpp>
#include <eosio/chain/producer_object.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/generated_transaction_object.hpp>
#include <force.token/force.token.wast.hpp>
#include <force.token/force.token.abi.hpp>
#include <exchange/exchange.wast.hpp>
#include <exchange/exchange.abi.hpp>


#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;

struct exchange : public TESTER {
    exchange() : exc_acc("eosforce"), buyer("buyer"), seller("seller") {
        produce_blocks(2);
        
        // first, deploy exchange contract on eosforce contract
        push_action(config::system_account_name, name("freeze"), exc_acc, fc::mutable_variant_object()
           ("voter", exc_acc)
           ("stake", "10000.0000 SYS"));
        push_action(config::system_account_name, name("vote4ram"), exc_acc, fc::mutable_variant_object()
           ("voter", exc_acc)
           ("bpname", "biosbpa")
           ("stake", "10000.0000 SYS"));
        set_code(exc_acc, exchange_wast);
        set_abi(exc_acc, exchange_abi);
        
        // second, issue BTC¡¢ USDT tokens
        create_account(seller);// sell
        create_account(buyer);// buy
        push_action(config::system_account_name, name("create"), seller, fc::mutable_variant_object()
           ("issuer", seller)
           ("maximum_supply", "1000000000.0000 BTC"));
        push_action(config::system_account_name, name("create"), buyer, fc::mutable_variant_object()
           ("issuer", buyer)
           ("maximum_supply", "1000000000.0000 USDT"));
        
        push_action(config::system_account_name, name("issue"), seller, fc::mutable_variant_object()
           ("to", seller)
           ("quantity", "1000000000.0000 BTC")
           ("memo", "issue"));
        push_action(config::system_account_name, name("issue"), buyer, fc::mutable_variant_object()
           ("to", buyer)
           ("quantity", "1000000000.0000 USDT")
           ("memo", "issue"));
           
        // third, setfee
        
    }
    ~exchange() {}
    
    account_name exc_acc;
    account_name buyer;
    account_name seller;
};

BOOST_FIXTURE_TEST_SUITE(exchange_tests, exchange)

// partial match / full match when test price equals order price
BOOST_AUTO_TEST_CASE( test1 ) { try {

   produce_blocks(2);

   // make an ask order
   push_action(exc_acc, name("trade"), seller, fc::mutable_variant_object()
           ("maker", seller)
           ("base", "4.0000 BTC")
           ("price", "4000.00 USDT")
           ("bid_or_ask", "0"));
   

   produce_blocks(8);

} FC_LOG_AND_RETHROW() }






BOOST_AUTO_TEST_SUITE_END()
