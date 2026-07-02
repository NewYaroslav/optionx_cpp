//+------------------------------------------------------------------+
//|                                               charts-updater.mqh |
//|                     Copyright 2022-2024, MegaConnector Software. |
//|                                       https://mega-connector.com |
//+------------------------------------------------------------------+
// 
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_CHARTS_UPDATER_MQH
#define MEGA_CONNECTOR_CHARTS_UPDATER_MQH

#property copyright "Copyright 2022-2024, MegaConnector Software."
#property link      "https://mega-connector.com"
#property strict

#include <WinUser32.mqh>
#import "user32.dll"
    int PostMessageW(int hWnd, int Msg, int wParam, int lParam);
	int RegisterWindowMessageW(string lpString); 
#import

class MegaConnectorChartsUpdater {
private:
    // данные для перерисовки графика
    int hwnd;
    int MT4InternalMsg;
    
    bool symbol_error_flag[];
    
    string symbols[];   // список символов для обновления графиков
    int periods[];      // список периодов
   
public:

    virtual void on_missing_chart(const string &symbol, const int period);
    
    void init_update_window() {
        if(MT4InternalMsg == 0) {
            MT4InternalMsg = RegisterWindowMessageW("MetaTrader4_Internal_Message");
        }
    }
    
    void close() {
        ArrayFree(symbol_error_flag);
        ArrayFree(symbols);
        ArrayFree(periods);
    }

    MegaConnectorChartsUpdater() {
        hwnd = 0;
        MT4InternalMsg = 0;
        init_update_window();
    }
   
    ~MegaConnectorChartsUpdater() {
        close();
    }
    
    void set_periods(const string list_periods_str) {
        string result[];
        StringSplit(list_periods_str, StringGetCharacter(",",0), result);
        const int result_size = ArraySize(result);
        ArrayResize(periods, result_size);
        for(int m = 0; m < result_size; ++m) {
            periods[m] = (int)StringToInteger(result[m]);
        }
        ArrayFree(result);
    }
    
    void set_symbols(const string list_symbols_str) {
        StringSplit(list_symbols_str, StringGetCharacter(",",0), symbols);
    }
   
    void update() {
        const uint symbols_size = ArraySize(symbols);
        const uint periods_size = ArraySize(periods);
        const uint num_symbols_periods = symbols_size * periods_size;
        
        if(ArraySize(symbol_error_flag) != num_symbols_periods) {
            ArrayResize(symbol_error_flag, num_symbols_periods);
            for(uint i = 0; i < num_symbols_periods; ++i) {
                symbol_error_flag[i] = false;
            }
        }
        
        uint index = 0;
        for(uint s = 0; s < symbols_size; ++s) {
            for(uint p = 0; p < periods_size; ++p) {
                hwnd = WindowHandle(symbols[s], periods[p]);
                if (hwnd != 0) {
                    PostMessageW(hwnd, WM_COMMAND, 33324, 0);
                    PostMessageW(hwnd, MT4InternalMsg, 2, 1);
                    symbol_error_flag[index] = false;
                } else 
                if(!symbol_error_flag[index]) { 
                    on_missing_chart(symbols[s], periods[p]);
                    symbol_error_flag[index] = true;
                }
                ++index;
            }
        }
    }
};

#endif