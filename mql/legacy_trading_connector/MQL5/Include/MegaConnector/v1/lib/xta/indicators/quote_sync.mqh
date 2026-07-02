/*
* xtechnical_analysis - Technical analysis C++ library
*
* Copyright (c) 2018-2023 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef XTA_QUOTE_SYNC_MQH
#define XTA_QUOTE_SYNC_MQH
//+------------------------------------------------------------------+
//|                                   		 	      quote_sync.mqh |
//|								    Copyright 2018-2023, Elektro Yar |
//|			      https://github.com/NewYaroslav/xtechnical_analysis |
//+------------------------------------------------------------------+
#property copyright "Copyright 2018-2023, Elektro Yar"
#property link      "https://github.com/NewYaroslav/xtechnical_analysis"
#property version   "1.00"
#property strict

#include <Arrays\ArrayObj.mqh>

/** \brief Класс синхронизатора котировок
 * Данный класс позволяет синхронизировать данные с разных валютных пар
 */
template<typename VAL_TYPE>
class XtaQuoteSyncData {
public:
	VAL_TYPE    value;
	ulong       open_time;
	ulong       time;
	bool        is_filled;
	
	XtaQuoteSyncData() {
		open_time = 0;
		time = 0;
		is_filled = false;
	}

	XtaQuoteSyncData(
			const VAL_TYPE &v,
			const ulong ot,
			const ulong t,
			const bool f) :
		value(v), open_time(ot), time(t), is_filled(f)  {
	};
};
		
template<typename VAL_TYPE>
class XtaQuoteSync {
private:
	CArrayObj	*m_buffer[];
	bool		m_update_flag[];
	int         m_num_symbols;

	ulong	    m_timeframe;
	ulong	    m_last_open_time;
	ulong	    m_last_time;
	bool		m_auto_calc;

public:

	XtaQuoteSync() {
		m_timeframe = 0;
		m_last_open_time = 0;
		m_last_time = 0;
		m_num_symbols = 0;
		m_auto_calc = false;
	}
	
	/** \brief Конструктор синхронизатора котировок
	 * \param s     Количество символов
	 * \param tf    Таймфрейм
	 * \param ac    Использовать авторасчет (включает автоматический вызов calc())
	 */
	XtaQuoteSync(const int s = 1, const int tf = 60, const bool ac = false) :
			m_timeframe(tf), m_auto_calc(ac) {
		m_buffer.resize(s);
		ArrayResize(m_buffer, s);
		ArrayResize(m_update_flag, s);
		ArrayFill(m_update_flag, 0, ArraySize(m_update_flag), false);
		m_num_symbols = s;
		m_last_open_time = 0;
		m_last_time = 0;
	};

	~XtaQuoteSync() {
		ArrayFree(m_buffer);
		ArrayFree(m_update_flag);
	};

	/** \brief Обновить состояние синхронизатора котировок
     * \param index     Индекс символа
     * \param value     Значение цены
     * \param time      Время цены
     * \return Вернет true в случае успешного обновления состояния индикатора
     */
    bool update(const int index, const VAL_TYPE &value, const ulong time) {
        if (index >= m_num_symbols) return false;
        m_last_time = MathMax(m_last_time, time);
        // 1. Получаем время открытия бара
        const ulong open_time = time - time % m_timeframe;
        m_last_open_date = MathMax(m_last_open_date, open_date);
        // 2. Запоминаем бары
        if (m_buffer[index].Total() == 0) {
            // 2.1 Если буфер пустой, добавляем элемент
            m_buffer[index].Add(new XtaQuoteSyncData<VAL_TYPE>(value, open_time, time, false));
        } else
        if (open_time == m_buffer[index].At(m_buffer[index].Total() - 1).open_time) {
            // 2.2 Если буфер имеет уже данный бар, обновляем значения
            m_buffer[index].Update(m_buffer[index].Total() - 1, new XtaQuoteSyncData<VAL_TYPE>(value, open_time, time, false));
        } else {
            if (open_time < m_buffer[index].At(m_buffer[index].Total() - 1).open_time) return false;
            // 2.3 Если буфер имеет пропуски баров, заполняем его
            const ulong prev_open_time = open_time - m_timeframe;
            int back_index = m_buffer[index].Total() - 1;
            while (prev_open_time > m_buffer[index].At(back_index).open_time) {
                m_buffer[index].Add(
                    new XtaQuoteSyncData(
                        m_buffer[index].At(back_index).value,
                        m_buffer[index].At(back_index).open_time + m_timeframe,
                        m_buffer[index].At(back_index).time,
                        true)); // ставим флаг, что бар заполнен предыдущим значением
                back_index = m_buffer[index].Total() - 1;
            }
            m_buffer[index].Add(new XtaQuoteSyncData(value, open_time, time, false));
        }
        m_update_flag[index] = true;
        if (m_auto_calc) return calc();
        return true;
    }

    bool calc() {
        if (!on_update) return false;
        // Проверяем наступление события, когда данные есть по всем барам
        for (int s = 0; s < m_num_symbols; ++s) {
            if (m_buffer[s].Total() == 0) return false;
            if (m_buffer[s].At(m_buffer[index].Total() - 1).open_time != m_last_open_time) return false;
        }
        // поиск минимального размера массива баров
        int min_size = INT_MAX;
        for (int s = 0; s < m_num_symbols; ++s) {
            min_size = MathMin(min_size, m_buffer[s].Total());
        }
        if (min_size > 1) {
            // вызываем обновление данных
            const int last_index = min_size - 2;
            for (int i = 0; i <= last_index; ++i) {
                bool is_gap = true;
                for (int s = 0; s < m_num_symbols; ++s) {
                    const int pos = m_buffer[s].Total() - min_size + i;
                    if (!m_buffer[s].At(pos).is_filled) {
                        is_gap = false;
                        break;
                    }
                }
                for (int s = 0; s < m_num_symbols; ++s) {
                    const int pos = m_buffer[s].Total() - min_size + i;
                    const ulong delay = m_last_time - m_buffer[s].At(pos).time;
                    on_update(
                        s,
                        m_buffer[s].At(pos).value,
                        m_buffer[s].At(pos).open_date,
                        delay,
                        PriceType::Close,
                        false,
                        is_gap);
                }
            }
            // удаляем старые данные
            for (int s = 0; s < m_num_symbols; ++s) {
                m_buffer[s].erase(m_buffer[s].begin(), m_buffer[s].end() - 1);
            }
        }
        // вызываем последнее обновление для актуальных цен
        for (size_t s = 0; s < m_buffer.size(); ++s) {
            const size_t pos = m_buffer[s].size() - 1;
            const uint64_t delay_ms = m_last_time_ms - m_buffer[s][pos].time_ms;
            on_update(
                s,
                m_buffer[s][pos].value,
                m_buffer[s][pos].open_date,
                delay_ms,
                PriceType::IntraBar,
                m_update_flag[s],
                false);
            m_update_flag[s] = false;
        }
        return true;
    }
	
};

#endif // XTA_QUOTE_SYNC_MQH