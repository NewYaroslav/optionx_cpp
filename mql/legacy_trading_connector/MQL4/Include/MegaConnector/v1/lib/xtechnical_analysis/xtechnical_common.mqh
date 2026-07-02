#ifndef XTECHNICAL_COMMON_MQH
#define XTECHNICAL_COMMON_MQH
//+------------------------------------------------------------------+
//|                                   		   xtechnical_common.mqh |
//|										 Copyright 2022, Elektro Yar |
//|			      https://github.com/NewYaroslav/xtechnical_analysis |
//+------------------------------------------------------------------+
#property copyright "Copyright 2022, Elektro Yar"
#property link      "https://github.com/NewYaroslav/xtechnical_analysis"
#property version   "1.00"
#property strict

const double XtechnicalNaN() {
	// 0x7FF0000000000000
	static const double temp = MathSqrt(-1.0);
	return temp;
}

#endif // XTECHNICAL_COMMON_MQH