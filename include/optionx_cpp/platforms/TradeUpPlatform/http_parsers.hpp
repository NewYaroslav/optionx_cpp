#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_HTTP_PARSERS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_HTTP_PARSERS_HPP_INCLUDED

/// \file http_parsers.hpp
/// \brief JSON parsers for TradeUp HTTP responses.

#include <nlohmann/json.hpp>
#include "optionx_cpp/data/trading/enums.hpp"

namespace optionx::platforms::tradeup {

    inline bool parse_signin_response(const std::string& content, std::string& token, std::string& user_id) {
        try {
            auto j = nlohmann::json::parse(content);
            if (!j.value("success", false)) return false;
            const auto& data = j.at("data");
            token = data.value("token", std::string());
            user_id = data.value("userId", std::string());
            return !token.empty();
        } catch (...) {
            return false;
        }
    }

    inline bool parse_info_response(const std::string& content, double& balance, CurrencyType& currency) {
        try {
            auto j = nlohmann::json::parse(content);
            if (!j.value("success", false)) return false;
            const auto& balances = j.at("data").at("balances");
            if (balances.empty()) return false;
            const auto& b = balances.at(0);
            balance = b.value("amount", 0.0);
            currency = optionx::to_enum<CurrencyType>(b.value("currency", "UNKNOWN"));
            return true;
        } catch (...) {
            return false;
        }
    }

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_HTTP_PARSERS_HPP_INCLUDED
