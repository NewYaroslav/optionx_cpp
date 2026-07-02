//+------------------------------------------------------------------+
//|                                                       logger.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
//
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_LOGGER_MQH
#define MEGA_CONNECTOR_LOGGER_MQH

#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://mega-connector.com/"
#property strict

#include "../part/common.mqh"

#define McLogger MegaConnectorLogger
//+------------------------------------------------------------------+
class MegaConnectorLogger {
private:
    int              log_handle;
    ulong            file_date;
    string           save_path_name;
public:

    bool open(const string path_name) {
        save_path_name = path_name;
        static const ulong SECONDS_IN_DAY = 60*60*24;
        file_date = SECONDS_IN_DAY * ((ulong)TimeGMT() / SECONDS_IN_DAY);
        string log_name = "logs\\" + path_name + "\\" + TimeToStr(TimeGMT(),TIME_DATE) + ".txt";
        log_handle = FileOpen(log_name, FILE_READ | FILE_WRITE | FILE_TXT | FILE_ANSI | FILE_SHARE_READ |FILE_SHARE_WRITE, " ");
        if(log_handle < 0 ) {
            int log_last_error = GetLastError();
            Print( "MegaConnectorLogger: Could not open file '", log_name, "', error code = ", log_last_error);
            return false;
        }
        return true;
    }

    void close() {
        if( log_handle > 0 ) FileClose(log_handle);
        log_handle = -1;
    }

    MegaConnectorLogger() {
        file_date = 0;
        log_handle = -1;
    }

    MegaConnectorLogger(const string path_name) {
        file_date = 0;
        log_handle = -1;
        open(path_name);
    }

    ~MegaConnectorLogger() {
        close();
    }

    bool log(const string text) {
        int log_ast_error = 0;
        if (log_handle < 0 ) {
            Print("MegaConnectorLogger: Log write error! Text: ", text );
            return false;
        }

        static const ulong SECONDS_IN_DAY = 60*60*24;
        ulong new_file_date = SECONDS_IN_DAY * ((ulong)TimeGMT() / SECONDS_IN_DAY);
        if (new_file_date > file_date) {
            file_date = new_file_date;
            close();
            open(save_path_name);
        }

        //---- Перемещаем файловый указатель в конец файла
        if(!FileSeek(log_handle, 0, SEEK_END)) {
            log_ast_error = GetLastError();
            Print("MegaConnectorLogger: FileSeek (", log_handle, ", 0, SEEK_END) - Error #", log_ast_error);
            return false;
        }

        string temp = TimeToStr(TimeGMT(), TIME_SECONDS | TIME_MINUTES | TIME_DATE);
        temp += " | ";
        temp += text;
        //temp += "\n";

        if (FileWrite(log_handle, temp) <= 0) {
            log_ast_error = GetLastError();
            Print( "MegaConnectorLogger: FileWrite (", log_handle, ", ", text, ") - Error #", log_ast_error);
            return false;
        }
        //---- Сбрасываем записанный тест на диск
        FileFlush(log_handle);
        return true;
    }

    bool log_update_history(const string symbol, const ENUM_TIMEFRAMES timeframe, const int value) {
        string title;
        if (value == 0) title = "update history error: ";
        else title = "update history (" + IntegerToString(value) + ") ok: ";
        string message = title + symbol + " " + IntegerToString(timeframe) +
                         ", time cur " + TimeToStr(TimeCurrent(),TIME_SECONDS | TIME_MINUTES) +
                         ", time bar " + TimeToStr(iTime(symbol, timeframe, 0),TIME_SECONDS | TIME_MINUTES);
        Print(message);
        return log(message);
    }

    bool log_update_history_v2(const string symbol, const ENUM_TIMEFRAMES timeframe, const int value) {
        string title;
        if (value == 0) title = "check update history error: ";
        else title = "check update history (" + IntegerToString(value) + ") ok: ";
        string message = title + symbol + " " + IntegerToString(timeframe) +
                         ", time cur " + TimeToStr(TimeCurrent(),TIME_SECONDS | TIME_MINUTES) +
                         ", time bar " + TimeToStr(iTime(symbol, timeframe, 0),TIME_SECONDS | TIME_MINUTES);
        Print(message);
        return log(message);
    }

    bool log_bar_relevance(const string symbol, const ENUM_TIMEFRAMES timeframe, const int value) {
        string title;
        if (value == 0) title = "bar  error: ";
        else title = "bar (" + IntegerToString(value) + ") ok: ";
        string message = title + symbol + " " + IntegerToString(timeframe) +
                         ", time cur " + TimeToStr(TimeCurrent(),TIME_SECONDS | TIME_MINUTES) +
                         ", time bar " + TimeToStr(iTime(symbol, timeframe, 0),TIME_SECONDS | TIME_MINUTES);
        Print(message);
        return log(message);
    }

    bool log_bo(const MegaConnectorBoRequest &bo_request) {
        string message =
            get_str_mc_contract_type(bo_request.contract_type) + " " +
            bo_request.symbols + " " +
            IntegerToString(bo_request.duration) +
            " '" + bo_request.signal_name +
            "', winrate " + DoubleToString(bo_request.winrate * 100, 1) + "%, date " +
            TimeToStr(TimeCurrent(),TIME_SECONDS | TIME_MINUTES);
        Print(message);
        return log(message);
    }
};
//+------------------------------------------------------------------+
#endif
