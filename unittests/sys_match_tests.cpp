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

struct exchange : public TESTER {
    exchange() : sys_match_acc("sys.match"), exc_acc("eosforce"), buyer("buyer"), seller("seller"), escrow("sys.match"), rel_token_acc("relay.token") {
        produce_blocks(2);
        
        // deploy relay.token contract
        create_account(rel_token_acc);
        push_action(config::system_account_name, name("freeze"), exc_acc, fc::mutable_variant_object()
           ("voter", rel_token_acc)
           ("stake", "10000.0000 SYS"));
        push_action(config::system_account_name, name("vote4ram"), exc_acc, fc::mutable_variant_object()
           ("voter", rel_token_acc)
           ("bpname", "biosbpa")
           ("stake", "10000.0000 SYS"));
        set_code(rel_token_acc, relay_token_wast);
        set_abi(rel_token_acc, relay_token_abi);
        
        // first, deploy sys.match contract on eosforce contract
        create_account(sys_match_acc);
        push_action(config::system_account_name, name("freeze"), exc_acc, fc::mutable_variant_object()
           ("voter", sys_match_acc)
           ("stake", "10000.0000 SYS"));
        push_action(config::system_account_name, name("vote4ram"), exc_acc, fc::mutable_variant_object()
           ("voter", sys_match_acc)
           ("bpname", "biosbpa")
           ("stake", "10000.0000 SYS"));
        set_code(sys_match_acc, sys_match_wast);
        set_abi(sys_match_acc, sys_match_abi);
        
        // second, issue BTC¡¢ USDT tokens
        create_account(seller);// sell
        create_account(buyer);// buy
        push_action(rel_token_acc, name("create"), seller, fc::mutable_variant_object()
           ("issuer", seller)
           ("maximum_supply", "1000000000.0000 BTC"));
        push_action(rel_token_acc, name("create"), buyer, fc::mutable_variant_object()
           ("issuer", buyer)
           ("maximum_supply", "1000000000.0000 USDT"));
        
        push_action(rel_token_acc, name("issue"), seller, fc::mutable_variant_object()
           ("to", seller)
           ("quantity", "1000000000.0000 BTC")
           ("memo", "issue"));
        push_action(rel_token_acc, name("issue"), buyer, fc::mutable_variant_object()
           ("to", buyer)
           ("quantity", "1000000000.0000 USDT")
           ("memo", "issue"));
           
        // third, setfee
        set_fee(exc_acc, N(create), asset(100), 100000, 1000000, 1000);
        set_fee(exc_acc, N(trade), asset(100), 100000, 1000000, 1000);
        set_fee(exc_acc, N(cancel), asset(100), 100000, 1000000, 1000);
        
        // fourth, authorization
        //cleos push action eosio updateauth '{"account": "eosio", "permission": "owner", "parent": "", "auth": {"threshold": 1, "keys": [], "waits": [], "accounts": [{"weight": 1, "permission": {"actor": "eosio.prods", "permission": "active"}}]}}' -p eosio@owner
        auto auth = authority(get_public_key(N(eosforce), "active"), 0);
        auth.accounts.push_back( permission_level_weight{{N(eosforce), config::eosio_code_name}, 1} );
        push_action(config::system_account_name, updateauth::get_name(), escrow, fc::mutable_variant_object()
           ("account", escrow)
           ("permission", "active")
           ("parent", "owner")
           ("auth",  auth)
        );
        
        auth = authority(get_public_key(N(eosforce), "active"), 0);
        auth.accounts.push_back( permission_level_weight{{N(eosforce), config::eosio_code_name}, 1} );
        push_action(config::system_account_name, updateauth::get_name(), seller, fc::mutable_variant_object()
           ("account", seller)
           ("permission", "active")
           ("parent", "owner")
           ("auth",  auth)
        );
        
        auth = authority(get_public_key(N(eosforce), "active"), 0);
        auth.accounts.push_back( permission_level_weight{{N(eosforce), config::eosio_code_name}, 1} );
        push_action(config::system_account_name, updateauth::get_name(), buyer, fc::mutable_variant_object()
           ("account", buyer)
           ("permission", "active")
           ("parent", "owner")
           ("auth",  auth)
        );
        
        // fifth, create trading pair
        push_action(exc_acc, name("create"), exc_acc, fc::mutable_variant_object()
           ("base", "4,BTC")
           ("quote", "2,USDT"));
    }
    ~exchange() {}
    
    account_name sys_match_acc;
    account_name exc_acc;
    account_name buyer;
    account_name seller;
    account_name escrow;
    account_name rel_token_acc;
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
