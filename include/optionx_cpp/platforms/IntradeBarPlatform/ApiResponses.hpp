#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_API_RESPONSES_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_API_RESPONSES_HPP_INCLUDED

/// \file ApiResponses.hpp
/// \brief Typed response payloads for Intrade Bar HTTP workflows.

#include <cstdint>
#include <string>
#include <vector>

#include <optionx_cpp/data.hpp>

#include "../common/ApiResult.hpp"

namespace optionx::platforms::intrade_bar {

    /// \brief Result of selecting an available Intrade Bar domain.
    struct DomainSelection {
        std::string host;
    };

    /// \brief Availability check for the current HTTP host.
    struct HostAvailability {
        bool available = false;
    };

    /// \brief Hidden login challenge and cookies parsed from the main page.
    struct MainPageChallenge {
        std::string req_id;
        std::string req_value;
        std::string cookies;
    };

    /// \brief User credentials returned by the login page flow.
    struct LoginCredentials {
        std::string user_id;
        std::string user_hash;
        std::string cookies;
    };

    /// \brief Auth endpoint acknowledgement payload.
    struct AuthCheck {
    };

    /// \brief Profile settings visible after authentication.
    struct ProfileInfo {
        CurrencyType currency = CurrencyType::UNKNOWN;
        AccountType account_type = AccountType::UNKNOWN;
    };

    /// \brief Current account balance.
    struct BalanceInfo {
        double balance = 0.0;
        CurrencyType currency = CurrencyType::UNKNOWN;
    };

    /// \brief Account settings switch acknowledgement.
    struct SettingsSwitch {
    };

    /// \brief Active trade visible on the authenticated main page.
    struct ActiveTradeInfo {
        int64_t id = 0;
        std::string symbol;
        double open_price = 0.0;
        int64_t open_time_ms = 0;
        int64_t close_time_ms = 0;
        int status = 0;
        int contract = 0;
    };

    /// \brief Active trades snapshot from the authenticated main page.
    struct ActiveTradesSnapshot {
        std::vector<ActiveTradeInfo> trades;
    };

    /// \brief Tick snapshot returned by the polling price endpoint.
    struct PriceSnapshot {
        std::vector<TickData> ticks;
    };

    /// \brief Trade open response payload.
    struct TradeOpenInfo {
        int64_t option_id = 0;
        int64_t open_date = 0;
        double open_price = 0.0;
    };

    /// \brief Trade result check response payload.
    struct TradeCheckInfo {
        double price = 0.0;
        double profit = 0.0;
    };

    using DomainSelectionResult = ApiResult<DomainSelection>;
    using HostAvailabilityResult = ApiResult<HostAvailability>;
    using MainPageChallengeResult = ApiResult<MainPageChallenge>;
    using LoginCredentialsResult = ApiResult<LoginCredentials>;
    using AuthCheckResult = ApiResult<AuthCheck>;
    using ProfileInfoResult = ApiResult<ProfileInfo>;
    using BalanceInfoResult = ApiResult<BalanceInfo>;
    using SettingsSwitchResult = ApiResult<SettingsSwitch>;
    using ActiveTradesSnapshotResult = ApiResult<ActiveTradesSnapshot>;
    using PriceSnapshotResult = ApiResult<PriceSnapshot>;
    using TradeOpenResult = ApiResult<TradeOpenInfo>;
    using TradeCheckResult = ApiResult<TradeCheckInfo>;

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_API_RESPONSES_HPP_INCLUDED
