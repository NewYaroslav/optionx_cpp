//+------------------------------------------------------------------+
//|                                               signal-capture.mqh |
//|                     Copyright 2022-2024, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
//
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_SIGNAL_CAPTURE_MQH
#define MEGA_CONNECTOR_SIGNAL_CAPTURE_MQH

#property copyright "Copyright 2022-2024, MegaConnector Software."
#property link      "https://mega-connector.com"
#property strict

#include <Arrays\ArrayString.mqh>
#include "..\lib\legacy-time-utils.mqh"
#include "..\part\common.mqh"
#include "logger.mqh"
//+------------------------------------------------------------------+
class McSignalCaptureConfig {
public:
	McEntryType entry_type;                 /**< Type of entry point */
	string symbol;                          /**< Symbol name */
	int period;
	string indicator_file_name;             /**< Indicator file name */
	uint indicator_id_up;                   /**< Buffer index for UP signal */
	uint indicator_id_dn;                   /**< Buffer index for DN signal */
	long arrow_up_code;
	long arrow_dn_code;
	bool enable_signal_arrow_capture;
	bool enable_zero_signal_ignore;         /**< Enable zero signal ignore */
	bool enable_capture_previous_bar;       /**< Enable capture on the previous bar */
	bool enable_capture_before_bar_close;   /**< Enable capture before bar close */
	bool enable_capture_after_bar_close;    /**< Enable capture after bar close */
	int previous_bar_wait_time;             /**< Waiting time for the signal on the previous bar */
	int bar_close_wait_time;                /**< Bar closing time delay */
	int signal_block_time;                  /**< Signal blocking time */

	McSignalCaptureConfig() {
		entry_type = MC_BO_INTRABAR;
		symbol = Symbol();
		period = Period();
		indicator_id_up = 0;
		indicator_id_dn = 1;
		arrow_up_code = 233;
		arrow_dn_code = 234;
		enable_signal_arrow_capture     = false;
		enable_zero_signal_ignore       = true;
		enable_capture_previous_bar     = true;
		enable_capture_before_bar_close = false;
		enable_capture_after_bar_close  = false;
		previous_bar_wait_time          = 0;
		bar_close_wait_time             = 0;
		signal_block_time               = 0;
	}
};
//+------------------------------------------------------------------+
class McSignalCapture {
public:

	McSignalCaptureConfig config;

	virtual bool process_signal(const MegaConnectorBoContractType contract_type);

	McSignalCapture() {
        block_time              = 0;
        block_bar_time          = 0;
        prev_bar_counter        = 0;
        prev_bar_time           = get_candle_time(0);
        handle = INVALID_HANDLE;
        logger = NULL;
        is_connected = false;
    }

    ~McSignalCapture() {
        if (config.enable_signal_arrow_capture) {
            save_arrow_signals();
        }
    }

    void set_logger(McLogger *logger_ptr) {
        logger = logger_ptr;
    }

	void set_connected(const bool value) {
        is_connected = value;
    }

    void set_entry_type(const McEntryType _entry_type) {
        config.entry_type = _entry_type;
    }

    bool init() {
        if (config.enable_signal_arrow_capture) {
            return load_arrow_signals();
        }
        const double value_up = iCustom(
                        config.symbol,
                        config.period,
                        config.indicator_file_name,
                        config.indicator_id_up,
                        0);
        const int err_up = GetLastError();
        if (err_up != 0) return false;
        const double value_dn = iCustom(
                        config.symbol,
                        config.period,
                        config.indicator_file_name,
                        config.indicator_id_up,
                        0);
        const int err_dn = GetLastError();
        if (err_dn != 0) return false;
        return true;
    }

    void update() {
        if (!is_connected) return;
        capture_intra_bar();
        capture_end_of_bar();
    }

private:

    inline bool contains_arrow_signal(const string &arrow_name) {
        return (arrow_signals.Search(arrow_name) != -1);
    }

    inline bool add_arrow_signal(const string &arrow_name) {
        if (!contains_arrow_signal(arrow_name)) {
            if (arrow_signals.Total() > 1) {
                arrow_signals.InsertSort(arrow_name);
                return true;
            }
            arrow_signals.Add(arrow_name);
            arrow_signals.Sort();
            return true;
        }
        return false;
    }

