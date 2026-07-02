//+------------------------------------------------------------------+
//|                                              simple-bo-label.mqh |
//|                     Copyright 2022-2024, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_SIMPLE_BO_LABEL_MQH
#define MEGA_CONNECTOR_SIMPLE_BO_LABEL_MQH

#property copyright "Copyright 2022-2024, MegaConnector Software."
#property link      "https://mega-connector.com/"

#include "../part/common.mqh"
#include <Arrays\ArrayString.mqh>

class MegaConnectorSimpleBoLabel {
private:

    CArrayString    m_obj_name_list;
    CArrayString    m_signal_id_list;
    datetime        m_last_date;
    datetime        m_last_time;
    long            m_time_zone;
    Hash*           m_bo_hash;
    int             m_bo_hash_limit;
    bool            m_hidden;
    string          m_font;
    int             m_font_size;
    int             m_indent;
    int             m_indent_text;
    
    long get_time_zone() {
        const long div = 300;
        const long temp = ((TimeCurrent() - TimeGMT()) + (div/2)) / div;
        return temp * div;
    }
    
    void add_label_list(const string &obj_name) {
        if (m_obj_name_list.Search(obj_name) >= 0) return;
        m_obj_name_list.Add(obj_name);
        m_obj_name_list.Sort();
    }
    
    void add_signal_id_list(const string &signal_id) {
        if (m_signal_id_list.Search(signal_id) >= 0) return;
        m_signal_id_list.Add(signal_id);
        m_signal_id_list.Sort();
    }
    
    void remove() {
        ResetLastError();
        const int total = m_obj_name_list.Total();
        for (int i = 0; i < total; ++i) {
            const string obj_name = m_obj_name_list.At(i);
            ObjectDelete(0, obj_name);
        }
        m_obj_name_list.Clear();
        ChartRedraw();
    }

