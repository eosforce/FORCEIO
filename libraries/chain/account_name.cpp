#include <eosio/chain/account_name.hpp>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <eosio/chain/exceptions.hpp>

namespace eosio {
	namespace chain {
		using std::string;

		const static chain_enum get_chain_by_name(const string &chain_s) {
			if (force_chain_name == chain_s) {
				return force;
			} else if (eos_chain_name == chain_s) {
				return eos;
			}
			return force;
		}

		const string account_name::get_chain_name() const {
			switch (chain) {
				case force:
					return force_chain_name;
				case eos:
					return eos_chain_name;
			}
		}

		account_name::operator string() const {
			string str(get_chain_name());
			str.append(":");
			str.append(value);
			return str;
		};

		void account_name::set(const char *str) {
			char chain_s[16] = {};
			int i = 0;
			for (; str[i]; ++i) {
				EOS_ASSERT(i < 16, name_type_exception, "chain of Name is longer than 16 characters (${name}) ",
							  ("name", string(str)));
				if (':' == str[i]) {
					break;
				}
				chain_s[i] = str[i];
			}
			EOS_ASSERT(':' == str[i] && 0 != str[i + 1], name_type_exception, "wrong name : (${name}) ",
						  ("name", string(str)));
			set_value(&str[i + 1]);
			chain = get_chain_by_name(chain_s);
		}


		void account_name::set_value(const char *str) {
			const auto len = strnlen(str, MAX_NAME_LENGH + 1);
			EOS_ASSERT(len <= MAX_NAME_LENGH, name_type_exception, "Name is longer than 64 characters (${name}) ",
						  ("name", string(str)));
			value = str;
		}

		int account_name::compare(const account_name &b) const {
			const account_name &a = *this;
			if (a.chain < b.chain) {
				return -1;
			} else if (a.chain > b.chain) {
				return 1;
			}
			int i = 0;
			for (; a.value[i] && a.value[i]; i++) {
				if (a.value[i] < b.value[i]) {
					return -1;
				} else if (a.value[i] > b.value[i]) {
					return 1;
				}
			}
			if (a.value[i] && !b.value[i]) return 1;
			if (b.value[i] && !a.value[i]) return -1;
			return 0;
		}



	}
} /// eosio::chain

namespace fc {
	void to_variant(const eosio::chain::account_name &c, fc::variant &v) { v = std::string(c); }

	void from_variant(const fc::variant &v, eosio::chain::account_name &check) { check = v.get_string(); }
} // fc
