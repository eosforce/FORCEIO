#include "force.system.hpp"

namespace eosiosystem {

    void system_contract::newaccount() {
        print( "newaccount" );
    }

    void system_contract::updateauth() {
        print( "updateauth" );
    }

    void system_contract::deleteauth() {
        print( "deleteauth" );
    }

    void system_contract::linkauth() {
        print( "linkauth" );
    }

    void system_contract::unlinkauth() {
        print( "unlinkauth" );
    }

    void system_contract::canceldelay() {
        print( "canceldelay" );
    }

    void system_contract::onerror() {
        print( "onerror" );
    }

    void system_contract::setconfig(){
        print( "setconfig" );
    }

    void system_contract::setcode(){
        print( "setcode" );
    }
    
    void system_contract::setfee(){
        print( "setfee" );
    }
    void system_contract::setabi(){
        print( "setabi" );
    }

    void system_contract::onfee() {
        print( "onfee" );
    }
   

}