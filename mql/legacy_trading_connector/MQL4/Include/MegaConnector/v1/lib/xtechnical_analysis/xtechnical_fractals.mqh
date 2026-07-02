#ifndef XTECHNICAL_FRACTALS_MQH
#define XTECHNICAL_FRACTALS_MQH
//+------------------------------------------------------------------+
//|                                   		 xtechnical_fractals.mqh |
//|										 Copyright 2022, Elektro Yar |
//|			      https://github.com/NewYaroslav/xtechnical_analysis |
//+------------------------------------------------------------------+
#property copyright "Copyright 2022, Elektro Yar"
#property link      "https://github.com/NewYaroslav/xtechnical_analysis"
#property version   "1.00"
#property strict

#include "xtechnical_common.mqh"
#include "xtechnical_circular_buffer.mqh"

/** \brief Фракталы Билла Вильямса
 * Оригинал: https://www.mql5.com/en/code/viewcode/7982/130162/Fractals.mq4
 */
class XtechnicalFractals {
private:
	XtechnicalCircularBuffer buffer_up;
	XtechnicalCircularBuffer buffer_dn;

	double save_output_up;
	double save_output_dn;
	double output_up;
	double output_dn;
public:

	XtechnicalFractals() : buffer_up(9), buffer_dn(9) {
		save_output_up = save_output_dn = output_up = output_dn = XtechnicalNaN();
	};

	/** \brief Обновить состояние индикатора
	 * \param high		Максимальное значение бара
	 * \param low		Минимальное значение бара
	 * \param on_up		Функция обратного вызова для верхнего уровня
	 * \param on_dn		Функция обратного вызова для нижнего уровня
	 * \return Вернет true в случае успеха
	 */
	bool update(
			const double high, 
			const double low, 
			double &value_up,
			double &value_dn) {
		value_up = XtechnicalNaN();
		value_dn = XtechnicalNaN();
		buffer_up.update(high);
		buffer_dn.update(low);

		if(buffer_up.full()) {
			// Fractals up
			double values[];
			buffer_up.get_array(values);
			// 0 1 2 3 4 5 6 7 8
			
			//for (int i = 0; i < ArraySize(values); ++i) NormalizeDouble(values[i],5);

			// 5 bars Fractal
			if (values[6] > values[4] &&
				values[6] > values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				save_output_up = output_up = values[6];
				value_up = values[6];
			} else
			// 6 bars Fractal
			if (values[6] > values[3] &&
				values[6] > values[4] &&
				values[6] == values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				save_output_up = output_up = values[6];
				value_up = values[6];
			} else
			// 7 bars Fractal
			if (values[6] > values[2] &&
				values[6] > values[3] &&
				values[6] == values[4] &&
				values[6] >= values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				save_output_up = output_up = values[6];
				value_up = values[6];
			} else
			// 8 bars Fractal
			if (values[6] > values[1] &&
				values[6] > values[2] &&
				values[6] == values[3] &&
				values[6] == values[4] &&
				values[6] >= values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				save_output_up = output_up = values[6];
				value_up = values[6];
			} else
			// 9 bars Fractal
			if (values[6] > values[0] &&
				values[6] > values[1] &&
				values[6] == values[2] &&
				values[6] >= values[3] &&
				values[6] == values[4] &&
				values[6] >= values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				save_output_up = output_up = values[6];
				value_up = values[6];
			} else {
				output_up = save_output_up;
			}
			ArrayFree(values);
		} else return false;
		if(buffer_dn.full()) {
			// Fractals down
			double values[];
			buffer_dn.get_array(values);
			// 0 1 2 3 4 5 6 7 8
			//for (int i = 0; i < ArraySize(values); ++i) NormalizeDouble(values[i],5);

			// 5 bars Fractal
			if (values[6] < values[4] &&
				values[6] < values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				save_output_dn = output_dn = values[6];
				value_dn = values[6];
			} else
			// 6 bars Fractal
			if (values[6] < values[3] &&
				values[6] < values[4] &&
				values[6] == values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				save_output_dn = output_dn = values[6];
				value_dn = values[6];
			} else
			// 7 bars Fractal
			if (values[6] < values[2] &&
				values[6] < values[3] &&
				values[6] == values[4] &&
				values[6] <= values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				save_output_dn = output_dn = values[6];
				value_dn = values[6];
			} else
			// 8 bars Fractal
			if (values[6] < values[1] &&
				values[6] < values[2] &&
				values[6] == values[3] &&
				values[6] == values[4] &&
				values[6] <= values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				save_output_dn = output_dn = values[6];
				value_dn = values[6];
			} else
			// 9 bars Fractal
			if (values[6] < values[0] &&
				values[6] < values[1] &&
				values[6] == values[2] &&
				values[6] <= values[3] &&
				values[6] == values[4] &&
				values[6] <= values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				save_output_dn = output_dn = values[6];
				value_dn = values[6];
			} else {
				output_dn = save_output_dn;
			}
			ArrayFree(values);
		} else return false;
		return true;
	}

