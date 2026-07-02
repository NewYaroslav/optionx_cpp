//+------------------------------------------------------------------+
//|                                                simple-button.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_SIMPLE_BUTTON_MQH
#define MEGA_CONNECTOR_SIMPLE_BUTTON_MQH

#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://mega-connector.com/"

class MegaConnectorSimpleButton {
public:
    long              chart_ID;     // ID графика
    string            name;         // имя кнопки
    int               sub_window;   // номер подокна
    int               x;            // координата по оси X
    int               y;            // координата по оси Y
    int               width;        // ширина кнопки
    int               height;       // высота кнопки
    ENUM_BASE_CORNER  corner;       // угол графика для привязки
    string            text;         // текст
    string            font;         // шрифт
    int               font_size;    // размер шрифта
    color             clr;          // цвет текста
    color             back_clr;     // цвет фона
    color             border_clr;   // цвет границы
    bool              state;        // нажата/отжата
    bool              back;         // на заднем плане
    bool              selection;    // выделить для перемещений
    bool              hidden;       // скрыт в списке объектов
    long              z_order;      // приоритет на нажатие мышью

    bool              is_press;

                     MegaConnectorSimpleButton() {
        chart_ID=0;               // ID графика
        name="Button";            // имя кнопки
        sub_window=0;             // номер подокна
        x=0;                      // координата по оси X
        y=0;                      // координата по оси Y
        width=50;                 // ширина кнопки
        height=18;                // высота кнопки
        corner=CORNER_LEFT_UPPER; // угол графика для привязки
        text="Button";            // текст
        font="Arial";             // шрифт
        font_size=10;             // размер шрифта
        clr=clrBlack;             // цвет текста
        back_clr=C'236,233,216';  // цвет фона
        border_clr=clrNONE;       // цвет границы
        state=false;              // нажата/отжата
        back=false;               // на заднем плане
        selection=false;          // выделить для перемещений
        hidden=true;              // скрыт в списке объектов
        z_order=0;                // приоритет на нажатие мышью
        is_press = false;
    };

                    ~MegaConnectorSimpleButton() {
        remove();
    }

//+------------------------------------------------------------------+
//| Создает кнопку                                                   |
//+------------------------------------------------------------------+
    bool             create() {
        //--- сбросим значение ошибки
        ResetLastError();
        //--- создадим кнопку
        if(!ObjectCreate(chart_ID,name,OBJ_BUTTON,sub_window,0,0)) {
            Print(__FUNCTION__,": не удалось создать кнопку! Код ошибки = ",GetLastError());
            return(false);
        }
        //--- установим координаты кнопки
        ObjectSetInteger(chart_ID,name,OBJPROP_XDISTANCE,x);
        ObjectSetInteger(chart_ID,name,OBJPROP_YDISTANCE,y);
        //--- установим размер кнопки
        ObjectSetInteger(chart_ID,name,OBJPROP_XSIZE,width);
        ObjectSetInteger(chart_ID,name,OBJPROP_YSIZE,height);
        //--- установим угол графика, относительно которого будут определяться координаты точки
        ObjectSetInteger(chart_ID,name,OBJPROP_CORNER,corner);
        //--- установим текст
        ObjectSetString(chart_ID,name,OBJPROP_TEXT,text);
        //--- установим шрифт текста
        ObjectSetString(chart_ID,name,OBJPROP_FONT,font);
        //--- установим размер шрифта
        ObjectSetInteger(chart_ID,name,OBJPROP_FONTSIZE,font_size);
        //--- установим цвет текста
        ObjectSetInteger(chart_ID,name,OBJPROP_COLOR,clr);
        //--- установим цвет фона
        ObjectSetInteger(chart_ID,name,OBJPROP_BGCOLOR,back_clr);
        //--- установим цвет границы
        ObjectSetInteger(chart_ID,name,OBJPROP_BORDER_COLOR,border_clr);
        //--- отобразим на переднем (false) или заднем (true) плане
        ObjectSetInteger(chart_ID,name,OBJPROP_BACK,back);
        //--- переведем кнопку в заданное состояние
        ObjectSetInteger(chart_ID,name,OBJPROP_STATE,state);
        //--- включим (true) или отключим (false) режим перемещения кнопки мышью
        ObjectSetInteger(chart_ID,name,OBJPROP_SELECTABLE,selection);
        ObjectSetInteger(chart_ID,name,OBJPROP_SELECTED,selection);
        //--- скроем (true) или отобразим (false) имя графического объекта в списке объектов
        ObjectSetInteger(chart_ID,name,OBJPROP_HIDDEN,hidden);
        //--- установим приоритет на получение события нажатия мыши на графике
        ObjectSetInteger(chart_ID,name,OBJPROP_ZORDER,z_order);
        //--- успешное выполнение
        //перерисуем график
        ChartRedraw();
        return(true);
    }

//+------------------------------------------------------------------+
//| Удаляет кнопку                                                   |
//+------------------------------------------------------------------+
    bool             remove() {
        //--- сбросим значение ошибки
        ResetLastError();
        //--- удалим кнопку
        if(!ObjectDelete(chart_ID, name)) {
            Print(__FUNCTION__, ": не удалось удалить кнопку! Код ошибки = ",GetLastError());
            return(false);
        }
        //--- успешное выполнение

        return(true);
    }

    bool             pressured() {
        is_press = ObjectGetInteger(0, name, OBJPROP_STATE, true);

        if(is_press) return(true);
        return(false);
    };

    bool             release() {
        bool value = ObjectGetInteger(0, name, OBJPROP_STATE, true);
        if (!value && is_press) {
            is_press = value;
            return(true);
        }
        is_press = value;
        return(false);
    }

    void             reset() {
        //--- переведем кнопку в ненажатое состояние
        ObjectSetInteger(0, name, OBJPROP_STATE, false);
        //--- перерисуем график
        ChartRedraw();
    }
};

#endif
//+------------------------------------------------------------------+
