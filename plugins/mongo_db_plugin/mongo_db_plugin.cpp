/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/mongo_db_plugin/mongo_db_plugin.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/types.hpp>

#include <fc/io/json.hpp>
#include <fc/utf8.hpp>
#include <fc/variant.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/chrono.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <queue>
#include <string>

#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <eosio/chain/genesis_state.hpp>
#include <eosio/chain/config.hpp>

namespace fc { class variant; }

namespace eosio {

using chain::account_name;
using chain::action_name;
using chain::block_id_type;
using chain::permission_name;
using chain::transaction;
using chain::signed_transaction;
using chain::signed_block;
using chain::transaction_id_type;
using chain::packed_transaction;

static appbase::abstract_plugin& _mongo_db_plugin = app().register_plugin<mongo_db_plugin>();

struct filter_entry {
   name receiver;
   name action;
   name actor;

   friend bool operator<( const filter_entry& a, const filter_entry& b ) {
      return std::tie( a.receiver, a.action, a.actor ) < std::tie( b.receiver, b.action, b.actor );
   }

   //            receiver          action       actor
   bool match( const name& rr, const name& an, const name& ar ) const {
      return (receiver.value == 0 || receiver == rr) &&
             (action.value == 0 || action == an) &&
             (actor.value == 0 || actor == ar);
   }
};

class mongo_db_plugin_impl {
public:
   mongo_db_plugin_impl();
   ~mongo_db_plugin_impl();

   fc::optional<boost::signals2::scoped_connection> accepted_block_connection;
   fc::optional<boost::signals2::scoped_connection> irreversible_block_connection;
   fc::optional<boost::signals2::scoped_connection> accepted_transaction_connection;
   fc::optional<boost::signals2::scoped_connection> applied_transaction_connection;

   void consume_blocks();

   void accepted_block( const chain::block_state_ptr& );
   void applied_irreversible_block(const chain::block_state_ptr&);
   void accepted_transaction(const chain::transaction_metadata_ptr&);
   void applied_transaction(const chain::transaction_trace_ptr&);
   void process_accepted_transaction(const chain::transaction_metadata_ptr&);
   void _process_accepted_transaction(const chain::transaction_metadata_ptr&);
   void process_applied_transaction(const chain::transaction_trace_ptr&);
   void _process_applied_transaction(const chain::transaction_trace_ptr&);
   void process_accepted_block( const chain::block_state_ptr& );
   void _process_accepted_block( const chain::block_state_ptr& );
   void process_irreversible_block(const chain::block_state_ptr&);
   void _process_irreversible_block(const chain::block_state_ptr&);

   optional<abi_serializer> get_abi_serializer( account_name n );
   template<typename T> fc::variant to_variant_with_abi( const T& obj );

   void purge_abi_cache();

   bool add_action_trace( mongocxx::bulk_write& bulk_action_traces, const chain::action_trace& atrace,
                          const chain::transaction_trace_ptr& t,
                          bool executed, const std::chrono::milliseconds& now,
                          bool& write_ttrace );

   void update_account(const chain::action& act);

   void add_pub_keys( const vector<chain::key_weight>& keys, const account_name& name,
                      const permission_name& permission, const std::chrono::milliseconds& now );
   void remove_pub_keys( const account_name& name, const permission_name& permission );
   void add_account_control( const vector<chain::permission_level_weight>& controlling_accounts,
                             const account_name& name, const permission_name& permission,
                             const std::chrono::milliseconds& now );
   void remove_account_control( const account_name& name, const permission_name& permission );

   void insert_default_abi();
   bool b_insert_default_abi = false;

   bool on_cancel(const chain::action& a,
                                        const chain::transaction_trace_ptr& t);
   bool on_close2(const chain::action& a,
                                        const chain::transaction_trace_ptr& t);
   bool on_close(const chain::action_trace& atrace,
                                        const chain::transaction_trace_ptr& t);
   bool upd_order( mongocxx::bulk_write& bulk_actions, 
                                        const chain::action& a,
                                        const chain::transaction_trace_ptr& t);
   bool ins_order( mongocxx::bulk_write& bulk_actions, 
                                        const chain::action& a,
                                        const chain::transaction_trace_ptr& t);                              
   bool ins_deal( mongocxx::bulk_write& bulk_action_deals, 
                                        const chain::action& a,
                                        const chain::transaction_trace_ptr& t );
   bool add_match_action( mongocxx::bulk_write& bulk_actions, 
                                        const chain::action_trace& atrace,
                                        const chain::transaction_trace_ptr& t, name action,
                                        int op);                                 
   bool add_match( const chain::action_trace& atrace,
                                        const chain::transaction_trace_ptr& t);
   void add_trade( const chain::transaction_trace_ptr& t );

   /// @return true if act should be added to mongodb, false to skip it
   bool filter_include( const account_name& receiver, const action_name& act_name,
                        const vector<chain::permission_level>& authorization ) const;
   bool filter_include( const transaction& trx ) const;

   void init();
   void wipe_database();

   template<typename Queue, typename Entry> void queue(Queue& queue, const Entry& e);

   bool configured{false};
   bool wipe_database_on_startup{false};
   uint32_t start_block_num = 0;
   std::atomic_bool start_block_reached{false};

   bool is_producer = false;
   bool filter_on_star = true;
   std::set<filter_entry> filter_on;
   std::set<filter_entry> filter_out;
   bool update_blocks_via_block_num = false;
   bool store_blocks = true;
   bool store_block_states = true;
   bool store_transactions = true;
   bool store_transaction_traces = true;
   bool store_action_traces = true;

   std::string db_name;
   mongocxx::instance mongo_inst;
   fc::optional<mongocxx::pool> mongo_pool;

   // consum thread
   mongocxx::collection _accounts;
   mongocxx::collection _trans;
   mongocxx::collection _trans_traces;
   mongocxx::collection _action_traces;
   mongocxx::collection _block_states;
   mongocxx::collection _blocks;
   mongocxx::collection _pub_keys;
   mongocxx::collection _account_controls;
   mongocxx::collection _orders;
   mongocxx::collection _deals;

   size_t max_queue_size = 0;
   int queue_sleep_time = 0;
   size_t abi_cache_size = 0;
   std::deque<chain::transaction_metadata_ptr> transaction_metadata_queue;
   std::deque<chain::transaction_metadata_ptr> transaction_metadata_process_queue;
   std::deque<chain::transaction_trace_ptr> transaction_trace_queue;
   std::deque<chain::transaction_trace_ptr> transaction_trace_process_queue;
   std::deque<chain::block_state_ptr> block_state_queue;
   std::deque<chain::block_state_ptr> block_state_process_queue;
   std::deque<chain::block_state_ptr> irreversible_block_state_queue;
   std::deque<chain::block_state_ptr> irreversible_block_state_process_queue;
   boost::mutex mtx;
   boost::condition_variable condition;
   boost::thread consume_thread;
   std::atomic_bool done{false};
   std::atomic_bool startup{true};
   fc::optional<chain::chain_id_type> chain_id;
   fc::microseconds abi_serializer_max_time;

   struct by_account;
   struct by_last_access;

   struct abi_cache {
      account_name                     account;
      fc::time_point                   last_accessed;
      fc::optional<abi_serializer>     serializer;
   };

   typedef boost::multi_index_container<abi_cache,
         indexed_by<
               ordered_unique< tag<by_account>,  member<abi_cache,account_name,&abi_cache::account> >,
               ordered_non_unique< tag<by_last_access>,  member<abi_cache,fc::time_point,&abi_cache::last_accessed> >
         >
   > abi_cache_index_t;

   abi_cache_index_t abi_cache_index;

   static const action_name newaccount;
   static const action_name setabi;
   static const action_name updateauth;
   static const action_name deleteauth;
   static const permission_name owner;
   static const permission_name active;