	/** \brief Протестировать индикатор
	 * \param high		Максимальное значение бара
	 * \param low		Минимальное значение бара
	 * \param on_up		Функция обратного вызова для верхнего уровня
	 * \param on_dn		Функция обратного вызова для нижнего уровня
	 * \return Вернет 0 в случае успеха, иначе см. ErrorType
	 */
	bool test(
			const double high, 
			const double low, 
			double &value_up,
			double &value_dn) {
		
		value_up = XtechnicalNaN();
		value_dn = XtechnicalNaN();
		
		buffer_up.test(high);
		buffer_dn.test(low);

		if(buffer_up.full()) {
			// Fractals up
			double values[];
			buffer_up.get_array(values);
			// 0 1 2 3 4 5 6 7 8
			//for (int i = 0; i < ArraySize(values); ++i) NormalizeDouble(values[i],5);

			// 5 bars Fractal
			if (values[6] > values[4] &&
				values[6] > values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				output_up = values[6];
				value_up = values[6];
			} else
			// 6 bars Fractal
			if (values[6] > values[3] &&
				values[6] > values[4] &&
				values[6] == values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				output_up = values[6];
				value_up = values[6];
			} else
			// 7 bars Fractal
			if (values[6] > values[2] &&
				values[6] > values[3] &&
				values[6] == values[4] &&
				values[6] >= values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				output_up = values[6];
				value_up = values[6];
			} else
			// 8 bars Fractal
			if (values[6] > values[1] &&
				values[6] > values[2] &&
				values[6] == values[3] &&
				values[6] == values[4] &&
				values[6] >= values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				output_up = values[6];
				value_up = values[6];
			} else
			// 9 bars Fractal
			if (values[6] > values[0] &&
				values[6] > values[1] &&
				values[6] == values[2] &&
				values[6] >= values[3] &&
				values[6] == values[4] &&
				values[6] >= values[5] &&
				values[6] > values[7] &&
				values[6] > values[8]) {
				output_up = values[6];
				value_up = values[6];
			} else {
				output_up = save_output_up;
			}
			ArrayFree(values);
		} else return false;
		if(buffer_dn.full()) {
			// Fractals down
			double values[];
			buffer_dn.get_array(values);
			// 0 1 2 3 4 5 6 7 8
			//for (int i = 0; i < ArraySize(values); ++i) NormalizeDouble(values[i],5);

			// 5 bars Fractal
			if (values[6] < values[4] &&
				values[6] < values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				output_dn = values[6];
				value_dn = values[6];
			} else
			// 6 bars Fractal
			if (values[6] < values[3] &&
				values[6] < values[4] &&
				values[6] == values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				output_dn = values[6];
				value_dn = values[6];
			} else
			// 7 bars Fractal
			if (values[6] < values[2] &&
				values[6] < values[3] &&
				values[6] == values[4] &&
				values[6] <= values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				output_dn = values[6];
				value_dn = values[6];
			} else
			// 8 bars Fractal
			if (values[6] < values[1] &&
				values[6] < values[2] &&
				values[6] == values[3] &&
				values[6] == values[4] &&
				values[6] <= values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				output_dn = values[6];
				value_dn = values[6];
			} else
			// 9 bars Fractal
			if (values[6] < values[0] &&
				values[6] < values[1] &&
				values[6] == values[2] &&
				values[6] <= values[3] &&
				values[6] == values[4] &&
				values[6] <= values[5] &&
				values[6] < values[7] &&
				values[6] < values[8]) {
				output_dn = values[6];
				value_dn = values[6];
			} else {
				output_dn = save_output_dn;
			}
			ArrayFree(values);
		} else return false;
		return true;
	}

