#property strict
#property indicator_chart_window
#property indicator_plots 0

#include <OptionX/OptionXFileBridge.mqh>

input int    InpBridgeId = 1;
input string InpClientId = "mt5-terminal";
input string InpAccountId = "";
input double InpAmount = 1.0;
input int    InpDurationMs = 60000;
input string InpOrderType = "BUY";
input bool   InpSendTradeOpen = false;

COptionXFileBridge g_optionx;

int OnInit() {
   MathSrand((uint)GetTickCount());

   if (!g_optionx.Configure(InpBridgeId, InpClientId))
      return INIT_FAILED;
   Print("OptionX MT5 file bridge example");
   Print("OptionX client root under Common\\Files: ", g_optionx.ClientRoot());
   Print("OptionX command log: ", g_optionx.CommandsPath());

   bool balance_ok = g_optionx.AccountBalanceGet(InpAccountId);
   bool signal_ok = g_optionx.SignalSubmit(
      Symbol(),
      InpOrderType,
      InpAmount,
      "USD",
      InpDurationMs,
      "OptionX_MQL5_Example",
      InpAccountId);

   bool trade_ok = true;
   if (InpSendTradeOpen) {
      trade_ok = g_optionx.TradeOpen(
         Symbol(),
         InpOrderType,
         InpAmount,
         "USD",
         InpDurationMs,
         "OptionX_MQL5_Example",
         InpAccountId);
   }

   Print("OptionX sent commands: balance=", balance_ok,
         ", signal=", signal_ok,
         ", trade=", trade_ok);
   g_optionx.CleanupCommandsIfCheckpointCaughtUp();
   return INIT_SUCCEEDED;
}

int OnCalculate(
      const int rates_total,
      const int prev_calculated,
      const datetime &time[],
      const double &open[],
      const double &high[],
      const double &low[],
      const double &close[],
      const long &tick_volume[],
      const long &volume[],
      const int &spread[]) {
   return rates_total;
}
