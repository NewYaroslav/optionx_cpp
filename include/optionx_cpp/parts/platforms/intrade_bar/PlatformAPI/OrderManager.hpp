#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_ORDER_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_ORDER_MANAGER_HPP_INCLUDED

/// \file OrderManager.hpp
/// \brief

#include "../../../modules/OrderManager.hpp"

namespace optionx {
namespace platforms {
namespace intrade_bar {

    class OrderManager final : public modules::OrderManager {
    public:

        OrderManager() : modules::OrderManager() {

        }

        ~OrderManager() {

        }

    private:

        /// \brief Sends an order to be processed. Must be implemented by derived classes.
        /// \param transaction Trade transaction event to be processed.
        void send_order(const std::shared_ptr<TradeRequest>& request) override final {

        }

        /// \brief Sets account info data.
        /// \param type The type of account information.
        /// \param value The value to set.
        void set_account_info(AccountInfoType type, int64_t value) override final {

        }

        /// \brief Retrieves the API type of the account.
        /// \return ApiType of the account.
        ApiType get_api_type() override final {
            return ApiType::INTRADE_BAR;
        }

    }; // OrderManager


} // namespace intrade_bar
} // namespace platforms
} // namespace optionx

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_ORDER_MANAGER_HPP_INCLUDED