	/** \brief Получить значение нижнего фрактала
	 * \return Значение нижнего фрактала
	 */
	inline double get_up() {
		return output_up;
	}

	/** \brief Получить значение верхнего фрактала
	 * \return Значение верхнего фрактала
	 */
	inline double get_dn() {
		return output_dn;
	}

	/** \brief Очистить данные индикатора
	 */
	inline void clear() {
		buffer_up.clear();
		buffer_dn.clear();
		output_up = XtechnicalNaN();
		output_dn = XtechnicalNaN();
		save_output_up = XtechnicalNaN();
		save_output_dn = XtechnicalNaN();
	}
};
//+------------------------------------------------------------------+
class XtechnicalFractalsIndicator {
private:
    ulong start_time;
    ulong stop_time;
    int max_depth;
    int timeframe;
    string symbol;
public:
    XtechnicalFractals fractals;
    
    XtechnicalFractalsIndicator() {
        start_time = 0;
        stop_time = 0;
        timeframe = 0;
        max_depth = 0;
    }
    
    virtual void on_up(
        const string symbol, 
        const int timeframe, 
        const int index, 
        const bool is_close,
        const double value_up,
        const double value_dn);
        
    virtual void on_dn(
        const string symbol, 
        const int timeframe, 
        const int index, 
        const bool is_close,
        const double value_up,
        const double value_dn);
    
    void start(const string us, const int ut, const int umd = 10000) {
        symbol = us;
        timeframe = ut;
        max_depth = umd;
        fractals.clear();
        int bars = Bars(symbol, timeframe);
        if (bars > max_depth) bars = max_depth;
        int offset = bars <= 1 ? 0 : bars - 1;
        stop_time = iTime(symbol, timeframe, 0);
        start_time = iTime(symbol, timeframe, offset);
        
        for (int b = offset; b > 0; --b) {
            const double h = iHigh(symbol, timeframe, b);
            const double l = iLow(symbol, timeframe, b);
            double vu = 0, vd = 0;
            fractals.update(h, l, vu, vd);
            if (MathIsValidNumber(vu)) on_up(symbol, timeframe, b, true, vu, vd);
            if (MathIsValidNumber(vd)) on_dn(symbol, timeframe, b, true, vu, vd);
            Print("UPDATE ",iTime(symbol, timeframe, b));
        }
        const double h = iHigh(symbol, timeframe, 0);
        const double l = iLow(symbol, timeframe, 0);
        double vu = 0, vd = 0;
        fractals.test(h, l, vu, vd);
        if (MathIsValidNumber(vu)) on_up(symbol, timeframe, 0, false, vu, vd);
        if (MathIsValidNumber(vd)) on_dn(symbol, timeframe, 0, false, vu, vd);
        Print("TEST ",iTime(symbol, timeframe, 0));
    }
    
    void update() {
        // обновляем индикатор, пока есть закрытые бары
        const int offset = iBarShift(symbol, timeframe, stop_time, true);
        for (int b = offset; b > 0; --b) {
            const double h = iHigh(symbol, timeframe, b);
            const double l = iLow(symbol, timeframe, b);
            double vu = 0, vd = 0;
            fractals.update(h, l, vu, vd);
            if (MathIsValidNumber(vu)) on_up(symbol, timeframe, b, true, vu, vd);
            if (MathIsValidNumber(vd)) on_dn(symbol, timeframe, b, true, vu, vd);
            Print("UPDATE ",iTime(symbol, timeframe, b));
        }
        stop_time = iTime(symbol, timeframe, 0);
        const double h = iHigh(symbol, timeframe, 0);
        const double l = iLow(symbol, timeframe, 0);
        double vu = 0, vd = 0;
        fractals.test(h, l, vu, vd);
        if (MathIsValidNumber(vu)) on_up(symbol, timeframe, 0, false, vu, vd);
        if (MathIsValidNumber(vd)) on_dn(symbol, timeframe, 0, false, vu, vd);
        //Print("TEST ",iTime(symbol, timeframe, 0));
    }
};

#endif // XTECHNICAL_FRACTALS_MQH