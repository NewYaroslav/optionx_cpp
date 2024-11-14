#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_ORDER_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_ORDER_MANAGER_HPP_INCLUDED

/// \file OrderManager.hpp
/// \brief

namespace optionx {

    class OrderManager {
    public:

        bool place_trade(std::unique_ptr<TradeRequest> request) {
            if (!request) return false;
            if (request->account == AccountType::UNKNOWN) {
                request->account = m_account_info->account;
                request->currency = m_account_info->currency;
            }
            request->symbol = to_std_symbol(request->symbol);

            OrderDataSignal signal;
            signal.result = request->create_trade_result_shared();
            signal.result->place_date = OPTIONX_TIMESTAMP_MS;
            signal.result->api_type = ApiType::INTRADE_BAR;
            signal.request = std::shared_ptr<TradeRequest>(request.release());

            std::lock_guard<std::mutex> lock(m_pending_orders_mutex);
            m_pending_orders.push_back(std::move(signal));
        }

        void process() {
            process_pending_orders();
        };

    private:

        std::mutex                          m_pending_orders_mutex;
        std::list<TradeTransaction>         m_pending_orders;

        std::shared_ptr<AccountInfoData>    m_account_info;



        void process_pending_orders() {
            std::unique_lock<std::mutex> lock(m_pending_orders_mutex);
            if (m_pending_orders.empty()) return;

            std::list<TradeTransaction> calceled_orders;
            TradeTransaction new_order;
            bool is_new_order = false;
            const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
            const int64_t order_queue_timeout = m_account_info->get_account_info(AccountInfoType::ORDER_QUEUE_TIMEOUT);

            // Очищаем список ожидающих открытия сделок от устаревших сигналов
            auto it = m_pending_orders.begin();
            while (it != m_pending_orders.end()) {
                auto &order = *it;
                const int64_t delay = timestamp - order.result->place_date;
                if (delay >= order_queue_timeout) {
                    calceled_orders.push_back(std::move(order));
                    it = m_pending_orders.erase(it);
                    continue;
                }
                it++;
            }

            //
            if (!m_pending_orders.empty()) {
                const int64_t max_orders = m_account_info->get_account_info(AccountInfoType::MAX_ORDERS);
                const int64_t open_orders = m_account_info->get_account_info(AccountInfoType::OPEN_ORDERS);
                if (open_orders < max_orders) {
                    m_account_info->set_account_info(AccountInfoType::OPEN_ORDERS, open_orders + 1);
                    auto it = m_pending_orders.begin();
                    auto &order = *it;
                    new_order = std::move(order);
                    m_pending_orders.erase(it);
                    is_new_order = true;
                }
            }
            lock.unlock();
            if (!is_new_order) return;

            if (!check_order(new_order)) {
                m_account_info->set_account_info(AccountInfoType::OPEN_ORDERS, open_orders);
            }





        }

    }; // OrderManager

    bool OrderManager::place_trade(std::unique_ptr<BaseTradeRequest> &request) {
        if (!request) return false;
        // Устанавливаем тип счета, если нужно
        if (request->account == AccountType::UNKNOWN) {
            std::lock_guard<std::mutex> lock(m_account_data_mutex);
            request->account = m_account_data.account;
            request->currency = m_account_data.currency;
        }
        // Нормализуем имя символа
        request->symbol = to_std_symbol(request->symbol);

        OrderDataSignal signal;
        // Получаем указатель нужного типа с данными результата
        signal.result = request->create_trade_result_shared();
        // Инициализируем тип брокера и время сигнала
        if (!signal.result->place_date) signal.result->place_date = MC_FTIMESTAMP;
        signal.result->broker = BrokerType::UTE_LIMITED;
        signal.request = std::shared_ptr<BaseTradeRequest>(request.release());
        // Добавляем в очередь сделок
        std::lock_guard<std::mutex> lock(m_new_orders_mutex);
        m_new_orders.push(std::move(signal));
        return true;
    };

};

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_ORDER_MANAGER_HPP_INCLUDED
