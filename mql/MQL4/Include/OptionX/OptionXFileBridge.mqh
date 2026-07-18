#ifndef OPTIONX_FILE_BRIDGE_MQH
#define OPTIONX_FILE_BRIDGE_MQH

// Minimal OptionX MetaTrader Common\Files client.
// Copy this file to MQL4\Include or MQL5\Include, or place it next to an EA,
// script, or indicator that includes it.

class COptionXFileBridge {
private:
   int    m_bridge_id;
   string m_client_id;
   string m_namespace_subdir;
   int    m_default_valid_for_ms;
   long   m_next_file_seq;
   long   m_operation_counter;

   string NormalizePath(string value) const {
      StringReplace(value, "/", "\\");
      while (StringFind(value, "\\\\") >= 0)
         StringReplace(value, "\\\\", "\\");
      while (StringLen(value) > 0 && StringSubstr(value, 0, 1) == "\\")
         value = StringSubstr(value, 1);
      while (StringLen(value) > 0 &&
             StringSubstr(value, StringLen(value) - 1, 1) == "\\")
         value = StringSubstr(value, 0, StringLen(value) - 1);
      return value;
   }

   string JoinPath(const string left, const string right) const {
      if (left == "") return NormalizePath(right);
      if (right == "") return NormalizePath(left);
      return NormalizePath(left + "\\" + right);
   }

   string JsonEscape(const string value) const {
      string out = "";
      for (int i = 0; i < StringLen(value); ++i) {
         ushort ch = StringGetCharacter(value, i);
         if (ch == 34) out += "\\\"";
         else if (ch == 92) out += "\\\\";
         else if (ch == 8) out += "\\b";
         else if (ch == 9) out += "\\t";
         else if (ch == 10) out += "\\n";
         else if (ch == 12) out += "\\f";
         else if (ch == 13) out += "\\r";
         else out += StringSubstr(value, i, 1);
      }
      return out;
   }

   string JsonString(const string value) const {
      return "\"" + JsonEscape(value) + "\"";
   }

   string Base36Encode(long value) const {
      string digits = "0123456789abcdefghijklmnopqrstuvwxyz";
      if (value <= 0) return "0";

      string out = "";
      while (value > 0) {
         int index = (int)(value % 36);
         out = StringSubstr(digits, index, 1) + out;
         value /= 36;
      }
      return out;
   }

   string RandomBase36(const int length) const {
      string digits = "0123456789abcdefghijklmnopqrstuvwxyz";
      string out = "";
      for (int i = 0; i < length; ++i) {
         out += StringSubstr(digits, MathRand() % 36, 1);
      }
      return out;
   }

   long UnixTimeMs() const {
      return (long)TimeGMT() * 1000L + (long)(GetTickCount() % 1000);
   }

   bool EnsureDirectory(const string path) const {
      if (path == "") return true;
      if (FolderIsExist(path, FILE_COMMON)) return true;
      return FolderCreate(path, FILE_COMMON);
   }

   bool EnsureClientRoot() const {
      string current = "";
      string root = ClientRoot();
      int start = 0;
      while (start < StringLen(root)) {
         int pos = StringFind(root, "\\", start);
         string part = (pos < 0)
            ? StringSubstr(root, start)
            : StringSubstr(root, start, pos - start);
         current = JoinPath(current, part);
         if (!EnsureDirectory(current)) {
            Print("OptionX: could not create Common\\Files directory: ", current);
            return false;
         }
         if (pos < 0) break;
         start = pos + 1;
      }
      return true;
   }