    inline string get_file_name() {
        const string file_name = "signals\\AutoConnectorLite-" + config.symbol + "-M" + IntegerToString(config.period) + ".bin";
        return file_name;
    }

    bool load_arrow_signals() {
        const string file_name = get_file_name();
        int file_handle = FileOpen(file_name, FILE_READ|FILE_WRITE|FILE_BIN|FILE_ANSI);
        if (file_handle == INVALID_HANDLE) {
            PrintFormat("%s: Error opening signals file %s", config.symbol, file_name);
            if (logger) logger.log(config.symbol + ": Error opening signals file " + file_name);
            return false;
        }
        if (!arrow_signals.Load(file_handle)) {
            FileClose(file_handle);
            arrow_signals.Sort();
            PrintFormat("%s: Error loading signals from file %s", config.symbol, file_name);
            if (logger) logger.log(config.symbol + ": Error loading signals from file " + file_name);
            if (!FileDelete(file_name)) {
                PrintFormat("%s: Error deleting signals file %s", config.symbol, file_name);
                if (logger) logger.log(config.symbol + ": Error loading data from file " + file_name);
                return false;
            }
        }
        FileClose(file_handle);
        arrow_signals.Sort();
        return true;
    }

    bool save_arrow_signals() {
        const string file_name = get_file_name();
        int file_handle = FileOpen(file_name, FILE_READ|FILE_WRITE|FILE_BIN|FILE_ANSI);
        if (file_handle == INVALID_HANDLE) {
            PrintFormat("%s: Error opening signals file %s", config.symbol, file_name);
            if (logger) logger.log(config.symbol + ": Error opening signals file " + file_name);
            return false;
        }
        if (!arrow_signals.Save(file_handle)) {
            FileClose(file_handle);
            PrintFormat("%s: Error saving signals file.", config.symbol);
            if (logger) logger.log(config.symbol + ": Error saving signals file.");
            return false;
        }
        FileClose(file_handle);
        return true;
    }

