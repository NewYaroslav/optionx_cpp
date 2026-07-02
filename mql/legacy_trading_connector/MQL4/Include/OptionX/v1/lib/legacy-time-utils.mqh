//+------------------------------------------------------------------+
//|                                      legacy-time-utils.mqh       |
//|                 Legacy trading connector time helpers for MQL4   |
//+------------------------------------------------------------------+
#ifndef OPTIONX_LEGACY_TRADING_TIME_UTILS_MQH
#define OPTIONX_LEGACY_TRADING_TIME_UTILS_MQH

#property strict

#include <TimeShield.mqh>

uint legacy_trading_sec_of_day(const datetime timestamp) {
    return (uint)TimeShield::sec_of_day((long)timestamp);
}

uint legacy_trading_sec_of_day(const int hour, const int minute, const int second) {
    return (uint)TimeShield::sec_of_day(hour, minute, second);
}

uint legacy_trading_sec_of_day(const string value) {
    return (uint)TimeShield::sec_of_day(value);
}

class LegacyTradingTimer {
private:
    ulong m_start_time_ms;

public:
    LegacyTradingTimer() {
        reset();
    }

    void reset() {
        m_start_time_ms = TimeShield::monotonic_ms();
    }

    ulong get_elapsed_ms() {
        return TimeShield::monotonic_ms() - m_start_time_ms;
    }

    double get_elapsed() {
        return (double)get_elapsed_ms() / 1000.0;
    }
};

#endif // OPTIONX_LEGACY_TRADING_TIME_UTILS_MQH
