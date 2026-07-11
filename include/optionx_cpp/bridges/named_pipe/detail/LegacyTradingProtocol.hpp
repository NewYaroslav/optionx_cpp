#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_NAMED_PIPE_DETAIL_LEGACY_TRADING_PROTOCOL_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_NAMED_PIPE_DETAIL_LEGACY_TRADING_PROTOCOL_HPP_INCLUDED

/// \file LegacyTradingProtocol.hpp
/// \brief Defines helpers for the legacy named-pipe trading JSON protocol.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <time_shield.hpp>
#include <vector>

namespace optionx::bridges::named_pipe::detail {

    inline std::string parse_symbol(const std::string& symbol) {
        static const std::vector<std::string> symbols = {
            "EURUSD", "USDJPY", "USDCHF", "USDCAD", "EURJPY", "AUDUSD",
            "NZDUSD", "EURGBP", "EURCHF", "AUDJPY", "GBPJPY", "EURCAD",
            "AUDCAD", "CADJPY", "NZDJPY", "AUDNZD", "GBPAUD", "EURAUD",
            "GBPCHF", "AUDCHF", "GBPNZD", "BTCUSDT"
        };

        std::string normalized;
        normalized.reserve(symbol.size());
        for (const unsigned char ch : symbol) {
            if (std::isalnum(ch) != 0) {
                normalized.push_back(static_cast<char>(std::toupper(ch)));
            }
        }

        if (normalized == "BTCUSD" || normalized == "BTCUSDT") {
            return "BTCUSDT";
        }

        const auto it = std::find(symbols.begin(), symbols.end(), normalized);
        if (it != symbols.end()) {
            return *it;
        }

        throw std::invalid_argument("Invalid symbol in legacy trade request.");
    }

    inline void parse_note(const std::string& note, TradeSignal& signal) {
        const auto pos = note.find('&');
        if (pos == std::string::npos) {
            signal.user_data = note;
            return;
        }
        signal.signal_name = note.substr(0, pos);
        signal.user_data = note.substr(pos + 1);
    }

    inline OrderType parse_order_type(const std::string& direction) {
        OrderType order_type = OrderType::UNKNOWN;
        if (!to_enum(direction, order_type) || order_type == OrderType::UNKNOWN) {
            throw std::invalid_argument("Invalid order direction in legacy trade request.");
        }
        return order_type;
    }

    inline std::uint32_t parse_duration_value(std::int64_t value) {
        if (value < 0 ||
            value > static_cast<std::int64_t>((std::numeric_limits<std::uint32_t>::max)())) {
            throw std::invalid_argument("Invalid duration in legacy trade request.");
        }
        return static_cast<std::uint32_t>(value);
    }

    inline void parse_expiry_or_duration(
            const nlohmann::json& contract,
            TradeSignal& signal) {
        if (contract.contains("exp")) {
            if (!contract.at("exp").is_number()) {
                throw std::invalid_argument("Invalid expiry time in legacy trade request.");
            }

            const std::int64_t expiry_time = contract.at("exp").get<std::int64_t>();
            signal.option_type = OptionType::CLASSIC;
            if (expiry_time < time_shield::SEC_PER_DAY) {
                signal.duration = parse_duration_value(expiry_time);
                signal.expiry_time = 0;
            } else {
                signal.duration = 0;
                signal.expiry_time = expiry_time;
            }
            return;
        }

        if (contract.contains("dur")) {
            if (!contract.at("dur").is_number()) {
                throw std::invalid_argument("Invalid duration in legacy trade request.");
            }

            signal.duration = parse_duration_value(contract.at("dur").get<std::int64_t>());
            signal.option_type = OptionType::SPRINT;
            return;
        }

        throw std::invalid_argument("Missing expiry or duration in legacy trade request.");
    }

