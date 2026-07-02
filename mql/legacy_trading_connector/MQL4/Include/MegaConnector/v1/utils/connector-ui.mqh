//+------------------------------------------------------------------+
//|                                                 connector-ui.mqh |
//|                     Copyright 2022-2024, MegaConnector Software. |
//|                                       https://mega-connector.com |
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_CONNECTOR_UI_MQH
#define MEGA_CONNECTOR_CONNECTOR_UI_MQH

#property copyright "Copyright 2022-2024, MegaConnector Software."
#property link      "https://mega-connector.com"
#property strict

#resource "images\\icon-24x24.bmp"

#include <Controls\Defines.mqh>
#include "../part/common.mqh"

#undef CONTROLS_FONT_NAME
#undef CONTROLS_FONT_SIZE

#undef CONTROLS_CLIENT_COLOR_BG
#undef CONTROLS_CLIENT_COLOR_BORDER

#undef CONTROLS_DIALOG_COLOR_BORDER_LIGHT
#undef CONTROLS_DIALOG_COLOR_BORDER_DARK
#undef CONTROLS_DIALOG_COLOR_BG
#undef CONTROLS_DIALOG_COLOR_CAPTION_TEXT
#undef CONTROLS_DIALOG_COLOR_CLIENT_BG
#undef CONTROLS_DIALOG_COLOR_CLIENT_BORDER

#undef CONTROLS_DIALOG_CAPTION_HEIGHT
#undef CONTROLS_BORDER_WIDTH
#undef CONTROLS_SUBWINDOW_GAP
#undef CONTROLS_DIALOG_MINIMIZE_HEIGHT
#undef CONTROLS_DIALOG_BUTTON_OFF

#define CONTROLS_FONT_NAME                      "Trebuchet MS"
#define CONTROLS_FONT_SIZE                      10
//--- Client
#define CONTROLS_CLIENT_COLOR_BG                C'0x32,0x32,0x32'
#define CONTROLS_CLIENT_COLOR_BORDER            C'0x42,0x42,0x42'
//--- Dialog
#define CONTROLS_DIALOG_COLOR_BORDER_LIGHT      C'0x42,0x42,0x42'
#define CONTROLS_DIALOG_COLOR_BORDER_DARK       C'0x16,0x16,0x16'
#define CONTROLS_DIALOG_COLOR_BG                C'0x32,0x32,0x32'
#define CONTROLS_DIALOG_COLOR_CAPTION_TEXT      C'0xCF,0xD8,0xDC'
#define CONTROLS_DIALOG_COLOR_CLIENT_BG         C'0x11,0x11,0x11'
#define CONTROLS_DIALOG_COLOR_CLIENT_BORDER     C'0x16,0x16,0x16'
#define CONTROLS_DIALOG_COLOR_TEXT              C'0xCF,0xD8,0xDC'

// https://www.mql5.com/en/docs/constants/objectconstants/webcolors
#define CONTROLS_DIALOG_COLOR_CONNECTED_TEXT    clrDeepSkyBlue // clrSteelBlue
#define CONTROLS_DIALOG_COLOR_DISCONNECTED_TEXT clrOrangeRed
#define CONTROLS_DIALOG_COLOR_UNDEFINED_TEXT    clrDarkGray
#define CONTROLS_DIALOG_COLOR_BALANCE_TEXT      clrDarkOrange
#define CONTROLS_DIALOG_COLOR_ACCOUNT_TYPE_TEXT clrDarkOrange

#define CONTROLS_DIALOG_CAPTION_HEIGHT          (30)     // height of dialog header
#define CONTROLS_BORDER_WIDTH                   (1)      // border width
#define CONTROLS_SUBWINDOW_GAP                  (7)      // gap between sub-windows along the Y axis
#define CONTROLS_DIALOG_MINIMIZE_HEIGHT         (4*CONTROLS_BORDER_WIDTH+CONTROLS_DIALOG_CAPTION_HEIGHT)
#define CONTROLS_DIALOG_BUTTON_OFF              (7)      // offset of dialog buttons
#define CONTROLS_TEXT_GAP                       (8)

#define CONTROLS_DIALOG_ICON_HEIGHT             (24)
#define CONTROLS_DIALOG_ICON_WIDTH              (24)

