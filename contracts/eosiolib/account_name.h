#pragma once

#include <string>

#define AN(X) ::eosio::account_name(#X)
namespace eosio {

	enum chain_enum {
		force,
		eos
	};
	static const auto force_chain_name = "FORCE";
	static const auto eos_chain_name = "EOS";

	struct account_name {
		account_name(const char *str = "", chain_enum c = force) {
			value = str;
			chain = c;
		}

		const std::string get_chain_name() const {
			switch (chain) {
				case force:
					return force_chain_name;
				case eos:
					return eos_chain_name;
			}
		}

		// keep in sync with name::operator string() in eosio source code definition for name
		std::string to_string() const {
			std::string str(get_chain_name());
			str.append(":");
			str.append(value);
			return str;
		}

		const char *get_value() const {
			return value;
		}

		/**
		 * Equality Operator for name
		 *
		 * @brief Equality Operator for name
		 * @param a - First data to be compared
		 * @param b - Second data to be compared
		 * @return true - if equal
		 * @return false - if unequal
		 */
		friend bool operator<(const account_name &a, const account_name &b) { return a.compare(b) < 0; }

		friend bool operator<=(const account_name &a, const account_name &b) { return a.compare(b) <= 0; }

		friend bool operator>(const account_name &a, const account_name &b) { return a.compare(b) > 0; }

		friend bool operator>=(const account_name &a, const account_name &b) { return a.compare(b) >= 0; }

		friend bool operator==(const account_name &a, const account_name &b) { return a.compare(b) == 0; }

		friend bool operator!=(const account_name &a, const account_name &b) { return a.compare(b) != 0; }

		/**
		 * Internal Representation of the account name
		 *
		 * @brief Internal Representation of the account name
		 */
		const char *value;

		uint64_t chain;

	private:
		int compare(const account_name &b) const {
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
	};
}