#pragma once
#ifndef OPTIONX_HEADER_COMPONENTS_ITRADING_CONDITION_SUBSCRIBER_HPP_INCLUDED
#define OPTIONX_HEADER_COMPONENTS_ITRADING_CONDITION_SUBSCRIBER_HPP_INCLUDED

/// \file ITradingConditionSubscriber.hpp
/// \brief Defines the subscriber interface for broker trading-condition updates.

namespace optionx::components {

    /// \class ITradingConditionSubscriber
    /// \brief Interface for consumers that receive broker trading-condition updates.
    class ITradingConditionSubscriber {
    public:
        /// \brief Virtual destructor.
        virtual ~ITradingConditionSubscriber() = default;

        /// \brief Handles a trading-condition update.
        /// \param update Trading-condition payload.
        virtual void on_trading_condition(const TradingConditionUpdate& update) = 0;
    };

} // namespace optionx::components

#endif // OPTIONX_HEADER_COMPONENTS_ITRADING_CONDITION_SUBSCRIBER_HPP_INCLUDED
