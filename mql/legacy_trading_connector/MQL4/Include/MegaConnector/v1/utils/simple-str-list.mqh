//+------------------------------------------------------------------+
//|                                              simple-str-list.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_SIMPLE_STR_LIST_MQH
#define MEGA_CONNECTOR_SIMPLE_STR_LIST_MQH

#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://mega-connector.com/"

#include "simple-label.mqh"

class MegaConnectorSimpleStrList {
private:
    MegaConnectorSimpleLabel    label_list[];
public:
    string           empty_text;

    string           name;         // label name
    int              x;            // X coordinate
    int              y;            // Y coordinate
    int              indent;
    int              height;
    string           font;         // font
    int              font_size;    // font size
    color            clr;          // цвет текста
    bool             hidden;

    MegaConnectorSimpleStrList() {
        name        = "label_list_";
        x           = 0;              // X coordinate
        y           = 0;              // Y coordinate
        indent      = 8;
        height      = 24;
        font        = "Arial";        // font
        font_size   = 10;             // font size
        clr         = clrAzure;
        hidden      = true;
    };

    ~MegaConnectorSimpleStrList() {
        ArrayFree(label_list);
    }

    void show(const bool value) {
        hidden = !value;
        if (hidden) {
            ArrayFree(label_list);
        }
    }

    void set_list(const string &list_str[]) {
        if (hidden) {
            ArrayFree(label_list);
            return;
        }
        const int list_str_size = ArraySize(list_str);
        if (list_str_size == 0) {
            if (StringLen(empty_text) > 0) {
                if (ArraySize(label_list) > 1) {
                    ArrayResize(label_list, 1);
                }
                if (ArraySize(label_list) == 1) {
                    label_list[0].text_change(empty_text);
                    return;
                }
                ArrayResize(label_list, 1);
                label_list[0].name = name + IntegerToString(0);
                label_list[0].x = x;
                label_list[0].y = y + 8 + height * 0;
                label_list[0].text = empty_text;
                label_list[0].font = font;
                label_list[0].font_size = font_size;
                label_list[0].clr = clr;
                label_list[0].create();
                return;
            } else {
                if (ArraySize(label_list) > 0) ArrayFree(label_list);
            }
            return;
        }

        int i = 0;
        const int start_index = ArraySize(label_list);
        if (list_str_size >= start_index) {
            ArrayResize(label_list, list_str_size);
            for (i = 0; i < start_index; ++i) {
                label_list[i].text_change(list_str[i]);
            }
            for (i = start_index; i < list_str_size; ++i) {
                label_list[i].name = name + IntegerToString(i);
                label_list[i].x = x;
                label_list[i].y = y + indent + height * i;
                label_list[i].text = list_str[i];
                label_list[i].font = font;
                label_list[i].font_size = font_size;
                label_list[i].clr = clr;
                label_list[i].create();
            }
        } else {
            ArrayResize(label_list, list_str_size);
            for (i = 0; i < list_str_size; ++i) {
                label_list[i].text_change(list_str[i]);
            }
        }
    }
};

#endif