#define WINDOW_WIDTH                            (48 + 2 * 96 + CONTROLS_TEXT_GAP * 2 + CONTROLS_BORDER_WIDTH * 2)
#define WINDOW_HEIGH                            (24 + 4 * 16 + 4 * CONTROLS_BORDER_WIDTH + CONTROLS_DIALOG_CAPTION_HEIGHT)
#define WINDOW_GAP                              (32)
#define TEXT_WINDOW_NAME                        "Auto Connector"

#include <Controls\Dialog.mqh>
#include <Controls\Button.mqh>
#include <Controls\Picture.mqh>
#include <Controls\Label.mqh>
#include <Controls\Edit.mqh>
#include <Controls\ListView.mqh>
#include <Controls\ComboBox.mqh>
#include <Controls\SpinEdit.mqh>
#include <Controls\RadioGroup.mqh>
#include <Controls\CheckGroup.mqh>

/*
#import "shell32.dll"
int ShellExecuteW(int hwnd,string Operation,string File,string Parameters,string Directory,int ShowCmd);
#import
*/
//+------------------------------------------------------------------+
//| defines                                                          |
//+------------------------------------------------------------------+
//--- indents and gaps
#define INDENT_LEFT                         (16)      // indent from left (with allowance for border width)
#define INDENT_TOP                          (16)      // indent from top (with allowance for border width)
#define INDENT_RIGHT                        (16)      // indent from right (with allowance for border width)
#define INDENT_BOTTOM                       (16)      // indent from bottom (with allowance for border width)
#define CONTROLS_GAP_X                      (16)      // gap by X coordinate
#define CONTROLS_GAP_Y                      (16)      // gap by Y coordinate
//--- for buttons
#define BUTTON_WIDTH                        (100)     // size by X coordinate
#define BUTTON_HEIGHT                       (20)      // size by Y coordinate
//--- for the indication area
#define EDIT_HEIGHT                         (20)      // size by Y coordinate
//+------------------------------------------------------------------+
//| Class CPanelDialog                                               |
//| Usage: main dialog of the SimplePanel application                |
//+------------------------------------------------------------------+
class McUI : public CAppDialog {
private:
    CPicture        m_picture_icon;
    CLabel          m_software_connection_label;
    CLabel          m_software_connection_status;
    CLabel          m_broker_connection_label;
    CLabel          m_broker_connection_status;
    CLabel          m_balance_label;
    CLabel          m_balance_status;
    CLabel          m_account_type_label;
    CLabel          m_account_type_status;

    //--- acccount status
    bool    m_demo;
    string  m_currency;
    double  m_balance;
    int     m_digits;
    //--- connection status
    bool    m_software_connection;
    bool    m_broker_connection;
    //--- GUI options
    bool    m_enable_mouse_move;
    bool    m_update_chart_size;
    int     m_chart_width;
    int     m_chart_height;
    McAnchorPoint m_anchor_point;
    //--- Mouse
    int     m_windows_mouse_flags;
    bool    m_windows_mouse_drug;

    bool McUI::create_label(
        CLabel &label,
        const int width,
        const int heigh,
        const int x1,
        const int y1,
        const string name,
        const string text,
        const color text_color);

    bool update_balance();
    bool update_chart_size();

public:
    McUI(void);
    ~McUI(void);

    virtual bool    CreateButtonMinMax(void) {
        return true;
    };
    virtual bool    Create(const long chart,const string name,const int subwin,const int x1,const int y1,const int x2,const int y2);

    bool            init(const string name, const bool enable_mouse_move, const McAnchorPoint anchor_point);
    bool            set_software_connection(const bool value);
    bool            set_broker_connection(const bool value);
    bool            set_balance(const bool demo, const string currency, const double balance, const int digits = 2);
    bool            set_account_info(const McAccountInfo &arg_account_info);
    void            chart_event(const int id,const long &lparam,const double &dparam,const string &sparam);

    //--- chart event handler
    virtual bool    OnEvent(const int id,const long &lparam,const double &dparam,const string &sparam);
    virtual bool    OnDefault(const int id,const long &lparam,const double &dparam,const string &sparam);

protected:

    bool            create_icon(void);
    bool            create_picture_icon(void);
    bool            create_caption(void);
    bool            create_software_connection_label(void);
    bool            create_software_connection_status(void);
    bool            create_broker_connection_label(void);
    bool            create_broker_connection_status(void);
    bool            create_balance_label(void);
    bool            create_balance_status(void);
    bool            create_account_type_label(void);
    bool            create_account_type_status(void);

    //--- internal event handlers
    virtual bool    OnResize(void);

