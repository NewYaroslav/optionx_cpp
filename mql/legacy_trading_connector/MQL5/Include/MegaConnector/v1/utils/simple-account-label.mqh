//+------------------------------------------------------------------+
//|                                         simple-account-label.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_SIMPLE_ACCOUNT_LABEL_MQH
#define MEGA_CONNECTOR_SIMPLE_ACCOUNT_LABEL_MQH

#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://mega-connector.com/"

#include "..\part\common.mqh"
#include "..\utils\unique_id.mqh"
#include <Arrays\ArrayString.mqh>

class MegaConnectorSimpleAccountLabel {
private:

    CArrayString                m_obj_name_list;
    MegaConnectorAccountInfo    m_account_info;

    ENUM_BASE_CORNER            m_corner;
    ENUM_ANCHOR_POINT           m_anchor;

    string                      m_uid;
    string                      m_mega_connector_status;
    string                      m_broker;
    string                      m_broker_status;
    string                      m_balance_status;

    color                       m_color_back;
    color                       m_color_header;
    color                       m_color_border;
    color                       m_color_mega_connector;
    color                       m_color_broker;
    color                       m_color_balance;
    color                       m_color_state;

    long             m_chart_height;
    long             m_chart_width;

    bool             m_hidden;
    string           m_font;
    int              m_font_size;
    bool             m_init;

