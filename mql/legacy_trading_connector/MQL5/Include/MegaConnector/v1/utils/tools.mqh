//+------------------------------------------------------------------+
//|                                                        tools.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
//
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_TOOLS_MQH
#define MEGA_CONNECTOR_TOOLS_MQH

#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://mega-connector.com/"
#property strict

#include "..\lib\homura.mqh"
#include "..\part\common.mqh"
#include "logger.mqh"
//+------------------------------------------------------------------+
// Функция проверяет, открыт рынок или нет по локальному времени и времени сервера
bool is_market_open(const ulong max_delay) {
    static datetime mt_last_time = 0;
    static datetime pc_last_time = 0;

    const datetime pc_time = TimeLocal();
    const datetime mt_time = TimeCurrent();

    const datetime pc_time_div = pc_time / (datetime)max_delay;
    const datetime mt_time_div = mt_time / (datetime)max_delay;

// в самом начале инициализируем переменные
    if (mt_last_time == 0 || pc_last_time == 0) {
        mt_last_time = mt_time_div;
        pc_last_time = pc_time_div;
        return (true);
    }

// если время сервера поменялось, рынок открыт
    if (mt_last_time != mt_time_div) {
        mt_last_time = mt_time_div;
        pc_last_time = pc_time_div;
        return (true);
    } else
        // если время компьютера не изменилось, или изменилось незначительно - рынок открыт
        if (pc_last_time == pc_time_div || (pc_last_time + 1) == pc_time_div) {
            return (true);
        }

    return (false);
}
//+------------------------------------------------------------------+
datetime get_candle_time() {
    datetime bar_time = 0;
    const int attempts = 3;
    int attempt = 0;
    while(attempt < attempts) {
        bar_time = iTime(Symbol(), PERIOD_CURRENT, 0); 
        if (bar_time) {
            if (GetLastError() == 0) {
                return bar_time;
            } else {
                PrintFormat("%s: Candle are not synchronized yet. Time %s; Error=%d", Symbol(), TimeToString(TimeTradeServer(), TIME_DATE | TIME_MINUTES | TIME_SECONDS), _LastError);
            }
        }
        ++attempt;
        Sleep(1);
    }
    return 0;
}
//+------------------------------------------------------------------+
bool check_tick_deadtime(const string &symbol, const int user_deadtime = 60) {
    bool success = false;
    int attempts = 0;
    MqlTick tick_array[];
    while (attempts < 3) {
        const int received = CopyTicks(symbol, tick_array, COPY_TICKS_ALL, 0, 1); 
        if (received != -1) {
            if (GetLastError() == 0) {
                success = true;
                break;
            } else {
                PrintFormat("%s: Ticks are not synchronized yet, %d ticks received. Error=%d", symbol, received, _LastError);
            }
        }
        ++attempts;
        Sleep(1);
    }
    if (success) {
        if (ArraySize(tick_array) >= 1) {
            const long t_ms = (long)TimeTradeServer() * 1000;
            const long deadtime = MathAbs((t_ms - tick_array[0].time_msc) / 1000);
            if (deadtime > user_deadtime) success = false;
        } else {
            success = false;
        }
    }
    ArrayFree(tick_array);
    return success;
}
//+------------------------------------------------------------------+
bool check_bar_deadtime(const string &symbol, const int user_deadtime = 60) {
    bool success = false;
    int attempts = 0;
    datetime bar_time = 0;
    while(attempts < 3) {
        bar_time = iTime(symbol, PERIOD_M1, 0); 
        if (bar_time) {
            if (GetLastError() == 0) {
                success = true;
                break;
            } else {
                PrintFormat("%s: Candle are not synchronized yet. Time %s; Error=%d", symbol, TimeToString(TimeTradeServer(), TIME_DATE | TIME_MINUTES | TIME_SECONDS), _LastError);
            }
        }
        ++attempts;
        Sleep(1);
    }
    if (success) {
        const long bar_time_ms = bar_time * 1000;
        const long t_ms = (long)TimeTradeServer() * 1000;
        const long deadtime = (t_ms - bar_time_ms) / 1000;
        if (deadtime <= user_deadtime) return true;
    }
    return false;
}
//+------------------------------------------------------------------+
/*
bool is_symbol_market_open(const string &symbol) {

    MqlTradeRequest     request;
    MqlTradeCheckResult result;

    request.action    = TRADE_ACTION_DEAL;
    request.symbol    = symbol;
    request.volume    = 0.2;
    request.type      = ORDER_TYPE_SELL;
    request.price     = SymbolInfoDouble(symbol, SYMBOL_BID);
    request.deviation = 5;
    request.magic     = EXPERT_MAGIC;
    //--- отправка запроса
    if(!OrderCheck(request, result)) {
        PrintFormat("OrderSend error %d",GetLastError());
        return (true);
    }
    if (result.retcode == TRADE_RETCODE_MARKET_CLOSED) return (false);
    return (true);
}
*/
//+------------------------------------------------------------------+
// Проверка актуальности бара для защиты от ложного сигнада
bool check_bar_relevance(const string symbol, const ENUM_TIMEFRAMES timeframe, const ulong max_delay) {
    static const ulong SECONDS_IN_MINUTE = 60;
    if (!is_market_open(max_delay)) return (false);

    ulong div = 0;
    if (timeframe == PERIOD_CURRENT) div = SECONDS_IN_MINUTE * Period();
    else div = SECONDS_IN_MINUTE * timeframe;

    const datetime bar_time = iTime(symbol, timeframe, 0);
    if (bar_time == 0) return (false);
    const datetime pc_time = TimeCurrent();

    const ulong current_minute = (ulong)pc_time/div;
    const ulong current_bar_minute = (ulong)bar_time / div;

    if (current_minute == current_bar_minute ||
            (current_minute + 1) == current_bar_minute ||
            current_minute == (current_bar_minute + 1)) return (true);

    return (false);
}
//+------------------------------------------------------------------+
// Проверка актуальности бара для защиты от ложного сигнада
int check_bar_relevance_v2(const string symbol, const ENUM_TIMEFRAMES timeframe, const ulong max_delay, const int attempts = 5) {
    for (int i = 0; i < attempts; ++i) {
        if (check_bar_relevance(symbol, timeframe, max_delay)) return (i + 1);
    }
    return 0;
}
//+------------------------------------------------------------------+
// Проверка обновления иcторических данных
bool check_update_history(const string symbol, const ENUM_TIMEFRAMES timeframe) {
    datetime temp = iTime(symbol, timeframe, 0);
    if (temp == 0) return false;
    return true;
}
//+------------------------------------------------------------------+
int check_update_history_v2(const string symbol, const ENUM_TIMEFRAMES timeframe, const int attempts = 5) {
    for (int i = 0; i < attempts; ++i) {
        if (check_update_history(symbol, timeframe)) return (i + 1);
    }
    return 0;
}
//+------------------------------------------------------------------+
int update_history(const string symbol, const ENUM_TIMEFRAMES timeframe, const int attempts = 5) {
    MqlRates rates[];
    ArraySetAsSeries(rates, true);
    for (int i = 0; i < attempts; ++i) {
        const int err = CopyRates(symbol, timeframe, 0, 1, rates);
        if (err > 0) {
            ArrayFree(rates);
            return (i + 1);
        }
    }
    ArrayFree(rates);
    return 0;
}