    bool            is_dlls_allowed();
};
//+------------------------------------------------------------------+
//| Event Handling                                                   |
//+------------------------------------------------------------------+
EVENT_MAP_BEGIN(McUI)
EVENT_MAP_END(CAppDialog)
//+------------------------------------------------------------------+
//| Constructor                                                      |
//+------------------------------------------------------------------+
McUI::McUI(void) {
    m_demo = false;
    m_currency = "---";
    m_balance = 0;
    m_digits = 2;
    m_software_connection = false;
    m_broker_connection = false;
    m_enable_mouse_move = true;
    m_update_chart_size = false;
    m_chart_width = 0;
    m_chart_height = 0;
    m_anchor_point = McAnchorPoint::MC_BOTTOM_LEFT;
    m_windows_mouse_flags = 0;
    m_windows_mouse_drug = false;
}
//+------------------------------------------------------------------+
//| Destructor                                                       |
//+------------------------------------------------------------------+
McUI::~McUI(void) {}
//+------------------------------------------------------------------+
bool McUI::create_label(
    CLabel &label,
    const int width,
    const int heigh,
    const int x1,
    const int y1,
    const string name,
    const string text,
    const color text_color) {
    const int x2 = x1 + width;
    const int y2 = y1 + heigh;
    if (!label.Create(0, m_name+ name, 0, x1, y1, x2, y2)) return (false);
    if (!label.Color(text_color)) return (false);
    if (!label.ColorBackground(CONTROLS_DIALOG_COLOR_CLIENT_BG)) return (false);
    if (!label.ColorBorder(CONTROLS_DIALOG_COLOR_CLIENT_BG)) return (false);
    label.FontSize(CONTROLS_FONT_SIZE);
    label.Font(CONTROLS_FONT_NAME);
    if (!label.Text(text)) return (false);
    if (!Add(label)) return (false);
    return (true);
}
//+------------------------------------------------------------------+
bool McUI::update_balance() {
    if (!m_software_connection || !m_broker_connection) return true;
    const string text_balance = DoubleToString(m_balance, m_digits) + " " + m_currency;
    const string text_account_type = get_str_account_type(m_demo);
    if (!m_balance_status.Text(text_balance)) return (false);
    if (!m_balance_status.Color(CONTROLS_DIALOG_COLOR_BALANCE_TEXT)) return (false);
    if (!m_account_type_status.Text(text_account_type)) return (false);
    if (!m_account_type_status.Color(CONTROLS_DIALOG_COLOR_ACCOUNT_TYPE_TEXT)) return (false);
    return true;
}
//+------------------------------------------------------------------+
bool McUI::update_chart_size() {
    long chart_height = 0;
    long chart_width = 0;
    if (!ChartGetInteger(0, CHART_HEIGHT_IN_PIXELS, 0, chart_height)) return false;
    if (!ChartGetInteger(0, CHART_WIDTH_IN_PIXELS, 0, chart_width)) return false;
    if (chart_height != m_chart_height || chart_width != m_chart_width) {
        m_chart_height = (int)chart_height;
        m_chart_width = (int)chart_width;
        m_update_chart_size = true;
    }
    return true;
}
//+------------------------------------------------------------------+
bool McUI::Create(const long chart,const string name,const int subwin, const int x1, const int y1, const int x2, const int y2) {
    const int width = WINDOW_WIDTH;
    const int heigh = WINDOW_HEIGH;
    if (!CAppDialog::Create(0,"     " + name, 0,
                            x1, y1, x1 + width, y1 + heigh)) return(false);

    if (!create_picture_icon()) return false;
    if (!create_software_connection_label()) return false;
    if (!create_software_connection_status()) return false;
    if (!create_broker_connection_label()) return false;
    if (!create_broker_connection_status()) return false;
    if (!create_balance_label()) return false;
    if (!create_balance_status()) return false;
    if (!create_account_type_label()) return false;
    if (!create_account_type_status()) return false;
    return true;
}
//+------------------------------------------------------------------+
bool McUI::is_dlls_allowed() {
    const bool iz = (bool)MQLInfoInteger(MQL_DLLS_ALLOWED);
    return iz;
}
//+------------------------------------------------------------------+
bool McUI::create_picture_icon(void) {
    const int width = 24;
    const int heigh = 24;
    int x1 = 3 * CONTROLS_BORDER_WIDTH;
    int y1 = CONTROLS_BORDER_WIDTH + (CONTROLS_DIALOG_CAPTION_HEIGHT - heigh)/2;
    int x2 = x1 + width;
    int y2 = y1 + heigh;
    if (!m_picture_icon.Create(m_chart_id, m_name + "_logo", m_subwin, x1, y1, x2, y2)) return(false);
    m_picture_icon.BmpName("::images\\icon-24x24.bmp");
    if (!CWndContainer::Add(m_picture_icon)) return(false);
    m_picture_icon.Alignment(WND_ALIGN_LEFT, 0, CONTROLS_BORDER_WIDTH, 0, 0);
    return true;
}
//+------------------------------------------------------------------+
bool McUI::create_software_connection_label(void) {
    return create_label(m_software_connection_label,
                        96, 16, CONTROLS_TEXT_GAP, CONTROLS_BORDER_WIDTH,
                        "_software_connection_label", "Software:",
                        CONTROLS_DIALOG_COLOR_TEXT);
}
//+------------------------------------------------------------------+
bool McUI::create_software_connection_status(void) {
    const color text_color = m_software_connection ? CONTROLS_DIALOG_COLOR_CONNECTED_TEXT : CONTROLS_DIALOG_COLOR_DISCONNECTED_TEXT;
    return create_label(m_software_connection_status,
                        96, 16, CONTROLS_TEXT_GAP + 96, CONTROLS_BORDER_WIDTH,
                        "_software_connection_status", get_str_connection_status(m_software_connection),
                        text_color);
}
//+------------------------------------------------------------------+
bool McUI::create_broker_connection_label(void) {
    return create_label(m_broker_connection_label,
                        96, 16, CONTROLS_TEXT_GAP, 2*CONTROLS_BORDER_WIDTH + 16,
                        "_broker_connection_label", "Broker:",
                        CONTROLS_DIALOG_COLOR_TEXT);
}
//+------------------------------------------------------------------+
bool McUI::create_broker_connection_status(void) {
    const color text_color = m_broker_connection ? CONTROLS_DIALOG_COLOR_CONNECTED_TEXT : CONTROLS_DIALOG_COLOR_DISCONNECTED_TEXT;
    return create_label(m_broker_connection_status,
                        96, 16, CONTROLS_TEXT_GAP + 96, 2*CONTROLS_BORDER_WIDTH + 16,
                        "_broker_connection_status", get_str_connection_status(m_broker_connection),
                        text_color);
}
//+------------------------------------------------------------------+
bool McUI::create_balance_label(void) {
    return create_label(m_balance_label,
                        96, 16, CONTROLS_TEXT_GAP, 3*CONTROLS_BORDER_WIDTH + 32,
                        "_balance_label", "Balance:",
                        CONTROLS_DIALOG_COLOR_TEXT);
}
//+------------------------------------------------------------------+
bool McUI::create_balance_status(void) {
    return create_label(m_balance_status,
                        96, 16, CONTROLS_TEXT_GAP + 96, 3*CONTROLS_BORDER_WIDTH + 32,
                        "_balance_status", "---",
                        CONTROLS_DIALOG_COLOR_UNDEFINED_TEXT);
}
//+------------------------------------------------------------------+
bool McUI::create_account_type_label(void) {
    return create_label(m_account_type_label,
                        96, 16, CONTROLS_TEXT_GAP, 4*CONTROLS_BORDER_WIDTH + 48,
                        "_account_type_label", "Account:",
                        CONTROLS_DIALOG_COLOR_TEXT);
}
//+------------------------------------------------------------------+
bool McUI::create_account_type_status(void) {
    return create_label(m_account_type_status,
                        96, 16, CONTROLS_TEXT_GAP + 96, 4*CONTROLS_BORDER_WIDTH + 48,
                        "_account_type_status", "---",
                        CONTROLS_DIALOG_COLOR_UNDEFINED_TEXT);
}
//+------------------------------------------------------------------+
//| Handler of resizing                                              |
//+------------------------------------------------------------------+
bool McUI::OnResize(void) {
    if(!CAppDialog::OnResize()) return(false);
    return(true);
}
//+------------------------------------------------------------------+
//| Event handler                                                    |
//+------------------------------------------------------------------+
bool McUI::OnDefault(const int id, const long &lparam, const double &dparam, const string &sparam) {
    return(false);
}
//+------------------------------------------------------------------+
bool McUI::init(const string arg_name, const bool arg_enable_mouse_move, const McAnchorPoint arg_anchor_point) {
    m_enable_mouse_move = arg_enable_mouse_move;
    if (m_enable_mouse_move) {
        m_anchor_point = McAnchorPoint::MC_BOTTOM_LEFT;
    } else {
        m_anchor_point = arg_anchor_point;
    }
    const int offset = WINDOW_GAP;
    int x = offset;
    int y = offset;
    update_chart_size();
    switch (m_anchor_point) {
    case McAnchorPoint::MC_TOP_LEFT:
        break;
    case McAnchorPoint::MC_TOP_RIGHT:
        x = m_chart_width - WINDOW_WIDTH - offset;
        break;
    case McAnchorPoint::MC_BOTTOM_LEFT:
        y = m_chart_height - WINDOW_HEIGH - offset;
        break;
    case McAnchorPoint::MC_BOTTOM_RIGHT:
        x = m_chart_width - WINDOW_WIDTH - offset;
        y = m_chart_height - WINDOW_HEIGH - offset;
        break;
    }
    if (!Create(0, arg_name, 0, x, y, 0, 0)) return false;
    return true;
}
//+------------------------------------------------------------------+
bool McUI::set_software_connection(const bool value) {
    if (m_software_connection == value) return true;
    m_software_connection = value;
    const string text = get_str_connection_status(m_software_connection);
    if (!m_software_connection_status.Text(text)) return (false);
    const color text_color = m_software_connection ? CONTROLS_DIALOG_COLOR_CONNECTED_TEXT : CONTROLS_DIALOG_COLOR_DISCONNECTED_TEXT;
    if (!m_software_connection_status.Color(text_color)) return (false);
    return update_balance();
}
//+------------------------------------------------------------------+
bool McUI::set_broker_connection(const bool value) {
    if (m_broker_connection == value) return true;
    m_broker_connection = value;
    const string text = get_str_connection_status(m_broker_connection);
    if (!m_broker_connection_status.Text(text)) return (false);
    const color text_color = m_broker_connection ? CONTROLS_DIALOG_COLOR_CONNECTED_TEXT : CONTROLS_DIALOG_COLOR_DISCONNECTED_TEXT;
    if (!m_broker_connection_status.Color(text_color)) return (false);
    return update_balance();
}
//+------------------------------------------------------------------+
bool McUI::set_balance(const bool demo, const string currency, const double balance, const int digits = 2) {
    m_demo = demo;
    m_currency = currency;
    m_balance = balance;
    m_digits = digits;
    return update_balance();
}
//+------------------------------------------------------------------+
bool McUI::set_account_info(const McAccountInfo &arg_account_info) {
    int digits = 2;
    if (    arg_account_info.currency == "BTC" ||
            arg_account_info.currency == "ETH") {
        digits = 8;
    }
    set_broker_connection(arg_account_info.is_connected);
    return set_balance(
        arg_account_info.is_demo, 
        arg_account_info.currency, 
        arg_account_info.balance, 
        digits);
}
//+------------------------------------------------------------------+
void McUI::chart_event(const int id, const long &lparam,const double &dparam,const string &sparam) {
    if (id == CHARTEVENT_MOUSE_MOVE) {
        const int flags = (int)StringToInteger(sparam);
        if ((flags & MOUSE_LEFT) != 0) {
            m_windows_mouse_drug = true;
        } else {
            if ((m_windows_mouse_flags & MOUSE_LEFT) != 0) {
                m_windows_mouse_drug = false;
            }
        }
        m_windows_mouse_flags = flags;
    }
    while (!m_enable_mouse_move && !m_windows_mouse_drug) {
        if (!update_chart_size()) break;
        if (!m_update_chart_size) break;

        const int offset = WINDOW_GAP;
        int x = offset;
        int y = offset;
        switch (m_anchor_point) {
        case McAnchorPoint::MC_TOP_LEFT:
            break;
        case McAnchorPoint::MC_TOP_RIGHT:
            x = m_chart_width - WINDOW_WIDTH - offset;
            break;
        case McAnchorPoint::MC_BOTTOM_LEFT:
            y = m_chart_height - WINDOW_HEIGH - offset;
            break;
        case McAnchorPoint::MC_BOTTOM_RIGHT:
            x = m_chart_width - WINDOW_WIDTH - offset;
            y = m_chart_height - WINDOW_HEIGH - offset;
            break;
        }
        Move(x, y);
        break;
    }
    ChartEvent(id,lparam,dparam,sparam);
}
//+------------------------------------------------------------------+
#endif