    void add_label_list(const string &obj_name) {
        if (m_obj_name_list.Search(obj_name) >= 0) return;
        m_obj_name_list.Add(obj_name);
        m_obj_name_list.Sort();
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

    bool rect_label_create(
        const long              chart_ID     = 0,               // ID графика
        const string            name         = "RectLabel",     // имя метки
        const int               sub_window   = 0,               // номер подокна
        const int               x        = 0,                   // координата по оси X
        const int               y        = 0,                   // координата по оси Y
        const int               width    = 256,                 // ширина
        const int               height   = 128,                 // высота
        const color             back_clr = C'33,33,33',         // цвет фона
        const ENUM_BORDER_TYPE  border   = BORDER_FLAT,         // тип границы
        const color             clr      = C'42,42,42',         // цвет плоской границы (Flat)
        const ENUM_LINE_STYLE   style    = STYLE_SOLID,         // стиль плоской границы
        const int               line_width  =1,                 // толщина плоской границы
        const bool              back        = false,            // на заднем плане
        const bool              selection   = false,            // выделить для перемещений
        const bool              hidden      = true,             // скрыт в списке объектов
        const long              z_order     = 0) {              // приоритет на нажатие мышью
        //--- сбросим значение ошибки
        ResetLastError();
        //--- создадим прямоугольную метку
        if (!m_init) {
            if(!ObjectCreate(chart_ID,name,OBJ_RECTANGLE_LABEL,sub_window,0,0)) {
                Print(__FUNCTION__, ": failed to create text label! Error code = ",GetLastError());
                return(false);
            }
        }
        //--- установим координаты метки
        ObjectSetInteger(chart_ID,name,OBJPROP_XDISTANCE,x);
        ObjectSetInteger(chart_ID,name,OBJPROP_YDISTANCE,y);
        //--- установим размеры метки
        ObjectSetInteger(chart_ID,name,OBJPROP_XSIZE,width);
        ObjectSetInteger(chart_ID,name,OBJPROP_YSIZE,height);
        //--- установим цвет фона
        ObjectSetInteger(chart_ID,name,OBJPROP_BGCOLOR,back_clr);
        //--- установим тип границы
        ObjectSetInteger(chart_ID,name,OBJPROP_BORDER_TYPE,border);
        //--- установим угол графика, относительно которого будут определяться координаты точки
        ObjectSetInteger(chart_ID,name,OBJPROP_CORNER,CORNER_LEFT_UPPER);
        ObjectSetInteger(chart_ID,name,OBJPROP_ANCHOR,ANCHOR_LEFT_UPPER);
        //--- установим цвет плоской рамки (в режиме Flat)
        ObjectSetInteger(chart_ID,name,OBJPROP_COLOR,clr);
        //--- установим стиль линии плоской рамки
        ObjectSetInteger(chart_ID,name,OBJPROP_STYLE,style);
        //--- установим толщину плоской границы
        ObjectSetInteger(chart_ID,name,OBJPROP_WIDTH,line_width);
        //--- отобразим на переднем (false) или заднем (true) плане
        ObjectSetInteger(chart_ID,name,OBJPROP_BACK,back);
        //--- включим (true) или отключим (false) режим перемещения метки мышью
        ObjectSetInteger(chart_ID,name,OBJPROP_SELECTABLE,selection);
        ObjectSetInteger(chart_ID,name,OBJPROP_SELECTED,selection);
        //--- скроем (true) или отобразим (false) имя графического объекта в списке объектов
        ObjectSetInteger(chart_ID,name,OBJPROP_HIDDEN,hidden);
        //--- установим приоритет на получение события нажатия мыши на графике
        ObjectSetInteger(chart_ID,name,OBJPROP_ZORDER,z_order);
        //--- успешное выполнение
        return(true);
    }

    bool label_create_v0(
        const long              chart_ID=0,               // chart's ID
        const string            name="Label",             // label name
        const int               sub_window=0,             // subwindow index
        const int               x=0,                      // X coordinate
        const int               y=0,                      // Y coordinate
        const string            text="Label",             // text
        const string            font="Arial",             // font
        const int               font_size=10,             // font size
        const color             clr=clrRed,               // color
        const double            angle=0.0,                // text slope
        const bool              back=false,               // in the background
        const bool              selection=false,          // highlight to move
        const bool              hidden=true,              // hidden in the object list
        const long              z_order=0) {              // priority for mouse click
        ResetLastError();
        if (!m_init) {
            if(!ObjectCreate(chart_ID,name,OBJ_LABEL,sub_window,0,0)) {
                Print(__FUNCTION__, ": failed to create text label! Error code = ",GetLastError());
                return false;
            }
        }
        //--- set label coordinates
        ObjectSetInteger(chart_ID,name,OBJPROP_XDISTANCE,x);
        ObjectSetInteger(chart_ID,name,OBJPROP_YDISTANCE,y);
        //--- set the chart's corner, relative to which point coordinates are defined
        ObjectSetInteger(chart_ID,name,OBJPROP_CORNER,CORNER_LEFT_UPPER);
        //--- set the text
        ObjectSetString(chart_ID,name,OBJPROP_TEXT,text);
        //--- set text font
        ObjectSetString(chart_ID,name,OBJPROP_FONT,font);
        //--- set font size
        ObjectSetInteger(chart_ID,name,OBJPROP_FONTSIZE,font_size);
        //--- set the slope angle of the text
        ObjectSetDouble(chart_ID,name,OBJPROP_ANGLE,angle);
        //--- set anchor type
        ObjectSetInteger(chart_ID,name,OBJPROP_ANCHOR, ANCHOR_LEFT_UPPER);
        //--- set color
        ObjectSetInteger(chart_ID,name,OBJPROP_COLOR,clr);
        //--- display in the foreground (false) or background (true)
        ObjectSetInteger(chart_ID,name,OBJPROP_BACK,back);
        //--- enable (true) or disable (false) the mode of moving the label by mouse
        ObjectSetInteger(chart_ID,name,OBJPROP_SELECTABLE,selection);
        ObjectSetInteger(chart_ID,name,OBJPROP_SELECTED,selection);
        //--- hide (true) or display (false) graphical object name in the object list
        ObjectSetInteger(chart_ID,name,OBJPROP_HIDDEN,hidden);
        //--- set the priority for receiving the event of a mouse click in the chart
        ObjectSetInteger(chart_ID,name,OBJPROP_ZORDER,z_order);
        //--- successful execution
        return true;
    }
    
    bool label_create(
                 const long              chart_ID=0,               // chart's ID
                 const string            name="Label",             // label name
                 const int               sub_window=0,             // subwindow index
                 const int               x=0,                      // X coordinate
                 const int               y=0,                      // Y coordinate
                 const ENUM_BASE_CORNER  corner=CORNER_LEFT_UPPER, // chart corner for anchoring
                 const string            text="Label",             // text
                 const string            font="Arial",             // font
                 const int               font_size=10,             // font size
                 const color             clr=clrRed,               // color
                 const double            angle=0.0,                // text slope
                 const ENUM_ANCHOR_POINT anchor=ANCHOR_LEFT_UPPER, // anchor type
                 const bool              back=false,               // in the background
                 const bool              selection=false,          // highlight to move
                 const bool              hidden=true,              // hidden in the object list
                 const long              z_order=0)                // priority for mouse click
    {
        ResetLastError();
        if (!m_init) {
            if(!ObjectCreate(chart_ID,name,OBJ_LABEL,sub_window,0,0)) {
              Print(__FUNCTION__, ": failed to create text label! Error code = ",GetLastError());
              return false;
            }
        }
        //--- set label coordinates
        ObjectSetInteger(chart_ID,name,OBJPROP_XDISTANCE,x);
        ObjectSetInteger(chart_ID,name,OBJPROP_YDISTANCE,y);
        //--- set the chart's corner, relative to which point coordinates are defined
        ObjectSetInteger(chart_ID,name,OBJPROP_CORNER,corner);
        //--- set the text
        ObjectSetString(chart_ID,name,OBJPROP_TEXT,text);
        //--- set text font
        ObjectSetString(chart_ID,name,OBJPROP_FONT,font);
        //--- set font size
        ObjectSetInteger(chart_ID,name,OBJPROP_FONTSIZE,font_size);
        //--- set the slope angle of the text
        ObjectSetDouble(chart_ID,name,OBJPROP_ANGLE,angle);
        //--- set anchor type
        ObjectSetInteger(chart_ID,name,OBJPROP_ANCHOR,anchor);
        //--- set color
        ObjectSetInteger(chart_ID,name,OBJPROP_COLOR,clr);
        //--- display in the foreground (false) or background (true)
        ObjectSetInteger(chart_ID,name,OBJPROP_BACK,back);
        //--- enable (true) or disable (false) the mode of moving the label by mouse
        ObjectSetInteger(chart_ID,name,OBJPROP_SELECTABLE,selection);
        ObjectSetInteger(chart_ID,name,OBJPROP_SELECTED,selection);
        //--- hide (true) or display (false) graphical object name in the object list
        ObjectSetInteger(chart_ID,name,OBJPROP_HIDDEN,hidden);
        //--- set the priority for receiving the event of a mouse click in the chart
        ObjectSetInteger(chart_ID,name,OBJPROP_ZORDER,z_order);
        //--- successful execution
        return true;
    }

/*
    bool draw_v1() {
        if (m_hidden) return true;
        //const string obj_name_bg = "window_" + m_uid;
        //const string obj_name_hr = "hr_" + m_uid;
        const string obj_name_header_1 = "mega_connector_" + m_uid;
        const string obj_name_status_1 = "mega_connector_status_" + m_uid;
        const string obj_name_header_2 = "broker_" + m_uid;
        const string obj_name_status_2 = "broker_status_" + m_uid;
        const string obj_name_header_3 = "balance_" + m_uid;
        const string obj_name_status_3 = "balance_status_" + m_uid;
        
        const int hr_indent_y = 64;
        const int bg_indent_x = 8;
        const int bg_indent_y = 8;

        const int indent_x = 32;
        const int indent_y = 32;
        const int spacing_x = 8;
        const int spacing_y = 2;
 
        long chart_height = 0;
        if (!ChartGetInteger(0, CHART_HEIGHT_IN_PIXELS, 0, chart_height)) return false;

        const string label_col_1[3] = {
            "MegaConnector: ",
            "Balance: ",
            "Broker: "
        };
        
        int label_col_1_w = 0;
        for (int i = 0; i < ArraySize(label_col_1); ++i) {
            TextSetFont(m_font, m_font_size * -10);
            int text_w = 0;
            int text_h = 0;
            TextGetSize(label_col_1[i], text_w, text_h);
            label_col_1_w = MathMax(label_col_1_w, text_w);
        }

        int header_len = 0;
        int font_h = 0;
        
        int header1_x = 0;
        int header2_x = 0;
        int broker_x = 0;
        TextSetFont(m_font, m_font_size * -10);
        TextGetSize(m_mega_connector_status, header1_x, font_h);
        TextSetFont(m_font, m_font_size * -10);
        TextGetSize(m_broker_status, header2_x, font_h);
        TextSetFont(m_font, m_font_size * -10);
        TextGetSize(m_balance_status, broker_x, font_h);
        header_len = MathMax(MathMax(header1_x, header2_x),broker_x);

        const int pos_y1 = (int)chart_height - 3*font_h - indent_y - hr_indent_y;
        const int pos_y2 = (int)chart_height - 2*font_h - indent_y - hr_indent_y;
        const int pos_y3 = (int)chart_height - 1*font_h - indent_y - hr_indent_y;
        const int pos_x = indent_x + header_len;
        const int bg_height = pos_y3 - pos_y1 + font_h + indent_y;
 
        const int bg_start_x = indent_x;
        const int bg_start_y = indent_y;
        
        const int item_col_1_x = indent_x + bg_indent_x;
        const int item_col_2_x = item_col_1_x + label_col_1_w + spacing_x;
        
        //if (!rect_label_create(0, obj_name_bg, 0, bg_start_x, bg_start_y, header_len + 2 * bg_indent_x, bg_height  + 2 * bg_indent_y, m_color_back, BORDER_SUNKEN, m_color_border)) return false;

        int pos_y = indent_y + spacing_y + font_h;
        if (!label_create(0, obj_name_header_1, 0, item_col_1_x, pos_y, label_col_1[0], m_font, m_font_size, m_color_mega_connector, 0)) return false;
        if (!label_create(0, obj_name_status_1, 0, item_col_2_x, pos_y, m_mega_connector_status, m_font, m_font_size, m_color_state, 0)) return false;

        pos_y += spacing_y + font_h;
        if (!label_create(0, obj_name_header_2, 0, item_col_1_x, pos_y, label_col_1[1], m_font, m_font_size, m_color_broker, 0)) return false;
        if (!label_create(0, obj_name_status_2, 0, item_col_2_x, pos_y, m_broker_status, m_font, m_font_size, m_color_state, 0)) return false;

        pos_y += spacing_y + font_h;
        if (!label_create(0, obj_name_header_3, 0, item_col_1_x, pos_y, label_col_1[2], m_font, m_font_size, m_color_balance, 0)) return false;
        if (!label_create(0, obj_name_status_3, 0, item_col_2_x, pos_y, m_balance_status, m_font, m_font_size, m_color_state, 0)) return false;

        //add_label_list(obj_name_bg);
        //add_label_list(obj_name_hr);
        add_label_list(obj_name_header_1);
        add_label_list(obj_name_status_1);
        add_label_list(obj_name_header_2);
        add_label_list(obj_name_status_2);
        add_label_list(obj_name_header_3);
        add_label_list(obj_name_status_3);
        //перерисуем график
        ChartRedraw();
        m_init = true;
        return true;
    }
    */
    
    bool draw() {
        if (m_hidden) return true;
        const string obj_name_header_1 = "mega_connector_" + m_uid;
        const string obj_name_status_1 = "mega_connector_status_" + m_uid;
        const string obj_name_header_2 = "broker_" + m_uid;
        const string obj_name_status_2 = "broker_status_" + m_uid;
        const string obj_name_header_3 = "balance_" + m_uid;
        const string obj_name_status_3 = "balance_status_" + m_uid;

        const int indent_x = 8;
        const int indent_y = 16;

        long chart_height = 0;
        if (!ChartGetInteger(0, CHART_HEIGHT_IN_PIXELS, 0, chart_height)) return false;

        const string label_header_1 = "MegaConnector: ";
        const string label_header_2 = "Balance: ";
        
        int header_len = 0;
        int font_h = 0;
        if (m_anchor == ANCHOR_RIGHT_UPPER || m_anchor == ANCHOR_RIGHT_LOWER) {
            int header1_x = 0;
            int header2_x = 0;
            int broker_x = 0;
            TextSetFont(m_font, m_font_size * -10);
            TextGetSize(m_mega_connector_status, header1_x, font_h);
            TextSetFont(m_font, m_font_size * -10);
            TextGetSize(m_broker_status, header2_x, font_h);
            TextSetFont(m_font, m_font_size * -10);
            TextGetSize(m_balance_status, broker_x, font_h);
            header_len = MathMax(MathMax(header1_x, header2_x),broker_x);
        } else {
            int header1_x = 0;
            int header2_x = 0;
            int broker_x = 0;
            TextSetFont(m_font, m_font_size * -10);
            TextGetSize(label_header_1, header1_x, font_h);
            TextSetFont(m_font, m_font_size * -10);
            TextGetSize(label_header_2, header2_x, font_h);
            TextSetFont(m_font, m_font_size * -10);
            TextGetSize(m_broker, broker_x, font_h);
            header_len = MathMax(MathMax(header1_x, header2_x),broker_x);
        }
        
        const int pos_y1 = (int)chart_height - 3*font_h - indent_y;
        const int pos_y2 = (int)chart_height - 2*font_h - indent_y;
        const int pos_y3 = (int)chart_height - 1*font_h - indent_y;
        const int pos_x = indent_x + header_len;

        if (m_anchor == ANCHOR_RIGHT_UPPER) {
            if (!label_create(0, obj_name_header_1, 0, pos_x, pos_y1, m_corner, label_header_1, m_font, m_font_size, m_color_mega_connector, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_1, 0, indent_x, pos_y1, m_corner, m_mega_connector_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        
            if (!label_create(0, obj_name_header_2, 0, pos_x, pos_y2, m_corner, m_broker, m_font, m_font_size, m_color_broker, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_2, 0, indent_x, pos_y2, m_corner, m_broker_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        
            if (!label_create(0, obj_name_header_3, 0, pos_x, pos_y3, m_corner, label_header_2, m_font, m_font_size, m_color_balance, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_3, 0, indent_x, pos_y3, m_corner, m_balance_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        } else 
        if (m_anchor == ANCHOR_LEFT_LOWER) {
            if (!label_create(0, obj_name_header_1, 0, indent_x, pos_y3, m_corner, label_header_1, m_font, m_font_size, m_color_mega_connector, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_1, 0, pos_x, pos_y3, m_corner, m_mega_connector_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        
            if (!label_create(0, obj_name_header_2, 0, indent_x, pos_y2, m_corner, m_broker, m_font, m_font_size, m_color_broker, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_2, 0, pos_x, pos_y2, m_corner, m_broker_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        
            if (!label_create(0, obj_name_header_3, 0, indent_x, pos_y1, m_corner, label_header_2, m_font, m_font_size, m_color_balance, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_3, 0, pos_x, pos_y1, m_corner, m_balance_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false;
        } else 
        if (m_anchor == ANCHOR_RIGHT_LOWER) {
            if (!label_create(0, obj_name_header_1, 0, pos_x, pos_y3, m_corner, label_header_1, m_font, m_font_size, m_color_mega_connector, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_1, 0, indent_x, pos_y3, m_corner, m_mega_connector_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        
            if (!label_create(0, obj_name_header_2, 0, pos_x, pos_y2, m_corner, m_broker, m_font, m_font_size, m_color_broker, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_2, 0, indent_x, pos_y2, m_corner, m_broker_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        
            if (!label_create(0, obj_name_header_3, 0, pos_x, pos_y1, m_corner, label_header_2, m_font, m_font_size, m_color_balance, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_3, 0, indent_x, pos_y1, m_corner, m_balance_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false;
        } else {
            if (!label_create(0, obj_name_header_1, 0, indent_x, pos_y1, m_corner, label_header_1, m_font, m_font_size, m_color_mega_connector, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_1, 0, pos_x, pos_y1, m_corner, m_mega_connector_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        
            if (!label_create(0, obj_name_header_2, 0, indent_x, pos_y2, m_corner, m_broker, m_font, m_font_size, m_color_broker, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_2, 0, pos_x, pos_y2, m_corner, m_broker_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        
            if (!label_create(0, obj_name_header_3, 0, indent_x, pos_y3, m_corner, label_header_2, m_font, m_font_size, m_color_balance, 0, m_anchor)) return false; 
            if (!label_create(0, obj_name_status_3, 0, pos_x, pos_y3, m_corner, m_balance_status, m_font, m_font_size, m_color_state, 0, m_anchor)) return false; 
        }
        
        add_label_list(obj_name_header_1);
        add_label_list(obj_name_status_1);
        add_label_list(obj_name_header_2);
        add_label_list(obj_name_status_2);
        add_label_list(obj_name_header_3);
        add_label_list(obj_name_status_3);
        //перерисуем график
        ChartRedraw();
        m_init = true;
        return true;
    }

public:

    MegaConnectorSimpleAccountLabel() {
        m_corner        = CORNER_LEFT_UPPER;
        m_anchor        = ANCHOR_LEFT_UPPER;
        m_font          = "Arial";
        m_font_size     = 10;
        m_uid           = get_str_unique_id();
        m_mega_connector_status = get_str_connection_status(false);
        m_broker                = "Broker: ";
        m_broker_status         = get_str_connection_status(false);
        m_balance_status        = "---";
        m_hidden        = false;
        m_init          = false;

        m_color_mega_connector  = clrSteelBlue;
        m_color_broker          = clrSkyBlue;
        m_color_balance         = clrLightSkyBlue;
        m_color_state           = clrLightGray;
        
        m_color_back            = C'50,50,50';
        m_color_header          = C'33,33,33';
        m_color_border          = C'42,42,42';

        ChartGetInteger(0, CHART_HEIGHT_IN_PIXELS, 0, m_chart_height);
        ChartGetInteger(0, CHART_WIDTH_IN_PIXELS, 0, m_chart_width);
    };

    ~MegaConnectorSimpleAccountLabel() {
        remove();
    }

    bool show(const bool arg_value) {
        m_hidden = !arg_value;
        return draw();
    }

    bool set_anchor_point(const MegaConnectorAnchorPoint arg_anchor) {
        switch (arg_anchor) {
        case MegaConnectorAnchorPoint::MC_TOP_LEFT:
            m_corner = CORNER_LEFT_LOWER;
            m_anchor = ANCHOR_LEFT_LOWER;
            break;
        case MegaConnectorAnchorPoint::MC_TOP_RIGHT:
            m_corner = CORNER_RIGHT_LOWER;
            m_anchor = ANCHOR_RIGHT_LOWER;
            break;
        case MegaConnectorAnchorPoint::MC_BOTTOM_LEFT:
            m_corner = CORNER_LEFT_UPPER;
            m_anchor = ANCHOR_LEFT_UPPER;
            break;
        case MegaConnectorAnchorPoint::MC_BOTTOM_RIGHT:
            m_corner = CORNER_RIGHT_UPPER;
            m_anchor = ANCHOR_RIGHT_UPPER;
            break;
        }
        return draw();
    };

    bool set_mega_connector_status(const bool value) {
        if (m_hidden) return true;
        m_mega_connector_status = get_str_connection_status(value);
        return draw();
    }

    bool set_account_info(const MegaConnectorAccountInfo &arg_account_info) {
        if (m_hidden) return true;
        m_account_info = arg_account_info;
        if (arg_account_info.broker == MC_BROKER_INTRADE_BAR) {
            m_broker = "Intrade.Bar: ";
        } else if (arg_account_info.broker == MC_BROKER_OLYMP_TRADE) {
            m_broker = "Olymp Trade: ";
        } else if (arg_account_info.broker == MC_BROKER_TURBO_XBT) {
            m_broker = "Turbo-XBT: ";
        } else {
            m_broker = "Broker: ";
        }

        if (arg_account_info.currency == "BTC" ||
                arg_account_info.currency == "ETH") {
            m_balance_status = DoubleToString(arg_account_info.balance, 8) + " " + arg_account_info.currency;
        } else {
            m_balance_status = DoubleToString(arg_account_info.balance, 2) + " " + arg_account_info.currency;
        }

        if (arg_account_info.is_demo) {
            m_balance_status += "; (DEMO)";
        } else {
            m_balance_status += "; (REAL)";
        }

        m_broker_status = get_str_connection_status(arg_account_info.is_connected);

        return draw();
    }

    void update() {
        if (m_hidden) return;
        long chart_height = 0;
        long chart_width = 0;
        if (!ChartGetInteger(0, CHART_HEIGHT_IN_PIXELS, 0, chart_height)) return;
        if (!ChartGetInteger(0, CHART_WIDTH_IN_PIXELS, 0, chart_width)) return;
        if (m_chart_height == chart_height && m_chart_width == chart_width) return;
        draw();
    }

    void clear() {
        remove();
    }
};

#endif
//+------------------------------------------------------------------+
