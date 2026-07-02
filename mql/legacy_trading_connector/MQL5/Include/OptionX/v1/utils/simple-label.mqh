//+------------------------------------------------------------------+
//|                                                 simple-label.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_SIMPLE_LABEL_MQH
#define MEGA_CONNECTOR_SIMPLE_LABEL_MQH

#property copyright "Copyright 2022, MegaConnector."
#property link      "https://mega-connector.com/"

class MegaConnectorSimpleLabel {
public:
    long              chart_ID;     // chart's ID
    string            name;         // label name
    int               sub_window;   // subwindow index
    int               x;            // X coordinate
    int               y;            // Y coordinate
    ENUM_BASE_CORNER  corner;       // chart corner for anchoring
    string            text;         // text
    string            font;         // font
    int               font_size;    // font size
    color             clr;          // color
    double            angle;        // text slope
    ENUM_ANCHOR_POINT anchor;       // anchor type
    bool              back;         // in the background
    bool              selection;    // highlight to move
    bool              hidden;       // hidden in the object list
    long              z_order;

                     MegaConnectorSimpleLabel() {
        chart_ID=0;               // chart's ID
        name="Label";             // label name
        sub_window=0;             // subwindow index
        x=0;                      // X coordinate
        y=0;                      // Y coordinate
        corner=CORNER_LEFT_UPPER; // chart corner for anchoring
        text="Label";             // text
        font="Arial";             // font
        font_size=10;             // font size
        clr=clrRed;               // color
        angle=0.0;                // text slope
        anchor=ANCHOR_LEFT_UPPER; // anchor type
        back=false;               // in the background
        selection=false;          // highlight to move
        hidden=true;              // hidden in the object list
        z_order=0;
    };

                    ~MegaConnectorSimpleLabel() {
        remove();
    }

//+------------------------------------------------------------------+
//| Создает кнопку                                                   |
//+------------------------------------------------------------------+
    bool             create() {
        //--- reset the error value
        ResetLastError();
        //--- create a text label
        if(!ObjectCreate(chart_ID,name,OBJ_LABEL,sub_window,0,0)) {
            Print(__FUNCTION__,": failed to create text label! Error code = ",GetLastError());
            return(false);
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

        //--- успешное выполнение
        //перерисуем график
        ChartRedraw();
        return(true);
    }

    bool move  (const int    move_x=0,   // X coordinate
                const int    move_y=0) { // Y coordinate
        //--- reset the error value
        ResetLastError();
        //--- move the text label
        if(!ObjectSetInteger(chart_ID,name,OBJPROP_XDISTANCE,move_x)) {
            Print(__FUNCTION__, ": failed to move X coordinate of the label! Error code = ",GetLastError());
            return(false);
        }
        if(!ObjectSetInteger(chart_ID,name,OBJPROP_YDISTANCE,move_y)) {
            Print(__FUNCTION__,": failed to move Y coordinate of the label! Error code = ",GetLastError());
            return(false);
        }
        //--- successful execution
        return(true);
    }

    bool             change_corner(const ENUM_BASE_CORNER new_corner=CORNER_LEFT_UPPER) { // chart corner for anchoring
        //--- reset the error value
        ResetLastError();
        //--- change anchor corner
        if(!ObjectSetInteger(chart_ID,name,OBJPROP_CORNER,new_corner)) {
            Print(__FUNCTION__,": failed to change the anchor corner! Error code = ",GetLastError());
            return(false);
        }
        //--- successful execution
        return(true);
    }

    bool             text_change(const string new_text="Text") { // text
        //--- reset the error value
        ResetLastError();
        //--- change object text
        if(!ObjectSetString(chart_ID,name,OBJPROP_TEXT,new_text)) {
            Print(__FUNCTION__,": failed to change the text! Error code = ",GetLastError());
            return(false);
        }
        //--- successful execution
        return(true);
    }

    bool             remove() {
        //--- reset the error value
        ResetLastError();
        //--- delete the label
        if(!ObjectDelete(chart_ID,name)) {
            Print(__FUNCTION__,": failed to delete a text label! Error code = ",GetLastError());
            return(false);
        }
        //--- successful execution
        return(true);
    }
};

#endif
//+------------------------------------------------------------------+
