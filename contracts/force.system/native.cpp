#include "force.system.hpp"

namespace eosiosystem {
    
    void system_contract::newaccount(account_name creator,account_name name,authority owner,authority active) {

    }
    
    void system_contract::updateauth(account_name account,permission_name permission,permission_name parent,authority auth) {
        
    }

    void system_contract::deleteauth(account_name account,permission_name permission) {
        
    }

    void system_contract::linkauth(account_name account,account_name code,action_name type,permission_name requirement) {
        
    }

    void system_contract::unlinkauth(account_name account,account_name code,action_name type) {
        
    }

    void system_contract::canceldelay(permission_level canceling_auth,transaction_id_type trx_id) {
    }
        

    void system_contract::onerror(uint128_t sender_id,bytes sent_trx) {
       
    }

    void system_contract::setconfig(account_name typ,int64_t num,account_name key,asset fee){
        
    }

    void system_contract::setcode(account_name account,uint8_t vmtype,uint8_t vmversion,bytes code){
        
    }
    
    void system_contract::setfee(account_name account,action_name action,asset fee,uint32_t cpu_limit,uint32_t net_limit,uint32_t ram_limit){
        
    }
    void system_contract::setabi(account_name account,bytes abi){
       
    }

 

}