    void draw(const string &signal_id) {
        if (!m_bo_hash.hContainsKey(signal_id)) return;
        MegaConnectorBoResult *bo_result = m_bo_hash.hGet(signal_id);
        const string obj_name_open = "#MC_" + bo_result.symbol + "_" + signal_id;
        const string obj_name_open_text = obj_name_open + "_text";
        
        if (bo_result.open_date > 0 && 
            (bo_result.status == MegaConnectorBoStatus::MC_BO_WAITING_COMPLETION ||
            bo_result.status == MegaConnectorBoStatus::MC_BO_UPDATE)) {

            const int shift = iBarShift(Symbol(), Period(), bo_result.open_date + m_time_zone,false);
            double price = 0;
            double price_text = 0;
            if (bo_result.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_BUY) {
                price_text = price = iLow(Symbol(), Period(), shift);
                price -= m_indent * Point();
                price_text -= (m_indent_text + m_indent) * Point(); 
                
            } else 
            if (bo_result.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_SELL) {
                price_text = price = iHigh(Symbol(), Period(), shift);
                price += m_indent * Point();
                price_text += (m_indent_text + m_indent) * Point(); 
            }
            
            if (m_obj_name_list.Search(obj_name_open) < 0) {
                const datetime time_point = iTime(Symbol(), Period(), shift);
                
                if (m_last_date == 0 || time_point > m_last_date) {
                    m_signal_id_list.Clear();
                    m_last_date = time_point;
                }

                if (m_obj_name_list.Search(obj_name_open) < 0) {
                    ObjectCreate(0, obj_name_open,OBJ_ARROW,0,time_point, price);
                    ObjectCreate(0, obj_name_open_text,OBJ_TEXT,0,time_point, price_text);
                    ObjectSetInteger(0, obj_name_open,OBJPROP_HIDDEN,true);
                    ObjectSetInteger(0, obj_name_open_text,OBJPROP_HIDDEN,true);
                    ObjectSetInteger(0, obj_name_open, OBJPROP_ANCHOR, ANCHOR_CENTER);
                    ObjectSetInteger(0, obj_name_open_text, OBJPROP_ANCHOR, ANCHOR_CENTER);
                    ObjectSetInteger(0, obj_name_open_text, OBJPROP_ANCHOR, ANCHOR_RIGHT);
                }
                
                if (bo_result.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_BUY) {
                    ObjectSetInteger(0, obj_name_open, OBJPROP_COLOR, clrLime);
                    ObjectSetInteger(0, obj_name_open, OBJPROP_ARROWCODE, 233);
      
                    ObjectSetString(0, obj_name_open_text, OBJPROP_FONT, m_font);
                    ObjectSetInteger(0, obj_name_open_text, OBJPROP_FONTSIZE, m_font_size);
                    ObjectSetInteger(0, obj_name_open_text, OBJPROP_COLOR, clrAqua);
                    ObjectSetDouble(0, obj_name_open_text, OBJPROP_ANGLE, 90);
                    if (bo_result.duration <= (500 * 60)) {
                        ObjectSetString(0, obj_name_open_text, OBJPROP_TEXT,
                            "BUY " + DoubleToString(bo_result.amount,2) +
                            "; " + IntegerToString(bo_result.duration/60) + "m");
                    } else {
                        ObjectSetString(0, obj_name_open_text, OBJPROP_TEXT,
                            "BUY " + DoubleToString(bo_result.amount,2));
                    }
                } else 
                if (bo_result.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_SELL) {
                    ObjectSetInteger(0, obj_name_open, OBJPROP_COLOR, clrRed);
                    ObjectSetInteger(0, obj_name_open, OBJPROP_ARROWCODE, 234);
   
                    ObjectSetString(0, obj_name_open_text, OBJPROP_FONT, m_font);
                    ObjectSetInteger(0, obj_name_open_text, OBJPROP_FONTSIZE, m_font_size);
                    ObjectSetInteger(0, obj_name_open_text, OBJPROP_COLOR, clrAqua);
                    ObjectSetDouble(0, obj_name_open_text, OBJPROP_ANGLE,-90);
                    if (bo_result.duration <= (500 * 60)) {
                        ObjectSetString(0, obj_name_open_text, OBJPROP_TEXT,
                            "SELL " + DoubleToString(bo_result.amount,2) +
                            "; " + IntegerToString(bo_result.duration/60) + "m");
                    } else {
                        ObjectSetString(0, obj_name_open_text, OBJPROP_TEXT,
                            "SELL " + DoubleToString(bo_result.amount,2));
                    }
                }
    
                add_label_list(obj_name_open);
                add_label_list(obj_name_open_text);
                add_signal_id_list(signal_id);
            } else {
                ObjectSetDouble(0, obj_name_open, OBJPROP_PRICE, price);
                ObjectSetDouble(0, obj_name_open_text, OBJPROP_PRICE, price_text);
            }
        }
        if (bo_result.open_date > 0 && (
            bo_result.status == MegaConnectorBoStatus::MC_BO_WIN || 
            bo_result.status == MegaConnectorBoStatus::MC_BO_LOSS ||
            bo_result.status == MegaConnectorBoStatus::MC_BO_STANDOFF ||
            bo_result.status == MegaConnectorBoStatus::MC_BO_CHECK_ERROR)) {

            const string obj_name_close = "#MC_" + bo_result.symbol + "_" + signal_id;
            const string obj_name_close_text = obj_name_close + "_text";
            
            const int shift = iBarShift(Symbol(), Period(), bo_result.open_date + m_time_zone,false);
            
            double price = 0;
            double price_text = 0;
            if (bo_result.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_BUY) {
                price_text = price = iLow(Symbol(), Period(), shift);
                price -= m_indent * Point();
                price_text -= (m_indent_text + m_indent) * Point(); 
                
            } else 
            if (bo_result.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_SELL) {
                price_text = price = iHigh(Symbol(), Period(), shift);
                price += m_indent * Point();
                price_text += (m_indent_text + m_indent) * Point(); 
            }
            
            if (m_obj_name_list.Search(obj_name_close) < 0) {
                const datetime time_point = iTime(Symbol(), Period(), shift);
                
                if (m_last_date == 0 || time_point > m_last_date) {
                    m_signal_id_list.Clear();
                    m_last_date = time_point;
                }

                ObjectCreate(0, obj_name_close,OBJ_ARROW,0,time_point, price);
                ObjectCreate(0, obj_name_close_text,OBJ_TEXT,0,time_point, price_text);
                ObjectSetInteger(0, obj_name_close,OBJPROP_HIDDEN,true);
                ObjectSetInteger(0, obj_name_close_text,OBJPROP_HIDDEN,true);
                ObjectSetInteger(0, obj_name_close, OBJPROP_ANCHOR, ANCHOR_CENTER);
                //ObjectSetInteger(0, obj_name_close_text, OBJPROP_CORNER, CORNER_RIGHT_UPPER);
                //ObjectSetInteger(0, obj_name_close_text, OBJPROP_ANCHOR, ANCHOR_RIGHT_UPPER);
                ObjectSetInteger(0, obj_name_close_text, OBJPROP_ANCHOR, ANCHOR_CENTER);
                ObjectSetInteger(0, obj_name_close_text, OBJPROP_ANCHOR, ANCHOR_RIGHT);
                
                if (bo_result.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_BUY) {
                    ObjectSetInteger(0, obj_name_close, OBJPROP_COLOR, clrLime);
                    ObjectSetInteger(0, obj_name_close, OBJPROP_ARROWCODE, 233);
                    ObjectSetDouble(0, obj_name_close_text, OBJPROP_ANGLE,90);
                } else 
                if (bo_result.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_SELL) {
                    ObjectSetInteger(0, obj_name_close, OBJPROP_COLOR, clrRed);
                    ObjectSetInteger(0, obj_name_close, OBJPROP_ARROWCODE, 234);
                    ObjectSetDouble(0, obj_name_close_text, OBJPROP_ANGLE,-90);
                }
                
                add_label_list(obj_name_close);
                add_label_list(obj_name_close_text);
                add_signal_id_list(signal_id);
            } else {
                ObjectSetDouble(0, obj_name_close,OBJPROP_PRICE,price);
                ObjectSetDouble(0, obj_name_close_text,OBJPROP_PRICE,price_text);
            }

            ObjectSetString(0, obj_name_close_text, OBJPROP_FONT, m_font);
            ObjectSetInteger(0, obj_name_close_text, OBJPROP_FONTSIZE, m_font_size);

            if (bo_result.status == MegaConnectorBoStatus::MC_BO_WIN) {
                ObjectSetInteger(0, obj_name_close_text, OBJPROP_COLOR, clrLime);
                ObjectSetString(0, obj_name_close_text,OBJPROP_TEXT, "WIN: +" + DoubleToString(bo_result.profit,2) + "; " + DoubleToString(bo_result.payout * 100.0,1) + "%");
            } else 
            if (bo_result.status == MegaConnectorBoStatus::MC_BO_LOSS) {
                ObjectSetInteger(0, obj_name_close_text, OBJPROP_COLOR, clrRed);
                ObjectSetString(0, obj_name_close_text,OBJPROP_TEXT, "LOSS: -" + DoubleToString(bo_result.amount,2) + "; " + DoubleToString(bo_result.payout * 100.0,1) + "%");
            } else 
            if (bo_result.status == MegaConnectorBoStatus::MC_BO_STANDOFF) {
                ObjectSetInteger(0, obj_name_close_text, OBJPROP_COLOR, clrGreenYellow);
                ObjectSetString(0, obj_name_close_text,OBJPROP_TEXT, "STANDOFF: 0; " + DoubleToString(bo_result.payout * 100.0,1) + "%");
            } else {
                ObjectSetInteger(0, obj_name_close_text, OBJPROP_COLOR, clrYellow);
                ObjectSetString(0, obj_name_close_text, OBJPROP_TEXT, "Error");
            }
        }
        //перерисуем график
        ChartRedraw();
    }

public:

