//+------------------------------------------------------------------+
//|                                      legacy-time-utils.mqh       |
//|                 Legacy trading connector time helpers for MQL5   |
//+------------------------------------------------------------------+
#ifndef OPTIONX_LEGACY_TRADING_TIME_UTILS_MQH
#define OPTIONX_LEGACY_TRADING_TIME_UTILS_MQH

#property strict

#include <TimeShield.mqh>

int legacy_trading_sec_of_day(const datetime timestamp) {
    return time_shield::sec_of_day((long)timestamp);
}

int legacy_trading_sec_of_day(const int hour, const int minute, const int second) {
    return time_shield::sec_of_day(hour, minute, second);
}

int legacy_trading_sec_of_day(const string value) {
    return time_shield::sec_of_day(value);
}

class LegacyTradingTimer {
private:
    long m_start_time_ms;

public:
    LegacyTradingTimer() {
        reset();
    }

    void reset() {
        m_start_time_ms = time_shield::now();
    }

    ulong get_elapsed_ms() const {
        const long now_ms = time_shield::now();
        if (now_ms <= m_start_time_ms) return 0;
        return (ulong)(now_ms - m_start_time_ms);
    }

    double get_elapsed() const {
        return (double)get_elapsed_ms() / 1000.0;
    }
};

#endif // OPTIONX_LEGACY_TRADING_TIME_UTILS_MQH