   static const std::string block_states_col;
   static const std::string blocks_col;
   static const std::string trans_col;
   static const std::string trans_traces_col;
   static const std::string action_traces_col;
   static const std::string accounts_col;
   static const std::string pub_keys_col;
   static const std::string account_controls_col;
   static const std::string orders_col;
   static const std::string deals_col;
};

const action_name mongo_db_plugin_impl::newaccount = chain::newaccount::get_name();
const action_name mongo_db_plugin_impl::setabi = chain::setabi::get_name();
const action_name mongo_db_plugin_impl::updateauth = chain::updateauth::get_name();
const action_name mongo_db_plugin_impl::deleteauth = chain::deleteauth::get_name();
const permission_name mongo_db_plugin_impl::owner = chain::config::owner_name;
const permission_name mongo_db_plugin_impl::active = chain::config::active_name;

const std::string mongo_db_plugin_impl::block_states_col = "block_states";
const std::string mongo_db_plugin_impl::blocks_col = "blocks";
const std::string mongo_db_plugin_impl::trans_col = "transactions";
const std::string mongo_db_plugin_impl::trans_traces_col = "transaction_traces";
const std::string mongo_db_plugin_impl::action_traces_col = "action_traces";
const std::string mongo_db_plugin_impl::accounts_col = "accounts";
const std::string mongo_db_plugin_impl::pub_keys_col = "pub_keys";
const std::string mongo_db_plugin_impl::account_controls_col = "account_controls";
const std::string mongo_db_plugin_impl::orders_col = "orderbook";
const std::string mongo_db_plugin_impl::deals_col = "deals";

bool mongo_db_plugin_impl::filter_include( const account_name& receiver, const action_name& act_name,
                                           const vector<chain::permission_level>& authorization ) const
{
   bool include = false;
   if( filter_on_star ) {
      include = true;
   } else {
      auto itr = std::find_if( filter_on.cbegin(), filter_on.cend(), [&receiver, &act_name]( const auto& filter ) {
         return filter.match( receiver, act_name, 0 );
      } );
      if( itr != filter_on.cend() ) {
         include = true;
      } else {
         for( const auto& a : authorization ) {
            auto itr = std::find_if( filter_on.cbegin(), filter_on.cend(), [&receiver, &act_name, &a]( const auto& filter ) {
               return filter.match( receiver, act_name, a.actor );
            } );
            if( itr != filter_on.cend() ) {
               include = true;
               break;
            }
         }
      }
   }

   if( !include ) { return false; }
   if( filter_out.empty() ) { return true; }

   auto itr = std::find_if( filter_out.cbegin(), filter_out.cend(), [&receiver, &act_name]( const auto& filter ) {
      return filter.match( receiver, act_name, 0 );
   } );
   if( itr != filter_out.cend() ) { return false; }

   for( const auto& a : authorization ) {
      auto itr = std::find_if( filter_out.cbegin(), filter_out.cend(), [&receiver, &act_name, &a]( const auto& filter ) {
         return filter.match( receiver, act_name, a.actor );
      } );
      if( itr != filter_out.cend() ) { return false; }
   }

   return true;
}

bool mongo_db_plugin_impl::filter_include( const transaction& trx ) const
{
   if( !filter_on_star || !filter_out.empty() ) {
      bool include = false;
      for( const auto& a : trx.actions ) {
         if( filter_include( a.account, a.name, a.authorization ) ) {
            include = true;
            break;
         }
      }
      if( !include ) {
         for( const auto& a : trx.context_free_actions ) {
            if( filter_include( a.account, a.name, a.authorization ) ) {
               include = true;
               break;
            }
         }
      }
      return include;
   }
   return true;
}


template<typename Queue, typename Entry>
void mongo_db_plugin_impl::queue( Queue& queue, const Entry& e ) {
   boost::mutex::scoped_lock lock( mtx );
   auto queue_size = queue.size();
   if( queue_size > max_queue_size ) {
      lock.unlock();
      condition.notify_one();
      queue_sleep_time += 10;
      if( queue_sleep_time > 1000 )
         wlog("queue size: ${q}", ("q", queue_size));
      boost::this_thread::sleep_for( boost::chrono::milliseconds( queue_sleep_time ));
      lock.lock();
   } else {
      queue_sleep_time -= 10;
      if( queue_sleep_time < 0 ) queue_sleep_time = 0;
   }
   queue.emplace_back( e );
   lock.unlock();
   condition.notify_one();
}

void mongo_db_plugin_impl::accepted_transaction( const chain::transaction_metadata_ptr& t ) {
   try {
      if( store_transactions ) {
         queue( transaction_metadata_queue, t );
      }
   } catch (fc::exception& e) {
      elog("FC Exception while accepted_transaction ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while accepted_transaction ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while accepted_transaction");
   }
}

void mongo_db_plugin_impl::applied_transaction( const chain::transaction_trace_ptr& t ) {
   try {
      // Traces emitted from an incomplete block leave the producer_block_id as empty.
      //
      // Avoid adding the action traces or transaction traces to the database if the producer_block_id is empty.
      // This way traces from speculatively executed transactions are not included in the Mongo database which can
      // avoid potential confusion for consumers of that database.
      //
      // Due to forks, it could be possible for multiple incompatible action traces with the same block_num and trx_id
      // to exist in the database. And if the producer double produces a block, even the block_time may not
      // disambiguate the two action traces. Without a producer_block_id to disambiguate and determine if the action
      // trace comes from an orphaned fork branching off of the blockchain, consumers of the Mongo DB database may be
      // reacting to a stale action trace that never actually executed in the current blockchain.
      //
      // It is better to avoid this potential confusion by not logging traces from speculative execution, i.e. emitted
      // from an incomplete block. This means that traces will not be recorded in speculative read-mode, but
      // users should not be using the mongo_db_plugin in that mode anyway.
      //
      // Allow logging traces if node is a producer for testing purposes, so a single nodeos can do both for testing.
      //
      // It is recommended to run mongo_db_plugin in read-mode = read-only.
      //
      if( !is_producer && !t->producer_block_id.valid() )
         return;
      // always queue since account information always gathered
      queue( transaction_trace_queue, t );
   } catch (fc::exception& e) {
      elog("FC Exception while applied_transaction ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while applied_transaction ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while applied_transaction");
   }
}

void mongo_db_plugin_impl::applied_irreversible_block( const chain::block_state_ptr& bs ) {
   try {
      if( store_blocks || store_block_states || store_transactions ) {
         queue( irreversible_block_state_queue, bs );
      }
   } catch (fc::exception& e) {
      elog("FC Exception while applied_irreversible_block ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while applied_irreversible_block ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while applied_irreversible_block");
   }
}

void mongo_db_plugin_impl::accepted_block( const chain::block_state_ptr& bs ) {
   try {
      if( !start_block_reached ) {
         if( bs->block_num >= start_block_num ) {
            start_block_reached = true;
         }
      }
      if( store_blocks || store_block_states ) {
         queue( block_state_queue, bs );
      }
   } catch (fc::exception& e) {
      elog("FC Exception while accepted_block ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while accepted_block ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while accepted_block");
   }
}

void mongo_db_plugin_impl::consume_blocks() {
   try {
      auto mongo_client = mongo_pool->acquire();
      auto& mongo_conn = *mongo_client;

      _accounts = mongo_conn[db_name][accounts_col];
      _trans = mongo_conn[db_name][trans_col];
      _trans_traces = mongo_conn[db_name][trans_traces_col];
      _action_traces = mongo_conn[db_name][action_traces_col];
      _blocks = mongo_conn[db_name][blocks_col];
      _block_states = mongo_conn[db_name][block_states_col];
      _pub_keys = mongo_conn[db_name][pub_keys_col];
      _account_controls = mongo_conn[db_name][account_controls_col];
      _orders = mongo_conn[db_name][orders_col];
      _deals = mongo_conn[db_name][deals_col];
      //insert_default_abi();
      while (true) {
         boost::mutex::scoped_lock lock(mtx);
         while ( transaction_metadata_queue.empty() &&
                 transaction_trace_queue.empty() &&
                 block_state_queue.empty() &&
                 irreversible_block_state_queue.empty() &&
                 !done ) {
            condition.wait(lock);
         }

         // capture for processing
         size_t transaction_metadata_size = transaction_metadata_queue.size();
         if (transaction_metadata_size > 0) {
            transaction_metadata_process_queue = move(transaction_metadata_queue);
            transaction_metadata_queue.clear();
         }
         size_t transaction_trace_size = transaction_trace_queue.size();
         if (transaction_trace_size > 0) {
            transaction_trace_process_queue = move(transaction_trace_queue);
            transaction_trace_queue.clear();
         }
         size_t block_state_size = block_state_queue.size();
         if (block_state_size > 0) {
            block_state_process_queue = move(block_state_queue);
            block_state_queue.clear();
         }
         size_t irreversible_block_size = irreversible_block_state_queue.size();
         if (irreversible_block_size > 0) {
            irreversible_block_state_process_queue = move(irreversible_block_state_queue);
            irreversible_block_state_queue.clear();
         }

         lock.unlock();

         if (done) {
            ilog("draining queue, size: ${q}", ("q", transaction_metadata_size + transaction_trace_size + block_state_size + irreversible_block_size));
         }

         // process transactions
         auto start_time = fc::time_point::now();
         auto size = transaction_trace_process_queue.size();
         while (!transaction_trace_process_queue.empty()) {
            const auto& t = transaction_trace_process_queue.front();
            process_applied_transaction(t);
            transaction_trace_process_queue.pop_front();
         }
         auto time = fc::time_point::now() - start_time;
         auto per = size > 0 ? time.count()/size : 0;
         if( time > fc::microseconds(500000) ) // reduce logging, .5 secs
            ilog( "process_applied_transaction,  time per: ${p}, size: ${s}, time: ${t}", ("s", size)("t", time)("p", per) );

         start_time = fc::time_point::now();
         size = transaction_metadata_process_queue.size();
         while (!transaction_metadata_process_queue.empty()) {
            const auto& t = transaction_metadata_process_queue.front();
            process_accepted_transaction(t);
            transaction_metadata_process_queue.pop_front();
         }
         time = fc::time_point::now() - start_time;
         per = size > 0 ? time.count()/size : 0;
         if( time > fc::microseconds(500000) ) // reduce logging, .5 secs
            ilog( "process_accepted_transaction, time per: ${p}, size: ${s}, time: ${t}", ("s", size)( "t", time )( "p", per ));

         // process blocks
         start_time = fc::time_point::now();
         size = block_state_process_queue.size();
         while (!block_state_process_queue.empty()) {
            const auto& bs = block_state_process_queue.front();
            process_accepted_block( bs );
            block_state_process_queue.pop_front();
         }
         time = fc::time_point::now() - start_time;
         per = size > 0 ? time.count()/size : 0;
         if( time > fc::microseconds(500000) ) // reduce logging, .5 secs
            ilog( "process_accepted_block,       time per: ${p}, size: ${s}, time: ${t}", ("s", size)("t", time)("p", per) );

         // process irreversible blocks
         start_time = fc::time_point::now();
         size = irreversible_block_state_process_queue.size();
         while (!irreversible_block_state_process_queue.empty()) {
            const auto& bs = irreversible_block_state_process_queue.front();
            process_irreversible_block(bs);
            irreversible_block_state_process_queue.pop_front();
         }
         time = fc::time_point::now() - start_time;
         per = size > 0 ? time.count()/size : 0;
         if( time > fc::microseconds(500000) ) // reduce logging, .5 secs
            ilog( "process_irreversible_block,   time per: ${p}, size: ${s}, time: ${t}", ("s", size)("t", time)("p", per) );

         if( transaction_metadata_size == 0 &&
             transaction_trace_size == 0 &&
             block_state_size == 0 &&
             irreversible_block_size == 0 &&
             done ) {
            break;
         }
      }
      ilog("mongo_db_plugin consume thread shutdown gracefully");
   } catch (fc::exception& e) {
      elog("FC Exception while consuming block ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while consuming block ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while consuming block");
   }
}

namespace {

auto find_account( mongocxx::collection& accounts, const account_name& name ) {
   using bsoncxx::builder::basic::make_document;
   using bsoncxx::builder::basic::kvp;
   return accounts.find_one( make_document( kvp( "name", name.to_string())));
}

auto find_block( mongocxx::collection& blocks, const string& id ) {
   using bsoncxx::builder::basic::make_document;
   using bsoncxx::builder::basic::kvp;

   mongocxx::options::find options;
   options.projection( make_document( kvp( "_id", 1 )) ); // only return _id
   return blocks.find_one( make_document( kvp( "block_id", id )), options);
}

void handle_mongo_exception( const std::string& desc, int line_num ) {
   bool shutdown = true;
   try {
      try {
         throw;
      } catch( mongocxx::logic_error& e) {
         // logic_error on invalid key, do not shutdown
         wlog( "mongo logic error, ${desc}, line ${line}, code ${code}, ${what}",
               ("desc", desc)( "line", line_num )( "code", e.code().value() )( "what", e.what() ));
         shutdown = false;
      } catch( mongocxx::operation_exception& e) {
         elog( "mongo exception, ${desc}, line ${line}, code ${code}, ${details}",
               ("desc", desc)( "line", line_num )( "code", e.code().value() )( "details", e.code().message() ));
         if (e.raw_server_error()) {
            elog( "  raw_server_error: ${e}", ( "e", bsoncxx::to_json(e.raw_server_error()->view())));
         }
      } catch( mongocxx::exception& e) {
         elog( "mongo exception, ${desc}, line ${line}, code ${code}, ${what}",
               ("desc", desc)( "line", line_num )( "code", e.code().value() )( "what", e.what() ));
      } catch( bsoncxx::exception& e) {
         elog( "bsoncxx exception, ${desc}, line ${line}, code ${code}, ${what}",
               ("desc", desc)( "line", line_num )( "code", e.code().value() )( "what", e.what() ));
      } catch( fc::exception& er ) {
         elog( "mongo fc exception, ${desc}, line ${line}, ${details}",
               ("desc", desc)( "line", line_num )( "details", er.to_detail_string()));
      } catch( const std::exception& e ) {
         elog( "mongo std exception, ${desc}, line ${line}, ${what}",
               ("desc", desc)( "line", line_num )( "what", e.what()));
      } catch( ... ) {
         elog( "mongo unknown exception, ${desc}, line ${line_nun}", ("desc", desc)( "line_num", line_num ));
      }
   } catch (...) {
      std::cerr << "Exception attempting to handle exception for " << desc << " " << line_num << std::endl;
   }

   if( shutdown ) {
      // shutdown if mongo failed to provide opportunity to fix issue and restart
      app().quit();
   }
}

} // anonymous namespace

void mongo_db_plugin_impl::purge_abi_cache() {
   if( abi_cache_index.size() < abi_cache_size ) return;

   // remove the oldest (smallest) last accessed
   auto& idx = abi_cache_index.get<by_last_access>();
   auto itr = idx.begin();
   if( itr != idx.end() ) {
      idx.erase( itr );
   }
}

optional<abi_serializer> mongo_db_plugin_impl::get_abi_serializer( account_name n ) {
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   if( n.good()) {
      try {

         auto itr = abi_cache_index.find( n );
         if( itr != abi_cache_index.end() ) {
            abi_cache_index.modify( itr, []( auto& entry ) {
               entry.last_accessed = fc::time_point::now();
            });

            return itr->serializer;
         }

         auto account = _accounts.find_one( make_document( kvp("name", n.to_string())) );
         if(account) {
            auto view = account->view();
            abi_def abi;
            if( view.find( "abi" ) != view.end()) {
               try {
                  abi = fc::json::from_string( bsoncxx::to_json( view["abi"].get_document())).as<abi_def>();
               } catch (...) {
                  ilog( "Unable to convert account abi to abi_def for ${n}", ( "n", n ));
                  return optional<abi_serializer>();
               }

               purge_abi_cache(); // make room if necessary
               abi_cache entry;
               entry.account = n;
               entry.last_accessed = fc::time_point::now();
               abi_serializer abis;
               if( n == chain::config::system_account_name ) {
                  // redefine eosio setabi.abi from bytes to abi_def
                  // Done so that abi is stored as abi_def in mongo instead of as bytes
                  auto itr = std::find_if( abi.structs.begin(), abi.structs.end(),
                                           []( const auto& s ) { return s.name == "setabi"; } );
                  if( itr != abi.structs.end() ) {
                     auto itr2 = std::find_if( itr->fields.begin(), itr->fields.end(),
                                               []( const auto& f ) { return f.name == "abi"; } );
                     if( itr2 != itr->fields.end() ) {
                        if( itr2->type == "bytes" ) {
                           itr2->type = "abi_def";
                           // unpack setabi.abi as abi_def instead of as bytes
                           abis.add_specialized_unpack_pack( "abi_def",
                                 std::make_pair<abi_serializer::unpack_function, abi_serializer::pack_function>(
                                       []( fc::datastream<const char*>& stream, bool is_array, bool is_optional ) -> fc::variant {
                                          EOS_ASSERT( !is_array && !is_optional, chain::mongo_db_exception, "unexpected abi_def");
                                          chain::bytes temp;
                                          fc::raw::unpack( stream, temp );
                                          return fc::variant( fc::raw::unpack<abi_def>( temp ) );
                                       },
                                       []( const fc::variant& var, fc::datastream<char*>& ds, bool is_array, bool is_optional ) {
                                          EOS_ASSERT( false, chain::mongo_db_exception, "never called" );
                                       }
                                 ) );
                        }
                     }
                  }
               }
               // mongo does not like empty json keys
               // make abi_serializer use empty_name instead of "" for the action data
               for( auto& s : abi.structs ) {
                  if( s.name.empty() ) {
                     s.name = "empty_struct_name";
                  }
                  for( auto& f : s.fields ) {
                     if( f.name.empty() ) {
                        f.name = "empty_field_name";
                     }
                  }
               }
               abis.set_abi( abi, abi_serializer_max_time );
               entry.serializer.emplace( std::move( abis ) );
               abi_cache_index.insert( entry );
               return entry.serializer;
            }
         }
      } FC_CAPTURE_AND_LOG((n))
   }
   return optional<abi_serializer>();
}

template<typename T>
fc::variant mongo_db_plugin_impl::to_variant_with_abi( const T& obj ) {
   fc::variant pretty_output;
   abi_serializer::to_variant( obj, pretty_output,
                               [&]( account_name n ) { return get_abi_serializer( n ); },
                               abi_serializer_max_time );
   return pretty_output;
}

void mongo_db_plugin_impl::process_accepted_transaction( const chain::transaction_metadata_ptr& t ) {
   try {
      if( start_block_reached ) {
         _process_accepted_transaction( t );
      }
   } catch (fc::exception& e) {
      elog("FC Exception while processing accepted transaction metadata: ${e}", ("e", e.to_detail_string()));
   } catch (std::exception& e) {
      elog("STD Exception while processing accepted tranasction metadata: ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while processing accepted transaction metadata");
   }
}

void mongo_db_plugin_impl::process_applied_transaction( const chain::transaction_trace_ptr& t ) {
   try {
      // always call since we need to capture setabi on accounts even if not storing transaction traces
      _process_applied_transaction( t );
   } catch (fc::exception& e) {
      elog("FC Exception while processing applied transaction trace: ${e}", ("e", e.to_detail_string()));
   } catch (std::exception& e) {
      elog("STD Exception while processing applied transaction trace: ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while processing applied transaction trace");
   }
}

void mongo_db_plugin_impl::process_irreversible_block(const chain::block_state_ptr& bs) {
  try {
     if( start_block_reached ) {
        _process_irreversible_block( bs );
     }
  } catch (fc::exception& e) {
     elog("FC Exception while processing irreversible block: ${e}", ("e", e.to_detail_string()));
  } catch (std::exception& e) {
     elog("STD Exception while processing irreversible block: ${e}", ("e", e.what()));
  } catch (...) {
     elog("Unknown exception while processing irreversible block");
  }
}

void mongo_db_plugin_impl::process_accepted_block( const chain::block_state_ptr& bs ) {
   try {
      if( start_block_reached ) {
         _process_accepted_block( bs );
      }
   } catch (fc::exception& e) {
      elog("FC Exception while processing accepted block trace ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while processing accepted block trace ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while processing accepted block trace");
   }
}

void mongo_db_plugin_impl::_process_accepted_transaction( const chain::transaction_metadata_ptr& t ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   using bsoncxx::builder::basic::make_array;
   namespace bbb = bsoncxx::builder::basic;

   const auto& trx = t->trx;

   if( !filter_include( trx ) ) return;
   
   auto trans_doc = bsoncxx::builder::basic::document{};

   auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
         std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

   const auto& trx_id = t->id;
   const auto trx_id_str = trx_id.str();

   trans_doc.append( kvp( "trx_id", trx_id_str ) );

   auto v = to_variant_with_abi( trx );
   string trx_json = fc::json::to_string( v );

   try {
      const auto& trx_value = bsoncxx::from_json( trx_json );
      trans_doc.append( bsoncxx::builder::concatenate_doc{trx_value.view()} );
   } catch( bsoncxx::exception& ) {
      try {
         trx_json = fc::prune_invalid_utf8( trx_json );
         const auto& trx_value = bsoncxx::from_json( trx_json );
         trans_doc.append( bsoncxx::builder::concatenate_doc{trx_value.view()} );
         trans_doc.append( kvp( "non-utf8-purged", b_bool{true} ) );
      } catch( bsoncxx::exception& e ) {
         elog( "Unable to convert transaction JSON to MongoDB JSON: ${e}", ("e", e.what()) );
         elog( "  JSON: ${j}", ("j", trx_json) );
      }
   }

   string signing_keys_json;
   if( t->signing_keys.valid() ) {
      signing_keys_json = fc::json::to_string( t->signing_keys->second );
   } else {
      auto signing_keys = trx.get_signature_keys( *chain_id, false, false );
      if( !signing_keys.empty() ) {
         signing_keys_json = fc::json::to_string( signing_keys );
      }
   }

   if( !signing_keys_json.empty() ) {
      try {
         const auto& keys_value = bsoncxx::from_json( signing_keys_json );
         trans_doc.append( kvp( "signing_keys", keys_value ) );
      } catch( bsoncxx::exception& e ) {
         // should never fail, so don't attempt to remove invalid utf8
         elog( "Unable to convert signing keys JSON to MongoDB JSON: ${e}", ("e", e.what()) );
         elog( "  JSON: ${j}", ("j", signing_keys_json) );
      }
   }

   trans_doc.append( kvp( "accepted", b_bool{t->accepted} ) );
   trans_doc.append( kvp( "implicit", b_bool{t->implicit} ) );
   trans_doc.append( kvp( "scheduled", b_bool{t->scheduled} ) );

   trans_doc.append( kvp( "createdAt", b_date{now} ) );

   try {
      mongocxx::options::update update_opts{};
      update_opts.upsert( true );
      if( !_trans.update_one( make_document( kvp( "trx_id", trx_id_str ) ),
                              make_document( kvp( "$set", trans_doc.view() ) ), update_opts ) ) {
         EOS_ASSERT( false, chain::mongo_db_insert_fail, "Failed to insert trans ${id}", ("id", trx_id) );
      }
   } catch( ... ) {
      handle_mongo_exception( "trans insert", __LINE__ );
   }
}

bool
mongo_db_plugin_impl::on_cancel(const chain::action& a,
                                        const chain::transaction_trace_ptr& t)
{
   using namespace bsoncxx::types;
   using bsoncxx::type;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   auto done_data_doc = bsoncxx::builder::basic::document{};
   const chain::action& base = a; // without inline action traces
   bool updated = false;
   std::string maker;
   int32_t type;
   std::string order_or_pair_id;

   try {
      auto v = to_variant_with_abi( base );
      
      string json = fc::json::to_string( v );
      //ilog( " on_cancel JSON: ${j}", ("j", json) );
      try {
         const auto& value = bsoncxx::from_json( json );
         bsoncxx::document::view value_view = value.view();
         bsoncxx::document::element data_ele{value_view["data"]};  
         //ilog( " on_cancel data JSON : ${data}", ("data", bsoncxx::to_json( data_ele.get_document().value ) ) );
         bsoncxx::document::view data_view = data_ele.get_document().value;
         bsoncxx::document::element maker_ele{value_view["data"]["maker"]};
         auto  maker_view = maker_ele.get_utf8().value;
         maker = ::std::string(maker_view.data(), maker_view.size());
         //ilog( " on_cancel deal maker JSON : ${maker}", ( "maker", maker ) );
         bsoncxx::document::element type_ele{value_view["data"]["type"]};
         if (type_ele && type_ele.type() == type::k_int32) {
            ilog( " on_cancel 111111   type JSON: ${data}", ("data", type_ele.get_int32().value ) );
            type = (int64_t)type_ele.get_int32().value;
         } else {
            ilog( " on_cancel 2222   data_id unknown type ${type}", ("type", (uint8_t)type_ele.type()) );
            return false;
         }
         
         bsoncxx::document::element order_or_pair_id_ele{value_view["data"]["order_or_pair_id"]};
         if (order_or_pair_id_ele && order_or_pair_id_ele.type() == type::k_int32) {
            ilog( " on_cancel 111111   order_or_pair_id JSON: ${data}", ("data", order_or_pair_id_ele.get_int32().value ) );
            //order_or_pair_id = (int64_t)order_or_pair_id_ele.get_int32().value;
            order_or_pair_id = fc::to_string(order_or_pair_id_ele.get_int32().value);
         } else if (order_or_pair_id_ele && order_or_pair_id_ele.type() == type::k_int64) {
            ilog( " on_cancel 111111   order_or_pair_id JSON: ${data}", ("data", order_or_pair_id_ele.get_int64().value ) );
            //order_or_pair_id = order_or_pair_id_ele.get_int64().value;
            order_or_pair_id = fc::to_string(order_or_pair_id_ele.get_int32().value);
         } else if (order_or_pair_id_ele && order_or_pair_id_ele.type() == type::k_utf8) {
            auto order_or_pair_id_view = order_or_pair_id_ele.get_utf8().value;
            order_or_pair_id = ::std::string(order_or_pair_id_view.data(), order_or_pair_id_view.size());
            //ilog( " ins_deal 111111   order_or_pair_id JSON: ${data}", ("data", order_or_pair_id ) );
         } else {
            ilog( " on_cancel 2222   order_or_pair_id unknown type ${type}", ("type", (uint8_t)order_or_pair_id_ele.type()) );
            return false;
         }
      } catch( bsoncxx::exception& ) {
         elog( " on_cancel error, JSON: ${j}", ("j", json) );
         return false;
      }
      
      auto filter = make_document(kvp("1", 1));
      if (type == 0) {
         filter = make_document(kvp("from", maker), kvp("id", order_or_pair_id));
      } else if (type == 1) {
         filter = make_document(kvp("from", maker), kvp("pairId", order_or_pair_id));
      } else {
         filter = make_document(kvp("from", maker));
      }
      auto updated_doc = make_document(kvp("$set", make_document(kvp("status", 4) )));
     
      _orders.update_one( filter.view(), updated_doc.view() );
      updated = true;
   } catch( ... ) {
      handle_mongo_exception( "trans_traces serialization: " + t->id.str(), __LINE__ );
   }
   
   return updated;
}

bool
mongo_db_plugin_impl::on_close2(const chain::action& a,
                                        const chain::transaction_trace_ptr& t)
{
   using namespace bsoncxx::types;
   using bsoncxx::type;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   using bsoncxx::builder::basic::make_array;

   auto done_data_doc = bsoncxx::builder::basic::document{};
   const chain::action& base = a; // without inline action traces
   bool updated = false;
   std::string exc_acc;
   int32_t pair_id;

   try {
      auto v = to_variant_with_abi( base );
      
      string json = fc::json::to_string( v );
      try {
         const auto& value = bsoncxx::from_json( json );
         bsoncxx::document::view value_view = value.view();
         bsoncxx::document::element data_ele{value_view["data"]};  
         ilog( " on_close2 data JSON : ${data}", ("data", bsoncxx::to_json( data_ele.get_document().value ) ) );
         bsoncxx::document::view data_view = data_ele.get_document().value;
         bsoncxx::document::element exc_acc_ele{value_view["data"]["exc_acc"]};
         auto  exc_acc_view = exc_acc_ele.get_utf8().value;
         exc_acc = ::std::string(exc_acc_view.data(), exc_acc_view.size());
         bsoncxx::document::element pair_id_ele{value_view["data"]["pair_id"]};
         if (pair_id_ele && pair_id_ele.type() == type::k_int32) {
            pair_id = pair_id_ele.get_int32().value;
         } else {
            ilog( " on_close2 2222   pair_id unknown type ${type}", ("type", (uint8_t)pair_id_ele.type()) );
            return false;
         }
      } catch( bsoncxx::exception& ) {
         elog( " on_close2 error, JSON: ${j}", ("j", json) );
         return false;
      }
      auto filter = make_document(kvp("dexAccount", exc_acc), kvp("pairId", pair_id), kvp("status", make_document(kvp("$in", make_array(1, 2)))));
      auto updated_doc = make_document(kvp("$set", make_document(kvp("status", 5) )));

      _orders.update_one( filter.view(), updated_doc.view() );
      updated = true;
   } catch( ... ) {
      handle_mongo_exception( "on_close2: trans_traces serialization: " + t->id.str(), __LINE__ );
   }
   
   return updated;
}

bool
mongo_db_plugin_impl::on_close(const chain::action_trace& atrace,
                                        const chain::transaction_trace_ptr& t)
{
   bool added = false;
   
   for( const auto& iline_atrace : atrace.inline_traces ) {
      if (iline_atrace.act.name == N(close2)) {
         on_close2(iline_atrace.act, t);
         continue;
      }
      added |= on_close( iline_atrace, t );
   }
  
   return added;
}

string extract_asset_num(const asset& a) {
   string sign = a.get_amount() < 0 ? "-" : "";
   int64_t abs_amount = std::abs(a.get_amount());
   string result = fc::to_string( static_cast<int64_t>(abs_amount) / a.precision());
   if( a.decimals() )
   {
      auto fract = static_cast<int64_t>(abs_amount) % a.precision();
      if (fract > 0) {
         auto fractions = fc::to_string(a.precision() + fract).erase(0,1);
         result += "." + fractions.substr(0, fractions.find_last_not_of('0') + 1);
      }
   }
   return sign + result;
}

bool
mongo_db_plugin_impl::upd_order( mongocxx::bulk_write& bulk_actions, 
                                        const chain::action& a,
                                        const chain::transaction_trace_ptr& t)
{
   using namespace bsoncxx::types;
   using bsoncxx::type;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   auto done_data_doc = bsoncxx::builder::basic::document{};
   const chain::action& base = a; // without inline action traces
   bool updated = false;
   std::string buy_order_id;
   std::string sell_order_id;
   asset quantity;

   try {
      auto v = to_variant_with_abi( base );
      
      string json = fc::json::to_string( v );
      try {
         const auto& value = bsoncxx::from_json( json );
         bsoncxx::document::view value_view = value.view();
         bsoncxx::document::element data_ele{value_view["data"]};  
         bsoncxx::document::view data_view = data_ele.get_document().value;
         bsoncxx::document::element id_ele{value_view["data"]["id"]};
         std:string id;
         if (id_ele && id_ele.type() == type::k_utf8) {
            auto id_view = id_ele.get_utf8().value;
            id = ::std::string(id_view.data(), id_view.size());
            //ilog( " upd_order 111111   id JSON: ${id}", ("id", id ) );
         } else {
            ilog( " upd_order 2222   id unknown type ${type}", ("type", (uint8_t)id_ele.type()) );
            return false;
         }
         
         mongocxx::collection _tab;
         auto _deal = _deals.find_one( bsoncxx::builder::basic::make_document( kvp("id", id/*id_ele.get_document().value*/) ));
         if (_deal) {
            ilog( " upd_order:  action ${action}, deal id ${id} has already been processed!", ("action", base.name)("id", id) );
            return false;
         }
            
         bsoncxx::document::element matched = data_view["quantity"];
         auto  matchedQty = matched.get_utf8().value;
         auto  qty = ::std::string(matchedQty.data(), matchedQty.size());
         //ilog( " upd_order deal quantity JSON : ${data}", ( "data", qty ) );
         quantity = asset::from_string(qty);
         
         bsoncxx::document::element buy_id_ele{data_view["buy_order_id"]};
         if (buy_id_ele && buy_id_ele.type() == type::k_int32) {
            //ilog( " upd_order 111111   buy_order_id JSON: ${data}", ("data", buy_id_ele.get_int32().value ) );
            //buy_order_id = (int64_t)buy_id_ele.get_int32().value;
            buy_order_id = fc::to_string(buy_id_ele.get_int32().value);
         } else if (buy_id_ele && buy_id_ele.type() == type::k_int64) {
            //ilog( " upd_order 111111   buy_order_id JSON: ${data}", ("data", buy_id_ele.get_int64().value ) );
            //buy_order_id = buy_id_ele.get_int64().value;
            buy_order_id = fc::to_string(buy_id_ele.get_int64().value);
         } else if (buy_id_ele && buy_id_ele.type() == type::k_utf8) {
            auto buy_id_view = buy_id_ele.get_utf8().value;
            buy_order_id = ::std::string(buy_id_view.data(), buy_id_view.size());
            //ilog( " upd_order 111111   id JSON: ${id}", ("id", buy_order_id ) );
         } else {
            ilog( " upd_order 2222   buy_order_id unknown type ${type}", ("type", (uint8_t)buy_id_ele.type()) );
            return false;
         }
         
         bsoncxx::document::element sell_id_ele{data_view["sell_order_id"]};
         if (sell_id_ele && sell_id_ele.type() == type::k_int32) {
            //ilog( " upd_order 111111   sell_order_id JSON: ${data}", ("data", sell_id_ele.get_int32().value ) );
            //sell_order_id = (uint64_t)sell_id_ele.get_int32().value;
            sell_order_id = fc::to_string(sell_id_ele.get_int32().value);
         } else if (sell_id_ele && sell_id_ele.type() == type::k_int64) {
            ilog( " upd_order 111111   sell_order_id JSON: ${data}", ("data", sell_id_ele.get_int64().value ) );
            //sell_order_id = sell_id_ele.get_int64().value;
            sell_order_id = fc::to_string(sell_id_ele.get_int64().value);
         } else if (sell_id_ele && sell_id_ele.type() == type::k_utf8) {
            auto sell_id_view = sell_id_ele.get_utf8().value;
            sell_order_id = ::std::string(sell_id_view.data(), sell_id_view.size());
            //ilog( " ins_deal 111111   id JSON: ${id}", ("id", sell_order_id ) );
         } else {
            ilog( " upd_order 2222   sell_order_id unknown type ${type}", ("type", (uint8_t)sell_id_ele.type()) );
            return false;
         }
         
         EOS_ASSERT( buy_order_id != sell_order_id, chain::mongo_db_insert_fail, 
               "buy_order_id ${buy_order_id} can not be same with sell_order_id ${sell_order_id}", ("buy_order_id", buy_order_id)("sell_order_id", sell_order_id) );
      } catch( bsoncxx::exception& ) {
         elog( " upd_order error, JSON: ${j}", ("j", json) );
         return false;
      }
      // buy order
      auto order = _orders.find_one( bsoncxx::builder::basic::make_document( kvp("id", buy_order_id) ));
      if (!order) {
         ilog( " upd_order:  can not find the buy order in orderbook, action ${action}, id ${id}", ("action", base.name)("id", buy_order_id) );
         return false;
      }
      auto order_view = order->view();
      bsoncxx::document::element total_qty_elem = order_view["totalQty"];
      auto total_qty_view = total_qty_elem.get_utf8().value;
      auto total_qty = asset::from_string(::std::string( total_qty_view.data(), total_qty_view.size() ) );
      bsoncxx::document::element order_qty_elem = order_view["matchedQty"];
      auto order_qty_view = order_qty_elem.get_utf8().value;
      auto  order_qty = asset::from_string(::std::string( order_qty_view.data(), order_qty_view.size() ) );
      
      auto filter = make_document(kvp("id", buy_order_id));
      order_qty += quantity;
      
      //ilog( " upd_order buy order deal quantity -------- JSON : ${data},  order_qty ${order_qty}", ( "data", quantity )("order_qty", order_qty) );
      auto updated_doc = make_document(kvp("1", 1));
      if (order_qty == total_qty) {
         updated_doc = make_document(kvp("$set", make_document( kvp("matchedQty", order_qty.to_string()), kvp("matchedQtyNum", extract_asset_num(order_qty)), kvp("status", 3) )));
      } else {
         updated_doc = make_document(kvp("$set", make_document( kvp("matchedQty", order_qty.to_string()), kvp("matchedQtyNum", extract_asset_num(order_qty)), kvp("status", 2) )));
      }
      _orders.update_one(filter.view(), updated_doc.view());
      //mongocxx::model::update_one update_op{filter.view(), updated_doc.view()};
      //bulk_actions.append( update_op );
      //updated = true;
      
      // sell order
      order = _orders.find_one( bsoncxx::builder::basic::make_document( kvp("id", sell_order_id) ));
      if (!order) {
         ilog( " upd_order:  can not find the sell order in orderbook, action ${action}, id ${id}", ("action", base.name)("id", sell_order_id) );
         return false;
      }
      order_view = order->view();
      total_qty_elem = order_view["totalQty"];
      total_qty_view = total_qty_elem.get_utf8().value;
      total_qty = asset::from_string(::std::string( total_qty_view.data(), total_qty_view.size() ) );
      order_qty_elem = order_view["matchedQty"];
      order_qty_view = order_qty_elem.get_utf8().value;
      order_qty = asset::from_string(::std::string( order_qty_view.data(), order_qty_view.size() ) );
      filter = make_document(kvp("id", sell_order_id));
      order_qty += quantity;
      ilog( " upd_order sell order deal quantity -------- JSON : ${data},  order_qty ${order_qty}", ( "data", quantity )("order_qty", order_qty) );
      if (order_qty == total_qty) {
         updated_doc = make_document(kvp("$set", make_document( kvp("matchedQty", order_qty.to_string()), kvp("matchedQtyNum", extract_asset_num(order_qty)), kvp("status", 3) )));
      } else {
         updated_doc = make_document(kvp("$set", make_document(kvp("matchedQty", order_qty.to_string()), kvp("matchedQtyNum", extract_asset_num(order_qty)), kvp("status", 2) )));
      }
      _orders.update_one(filter.view(), updated_doc.view());
      //update_op = mongocxx::model::update_one{filter.view(), updated_doc.view()};
      //bulk_actions.append( update_op );
   } catch( ... ) {
      handle_mongo_exception( "trans_traces serialization: " + t->id.str(), __LINE__ );
   }
   
   return updated;
}

bool
mongo_db_plugin_impl::ins_order( mongocxx::bulk_write& bulk_actions, 
                                        const chain::action& a,
                                        const chain::transaction_trace_ptr& t)
{
   using namespace bsoncxx::types;
   using bsoncxx::type;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   auto done_data_doc = bsoncxx::builder::basic::document{};
   const chain::action& base = a; // without inline action traces
   bool added = false;

   try {
      auto v = to_variant_with_abi( base );
      
      string json = fc::json::to_string( v );
      //ilog( " insert_order JSON: ${j}", ("j", json) );
      try {
         const auto& value = bsoncxx::from_json( json );
         bsoncxx::document::view value_view = value.view();
         bsoncxx::document::element data_ele{value_view["data"]};  
        
         bsoncxx::document::view data_view = data_ele.get_document().value;
         bsoncxx::document::element id_ele{data_view["id"]};
         std::string id;
         if (id_ele && id_ele.type() == type::k_utf8) {
            auto id_view = id_ele.get_utf8().value;
            id = ::std::string(id_view.data(), id_view.size());
            //ilog( " ins_order 111111   id JSON: ${id}", ("id", id ) );
         } else {
            ilog( " insert_order:   id unknown type ${type}", ("type", (uint8_t)id_ele.type()) );
            return false;
         }
         
         auto act = _orders.find_one( bsoncxx::builder::basic::make_document( kvp("id", id/*id_ele.get_document().value*/) ));
         if (act) {
            ilog( " insert_order:  action ${action} with the same id ${id} has already exists!", ("action", base.name)("id", id) );
            return false;
         }
         //ilog( " insert_order 22222 JSON: ${data}", ("data", bsoncxx::to_json( data_ele.get_document().value ) ) );
         done_data_doc.append( bsoncxx::builder::concatenate_doc{data_ele.get_document().value} );
         
         // asset to num, "totalQty", "matchedQty", "matchedQty", "price"
         auto asset_elem = value_view["data"]["totalQty"];
         auto asset_view = asset_elem.get_utf8().value;
         auto ass = asset::from_string(::std::string( asset_view.data(), asset_view.size() ) );
         done_data_doc.append( kvp( "totalQtyNum", extract_asset_num(ass) ),  kvp( "totalQtySym", ass.symbol_name() ));
         
         asset_elem = value_view["data"]["matchedQty"];
         asset_view = asset_elem.get_utf8().value;
         ass = asset::from_string(::std::string( asset_view.data(), asset_view.size() ) );
         done_data_doc.append( kvp( "matchedQtyNum", extract_asset_num(ass) ),  kvp( "matchedQtySym", ass.symbol_name() ));

         asset_elem = value_view["data"]["price"];
         asset_view = asset_elem.get_utf8().value;
         ass = asset::from_string(::std::string( asset_view.data(), asset_view.size() ) );
         done_data_doc.append( kvp( "priceNum", extract_asset_num(ass) ),  kvp( "priceSym", ass.symbol_name() ));
         
      } catch( bsoncxx::exception& e) {
         elog( "insert_order: Unable to convert transaction JSON to MongoDB JSON: ${e}", ("e", e.what()) );
         elog( "  JSON: ${j}", ("j", json) );
         return false;
      }
      done_data_doc.append( kvp( "trxId", t->id.str() ) );
      mongocxx::model::insert_one insert_op{done_data_doc.view()};
      bulk_actions.append( insert_op );
      added = true;
   } catch( ... ) {
      handle_mongo_exception( "trans_traces serialization: " + t->id.str(), __LINE__ );
   }
 
   return added;
}

bool
mongo_db_plugin_impl::ins_deal( mongocxx::bulk_write& bulk_actions, 
                                        const chain::action& a,
                                        const chain::transaction_trace_ptr& t)
{
   using namespace bsoncxx::types;
   using bsoncxx::type;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   auto done_data_doc = bsoncxx::builder::basic::document{};
   const chain::action& base = a; // without inline action traces
   bool added = false;

   try {
      auto v = to_variant_with_abi( base );
      
      string json = fc::json::to_string( v );
      //ilog( " ins_deal JSON: ${j}", ("j", json) );
      try {
         const auto& value = bsoncxx::from_json( json );
         bsoncxx::document::view value_view = value.view();
         bsoncxx::document::element data_ele{value_view["data"]};  
        
         bsoncxx::document::view data_view = data_ele.get_document().value;
         bsoncxx::document::element id_ele{data_view["id"]};
         std::string id;
         if (id_ele && id_ele.type() == type::k_int32) {
            //ilog( " ins_deal 111111   id JSON: ${id}", ("id", id_ele.get_int32().value ) );
            //id = (uint64_t)id_ele.get_int32().value;
            id = fc::to_string(id_ele.get_int32().value);
         } else if (id_ele && id_ele.type() == type::k_int64) {
            //ilog( " ins_deal 111111   id JSON: ${id}", ("id", id_ele.get_int64().value ) );
            //id = id_ele.get_int64().value;
            id = fc::to_string(id_ele.get_int64().value);
         } else if (id_ele && id_ele.type() == type::k_utf8) {
            auto id_view = id_ele.get_utf8().value;
            id = ::std::string(id_view.data(), id_view.size());
            //ilog( " ins_deal 111111   id JSON: ${id}", ("id", id ) );
         } else {
            ilog( " ins_deal:   data_id unknown type ${type}", ("type", (uint8_t)id_ele.type()) );
            return false;
         }
         
         auto act = _deals.find_one( bsoncxx::builder::basic::make_document( kvp("id", id/*id_ele.get_document().value*/) ));
         if (act) {
            ilog( " ins_deal:  action ${action} with the same id ${id} has already exists!", ("action", base.name)("id", id) );
            return false;
         }
         //ilog( " ins_deal 22222 JSON: ${data}", ("data", bsoncxx::to_json( data_ele.get_document().value ) ) );
         done_data_doc.append( bsoncxx::builder::concatenate_doc{data_ele.get_document().value} );
         
         // asset to num, "price", "quantity", "buy_fee", "sell_fee"
         auto asset_elem = value_view["data"]["price"];
         auto asset_view = asset_elem.get_utf8().value;
         auto ass = asset::from_string(::std::string( asset_view.data(), asset_view.size() ) );
         done_data_doc.append( kvp( "priceNum", extract_asset_num(ass) ),  kvp( "priceSym", ass.symbol_name() ));
         
         asset_elem = value_view["data"]["quantity"];
         asset_view = asset_elem.get_utf8().value;
         ass = asset::from_string(::std::string( asset_view.data(), asset_view.size() ) );
         done_data_doc.append( kvp( "quantityNum", extract_asset_num(ass) ),  kvp( "quantitySym", ass.symbol_name() ));
            
         asset_elem = value_view["data"]["buy_fee"];
         asset_view = asset_elem.get_utf8().value;
         ass = asset::from_string(::std::string( asset_view.data(), asset_view.size() ) );
         done_data_doc.append( kvp( "buy_feeNum", extract_asset_num(ass) ),  kvp( "buy_feeSym", ass.symbol_name() ));
         
         asset_elem = value_view["data"]["sell_fee"];
         asset_view = asset_elem.get_utf8().value;
         ass = asset::from_string(::std::string( asset_view.data(), asset_view.size() ) );
         done_data_doc.append( kvp( "sell_feeNum", extract_asset_num(ass) ),  kvp( "sell_feeSym", ass.symbol_name() ));   
         
      } catch( bsoncxx::exception& e) {
         elog( "Unable to convert transaction JSON to MongoDB JSON: ${e}", ("e", e.what()) );
         elog( "  JSON: ${j}", ("j", json) );
         return false;
      }
      done_data_doc.append( kvp( "trxId", t->id.str() ) );
      mongocxx::model::insert_one insert_op{done_data_doc.view()};
      bulk_actions.append( insert_op );
      added = true;
   } catch( ... ) {
      handle_mongo_exception( "trans_traces serialization: " + t->id.str(), __LINE__ );
   }
 
   return added;
}

bool
mongo_db_plugin_impl::add_match_action( mongocxx::bulk_write& bulk_actions, const chain::action_trace& atrace,
                                        const chain::transaction_trace_ptr& t, name action, int op)
{
   bool added = false;
   
   for( const auto& iline_atrace : atrace.inline_traces ) {
      if (iline_atrace.act.name == N(morder)) {
         added |= ins_order(bulk_actions, iline_atrace.act, t);
         continue;
      } else if (iline_atrace.act.name == action) {
         if (op == 1)
            added |= ins_deal(bulk_actions, iline_atrace.act, t);
         else
            added |= upd_order(bulk_actions, iline_atrace.act, t);
         continue;
      }
      
      added |= add_match_action( bulk_actions, iline_atrace, t, action, op );
   }
  
   return added;
}

bool
mongo_db_plugin_impl::add_match( const chain::action_trace& atrace,
                                        const chain::transaction_trace_ptr& t)
{
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;

   bool added = false;

   mongocxx::options::bulk_write bulk_opts;
   bulk_opts.ordered(false);
   mongocxx::bulk_write bulk_action_orders = _orders.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_deals = _deals.create_bulk_write(bulk_opts);
   bool write_data = false;
   
   write_data |= add_match_action( bulk_action_orders, atrace, t, N(morder), 1 );
   if (write_data) {
      // insert deals
      try {
         if( !bulk_action_orders.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action orders insert", __LINE__ );
      }
   }
   
   write_data = false;
   write_data |= add_match_action( bulk_action_orders, atrace, t, N(done), 2 );
   if (write_data) {
      // insert deals
      try {
         if( !bulk_action_orders.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action orders update", __LINE__ );
      }
   }

   write_data = false;
   write_data |= add_match_action( bulk_action_deals, atrace, t, N(done), 1 );
   if (write_data) {
      // insert deals
      try {
         if( !bulk_action_deals.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action deals insert", __LINE__ );
      }
   }

   return added;
}

void
mongo_db_plugin_impl::add_trade( const chain::transaction_trace_ptr& t )
{
   for( const auto& atrace : t->action_traces ) {
      try {
         if (atrace.act.account == chain::config::match_account_name && atrace.act.name == N(cancel))
            on_cancel(atrace.act, t);
         else if (atrace.act.account == chain::config::match_account_name && atrace.act.name == N(close2))
            on_close2(atrace.act, t);
         else if (atrace.act.account == chain::config::match_account_name && atrace.act.name == N(close))
            on_close(atrace, t);
         else if ( (atrace.act.account == chain::config::relay_token_account_name || atrace.act.account == chain::config::token_account_name) && atrace.act.name == N(trade))
            add_match( atrace, t );
      } catch(...) {
         handle_mongo_exception("add_trade", __LINE__);
      }
   }
}

bool
mongo_db_plugin_impl::add_action_trace( mongocxx::bulk_write& bulk_action_traces, const chain::action_trace& atrace,
                                        const chain::transaction_trace_ptr& t,
                                        bool executed, const std::chrono::milliseconds& now,
                                        bool& write_ttrace )
{
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;

   if( executed && atrace.receipt.receiver == chain::config::system_account_name ) {
      update_account( atrace.act );
   }

   bool added = false;
   const bool in_filter = (store_action_traces || store_transaction_traces) && start_block_reached &&
                    filter_include( atrace.receipt.receiver, atrace.act.name, atrace.act.authorization );
   write_ttrace |= in_filter;
   if( start_block_reached && store_action_traces && in_filter ) {
      auto action_traces_doc = bsoncxx::builder::basic::document{};
      const chain::base_action_trace& base = atrace; // without inline action traces

      auto v = to_variant_with_abi( base );
      string json = fc::json::to_string( v );
      try {
         const auto& value = bsoncxx::from_json( json );
         action_traces_doc.append( bsoncxx::builder::concatenate_doc{value.view()} );
      } catch( bsoncxx::exception& ) {
         try {
            json = fc::prune_invalid_utf8( json );
            const auto& value = bsoncxx::from_json( json );
            action_traces_doc.append( bsoncxx::builder::concatenate_doc{value.view()} );
            action_traces_doc.append( kvp( "non-utf8-purged", b_bool{true} ) );
         } catch( bsoncxx::exception& e ) {
            elog( "Unable to convert action trace JSON to MongoDB JSON: ${e}", ("e", e.what()) );
            elog( "  JSON: ${j}", ("j", json) );
         }
      }
      if( t->receipt.valid() ) {
         action_traces_doc.append( kvp( "trx_status", std::string( t->receipt->status ) ) );
      }
      action_traces_doc.append( kvp( "createdAt", b_date{now} ) );

      mongocxx::model::insert_one insert_op{action_traces_doc.view()};
      bulk_action_traces.append( insert_op );
      added = true;
   }

   for( const auto& iline_atrace : atrace.inline_traces ) {
      added |= add_action_trace( bulk_action_traces, iline_atrace, t, executed, now, write_ttrace );
   }

   return added;
}


void mongo_db_plugin_impl::_process_applied_transaction( const chain::transaction_trace_ptr& t ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;

   auto trans_traces_doc = bsoncxx::builder::basic::document{};

   auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
         std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()});

   mongocxx::options::bulk_write bulk_opts;
   bulk_opts.ordered(false);
   mongocxx::bulk_write bulk_action_traces = _action_traces.create_bulk_write(bulk_opts);
   bool write_atraces = false;
   bool write_ttrace = false; // filters apply to transaction_traces as well
   bool executed = t->receipt.valid() && t->receipt->status == chain::transaction_receipt_header::executed;

   for( const auto& atrace : t->action_traces ) {
      try {
         write_atraces |= add_action_trace( bulk_action_traces, atrace, t, executed, now, write_ttrace );
      } catch(...) {
         handle_mongo_exception("add action traces", __LINE__);
      }
   }

   if( !start_block_reached ) return; //< add_action_trace calls update_account which must be called always

   // transaction trace insert

   if( store_transaction_traces && write_ttrace ) {
      try {
         auto v = to_variant_with_abi( *t );
         string json = fc::json::to_string( v );
         try {
            const auto& value = bsoncxx::from_json( json );
            trans_traces_doc.append( bsoncxx::builder::concatenate_doc{value.view()} );
         } catch( bsoncxx::exception& ) {
            try {
               json = fc::prune_invalid_utf8( json );
               const auto& value = bsoncxx::from_json( json );
               trans_traces_doc.append( bsoncxx::builder::concatenate_doc{value.view()} );
               trans_traces_doc.append( kvp( "non-utf8-purged", b_bool{true} ) );
            } catch( bsoncxx::exception& e ) {
               elog( "Unable to convert transaction JSON to MongoDB JSON: ${e}", ("e", e.what()) );
               elog( "  JSON: ${j}", ("j", json) );
            }
         }
         trans_traces_doc.append( kvp( "createdAt", b_date{now} ) );

         try {
            if( !_trans_traces.insert_one( trans_traces_doc.view() ) ) {
               EOS_ASSERT( false, chain::mongo_db_insert_fail, "Failed to insert trans ${id}", ("id", t->id) );
            }
         } catch( ... ) {
            handle_mongo_exception( "trans_traces insert: " + json, __LINE__ );
         }
      } catch( ... ) {
         handle_mongo_exception( "trans_traces serialization: " + t->id.str(), __LINE__ );
      }
   }

   // insert action_traces
   if( write_atraces ) {
      try {
         if( !bulk_action_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   add_trade(t);
}

void mongo_db_plugin_impl::_process_accepted_block( const chain::block_state_ptr& bs ) {
   using namespace bsoncxx::types;
   using namespace bsoncxx::builder;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   auto block_num = bs->block_num;
   if( block_num % 1000 == 0 )
      ilog( "block_num: ${b}", ("b", block_num) );
   const auto& block_id = bs->id;
   const auto block_id_str = block_id.str();

   auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
         std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()});

   if( store_block_states ) {
      auto block_state_doc = bsoncxx::builder::basic::document{};
      block_state_doc.append( kvp( "block_num", b_int32{static_cast<int32_t>(block_num)} ),
                              kvp( "block_id", block_id_str ),
                              kvp( "validated", b_bool{bs->validated} ) );

      const chain::block_header_state& bhs = *bs;

      auto json = fc::json::to_string( bhs );
      try {
         const auto& value = bsoncxx::from_json( json );
         block_state_doc.append( kvp( "block_header_state", value ) );
      } catch( bsoncxx::exception& ) {
         try {
            json = fc::prune_invalid_utf8( json );
            const auto& value = bsoncxx::from_json( json );
            block_state_doc.append( kvp( "block_header_state", value ) );
            block_state_doc.append( kvp( "non-utf8-purged", b_bool{true} ) );
         } catch( bsoncxx::exception& e ) {
            elog( "Unable to convert block_header_state JSON to MongoDB JSON: ${e}", ("e", e.what()) );
            elog( "  JSON: ${j}", ("j", json) );
         }
      }
      block_state_doc.append( kvp( "createdAt", b_date{now} ) );

      try {
         if( update_blocks_via_block_num ) {
            if( !_block_states.update_one( make_document( kvp( "block_num", b_int32{static_cast<int32_t>(block_num)} ) ),
                                           make_document( kvp( "$set", block_state_doc.view() ) ), update_opts ) ) {
               EOS_ASSERT( false, chain::mongo_db_insert_fail, "Failed to insert block_state ${num}", ("num", block_num) );
            }
         } else {
            if( !_block_states.update_one( make_document( kvp( "block_id", block_id_str ) ),
                                           make_document( kvp( "$set", block_state_doc.view() ) ), update_opts ) ) {
               EOS_ASSERT( false, chain::mongo_db_insert_fail, "Failed to insert block_state ${bid}", ("bid", block_id) );
            }
         }
      } catch( ... ) {
         handle_mongo_exception( "block_states insert: " + json, __LINE__ );
      }
   }

   if( store_blocks ) {
      auto block_doc = bsoncxx::builder::basic::document{};
      block_doc.append( kvp( "block_num", b_int32{static_cast<int32_t>(block_num)} ),
                        kvp( "block_id", block_id_str ) );

      auto v = to_variant_with_abi( *bs->block );
      auto json = fc::json::to_string( v );
      try {
         const auto& value = bsoncxx::from_json( json );
         block_doc.append( kvp( "block", value ) );
      } catch( bsoncxx::exception& ) {
         try {
            json = fc::prune_invalid_utf8( json );
            const auto& value = bsoncxx::from_json( json );
            block_doc.append( kvp( "block", value ) );
            block_doc.append( kvp( "non-utf8-purged", b_bool{true} ) );
         } catch( bsoncxx::exception& e ) {
            elog( "Unable to convert block JSON to MongoDB JSON: ${e}", ("e", e.what()) );
            elog( "  JSON: ${j}", ("j", json) );
         }
      }
      block_doc.append( kvp( "createdAt", b_date{now} ) );

      try {
         if( update_blocks_via_block_num ) {
            if( !_blocks.update_one( make_document( kvp( "block_num", b_int32{static_cast<int32_t>(block_num)} ) ),
                                     make_document( kvp( "$set", block_doc.view() ) ), update_opts ) ) {
               EOS_ASSERT( false, chain::mongo_db_insert_fail, "Failed to insert block ${num}", ("num", block_num) );
            }
         } else {
            if( !_blocks.update_one( make_document( kvp( "block_id", block_id_str ) ),
                                     make_document( kvp( "$set", block_doc.view() ) ), update_opts ) ) {
               EOS_ASSERT( false, chain::mongo_db_insert_fail, "Failed to insert block ${bid}", ("bid", block_id) );
            }
         }
      } catch( ... ) {
         handle_mongo_exception( "blocks insert: " + json, __LINE__ );
      }
   }
}

void mongo_db_plugin_impl::_process_irreversible_block(const chain::block_state_ptr& bs)
{
   using namespace bsoncxx::types;
   using namespace bsoncxx::builder;
   using bsoncxx::builder::basic::make_document;
   using bsoncxx::builder::basic::kvp;


   const auto block_id = bs->block->id();
   const auto block_id_str = block_id.str();

   auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
         std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()});

   if( store_blocks ) {
      auto ir_block = find_block( _blocks, block_id_str );
      if( !ir_block ) {
         _process_accepted_block( bs );
         ir_block = find_block( _blocks, block_id_str );
         if( !ir_block ) return; // should never happen
      }

      auto update_doc = make_document( kvp( "$set", make_document( kvp( "irreversible", b_bool{true} ),
                                                                   kvp( "validated", b_bool{bs->validated} ),
                                                                   kvp( "updatedAt", b_date{now} ) ) ) );

      _blocks.update_one( make_document( kvp( "_id", ir_block->view()["_id"].get_oid() ) ), update_doc.view() );
   }

   if( store_block_states ) {
      auto ir_block = find_block( _block_states, block_id_str );
      if( !ir_block ) {
         _process_accepted_block( bs );
         ir_block = find_block( _block_states, block_id_str );
         if( !ir_block ) return; // should never happen
      }

      auto update_doc = make_document( kvp( "$set", make_document( kvp( "irreversible", b_bool{true} ),
                                                                   kvp( "validated", b_bool{bs->validated} ),
                                                                   kvp( "updatedAt", b_date{now} ) ) ) );

      _block_states.update_one( make_document( kvp( "_id", ir_block->view()["_id"].get_oid() ) ), update_doc.view() );
   }

   if( store_transactions ) {
      const auto block_num = bs->block->block_num();
      bool transactions_in_block = false;
      mongocxx::options::bulk_write bulk_opts;
      bulk_opts.ordered( false );
      auto bulk = _trans.create_bulk_write( bulk_opts );

      for( const auto& receipt : bs->block->transactions ) {
         string trx_id_str;
         if( receipt.trx.contains<packed_transaction>() ) {
            const auto& pt = receipt.trx.get<packed_transaction>();
            // get id via get_raw_transaction() as packed_transaction.id() mutates internal transaction state
            const auto& raw = pt.get_raw_transaction();
            const auto& trx = fc::raw::unpack<transaction>( raw );
            if( !filter_include( trx ) ) continue;
            const auto& id = trx.id();
            trx_id_str = id.str();
         } else {
            const auto& id = receipt.trx.get<transaction_id_type>();
            trx_id_str = id.str();
         }

         auto update_doc = make_document( kvp( "$set", make_document( kvp( "irreversible", b_bool{true} ),
                                                                      kvp( "block_id", block_id_str ),
                                                                      kvp( "block_num", b_int32{static_cast<int32_t>(block_num)} ),
                                                                      kvp( "updatedAt", b_date{now} ) ) ) );

         mongocxx::model::update_one update_op{make_document( kvp( "trx_id", trx_id_str ) ), update_doc.view()};
         update_op.upsert( false );
         bulk.append( update_op );
         transactions_in_block = true;
      }

      if( transactions_in_block ) {
         try {
            if( !bulk.execute() ) {
               EOS_ASSERT( false, chain::mongo_db_insert_fail, "Bulk transaction insert failed for block: ${bid}", ("bid", block_id) );
            }
         } catch( ... ) {
            handle_mongo_exception( "bulk transaction insert", __LINE__ );
         }
      }
   }
}

void mongo_db_plugin_impl::add_pub_keys( const vector<chain::key_weight>& keys, const account_name& name,
                                         const permission_name& permission, const std::chrono::milliseconds& now )
{
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   using namespace bsoncxx::types;

   if( keys.empty()) return;

   mongocxx::bulk_write bulk = _pub_keys.create_bulk_write();

   for( const auto& pub_key_weight : keys ) {
      auto find_doc = bsoncxx::builder::basic::document();

      find_doc.append( kvp( "account", name.to_string()),
                       kvp( "public_key", pub_key_weight.key.operator string()),
                       kvp( "permission", permission.to_string()) );

      auto update_doc = make_document( kvp( "$set", make_document( bsoncxx::builder::concatenate_doc{find_doc.view()},
                                                                   kvp( "createdAt", b_date{now} ))));

      mongocxx::model::update_one insert_op{find_doc.view(), update_doc.view()};
      insert_op.upsert(true);
      bulk.append( insert_op );
   }

   try {
      if( !bulk.execute()) {
         EOS_ASSERT( false, chain::mongo_db_insert_fail,
                     "Bulk pub_keys insert failed for account: ${a}, permission: ${p}",
                     ("a", name)( "p", permission ));
      }
   } catch (...) {
      handle_mongo_exception( "pub_keys insert", __LINE__ );
   }
}

void mongo_db_plugin_impl::remove_pub_keys( const account_name& name, const permission_name& permission )
{
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   try {
      auto result = _pub_keys.delete_many( make_document( kvp( "account", name.to_string()),
                                                         kvp( "permission", permission.to_string())));
      if( !result ) {
         EOS_ASSERT( false, chain::mongo_db_update_fail,
                     "pub_keys delete failed for account: ${a}, permission: ${p}",
                     ("a", name)( "p", permission ));
      }
   } catch (...) {
      handle_mongo_exception( "pub_keys delete", __LINE__ );
   }
}

void mongo_db_plugin_impl::add_account_control( const vector<chain::permission_level_weight>& controlling_accounts,
                                                const account_name& name, const permission_name& permission,
                                                const std::chrono::milliseconds& now )
{
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   using namespace bsoncxx::types;

   if( controlling_accounts.empty()) return;

   mongocxx::bulk_write bulk = _account_controls.create_bulk_write();

   for( const auto& controlling_account : controlling_accounts ) {
      auto find_doc = bsoncxx::builder::basic::document();

      find_doc.append( kvp( "controlled_account", name.to_string()),
                       kvp( "controlled_permission", permission.to_string()),
                       kvp( "controlling_account", controlling_account.permission.actor.to_string()) );

      auto update_doc = make_document( kvp( "$set", make_document( bsoncxx::builder::concatenate_doc{find_doc.view()},
                                                                   kvp( "createdAt", b_date{now} ))));


      mongocxx::model::update_one insert_op{find_doc.view(), update_doc.view()};
      insert_op.upsert(true);
      bulk.append( insert_op );
   }

   try {
      if( !bulk.execute()) {
         EOS_ASSERT( false, chain::mongo_db_insert_fail,
                     "Bulk account_controls insert failed for account: ${a}, permission: ${p}",
                     ("a", name)( "p", permission ));
      }
   } catch (...) {
      handle_mongo_exception( "account_controls insert", __LINE__ );
   }
}

void mongo_db_plugin_impl::remove_account_control( const account_name& name, const permission_name& permission )
{
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   try {
      auto result = _account_controls.delete_many( make_document( kvp( "controlled_account", name.to_string()),
                                                                  kvp( "controlled_permission", permission.to_string())));
      if( !result ) {
         EOS_ASSERT( false, chain::mongo_db_update_fail,
                     "account_controls delete failed for account: ${a}, permission: ${p}",
                     ("a", name)( "p", permission ));
      }
   } catch (...) {
      handle_mongo_exception( "account_controls delete", __LINE__ );
   }
}

namespace {

void create_account( mongocxx::collection& accounts, const name& name, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   const string name_str = name.to_string();
   auto update = make_document(
         kvp( "$set", make_document( kvp( "name", name_str),
                                     kvp( "createdAt", b_date{now} ))));
   try {
      if( !accounts.update_one( make_document( kvp( "name", name_str )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert account ${n}", ("n", name));
      }
   } catch (...) {
      handle_mongo_exception( "create_account", __LINE__ );
   }
}

}

void mongo_db_plugin_impl::update_account(const chain::action& act)
{
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   using namespace bsoncxx::types;

   if (act.account != chain::config::system_account_name)
      return;

   try {
      if( act.name == newaccount ) {
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
         auto newacc = act.data_as<chain::newaccount>();

         create_account( _accounts, newacc.name, now );

         add_pub_keys( newacc.owner.keys, newacc.name, owner, now );
         add_account_control( newacc.owner.accounts, newacc.name, owner, now );
         add_pub_keys( newacc.active.keys, newacc.name, active, now );
         add_account_control( newacc.active.accounts, newacc.name, active, now );

      } else if( act.name == updateauth ) {
         auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
         const auto update = act.data_as<chain::updateauth>();
         remove_pub_keys(update.account, update.permission);
         remove_account_control(update.account, update.permission);
         add_pub_keys(update.auth.keys, update.account, update.permission, now);
         add_account_control(update.auth.accounts, update.account, update.permission, now);

      } else if( act.name == deleteauth ) {
         const auto del = act.data_as<chain::deleteauth>();
         remove_pub_keys( del.account, del.permission );
         remove_account_control(del.account, del.permission);

      } else if( act.name == setabi ) {
         auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
         auto setabi = act.data_as<chain::setabi>();

         abi_cache_index.erase( setabi.account );

         auto account = find_account( _accounts, setabi.account );
         if( !account ) {
            create_account( _accounts, setabi.account, now );
            account = find_account( _accounts, setabi.account );
         }
         if( account ) {
            abi_def abi_def = fc::raw::unpack<chain::abi_def>( setabi.abi );
            const string json_str = fc::json::to_string( abi_def );

            try{
               auto update_from = make_document(
                     kvp( "$set", make_document( kvp( "abi", bsoncxx::from_json( json_str )),
                                                 kvp( "updatedAt", b_date{now} ))));

               try {
                  if( !_accounts.update_one( make_document( kvp( "_id", account->view()["_id"].get_oid())),
                                             update_from.view())) {
                     EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to udpdate account ${n}", ("n", setabi.account));
                  }
               } catch( ... ) {
                  handle_mongo_exception( "account update", __LINE__ );
               }
            } catch( bsoncxx::exception& e ) {
               elog( "Unable to convert abi JSON to MongoDB JSON: ${e}", ("e", e.what()));
               elog( "  JSON: ${j}", ("j", json_str));
            }
         }
      }
   } catch( fc::exception& e ) {
      // if unable to unpack native type, skip account creation
   }
}

mongo_db_plugin_impl::mongo_db_plugin_impl()
{
}

mongo_db_plugin_impl::~mongo_db_plugin_impl() {
   if (!startup) {
      try {
         ilog( "mongo_db_plugin shutdown in process please be patient this can take a few minutes" );
         done = true;
         condition.notify_one();

         consume_thread.join();

         mongo_pool.reset();
      } catch( std::exception& e ) {
         elog( "Exception on mongo_db_plugin shutdown of consume thread: ${e}", ("e", e.what()));
      }
   }
}

void mongo_db_plugin_impl::wipe_database() {
   ilog("mongo db wipe_database");

   auto client = mongo_pool->acquire();
   auto& mongo_conn = *client;

   auto block_states = mongo_conn[db_name][block_states_col];
   auto blocks = mongo_conn[db_name][blocks_col];
   auto trans = mongo_conn[db_name][trans_col];
   auto trans_traces = mongo_conn[db_name][trans_traces_col];
   auto action_traces = mongo_conn[db_name][action_traces_col];
   auto accounts = mongo_conn[db_name][accounts_col];
   auto pub_keys = mongo_conn[db_name][pub_keys_col];
   auto account_controls = mongo_conn[db_name][account_controls_col];
   auto orders = mongo_conn[db_name][orders_col];
   auto deals = mongo_conn[db_name][deals_col];

   block_states.drop();
   blocks.drop();
   trans.drop();
   trans_traces.drop();
   action_traces.drop();
   accounts.drop();
   pub_keys.drop();
   account_controls.drop();
   orders.drop();
   deals.drop();
   ilog("done wipe_database");
}

chain::private_key_type get_private_key( name keyname, string role ) {
   return chain::private_key_type::regenerate<fc::ecc::private_key_shim>(fc::sha256::hash(string(keyname)+role));
}

chain::public_key_type  get_public_key( name keyname, string role ) {
   return get_private_key( keyname, role ).get_public_key();
}



void mongo_db_plugin_impl::insert_default_abi()
{
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   using namespace bsoncxx::types;
   if (b_insert_default_abi) return ;
      std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
      account_name name_account = chain::config::token_account_name;
      {
         abi_cache_index.erase( name_account );
         chain::newaccount newacc{
                                 .creator  = chain::config::system_account_name,
                                 .name     = name_account,
                                 .owner    = authority( get_public_key( name_account, "owner" ) ),
                                 .active   = authority( get_public_key( name_account, "active" ) )
                                 };
         create_account( _accounts, name_account, now );
         add_pub_keys( newacc.owner.keys, name_account, owner, now );
         add_account_control( newacc.owner.accounts, name_account, owner, now );
         add_pub_keys( newacc.active.keys, name_account, active, now );
         add_account_control( newacc.active.accounts, name_account, active, now ); 

         auto account = find_account( _accounts, name_account );
         auto abiPath = app().config_dir() / "force.token" += ".abi";
         FC_ASSERT( fc::exists( abiPath ), "no abi file found ");
         auto abijson = fc::json::from_file(abiPath).as<abi_def>();
         auto abi = fc::raw::pack(abijson);
         abi_def abi_def = fc::raw::unpack<chain::abi_def>( abi );
         const string json_str = fc::json::to_string( abi_def );
         try{
               auto update_from = make_document(
                     kvp( "$set", make_document( kvp( "abi", bsoncxx::from_json( json_str )),
                                                 kvp( "updatedAt", b_date{now} ))));

               try {
                  if( !_accounts.update_one( make_document( kvp( "_id", account->view()["_id"].get_oid())),
                                            update_from.view())) {
                     EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to udpdate account ${n}", ("n", name_account));
                  }
               } catch( ... ) {
                  handle_mongo_exception( "account update", __LINE__ );
               }
            } catch( bsoncxx::exception& e ) {
               elog( "Unable to convert abi JSON to MongoDB JSON: ${e}", ("e", e.what()));
               elog( "  JSON: ${j}", ("j", json_str));
            }
      }
      get_abi_serializer(name_account);
      name_account = chain::config::system_account_name;
      {
         abi_cache_index.erase( name_account );
         auto account = find_account( _accounts, name_account );
         fc::path abiPath = app().config_dir() / "force.system" += ".abi";
         
         FC_ASSERT( fc::exists( abiPath ), "no abi file found ");
         auto abijson = fc::json::from_file(abiPath).as<abi_def>();
         auto abi = fc::raw::pack(abijson);
         abi_def abi_def = fc::raw::unpack<chain::abi_def>( abi );
         const string json_str = fc::json::to_string( abi_def );
         try{
               auto update_from = make_document(
                     kvp( "$set", make_document( kvp( "abi", bsoncxx::from_json( json_str )),
                                                 kvp( "updatedAt", b_date{now} ))));

               try {
                  if( !_accounts.update_one( make_document( kvp( "_id", account->view()["_id"].get_oid())),
                                            update_from.view())) {
                     EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to udpdate account ${n}", ("n", name_account));
                  }
               } catch( ... ) {
                  handle_mongo_exception( "account update", __LINE__ );
               }
            } catch( bsoncxx::exception& e ) {
               elog( "Unable to convert abi JSON to MongoDB JSON: ${e}", ("e", e.what()));
               elog( "  JSON: ${j}", ("j", json_str));
            }

      }
      get_abi_serializer(name_account);
      b_insert_default_abi = true;
}

void mongo_db_plugin_impl::init() {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::make_document;
   using bsoncxx::builder::basic::kvp;
   // Create the native contract accounts manually; sadly, we can't run their contracts to make them create themselves
   // See native_contract_chain_initializer::prepare_database()

   ilog("init mongo");
   try {
      auto client = mongo_pool->acquire();
      auto& mongo_conn = *client;

      auto accounts = mongo_conn[db_name][accounts_col];
      if( accounts.count( make_document()) == 0 ) {
         auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

         auto doc = make_document( kvp( "name", name( chain::config::system_account_name ).to_string()),
                                   kvp( "createdAt", b_date{now} ));

         try {
            if( !accounts.insert_one( doc.view())) {
               EOS_ASSERT( false, chain::mongo_db_insert_fail, "Failed to insert account ${n}",
                           ("n", name( chain::config::system_account_name ).to_string()));
            }
         } catch (...) {
            handle_mongo_exception( "account insert", __LINE__ );
         }

         try {
            // blocks indexes
            auto blocks = mongo_conn[db_name][blocks_col];
            blocks.create_index( bsoncxx::from_json( R"xxx({ "block_num" : 1 })xxx" ));
            blocks.create_index( bsoncxx::from_json( R"xxx({ "block_id" : 1 })xxx" ));

            auto block_states = mongo_conn[db_name][block_states_col];
            block_states.create_index( bsoncxx::from_json( R"xxx({ "block_num" : 1 })xxx" ));
            block_states.create_index( bsoncxx::from_json( R"xxx({ "block_id" : 1 })xxx" ));

            // accounts indexes
            accounts.create_index( bsoncxx::from_json( R"xxx({ "name" : 1 })xxx" ));

            // transactions indexes
            auto trans = mongo_conn[db_name][trans_col];
            trans.create_index( bsoncxx::from_json( R"xxx({ "trx_id" : 1 })xxx" ));

            auto trans_trace = mongo_conn[db_name][trans_traces_col];
            trans_trace.create_index( bsoncxx::from_json( R"xxx({ "id" : 1 })xxx" ));

            // action traces indexes
            auto action_traces = mongo_conn[db_name][action_traces_col];
            action_traces.create_index( bsoncxx::from_json( R"xxx({ "trx_id" : 1 })xxx" ));

            // pub_keys indexes
            auto pub_keys = mongo_conn[db_name][pub_keys_col];
            pub_keys.create_index( bsoncxx::from_json( R"xxx({ "account" : 1, "permission" : 1 })xxx" ));
            pub_keys.create_index( bsoncxx::from_json( R"xxx({ "public_key" : 1 })xxx" ));

            // account_controls indexes
            auto account_controls = mongo_conn[db_name][account_controls_col];
            account_controls.create_index(
                  bsoncxx::from_json( R"xxx({ "controlled_account" : 1, "controlled_permission" : 1 })xxx" ));
            account_controls.create_index( bsoncxx::from_json( R"xxx({ "controlling_account" : 1 })xxx" ));
               
            // orderbook indexes
            auto orders = mongo_conn[db_name][orders_col];
            orders.create_index( bsoncxx::from_json( R"xxx({ "id" : 1 })xxx" ));
            orders.create_index(
                  bsoncxx::from_json( R"xxx({ "from" : 1, "id" : 1 })xxx" ));
            orders.create_index(
                  bsoncxx::from_json( R"xxx({ "from" : 1, "pairId" : 1 })xxx" ));
            
            // deals indexes
            auto deals = mongo_conn[db_name][deals_col];
            deals.create_index( bsoncxx::from_json( R"xxx({ "id" : 1 })xxx" ));

         } catch (...) {
            handle_mongo_exception( "create indexes", __LINE__ );
         }
      }
   } catch (...) {
      handle_mongo_exception( "mongo init", __LINE__ );
   }

   ilog("starting db plugin thread");

   consume_thread = boost::thread([this] { consume_blocks(); });

   startup = false;
}

////////////
// mongo_db_plugin
////////////

mongo_db_plugin::mongo_db_plugin()
:my(new mongo_db_plugin_impl)
{
}

mongo_db_plugin::~mongo_db_plugin()
{
}

void mongo_db_plugin::set_program_options(options_description& cli, options_description& cfg)
{
   cfg.add_options()
         ("mongodb-queue-size,q", bpo::value<uint32_t>()->default_value(1024),
         "The target queue size between nodeos and MongoDB plugin thread.")
         ("mongodb-abi-cache-size", bpo::value<uint32_t>()->default_value(2048),
          "The maximum size of the abi cache for serializing data.")
         ("mongodb-wipe", bpo::bool_switch()->default_value(false),
         "Required with --replay-blockchain, --hard-replay-blockchain, or --delete-all-blocks to wipe mongo db."
         "This option required to prevent accidental wipe of mongo db.")
         ("mongodb-block-start", bpo::value<uint32_t>()->default_value(0),
         "If specified then only abi data pushed to mongodb until specified block is reached.")
         ("mongodb-uri,m", bpo::value<std::string>(),
         "MongoDB URI connection string, see: https://docs.mongodb.com/master/reference/connection-string/."
               " If not specified then plugin is disabled. Default database 'EOS' is used if not specified in URI."
               " Example: mongodb://127.0.0.1:27017/EOS")
         ("mongodb-update-via-block-num", bpo::value<bool>()->default_value(false),
          "Update blocks/block_state with latest via block number so that duplicates are overwritten.")
         ("mongodb-store-blocks", bpo::value<bool>()->default_value(true),
          "Enables storing blocks in mongodb.")
         ("mongodb-store-block-states", bpo::value<bool>()->default_value(true),
          "Enables storing block state in mongodb.")
         ("mongodb-store-transactions", bpo::value<bool>()->default_value(true),
          "Enables storing transactions in mongodb.")
         ("mongodb-store-transaction-traces", bpo::value<bool>()->default_value(true),
          "Enables storing transaction traces in mongodb.")
         ("mongodb-store-action-traces", bpo::value<bool>()->default_value(true),
          "Enables storing action traces in mongodb.")
         ("mongodb-filter-on", bpo::value<vector<string>>()->composing(),
          "Track actions which match receiver:action:actor. Receiver, Action, & Actor may be blank to include all. i.e. eosio:: or :transfer:  Use * or leave unspecified to include all.")
         ("mongodb-filter-out", bpo::value<vector<string>>()->composing(),
          "Do not track actions which match receiver:action:actor. Receiver, Action, & Actor may be blank to exclude all.")
         ;
}

void mongo_db_plugin::plugin_initialize(const variables_map& options)
{
   try {
      if( options.count( "mongodb-uri" )) {
         ilog( "initializing mongo_db_plugin" );
         my->configured = true;

         if( options.at( "replay-blockchain" ).as<bool>() || options.at( "hard-replay-blockchain" ).as<bool>() || options.at( "delete-all-blocks" ).as<bool>() ) {
            if( options.at( "mongodb-wipe" ).as<bool>()) {
               ilog( "Wiping mongo database on startup" );
               my->wipe_database_on_startup = true;
            } else if( options.count( "mongodb-block-start" ) == 0 ) {
               EOS_ASSERT( false, chain::plugin_config_exception, "--mongodb-wipe required with --replay-blockchain, --hard-replay-blockchain, or --delete-all-blocks"
                                 " --mongodb-wipe will remove all EOS collections from mongodb." );
            }
         }

         if( options.count( "abi-serializer-max-time-ms") == 0 ) {
            EOS_ASSERT(false, chain::plugin_config_exception, "--abi-serializer-max-time-ms required as default value not appropriate for parsing full blocks");
         }
         my->abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();

         if( options.count( "mongodb-queue-size" )) {
            my->max_queue_size = options.at( "mongodb-queue-size" ).as<uint32_t>();
         }
         if( options.count( "mongodb-abi-cache-size" )) {
            my->abi_cache_size = options.at( "mongodb-abi-cache-size" ).as<uint32_t>();
            EOS_ASSERT( my->abi_cache_size > 0, chain::plugin_config_exception, "mongodb-abi-cache-size > 0 required" );
         }
         if( options.count( "mongodb-block-start" )) {
            my->start_block_num = options.at( "mongodb-block-start" ).as<uint32_t>();
         }
         if( options.count( "mongodb-update-via-block-num" )) {
            my->update_blocks_via_block_num = options.at( "mongodb-update-via-block-num" ).as<bool>();
         }
         if( options.count( "mongodb-store-blocks" )) {
            my->store_blocks = options.at( "mongodb-store-blocks" ).as<bool>();
         }
         if( options.count( "mongodb-store-block-states" )) {
            my->store_block_states = options.at( "mongodb-store-block-states" ).as<bool>();
         }
         if( options.count( "mongodb-store-transactions" )) {
            my->store_transactions = options.at( "mongodb-store-transactions" ).as<bool>();
         }
         if( options.count( "mongodb-store-transaction-traces" )) {
            my->store_transaction_traces = options.at( "mongodb-store-transaction-traces" ).as<bool>();
         }
         if( options.count( "mongodb-store-action-traces" )) {
            my->store_action_traces = options.at( "mongodb-store-action-traces" ).as<bool>();
         }
         if( options.count( "mongodb-filter-on" )) {
            auto fo = options.at( "mongodb-filter-on" ).as<vector<string>>();
            my->filter_on_star = false;
            for( auto& s : fo ) {
               if( s == "*" ) {
                  my->filter_on_star = true;
                  break;
               }
               std::vector<std::string> v;
               boost::split( v, s, boost::is_any_of( ":" ));
               EOS_ASSERT( v.size() == 3, fc::invalid_arg_exception, "Invalid value ${s} for --mongodb-filter-on", ("s", s));
               filter_entry fe{v[0], v[1], v[2]};
               my->filter_on.insert( fe );
            }
         } else {
            my->filter_on_star = true;
         }
         if( options.count( "mongodb-filter-out" )) {
            auto fo = options.at( "mongodb-filter-out" ).as<vector<string>>();
            for( auto& s : fo ) {
               std::vector<std::string> v;
               boost::split( v, s, boost::is_any_of( ":" ));
               EOS_ASSERT( v.size() == 3, fc::invalid_arg_exception, "Invalid value ${s} for --mongodb-filter-out", ("s", s));
               filter_entry fe{v[0], v[1], v[2]};
               my->filter_out.insert( fe );
            }
         }
         if( options.count( "producer-name") ) {
            wlog( "mongodb plugin not recommended on producer node" );
            my->is_producer = true;
         }

         if( my->start_block_num == 0 ) {
            my->start_block_reached = true;
         }

         std::string uri_str = options.at( "mongodb-uri" ).as<std::string>();
         ilog( "connecting to ${u}", ("u", uri_str));
         mongocxx::uri uri = mongocxx::uri{uri_str};
         my->db_name = uri.database();
         if( my->db_name.empty())
            my->db_name = "EOS";
         my->mongo_pool.emplace(uri);

         // hook up to signals on controller
         chain_plugin* chain_plug = app().find_plugin<chain_plugin>();
         EOS_ASSERT( chain_plug, chain::missing_chain_plugin_exception, ""  );
         auto& chain = chain_plug->chain();
         my->chain_id.emplace( chain.get_chain_id());

         my->accepted_block_connection.emplace( chain.accepted_block.connect( [&]( const chain::block_state_ptr& bs ) {
            my->accepted_block( bs );
         } ));
         my->irreversible_block_connection.emplace(
               chain.irreversible_block.connect( [&]( const chain::block_state_ptr& bs ) {
                  my->applied_irreversible_block( bs );
               } ));
         my->accepted_transaction_connection.emplace(
               chain.accepted_transaction.connect( [&]( const chain::transaction_metadata_ptr& t ) {
                  my->accepted_transaction( t );
               } ));
         my->applied_transaction_connection.emplace(
               chain.applied_transaction.connect( [&]( const chain::transaction_trace_ptr& t ) {
                  my->applied_transaction( t );
               } ));

         if( my->wipe_database_on_startup ) {
            my->wipe_database();
         }
         my->init();
      } else {
         wlog( "eosio::mongo_db_plugin configured, but no --mongodb-uri specified." );
         wlog( "mongo_db_plugin disabled." );
      }
   } FC_LOG_AND_RETHROW()
}

void mongo_db_plugin::plugin_startup()
{
}

void mongo_db_plugin::plugin_shutdown()
{
   my->accepted_block_connection.reset();
   my->irreversible_block_connection.reset();
   my->accepted_transaction_connection.reset();
   my->applied_transaction_connection.reset();

   my.reset();
}

} // namespace eosio
