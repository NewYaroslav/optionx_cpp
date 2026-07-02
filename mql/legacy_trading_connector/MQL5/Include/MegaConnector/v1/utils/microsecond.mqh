//+------------------------------------------------------------------+
//|                                   mega-connector-microsecond.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://t.me/mega_connector |
//+------------------------------------------------------------------+
#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://t.me/mega_connector"

long get_microsecond() { 
    static long mcsec0= 0;
    if (mcsec0==0) { 
        datetime t=TimeGMT();  
        while(TimeGMT() < (t + 1));  
        mcsec0 = (long)GetMicrosecondCount(); 
    }
    long delta_mcsec = (long)GetMicrosecondCount() - mcsec0;
    //return (long)TimeLocal()*1000 + 
    return delta_mcsec % 1000000;
}