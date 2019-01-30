#pragma once

#include <string>
#include <fc/reflect/reflect.hpp>
#include <fc/array.hpp>
#include <iosfwd>

namespace eosio {
	namespace chain {
		using std::string;

#define AN(X) eosio::chain::account_name(#X)
#define MAX_NAME_LENGH 64

		enum chain_enum {
			force,
			eos
		};
		static const auto force_chain_name = "FORCE";

		static const auto eos_chain_name = "EOS";

		const static chain_enum get_chain_by_name(const string &chain_s);

		struct account_name {

			bool empty() const { return 0 == value[0]; }

			bool good() const { return !empty(); }

			account_name() {}

			account_name(const char *str, chain_enum c = force) {
				set_value(str);
				chain = c;
			}

			account_name(const string &str, chain_enum c = force) {
				set_value(str.c_str());
				chain = c;
			}

			void set(const char *str);

			//TODO:
			const char *get_value() const {
				return value;
			}

			const chain_enum get_chain() const {
				return chain_enum(chain);
			}

			const string get_chain_name() const;


			explicit operator string() const;

			string to_string() const { return string(*this); }

			account_name &operator=(const string &n) {
				set(n.c_str());
				return *this;
			}

			account_name &operator=(const char *n) {
				set(n);
				return *this;
			}

			friend std::ostream &operator<<(std::ostream &out, const account_name &n) {
				return out << string(n);
			}

			int compare(const account_name &b) const;

			friend bool operator<(const account_name &a, const account_name &b) { return a.compare(b) < 0; }

			friend bool operator<=(const account_name &a, const account_name &b) { return a.compare(b) <= 0; }

			friend bool operator>(const account_name &a, const account_name &b) { return a.compare(b) > 0; }

			friend bool operator>=(const account_name &a, const account_name &b) { return a.compare(b) >= 0; }

			friend bool operator==(const account_name &a, const account_name &b) { return a.compare(b) == 0; }

			friend bool operator!=(const account_name &a, const account_name &b) { return a.compare(b) != 0; }


			char* value;

			uint64_t chain;

		private:

			void set_value(const char *str);
		};
	}
} // eosio::chain

namespace std {
	template<>
	struct hash<eosio::chain::account_name> : private hash<string> {
		hash<string>::result_type operator()(const eosio::chain::account_name &name) const noexcept {
			return hash<string>::operator()(name.to_string());
		}
	};
};

namespace fc {
	class variant;

	void to_variant(const eosio::chain::account_name &c, fc::variant &v);

	void from_variant(const fc::variant &v, eosio::chain::account_name &check);
} // fc

FC_REFLECT(eosio::chain::account_name, (value)(chain))