    MegaConnectorSimpleBoLabel() {
        m_bo_hash_limit = 10;
        m_bo_hash = new Hash(m_bo_hash_limit, true); 
        m_time_zone     = get_time_zone();
        m_font          = "Arial";
        m_font_size     = 8;
        m_indent        = 16;
        m_indent_text   = 12;
        m_last_date     = 0;
        m_last_time     = 0;
    };
    
    ~MegaConnectorSimpleBoLabel() {
        remove();
        if (m_bo_hash) delete m_bo_hash;
    }
    
    void show(const bool arg_value){
        m_hidden = !arg_value;
    }
    
    void replace_bo(const MegaConnectorBoResult &arg_bo) {
        if (m_hidden) return;
        m_time_zone     = get_time_zone();
        if (m_bo_hash.getCount() == 0) {
            MegaConnectorBoResult *bo_result = new MegaConnectorBoResult();
            *bo_result = arg_bo;
            m_bo_hash.hPut(arg_bo.signal_id, bo_result);
        } else
        if (m_bo_hash.hContainsKey(arg_bo.signal_id)) {
            MegaConnectorBoResult *bo_result = m_bo_hash.hGet(arg_bo.signal_id);
            *bo_result = arg_bo;
        } else 
        if (m_bo_hash.getCount() < m_bo_hash_limit) {
            MegaConnectorBoResult *bo_result = new MegaConnectorBoResult();
            *bo_result = arg_bo;
            m_bo_hash.hPut(arg_bo.signal_id, bo_result);
        } else {
            // удаляем первый элемент
            HashLoop *l = new HashLoop(m_bo_hash);
            string del_signal_id = l.key();
            delete l;
            m_bo_hash.hDel(del_signal_id);
            // добавляем новый
            MegaConnectorBoResult *bo_result = new MegaConnectorBoResult();
            *bo_result = arg_bo;
            m_bo_hash.hPut(arg_bo.signal_id, bo_result);
        }   
        draw(arg_bo.signal_id);
    }
    
    void update() {
        const datetime current_bar_date = iTime(Symbol(),Period(),0);
        if (m_last_date != current_bar_date) return;
        const long delay = (long)(TimeCurrent() - m_last_time);
        if (delay > 5) {
            m_last_time = TimeCurrent();
            const int total = m_signal_id_list.Total();
            for (int i = 0; i < total; ++i) {
                const string signal_id = m_signal_id_list.At(i);
                draw(signal_id);
            }
        }
    }
    
    void clear() {
        remove();
    }
};

#endif