   bool AppendLine(const string relative_path, const string line) const {
      if (!EnsureClientRoot()) return false;

      int handle = FileOpen(
         relative_path,
         FILE_READ | FILE_WRITE | FILE_TXT | FILE_ANSI | FILE_COMMON);
      if (handle == INVALID_HANDLE) {
         handle = FileOpen(
            relative_path,
            FILE_WRITE | FILE_TXT | FILE_ANSI | FILE_COMMON);
      }
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not open command log: ", relative_path,
               ", error=", GetLastError());
         return false;
      }

      FileSeek(handle, 0, SEEK_END);
      FileWriteString(handle, line + "\n");
      FileFlush(handle);
      FileClose(handle);
      return true;
   }

   long ParseLongField(const string text, const string name) const {
      int key = StringFind(text, "\"" + name + "\"");
      if (key < 0) return 0;
      int colon = StringFind(text, ":", key);
      if (colon < 0) return 0;

      int start = colon + 1;
      while (start < StringLen(text)) {
         ushort ch = StringGetCharacter(text, start);
         if (ch != 32 && ch != 9 && ch != 13 && ch != 10) break;
         ++start;
      }

      int end = start;
      while (end < StringLen(text)) {
         ushort ch = StringGetCharacter(text, end);
         if (ch < 48 || ch > 57) break;
         ++end;
      }
      if (end <= start) return 0;
      return (long)StringToInteger(StringSubstr(text, start, end - start));
   }

   long MaxFileSeqInCommands() const {
      int handle = FileOpen(CommandsPath(), FILE_READ | FILE_TXT | FILE_ANSI | FILE_COMMON);
      if (handle == INVALID_HANDLE) return 0;

      long max_seq = 0;
      while (!FileIsEnding(handle)) {
         string line = FileReadString(handle);
         long seq = ParseLongField(line, "file_seq");
         if (seq > max_seq) max_seq = seq;
      }
      FileClose(handle);
      return max_seq;
   }

   long LastCheckpointSeq() const {
      int handle = FileOpen(
         CommandsCheckpointPath(),
         FILE_READ | FILE_TXT | FILE_ANSI | FILE_COMMON);
      if (handle == INVALID_HANDLE) return 0;

      string text = "";
      while (!FileIsEnding(handle))
         text += FileReadString(handle);
      FileClose(handle);
      return ParseLongField(text, "last_file_seq");
   }

   string RequestEnvelope(
         const long file_seq,
         const string id,
         const string method,
         const string params) const {
      return "{"
         "\"file_seq\":" + IntegerToString(file_seq) + ","
         "\"jsonrpc\":\"2.0\","
         "\"id\":" + JsonString(id) + ","
         "\"method\":" + JsonString(method) + ","
         "\"params\":" + params +
      "}";
   }

   string TradeParams(
         const string section_name,
         const string symbol,
         const string order_type,
         const double amount,
         const string currency,
         const int duration_ms,
         const string signal_name,
         const string account_id,
         const string operation_key,
         const long valid_until_ms) const {
      string identity = "\"unique_hash\":" + JsonString(operation_key);
      if (signal_name != "")
         identity += ",\"signal_name\":" + JsonString(signal_name);

      string params = "{"
         "\"context\":{"
            "\"idempotency_key\":" + JsonString(operation_key) + ","
            "\"valid_until_ms\":" + IntegerToString(valid_until_ms) +
         "},"
         "\"identity\":{" + identity + "},";

      if (account_id != "") {
         params += "\"routing\":{\"selector\":{"
            "\"kind\":\"account\","
            "\"account_id\":" + JsonString(account_id) +
         "}},";
      }

      params +=
         "\"" + section_name + "\":{"
            "\"symbol\":" + JsonString(symbol) + ","
            "\"order_type\":" + JsonString(order_type) + ","
            "\"option_type\":\"SPRINT\","
            "\"amount\":{"
               "\"value\":" + JsonString(DoubleToString(amount, 2)) + ","
               "\"currency\":" + JsonString(currency) +
            "},"
            "\"expiry\":{"
               "\"kind\":\"duration\","
               "\"duration_ms\":" + IntegerToString(duration_ms) +
            "}"
         "}"
      "}";
      return params;
   }

   bool AppendRequest(const string id, const string method, const string params) {
      if (m_next_file_seq <= 0) {
         long visible_max = MaxFileSeqInCommands();
         long checkpoint = LastCheckpointSeq();
         m_next_file_seq = MathMax(visible_max, checkpoint) + 1;
      }

      string line = RequestEnvelope(m_next_file_seq, id, method, params);
      if (!AppendLine(CommandsPath(), line))
         return false;

      Print("OptionX: wrote ", method, " file_seq=", m_next_file_seq,
            " id=", id);
      ++m_next_file_seq;
      return true;
   }