    McBoContractType get_arrow_signal(const int shift) {
        const int total_objects = ObjectsTotal();
        //PrintFormat("get_arrow_signal %d %d", total_objects, shift);
        for (int i = 0; i < total_objects; i++) {
            const string arrow_name = ObjectName(0, i);
            const ENUM_OBJECT type = (ENUM_OBJECT)ObjectGetInteger(0, arrow_name, OBJPROP_TYPE);
            if (type != OBJ_ARROW) continue;
            if (StringLen(arrow_name) > 4 && StringSubstr(arrow_name, 0, 4) == "#MC_") continue;

            datetime arrow_time = (datetime)ObjectGetInteger(0, arrow_name, OBJPROP_TIME);
            const datetime period_duration = config.period * 60;
            arrow_time -= arrow_time % period_duration;
            const datetime open_bar_time = get_candle_time(shift);
            //Print("arrow " + TimeToString(arrow_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS), " t: " + TimeToString(open_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS));
			   if (config.entry_type == MC_BO_ON_NEW_BAR) {
			      if (config.enable_capture_after_bar_close) {
			         if (arrow_time != (open_bar_time - config.period) && arrow_time != open_bar_time) {
			            //Print("---skip arrow " + TimeToString(arrow_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS), " t: " + TimeToString((open_bar_time - config.period), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
			            continue;
			         }
			      } else {
			         if (arrow_time != open_bar_time) {
			            //Print("--skip arrow " + TimeToString(arrow_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS), " t: " + TimeToString(open_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS));
			            continue;
			         }
			      }
			   } else {
			      if (arrow_time != open_bar_time) {
			         //Print("-skip arrow " + TimeToString(arrow_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS), " t: " + TimeToString(open_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS));
			         continue;
			      }
			   }


            const long arrow_code = (int)ObjectGetInteger(0, arrow_name, OBJPROP_ARROWCODE);
            if (arrow_code == config.arrow_up_code) {
                if (add_arrow_signal(arrow_name)) {
                    if (logger) {
                        logger.log(
                            config.symbol + ": Arrow code: " + IntegerToString(arrow_code) +
                            "; shift: " + IntegerToString(shift) +
                            "; bar time: " + TimeToString(open_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) +
                            "; arrow time: " + TimeToString(arrow_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) +
                            "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
                    }
                    Print("arrow: " + arrow_name + "; time: " + TimeToString(arrow_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS), "; bar: " + TimeToString(open_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS));
                    //arrow_signals.Save(file_handle);
                    //--- проверка блокировки сигнала
                    if (block_time > TimeGMT()) return MC_BO_CONTRACT_UNKNOWN_STATE;
                    return MC_BO_CONTRACT_BUY;
                }
            } else
            if (arrow_code == config.arrow_dn_code) {
                if (add_arrow_signal(arrow_name)) {
                    if (logger) {
                        logger.log(
                            config.symbol + ": Arrow code: " + IntegerToString(arrow_code) +
                            "; shift: " + IntegerToString(shift) +
                            "; bar time: " + TimeToString(open_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) +
                            "; arrow time: " + TimeToString(arrow_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) +
                            "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
                    }
                    Print("arrow: " + arrow_name + "; time: " + TimeToString(arrow_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS), "; bar: " + TimeToString(open_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS));
                    //arrow_signals.Save(file_handle);
                    //--- проверка блокировки сигнала
                    if (block_time > TimeGMT()) return MC_BO_CONTRACT_UNKNOWN_STATE;
                    return MC_BO_CONTRACT_SELL;
                }
            }
        }
        return MC_BO_CONTRACT_UNKNOWN_STATE;
    }

	McBoContractType get_signal(const int shift) {
	    if (config.enable_signal_arrow_capture) return get_arrow_signal(shift);

        const double up = iCustom(
                        config.symbol,
                        config.period,
                        config.indicator_file_name,
                        config.indicator_id_up,
                        shift);
        const int err_up = GetLastError();

        if (err_up != 0) {
            PrintFormat("%s: Failed to copy data from the indicator, error code: %d", config.symbol, err_up);
            if (logger) logger.log(config.symbol + ": Failed to copy data from the indicator, error code: %d" + IntegerToString(err_up) + "; shift: " + IntegerToString(shift) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            return MC_BO_CONTRACT_UNKNOWN_STATE;
        }

        const double dn = iCustom(
                        config.symbol,
                        config.period,
                        config.indicator_file_name,
                        config.indicator_id_dn,
                        shift);
        const int err_dn = GetLastError();

        if (err_dn != 0) {
            PrintFormat("%s: Failed to copy data from the indicator, error code: %d", config.symbol, err_dn);
            if (logger) logger.log(config.symbol + ": Failed to copy data from the indicator, error code: %d" + IntegerToString(err_dn) + "; shift: " + IntegerToString(shift) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            return MC_BO_CONTRACT_UNKNOWN_STATE;
        }

        if (up == EMPTY_VALUE && dn == EMPTY_VALUE) return MC_BO_CONTRACT_UNKNOWN_STATE;
        while (up != EMPTY_VALUE) {
            if (config.enable_zero_signal_ignore && up == 0.0) break;
            if (logger) logger.log("up: " + DoubleToString(up) + "; shift: " + IntegerToString(shift) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            return MC_BO_CONTRACT_BUY;
        }
        while (dn != EMPTY_VALUE) {
            if (config.enable_zero_signal_ignore && dn == 0.0) break;
            if (logger) logger.log("dn: " + DoubleToString(dn) + "; shift: " + IntegerToString(shift) + "; time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS));
            return MC_BO_CONTRACT_SELL;
        }
        return MC_BO_CONTRACT_UNKNOWN_STATE;
    }

    datetime get_candle_time(const int shift) {
        datetime bar_time = 0;
        const int attempts = 3;
        int attempt = 0;
        while (attempt < attempts) {
            ResetLastError();
            bar_time = iTime(config.symbol, PERIOD_CURRENT, shift);
            if (bar_time) {
                if (GetLastError() == 0) {
                    return bar_time;
                }
            }
            ++attempt;
        }
        PrintFormat("%s: Candle are not synchronized yet. Time: %s; Error=%d", Symbol(), TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS), _LastError);
        if (logger) logger.log(config.symbol + ": Candle are not synchronized yet. Time: " + TimeToString(TimeCurrent(), TIME_DATE | TIME_MINUTES | TIME_SECONDS) + "; Error: " + IntegerToString(_LastError));
        return 0;
    }

    inline bool is_block(const datetime signal_bar_time) {
        if (config.enable_signal_arrow_capture) return false;
        const datetime gmt_time = TimeGMT();
        if (block_time > gmt_time) return true;
        if (block_bar_time >= signal_bar_time) return true;
        return false;
    }

	void capture_intra_bar() {
		if (config.entry_type == MC_BO_ON_NEW_BAR) return;
		const datetime open_bar_time = get_candle_time(0);
      if (open_bar_time == 0) return;

		int shift = 0;
		datetime signal_bar_time = iTime(config.symbol, config.period, shift);
		if (is_block(signal_bar_time)) return;
		McBoContractType signal_type = get_signal(shift);

		const datetime current_time = TimeCurrent();

        if (signal_type == MC_BO_CONTRACT_UNKNOWN_STATE &&
			config.enable_capture_previous_bar &&
            config.previous_bar_wait_time > 0) {
            const datetime elapsed_time = current_time - open_bar_time;
			if (elapsed_time >= config.previous_bar_wait_time) return;
            shift = 1;
            signal_bar_time = iTime(config.symbol, config.period, shift);
            if (is_block(signal_bar_time)) return;
			signal_type = get_signal(shift);
        }
		if (signal_type == MC_BO_CONTRACT_UNKNOWN_STATE) return;

		if (process_signal(signal_type)) {
			block_bar_time = signal_bar_time;
			block_time = TimeGMT() + config.signal_block_time;
			if (logger) {
			logger.log(
				"(intrabar) signal: " + get_str_mc_contract_type(signal_type) +
				"; shift: " + IntegerToString(shift) +
				"; time: " + TimeToString(current_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) +
				"; bar time: " + TimeToString(signal_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS));
		}
		}
	}

	void capture_end_of_bar() {
		if (config.entry_type == MC_BO_INTRABAR) return;
		const datetime open_bar_time = get_candle_time(0);
        if (open_bar_time == 0) return;
		if (prev_bar_time == 0) prev_bar_time = open_bar_time;

		int shift = 0;
		McBoContractType signal_type = MC_BO_CONTRACT_UNKNOWN_STATE;
		datetime signal_bar_time = 0;

		const datetime current_time = TimeCurrent();
		const long period_duration = config.period * TimeShield::sec_per_min();
		const datetime close_bar_time = open_bar_time + (datetime)period_duration;

		// обрабатываем событие, когда бар еще не закрылся
		if (config.enable_capture_before_bar_close && close_bar_time >= current_time) {
			const datetime elapsed_time = close_bar_time - current_time;
			if (elapsed_time <= config.bar_close_wait_time) {
			    signal_type = get_signal(shift);
			    signal_bar_time = iTime(config.symbol, config.period, shift);
			    //Print("elapsed_time: ", elapsed_time, "; time: ", TimeToString(current_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS));
			}
		}

		// обрабатываем событие, когда бар закрылся
		if (config.enable_capture_after_bar_close && open_bar_time > prev_bar_time) {
		    const int max_prev_bar_counter = 2;
		    if (prev_bar_counter >= max_prev_bar_counter) {
		        prev_bar_counter = 0;
		    prev_bar_time = open_bar_time;
		    shift = 1;
		    signal_type = get_signal(shift);
			signal_bar_time = iTime(config.symbol, config.period, shift);
			} else {
			    prev_bar_counter++;
			}
		}

		if (signal_type == MC_BO_CONTRACT_UNKNOWN_STATE) return;
		if (is_block(signal_bar_time)) return;

        if (process_signal(signal_type)) {
			block_bar_time = signal_bar_time;
			block_time = TimeGMT() + config.signal_block_time;
			if (logger) {
			logger.log(
				"(end of bar) signal: " + get_str_mc_contract_type(signal_type) +
				"; shift: " + IntegerToString(shift) +
				"; time: " + TimeToString(current_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS) +
				"; bar time: " + TimeToString(signal_bar_time, TIME_DATE | TIME_MINUTES | TIME_SECONDS));
		}
		}
	}


    McLogger    *logger;
    int         handle;

    datetime    block_time;
    datetime    block_bar_time;
    datetime    prev_bar_time;
    int         prev_bar_counter;

    CArrayString arrow_signals;

    bool        is_connected;
};
//+------------------------------------------------------------------+
#endif
//+------------------------------------------------------------------+
