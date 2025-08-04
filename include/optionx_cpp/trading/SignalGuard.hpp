#pragma once
#ifndef _OPTIONX_TRADING_SIGNAL_GUARD_HPP_INCLUDED
#define _OPTIONX_TRADING_SIGNAL_GUARD_HPP_INCLUDED

/// \file SignalGuard.hpp
/// \brief

namespace optionx::trading {
	
	/// \class SignalGuard
	/// \brief
	class SignalGuard {
	public:
		using result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;
		
		/// \brief Конструктор. Автоматически регистрирует сигнал для контроля.
		/// \param handler Указатель на обработчик, который обрабатывает сигнал.
		/// \param signals Торговый запрос, который контролируется.
		/// \param default_cancel_reason Комментарий об отмене по умолчанию (если не передан другой).
		SignalGuard(
				ITradeHandlerNode* handler,
				std::vector<std::unique_ptr<TradeSignal>> signals,
				std::string default_cancel_reason = "Signal not processed")
			: m_handler(handler), 
			  m_signals(std::move(signals)),
			  m_cancel_reason(std::move(default_cancel_reason)) {
			m_committed.resize(m_signals.size(), false);
			m_send_signals.reserve(m_signals.size());
			m_cancel_signals.reserve(m_signals.size());
			m_cancel_reasons.reserve(m_signals.size());
		}

		/// \brief Деструктор. Если сигнал не был передан дальше, он будет отменён автоматически.
		~SignalGuard() {
			for (size_t i = 0; i < m_committed.size(); ++i) {
				if (m_committed[i]) continue;
				cancel(i, m_cancel_reason);
			}
		}
		
		/// \brief
		/// \return
		std::vector<std::unique_ptr<TradeSignal>> &signals() {
			return m_signals;
		}
		
		/// \brief
		/// \param callback
		void add_callback(result_callback_t callback) {
			for (size_t i = 0; i < m_signals.size(); ++i) {
				m_signals[i]->request->add_callback(std::move(callback));
			}
		}

		/// \brief Подтверждает успешную обработку сигнала и передаёт его следующему обработчику.
		/// \param next_handler_id ID следующего обработчика.
		void send_to(size_t signal_index, NodeID next_handler_id) {
			if (!m_signals[signal_index]) return;
			m_send_signals.push_back(std::move(m_signals[signal_index]));
			m_signals[signal_index].reset();
		}

		/// \brief Отменяет сигнал с указанной причиной.
		/// \param reason Причина отмены.
		void cancel(size_t signal_index, const std::string& reason = std::string()) {
			if (!m_signals[signal_index]) return;
			m_cancel_signals.push_back(std::move(m_signals[signal_index]));
			m_signals[signal_index].reset();
			
			m_handler->cancel_trade(signal_index, reason.empty() ? default_cancel_reason : reason);
			m_committed = true;  // Избегаем повторной отмены в деструкторе
			
						
			m_cancel_signals.push_back(std::move(m_signals[i]));
			m_signals[i] = nullptr;
			
			
			if (m_committed[signal_index]) return;
			m_handler->
			m_committed[signal_index] = true;
			
			if (m_handler && handler_map.count(next_handler_id)) {
				handler_map[next_handler_id]->process(m_trade_request, {});
				m_committed = true;  // Отмена больше не нужна
			}v
		}
		
		void commit() {
			for (size_t i = 0; i < m_signals.size(); ++i) {
				if (!m_signals[i]) continue;
				m_cancel_signals.push_back(std::move(m_signals[i]));
				m_cancel_reasons.push_back(std::move(m_signals[i]));
			}
			
			for (size_t i = 0; i < m_cancel_signals.size(); ++i) {
				cancel_trade(std::move(m_cancel_signals[i]), m_cancel_reasons[i]);
			}
			
			for (size_t i = 0; i < m_send_signals.size(); ++i) {
				
			}
		}

	private:
		ITradeHandlerNode* m_handler;
		std::vector<std::unique_ptr<TradeSignal>> m_signals;
		std::string m_cancel_reason;
		std::vector<bool> m_committed;
		
		std::vector<std::unique_ptr<TradeSignal>> m_send_signals;
		std::vector<std::unique_ptr<TradeSignal>> m_cancel_signals;
		std::vector<std::string> 				  m_cancel_reasons;
		
		void cancel_trade(std::unique_ptr<TradeSignal> signal, const std::string& reason) {
            for (size_t i = 0; i < trades.size(); ++i) {
                auto result = signal->request->create_trade_result_unique();
				result->trade_id = utils::TradeIdGenerator::instance().generate_id();
				result->place_date = OPTIONX_TIMESTAMP_MS;
				result->platform_type = platform_type;
				if (!preprocess(request, result)) return false;
				
				
				// Получаем указатель нужного типа с данными результата
                std::shared_ptr<api::bo::BaseTradeRequest> request = std::shared_ptr<api::bo::BridgeTradeRequest>(trades[i].release());
                std::shared_ptr<api::bo::BaseTradeResult>  result = request->create_trade_result_shared();
                // Инициализируем время сигнала
                if (!result->place_date) result->place_date = timestamp;

                result->error_desc = error_desc[i];
                result->error_code = api::bo::OrderErrorCode::CANCELED_TRADE;
                result->send_date = result->open_date = result->close_date = timestamp;
                result->state = result->current_state = api::bo::OrderState::CANCELED_TRADE;
                // Вызываем callback
                if (on_cancel_trade) on_cancel_trade(std::move(request->clone_unique()), std::move(result->clone_unique()));
                request->invoke_callback(request, result);
            }
        }
	};

};

#endif // _OPTIONX_TRADING_SIGNAL_GUARD_HPP_INCLUDED