//+------------------------------------------------------------------+
void shuffle_string_array(string &data[]) {
    MathSrand((int)TimeLocal());
    const int len = ArraySize(data);
    for (int index = 0; index < len; index++) {
        const string temp = data[index];
        int rnd = (int)(((double)MathRand() * (double)len) / 32768.0);
        if (rnd >= len) rnd = len - 1;
        data[index] = data[rnd];
        data[rnd] = temp;
    }
}
//+------------------------------------------------------------------+
class MegaConnectorTimeFilter {
private:
    uint             time_start;
    uint             time_stop;
    uint             error_code;

public:

    MegaConnectorTimeFilter() {
        time_start = 0;
        time_stop = 0;
        error_code = 0;
    }

    uint get_error_code() {
        return error_code;
    }

    bool set(const string &arg_str_time_start, const string &arg_str_time_stop) {
        time_start = homura::sec_of_day(arg_str_time_start);
        if (time_start == homura::DurationTimePeriod::SEC_PER_DAY) {
            error_code = 1;
            return false;
        }
        time_stop = homura::sec_of_day(arg_str_time_stop);
        if (time_stop == homura::DurationTimePeriod::SEC_PER_DAY) {
            error_code = 2;
            return false;
        }
        return true;
    }

    bool check() {
        const uint second_day = homura::sec_of_day(TimeCurrent());
        if (time_stop >= time_start) {
            if (second_day < time_start) return false;
            if (second_day > time_stop) return false;
        } else {
            if (second_day < time_start && second_day > time_stop) return false;
        }
        return true;
    }
};
//+------------------------------------------------------------------+
class MegaConnectorIndicatorSignalV1 {
public:

