#ifndef XTECHNICAL_FRACTALS_LEVEL_MQH
#define XTECHNICAL_FRACTALS_LEVEL_MQH
//+------------------------------------------------------------------+
//|                                    xtechnical_fractals_level.mqh |
//|										 Copyright 2022, Elektro Yar |
//|			      https://github.com/NewYaroslav/xtechnical_analysis |
//+------------------------------------------------------------------+
#property copyright "Copyright 2022, Elektro Yar"
#property link      "https://github.com/NewYaroslav/xtechnical_analysis"
#property version   "1.00"
#property strict

#include "xtechnical_common.mqh"
#include "xtechnical_fractals.mqh"
#include "xtechnical_circular_buffer.mqh"

/** \brief Уровни по фракталам Билла Вильямса
 */
class XtechnicalFractalsLevel {
private:
	XtechnicalFractals          fractals;
	XtechnicalCircularBuffer    buffer_up;
	XtechnicalCircularBuffer    buffer_dn;

	double output_up;
	double output_dn;
	double save_output_up;
	double save_output_dn;
public:

	XtechnicalFractalsLevel() : buffer_up(3), buffer_dn(3) {
        output_up = XtechnicalNaN();
        output_dn = XtechnicalNaN();
        save_output_up = XtechnicalNaN();
        save_output_dn = XtechnicalNaN();
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
		
		double buffer_value_up, buffer_value_dn;
		fractals.update(
			high, 
			low,
			buffer_value_up,
			buffer_value_dn
		);
		
		if (MathIsValidNumber(buffer_value_up)) buffer_up.update(buffer_value_up);
		if (MathIsValidNumber(buffer_value_dn)) buffer_dn.update(buffer_value_dn);
		
		if(buffer_up.full()) {
			// Fractals up
			double values[]; 
			buffer_up.get_array(values);
			// 0 1 2

			if (values[1] > values[0] &&
				values[1] > values[2]) {
				save_output_up = output_up = values[1];
				value_up = values[1];
			} else {
				output_up = save_output_up;
			}
			ArrayFree(values);
		} else return false;
		if(buffer_dn.full()) {
			// Fractals down
			double values[];
			buffer_dn.get_array(values);
			// 0 1 2

			if (values[1] < values[0] &&
				values[1] < values[2]) {
				save_output_dn = output_dn = values[1];
				value_dn = values[1];
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
		
		double buffer_value_up, buffer_value_dn;
		fractals.test(
			high, 
			low,
			buffer_value_up,
			buffer_value_dn
		);
		
		if (MathIsValidNumber(buffer_value_up)) buffer_up.test(buffer_value_up);
		if (MathIsValidNumber(buffer_value_dn)) buffer_dn.test(buffer_value_dn);
		
		if(buffer_up.full()) {
			// Fractals up
			double values[];
			buffer_up.get_array(values);
			// 0 1 2

			if (values[1] > values[0] &&
				values[1] > values[2]) {
				output_up = values[1];
				value_up = values[1];
			} else {
				output_up = save_output_up;
			}
			ArrayFree(values);
		} else return false;
		if(buffer_dn.full()) {
			// Fractals down
			double values[];
			buffer_dn.get_array(values);
			// 0 1 2

			if (values[1] < values[0] &&
				values[1] < values[2]) {
				output_dn = values[1];
				value_dn = values[1];
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
	void clear() {
		fractals.clear();
		buffer_up.clear();
		buffer_dn.clear();
		output_up = XtechnicalNaN();
		output_dn = XtechnicalNaN();
		save_output_up = XtechnicalNaN();
		save_output_dn = XtechnicalNaN();
	}
};
//+------------------------------------------------------------------+
class XtechnicalFractalsLevelIndicator {
private:
    ulong start_time;
    ulong stop_time;
    int max_depth;
    int timeframe;
    string symbol;
public:
    XtechnicalFractalsLevel fractals_level;
    
    XtechnicalFractalsLevelIndicator() {
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
        const double value_up);
        
    virtual void on_dn(
        const string symbol, 
        const int timeframe, 
        const int index, 
        const bool is_close,
        const double value_dn);
    
    void start(const string us, const int ut, const int umd = 10000) {
        symbol = us;
        timeframe = ut;
        max_depth = umd;
        fractals_level.clear();
        int bars = Bars(symbol, timeframe);
        if (bars > max_depth) bars = max_depth;
        int offset = bars <= 1 ? 0 : bars - 1;
        stop_time = iTime(symbol, timeframe, 0);
        start_time = iTime(symbol, timeframe, offset);
        
        for (int b = offset; b > 0; --b) {
            const double h = iHigh(symbol, timeframe, b);
            const double l = iLow(symbol, timeframe, b);
            double vu = 0, vd = 0;
            fractals_level.update(h, l, vu, vd);
            if (MathIsValidNumber(fractals_level.get_up())) on_up(symbol, timeframe, b, true, fractals_level.get_up());
            if (MathIsValidNumber(fractals_level.get_dn())) on_dn(symbol, timeframe, b, true, fractals_level.get_dn());
            //Print("up ",fractals_level.get_up(), " dn ", fractals_level.get_dn());
            //Print("UPDATE ",iTime(symbol, timeframe, b));
        }
        const double h = iHigh(symbol, timeframe, 0);
        const double l = iLow(symbol, timeframe, 0);
        double vu = 0, vd = 0;
        fractals_level.test(h, l, vu, vd);
        if (MathIsValidNumber(fractals_level.get_up())) on_up(symbol, timeframe, 0, false, fractals_level.get_up());
        if (MathIsValidNumber(fractals_level.get_dn())) on_dn(symbol, timeframe, 0, false, fractals_level.get_dn());
        //Print("TEST ",iTime(symbol, timeframe, 0));
    }
    
    void update() {
        // обновляем индикатор, пока есть закрытые бары
        const int offset = iBarShift(symbol, timeframe, stop_time, true);
        for (int b = offset; b > 0; --b) {
            const double h = iHigh(symbol, timeframe, b);
            const double l = iLow(symbol, timeframe, b);
            double vu = 0, vd = 0;
            fractals_level.update(h, l, vu, vd);
            if (MathIsValidNumber(fractals_level.get_up())) on_up(symbol, timeframe, b, true, fractals_level.get_up());
            if (MathIsValidNumber(fractals_level.get_dn())) on_dn(symbol, timeframe, b, true, fractals_level.get_dn());
            //Print("UPDATE ",iTime(symbol, timeframe, b));
        }
        stop_time = iTime(symbol, timeframe, 0);
        const double h = iHigh(symbol, timeframe, 0);
        const double l = iLow(symbol, timeframe, 0);
        double vu = 0, vd = 0;
        fractals_level.test(h, l, vu, vd);
        if (MathIsValidNumber(fractals_level.get_up())) on_up(symbol, timeframe, 0, false, fractals_level.get_up());
        if (MathIsValidNumber(fractals_level.get_dn())) on_dn(symbol, timeframe, 0, false, fractals_level.get_dn());
    }
};
//+------------------------------------------------------------------+
class XtechnicalFractalsLevelIndicatorV2 {
private:
    XtechnicalFractalsLevel fractals_level;
    ulong start_time;
    ulong stop_time;
    int max_depth;
    int timeframe;
    string symbol;
public:
    double up;
    double dn;
    
    XtechnicalFractalsLevelIndicatorV2() {
        start_time = 0;
        stop_time = 0;
        timeframe = 0;
        max_depth = 0;
        up = 0;
        dn = 0;
    }
    
    string get_symbol() {
        return symbol;
    }
    
    int get_timeframe() {
        return timeframe;
    }
    
    void start(const string us, const int ut, const int umd = 10000) {
        symbol = us;
        timeframe = ut;
        max_depth = umd;
        fractals_level.clear();
        int bars = Bars(symbol, timeframe);
        if (bars > max_depth) bars = max_depth;
        int offset = bars <= 1 ? 0 : bars - 1;
        stop_time = iTime(symbol, timeframe, 0);
        start_time = iTime(symbol, timeframe, offset);
        
        for (int b = offset; b > 0; --b) {
            const double h = iHigh(symbol, timeframe, b);
            const double l = iLow(symbol, timeframe, b);
            double vu = 0, vd = 0;
            fractals_level.update(h, l, vu, vd);
            if (MathIsValidNumber(fractals_level.get_up())) up = fractals_level.get_up();
            if (MathIsValidNumber(fractals_level.get_dn())) dn = fractals_level.get_dn();
            //Print("up ",fractals_level.get_up(), " dn ", fractals_level.get_dn());
            //Print("UPDATE ",iTime(symbol, timeframe, b));
        }
        const double h = iHigh(symbol, timeframe, 0);
        const double l = iLow(symbol, timeframe, 0);
        double vu = 0, vd = 0;
        fractals_level.test(h, l, vu, vd);
        if (MathIsValidNumber(fractals_level.get_up())) up = fractals_level.get_up();
        if (MathIsValidNumber(fractals_level.get_dn())) dn = fractals_level.get_dn();
        //Print("TEST ",iTime(symbol, timeframe, 0));
    }
    
    void update() {
        // обновляем индикатор, пока есть закрытые бары
        const int offset = iBarShift(symbol, timeframe, stop_time, true);
        for (int b = offset; b > 0; --b) {
            const double h = iHigh(symbol, timeframe, b);
            const double l = iLow(symbol, timeframe, b);
            double vu = 0, vd = 0;
            fractals_level.update(h, l, vu, vd);
            if (MathIsValidNumber(fractals_level.get_up())) up = fractals_level.get_up();
            if (MathIsValidNumber(fractals_level.get_dn())) dn = fractals_level.get_dn();
            //Print("UPDATE ",iTime(symbol, timeframe, b));
        }
        stop_time = iTime(symbol, timeframe, 0);
        const double h = iHigh(symbol, timeframe, 0);
        const double l = iLow(symbol, timeframe, 0);
        double vu = 0, vd = 0;
        fractals_level.test(h, l, vu, vd);
        if (MathIsValidNumber(fractals_level.get_up())) up = fractals_level.get_up();
        if (MathIsValidNumber(fractals_level.get_dn())) dn = fractals_level.get_dn();
    }
};
#endif // XTECHNICAL_FRACTALS_LEVEL_MQH