public:
   COptionXFileBridge() {
      m_bridge_id = 1;
      m_client_id = "default";
      m_namespace_subdir = "OptionX\\Bridge\\v1";
      m_default_valid_for_ms = 60000;
      m_next_file_seq = 0;
      m_operation_counter = 0;
   }

   void Configure(
         const int bridge_id,
         const string client_id,
         const string namespace_subdir = "OptionX\\Bridge\\v1",
         const int default_valid_for_ms = 60000) {
      m_bridge_id = bridge_id;
      m_client_id = client_id;
      m_namespace_subdir = NormalizePath(namespace_subdir);
      m_default_valid_for_ms = default_valid_for_ms;
      m_next_file_seq = 0;
   }

   string ClientRoot() const {
      return JoinPath(
         JoinPath(m_namespace_subdir, IntegerToString(m_bridge_id)),
         m_client_id);
   }

   string CommandsPath() const {
      return JoinPath(ClientRoot(), "commands.ndjson");
   }

   string CommandsCheckpointPath() const {
      return JoinPath(ClientRoot(), "commands.checkpoint.json");
   }

   string MakeOperationKey(const string prefix = "mql") {
      string key = prefix + "_" +
         Base36Encode(UnixTimeMs()) + "_" +
         RandomBase36(16) + "_" +
         Base36Encode(m_operation_counter);
      ++m_operation_counter;
      return key;
   }

   bool AccountBalanceGet(const string account_id = "", string operation_key = "") {
      if (operation_key == "")
         operation_key = MakeOperationKey();

      string params = "{}";
      if (account_id != "")
         params = "{\"account_id\":" + JsonString(account_id) + "}";
      return AppendRequest(operation_key, "account.balance.get", params);
   }

   bool SignalSubmit(
         const string symbol,
         const string order_type,
         const double amount,
         const string currency,
         const int duration_ms,
         const string signal_name,
         const string account_id = "",
         string operation_key = "",
         const int valid_for_ms = 0) {
      if (operation_key == "")
         operation_key = MakeOperationKey();
      int lifetime = valid_for_ms > 0 ? valid_for_ms : m_default_valid_for_ms;
      long valid_until_ms = UnixTimeMs() + lifetime;
      string params = TradeParams(
         "signal",
         symbol,
         order_type,
         amount,
         currency,
         duration_ms,
         signal_name,
         account_id,
         operation_key,
         valid_until_ms);
      return AppendRequest(operation_key, "signal.submit", params);
   }

   bool TradeOpen(
         const string symbol,
         const string order_type,
         const double amount,
         const string currency,
         const int duration_ms,
         const string signal_name,
         const string account_id = "",
         string operation_key = "",
         const int valid_for_ms = 0) {
      if (operation_key == "")
         operation_key = MakeOperationKey();
      int lifetime = valid_for_ms > 0 ? valid_for_ms : m_default_valid_for_ms;
      long valid_until_ms = UnixTimeMs() + lifetime;
      string params = TradeParams(
         "trade",
         symbol,
         order_type,
         amount,
         currency,
         duration_ms,
         signal_name,
         account_id,
         operation_key,
         valid_until_ms);
      return AppendRequest(operation_key, "trade.open", params);
   }

   bool CleanupCommandsIfCheckpointCaughtUp() const {
      long max_seq = MaxFileSeqInCommands();
      if (max_seq <= 0)
         return false;

      long checkpoint = LastCheckpointSeq();
      if (checkpoint < max_seq) {
         Print("OptionX: command cleanup skipped, checkpoint=", checkpoint,
               ", visible_max=", max_seq);
         return false;
      }

      int handle = FileOpen(
         CommandsPath(),
         FILE_WRITE | FILE_TXT | FILE_ANSI | FILE_COMMON);
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not clear command log: ", CommandsPath(),
               ", error=", GetLastError());
         return false;
      }
      FileClose(handle);
      Print("OptionX: cleared commands.ndjson after checkpoint ", checkpoint);
      return true;
   }
};

#endif // OPTIONX_FILE_BRIDGE_MQH