    class MCIndicatorSignalV1Config {
    public:
        MegaConnectorEntryType  entry_type;
        bool    is_calc_previous_signal;
        string  indicator_file_name;
        uint    indicator_id_up;
        uint    indicator_id_dn;
        bool    is_use_null_signal;
        bool    is_connected;
        int     time_previous_signal;
        int     signal_block_time;
        
        MCIndicatorSignalV1Config() {
            entry_type = MC_BO_INTRABAR;
            is_calc_previous_signal = true;
            indicator_id_up = 0;
            indicator_id_dn = 1;
            is_use_null_signal = false;
            is_connected = false;
            time_previous_signal = 0;
            signal_block_time = 0;
        }
    } config;

private:
    MegaConnectorBoContractType signal_type;
    datetime    				signal_bar_time;
    MegaConnectorBoContractType prev_signal_type;
    MegaConnectorLogger         *logger;
    
    int         handle;
    string      signal_symbol;

    bool        is_signal_canceled;
 
    datetime    block_time;
    datetime    block_bar_time;
    datetime    last_open_bar_time;
    datetime    prev_bar_time;

    MegaConnectorBoContractType get_signal(const int shift) {
        if (handle == INVALID_HANDLE) return MC_BO_CONTRACT_UNKNOWN_STATE;
        double up_buffer[1];
        double dn_buffer[1];
        if (CopyBuffer(handle, config.indicator_id_up, shift, 1, up_buffer) < 1) {
            PrintFormat("Failed to copy data from the indicator, error code %d",GetLastError());
            if (logger) logger.log("Failed to copy data from the indicator, error code %d" + IntegerToString(GetLastError()) + "; symbol " + signal_symbol + "; shift: " + IntegerToString(shift) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            return MC_BO_CONTRACT_UNKNOWN_STATE;
        }
        if (CopyBuffer(handle, config.indicator_id_dn, shift, 1, dn_buffer) < 1) {
            PrintFormat("Failed to copy data from the indicator, error code %d",GetLastError());
            if (logger) logger.log("Failed to copy data from the indicator, error code %d" + IntegerToString(GetLastError()) + "; symbol " + signal_symbol + "; shift: " + IntegerToString(shift) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            return MC_BO_CONTRACT_UNKNOWN_STATE;
        }
        const double up = up_buffer[0];
        const double dn = dn_buffer[0];

        if (up == EMPTY_VALUE && dn == EMPTY_VALUE) return MC_BO_CONTRACT_UNKNOWN_STATE;
        while (up != EMPTY_VALUE) {
            if (config.is_use_null_signal && up == 0.0) break;
            if (logger) logger.log(signal_symbol + " up: " + DoubleToString(up) + "; shift: " + IntegerToString(shift) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            return MC_BO_CONTRACT_BUY;
        }
        while (dn != EMPTY_VALUE) {
            if (config.is_use_null_signal && dn == 0.0) break;
            if (logger) logger.log(signal_symbol + " dn: " + DoubleToString(dn) + "; shift: " + IntegerToString(shift) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            return MC_BO_CONTRACT_SELL;
        }
        return MC_BO_CONTRACT_UNKNOWN_STATE;
    }
    
    void calc_signal() {
        datetime open_bar_time = get_candle_time();
        if (open_bar_time == 0) return;
        if (block_bar_time >= open_bar_time) return;
        if (prev_bar_time == 0) prev_bar_time = open_bar_time;
    
        //{ Handle the new bar event
        if (open_bar_time > prev_bar_time) {
            prev_bar_time = open_bar_time;
            prev_signal_type = signal_type;
            if (config.entry_type == MC_BO_ON_NEW_BAR &&
                config.is_calc_previous_signal && 
                prev_signal_type == MC_BO_CONTRACT_UNKNOWN_STATE) {
                prev_signal_type = get_signal(1);
                if (logger) logger.log(signal_symbol + " (new bar event) prev signal: " + get_str_mc_contract_type(prev_signal_type) + "; shift: 1; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            }
        }
        //}

        signal_type = get_signal(0);
        signal_bar_time = iTime(signal_symbol, PERIOD_CURRENT, 0); 

        if (config.is_calc_previous_signal && 
            config.entry_type != MC_BO_ON_NEW_BAR &&
            config.time_previous_signal > 0 &&
            signal_type == MC_BO_CONTRACT_UNKNOWN_STATE) {
            const datetime current_time = TimeCurrent();
            const datetime elapsed_time = current_time - open_bar_time;
            if (elapsed_time < config.time_previous_signal) {
                signal_type = get_signal(1);
                signal_bar_time = iTime(signal_symbol, PERIOD_CURRENT, 1); 
                if (logger) logger.log(signal_symbol + " prev signal: " + get_str_mc_contract_type(signal_type) + "; shift: 1; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            }
        }
    }

public:

    virtual bool process_signal(const MegaConnectorBoContractType contract_type);

    MegaConnectorIndicatorSignalV1() {
        signal_type             = MC_BO_CONTRACT_UNKNOWN_STATE;
        prev_signal_type        = MC_BO_CONTRACT_UNKNOWN_STATE;
        signal_bar_time         = 0;
        is_signal_canceled      = false;
        block_time              = 0;
        block_bar_time          = 0;
        const datetime current_time = TimeCurrent();
        const long period_duration = Period() * homura::DurationTimePeriod::SEC_PER_MIN;
        last_open_bar_time      = (datetime)((long)current_time - ((long)current_time % period_duration));
        prev_bar_time           = get_candle_time();
        handle = INVALID_HANDLE;
        logger = NULL;
        signal_symbol = Symbol();
    }
    
    void set_logger(MegaConnectorLogger *logger_ptr) {
        logger = logger_ptr;
    }
    
    void set_entry_type(const MegaConnectorEntryType _entry_type) {
        config.entry_type = _entry_type;
    }
    
    bool init() {
        handle = iCustom(
            NULL,
            PERIOD_CURRENT,
            config.indicator_file_name);
        if (handle == INVALID_HANDLE) return false;
        return true;
    }
    
    void update_price() {
        calc_signal();
  
        // On the intrabar entry
        if (config.entry_type == MC_BO_ON_NEW_BAR) return;
        if (signal_type == MC_BO_CONTRACT_UNKNOWN_STATE) return;
        if (!config.is_connected) return;
        
        datetime open_bar_time = get_candle_time();
        if (open_bar_time == 0) return;
        if (block_bar_time >= signal_bar_time) return;
        
        const datetime gmt_time = TimeGMT();
        if (block_time >= gmt_time) return;
        
        if (signal_type != MC_BO_CONTRACT_UNKNOWN_STATE) {
            if (process_signal(signal_type)) {
                block_bar_time = signal_bar_time;
                block_time = gmt_time + config.signal_block_time;
                if (logger) logger.log(signal_symbol + " (intrabar) signal time: " + TimeToString(block_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            }
        } 
    }
    
    void update_timer() {
        // On the new bar entry
        if (config.entry_type != MC_BO_ON_NEW_BAR) return;
        if (!config.is_connected) return;

        datetime open_bar_time = get_candle_time();
        if (open_bar_time == 0) return;
        
        const datetime gmt_time = TimeGMT();
        const datetime current_time = TimeCurrent();
        const long period_duration = Period() * homura::DurationTimePeriod::SEC_PER_MIN;
        const datetime last_close_bar_time = last_open_bar_time + (datetime)period_duration;
        
        if (current_time >= last_close_bar_time) {
            last_open_bar_time = (datetime)((long)current_time - ((long)current_time % period_duration));
            const datetime close_bar_time = open_bar_time + (datetime)period_duration;
            
            if (current_time >= close_bar_time) {
                if (signal_type == MC_BO_CONTRACT_UNKNOWN_STATE) return;
                if (block_bar_time >= close_bar_time) return;
                if (block_time >= gmt_time) return;
                
                if (process_signal(signal_type)) {
                    block_bar_time = close_bar_time;
                    block_time = gmt_time + config.signal_block_time;
                    if (logger) logger.log(signal_symbol +  " (new bar; 1) signal time: " + TimeToString(block_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
                }
            } else {
                if (prev_signal_type == MC_BO_CONTRACT_UNKNOWN_STATE) return;
                if (block_bar_time >= open_bar_time) return;
                if (block_time >= gmt_time) return;
                
                if (process_signal(prev_signal_type)) {
                    block_bar_time = open_bar_time;
                    block_time = gmt_time + config.signal_block_time;
                    if (logger) logger.log(signal_symbol +  " (new bar; 2) signal time: " + TimeToString(block_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
                }
            }
        }
        if (config.is_calc_previous_signal) {
            if (prev_signal_type == MC_BO_CONTRACT_UNKNOWN_STATE) return;
            if (block_bar_time >= open_bar_time) return;
            if (block_time >= gmt_time) return;
            
            if (process_signal(prev_signal_type)) {
                block_bar_time = open_bar_time;
                block_time = gmt_time + config.signal_block_time;
                if (logger) logger.log(signal_symbol +  " (new bar; prev) signal time: " + TimeToString(block_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            }
        }
        //}
    }
    
};
//+------------------------------------------------------------------+
#endif
//+------------------------------------------------------------------+
