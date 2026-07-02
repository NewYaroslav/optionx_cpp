//+------------------------------------------------------------------+
//|                                                    unique_id.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
//
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_UNIQUE_ID_MQH
#define MEGA_CONNECTOR_UNIQUE_ID_MQH

#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://mega-connector.com/"
#property strict

//+------------------------------------------------------------------+
inline string get_str_unique_id() {
    return IntegerToString(GetTickCount()) + IntegerToString(MathRand());
}

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
/*
inline string get_str_signal_id(const string &arg_app_id, string &arg_signal_id) {
    arg_signal_id = get_str_unique_id();
    return arg_app_id + "&" + arg_signal_id;
}
*/

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
/*
inline string get_str_signal_id_v2(const string &arg_app_id, const string &arg_signal_name, const string &arg_user_data, string &arg_signal_id) {
    arg_signal_id = get_str_unique_id();
    return arg_app_id + "&" + arg_signal_id + "&" + arg_signal_name + "&" + arg_user_data;
}
*/

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
inline string get_str_signal_id_v3(const string &arg_app_id, const string &arg_signal_name, const string &arg_user_data, string &arg_signal_id) {
    arg_signal_id = get_str_unique_id();
    return arg_signal_name + "&" + arg_app_id + "&" + arg_signal_id + "&" + arg_user_data;
}


//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
/*
inline bool check_app_id(const string arg_app_id, const string arg_unique_id, string &arg_signal_id) {
    string result[];               // массив для получения строк
    const ushort u_sep = StringGetCharacter("&", 0);
    const int k = StringSplit(arg_unique_id, u_sep, result);
    if (k != 2 || result[0] != arg_app_id) {
        ArrayFree(result);
        return false;
    }
    arg_signal_id = result[1];
    ArrayFree(result);
    return true;
}
*/

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
/*
inline bool check_app_id_v2(const string arg_data, const string arg_app_id, string &arg_signal_id, string &arg_signal_name, string &arg_user_data) {
    string result[];               // массив для получения строк
    const ushort u_sep = StringGetCharacter("&", 0);
    const int k = StringSplit(arg_data, u_sep, result);
    if (k == 2 && result[0] == arg_app_id) {
        arg_signal_id = result[1];
        arg_signal_name = "";
        arg_user_data = "";
        ArrayFree(result);
        return true;
    }
    if (k == 3 && result[0] == arg_app_id) {
        arg_signal_id = result[1];
        arg_signal_name = result[2];
        arg_user_data = "";
        ArrayFree(result);
        return true;
    }
    if (k == 4 && result[0] == arg_app_id) {
        arg_signal_id = result[1];
        arg_signal_name = result[2];
        arg_user_data = result[3];
        ArrayFree(result);
        return true;
    }
    ArrayFree(result);
    return false;
}
*/
//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
/*
inline bool check_app_id(const string arg_app_id, const string arg_unique_id, string &arg_signal_id) {
    string result[];               // массив для получения строк
    const ushort u_sep = StringGetCharacter("&", 0);
    const int k = StringSplit(arg_unique_id, u_sep, result);
    if (k != 2 || result[0] != arg_app_id) {
        ArrayFree(result);
        return false;
    }
    arg_signal_id = result[1];
    ArrayFree(result);
    return true;
}
*/

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
inline bool check_app_id_v3(const string arg_data, const string arg_app_id, string &arg_signal_id, string &arg_signal_name, string &arg_user_data) {
    string result[];               // массив для получения строк
    const ushort u_sep = StringGetCharacter("&", 0);
    const int k = StringSplit(arg_data, u_sep, result);
    if (k == 2 && result[1] == arg_app_id) {
        arg_signal_id = "";
        arg_signal_name = result[0];
        arg_user_data = "";
        ArrayFree(result);
        return true;
    }
    if (k == 3 && result[1] == arg_app_id) {
        arg_signal_id = result[2];
        arg_signal_name = result[0];
        arg_user_data = "";
        ArrayFree(result);
        return true;
    }
    if (k == 4 && result[1] == arg_app_id) {
        arg_signal_id = result[2];
        arg_signal_name = result[0];
        arg_user_data = result[3];
        ArrayFree(result);
        return true;
    }
    ArrayFree(result);
    return false;
}
//+------------------------------------------------------------------+
#endif
//+------------------------------------------------------------------+