    /// \brief Parses a legacy `contract` object into a TradeSignal.
    /// \param contract Legacy JSON contract payload.
    /// \param min_payout Minimum payout copied into the created signal.
    /// \return Newly allocated trade signal.
    /// \throws std::exception when required contract fields are invalid.
    inline std::unique_ptr<TradeSignal> parse_contract(
            const nlohmann::json& contract,
            double min_payout) {
        auto signal = std::make_unique<TradeSignal>();
        signal->symbol = parse_symbol(contract.at("s").get<std::string>());
        parse_note(contract.value("note", std::string()), *signal);
        signal->amount = contract.at("a").get<double>();
        signal->order_type = parse_order_type(contract.at("dir").get<std::string>());
        parse_expiry_or_duration(contract, *signal);
        signal->min_payout = min_payout;
        return signal;
    }

    inline std::string compose_note(const TradeRequest& request) {
        if (request.signal_name.empty()) {
            return request.user_data;
        }
        return request.signal_name + "&" + request.user_data;
    }

    inline std::string format_order_type(OrderType order_type) {
        if (order_type == OrderType::BUY) return "buy";
        if (order_type == OrderType::SELL) return "sell";
        return "none";
    }

    inline std::string format_trade_state(TradeState state) {
        switch (state) {
        case TradeState::UNKNOWN:
        case TradeState::WAITING_OPEN:
            return "unknown";
        case TradeState::CHECK_ERROR:
            return "error";
        case TradeState::OPEN_ERROR:
        case TradeState::CANCELED_TRADE:
            return "open_error";
        case TradeState::OPEN_SUCCESS:
        case TradeState::IN_PROGRESS:
        case TradeState::WAITING_CLOSE:
            return "wait";
        case TradeState::WIN:
            return "win";
        case TradeState::LOSS:
            return "loss";
        case TradeState::STANDOFF:
        case TradeState::REFUND:
            return "standoff";
        default:
            break;
        }
        return "unknown";
    }

    /// \brief Formats a trade result for the legacy `update_bet` message.
    /// \param request Original trade request.
    /// \param result Current trade result snapshot.
    /// \return Serialized JSON message.
    inline std::string format_trade_result(
            const TradeRequest& request,
            const TradeResult& result) {
        nlohmann::json update;
        update["s"] = request.symbol;
        update["note"] = compose_note(request);
        update["aid"] = result.trade_id;
        update["id"] = result.option_id;
        update["op"] = result.open_price;
        update["cp"] = result.close_price;
        update["dir"] = format_order_type(request.order_type);
        update["status"] = format_trade_state(result.trade_state);
        update["a"] = result.amount;
        update["profit"] = result.profit;
        update["payout"] = result.payout;
        update["dur"] = request.duration;
        update["ot"] = time_shield::ms_to_fsec(result.open_date);
        update["st"] = time_shield::ms_to_fsec(result.send_date);
        update["ct"] = time_shield::ms_to_fsec(result.close_date);

        nlohmann::json message;
        message["update_bet"] = std::move(update);
        return message.dump();
    }

    /// \brief Formats a legacy balance update message.
    /// \param info Account information snapshot.
    /// \return Serialized JSON message.
    inline std::string format_balance_update(const BaseAccountInfoData& info) {
        nlohmann::json message;
        message["b"] = info.get_info<double>(AccountInfoType::BALANCE);
        message["rub"] =
            info.get_info<CurrencyType>(AccountInfoType::CURRENCY) == CurrencyType::RUB ? 1 : 0;
        message["demo"] =
            info.get_info<AccountType>(AccountInfoType::ACCOUNT_TYPE) == AccountType::DEMO ? 1 : 0;
        return message.dump();
    }

    /// \brief Formats a legacy connection status update message.
    /// \param info Account information snapshot.
    /// \return Serialized JSON message.
    inline std::string format_connection_update(const BaseAccountInfoData& info) {
        nlohmann::json message;
        message["conn"] =
            info.get_info<bool>(AccountInfoType::CONNECTION_STATUS) ? 1 : 0;
        message["aid"] = info.get_info<std::int64_t>(AccountInfoType::USER_ID);
        return message.dump();
    }

} // namespace optionx::bridges::named_pipe::detail

#endif // OPTIONX_HEADER_BRIDGES_NAMED_PIPE_DETAIL_LEGACY_TRADING_PROTOCOL_HPP_INCLUDED
