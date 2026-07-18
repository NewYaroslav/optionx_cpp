#ifndef OPTIONX_FILE_BRIDGE_MQH
#define OPTIONX_FILE_BRIDGE_MQH

// Minimal OptionX MetaTrader Common\Files client.
// Place this file under MQL4\Include\OptionX or MQL5\Include\OptionX.

class COptionXFileBridge {
private:
   int    m_bridge_id;
   string m_client_id;
   string m_namespace_subdir;
   int    m_default_valid_for_ms;
   int    m_max_line_bytes;
   int    m_max_command_log_bytes;
   long   m_next_file_seq;
   bool   m_configured;

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

   bool IsSafeFileId(const string value) const {
      if (value == "" || value == "." || value == "..")
         return false;
      if (StringSubstr(value, StringLen(value) - 1, 1) == ".")
         return false;

      for (int i = 0; i < StringLen(value); ++i) {
         ushort ch = StringGetCharacter(value, i);
         bool ok =
            (ch >= 48 && ch <= 57) ||
            (ch >= 65 && ch <= 90) ||
            (ch >= 97 && ch <= 122) ||
            ch == 45 ||
            ch == 46;
         if (!ok) return false;
      }
      return true;
   }

   bool IsSafeNamespaceSubdir(const string value) const {
      string normalized = NormalizePath(value);
      if (normalized == "") return false;
      int start = 0;
      while (start < StringLen(normalized)) {
         int pos = StringFind(normalized, "\\", start);
         string part = (pos < 0)
            ? StringSubstr(normalized, start)
            : StringSubstr(normalized, start, pos - start);
         if (!IsSafeFileId(part))
            return false;
         if (pos < 0) break;
         start = pos + 1;
      }
      return true;
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

   long UnixTimeMs() const {
      return (long)TimeGMT() * 1000L + (long)(GetTickCount() % 1000);
   }

   bool EnsureDirectory(const string path) const {
      if (path == "") return true;
      ResetLastError();
      if (FolderCreate(path, FILE_COMMON))
         return true;
      int error = GetLastError();
      return error == 5018;
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

   int TextReadFlags() const {
      return FILE_READ | FILE_TXT | FILE_ANSI | FILE_COMMON |
         FILE_SHARE_READ | FILE_SHARE_WRITE;
   }

   int BinaryReadFlags() const {
      return FILE_READ | FILE_BIN | FILE_COMMON |
         FILE_SHARE_READ | FILE_SHARE_WRITE;
   }

   int BinaryWriteFlags() const {
      return FILE_WRITE | FILE_BIN | FILE_COMMON |
         FILE_SHARE_READ | FILE_SHARE_WRITE;
   }

   int ExclusiveBinaryLockFlags() const {
      return FILE_READ | FILE_WRITE | FILE_BIN | FILE_COMMON;
   }

   string CommandsLockPath() const {
      return JoinPath(ClientRoot(), "commands.ndjson.lock");
   }

   int AcquireCommandLock() const {
      if (!EnsureClientRoot()) return INVALID_HANDLE;

      for (int attempt = 0; attempt < 256; ++attempt) {
         ResetLastError();
         int handle = FileOpen(CommandsLockPath(), ExclusiveBinaryLockFlags());
         if (handle != INVALID_HANDLE)
            return handle;
      }

      Print("OptionX: could not acquire command log lock: ", CommandsLockPath(),
            ", error=", GetLastError());
      return INVALID_HANDLE;
   }

   void ReleaseCommandLock(const int handle) const {
      if (handle != INVALID_HANDLE)
         FileClose(handle);
   }

   bool EncodeUtf8Bytes(const string text, uchar &bytes[], int &byte_count) const {
      ArrayResize(bytes, 0);
      byte_count = StringToCharArray(text, bytes, 0, StringLen(text), CP_UTF8);
      if (byte_count < 0) {
         Print("OptionX: could not encode command line as UTF-8.");
         return false;
      }
      ArrayResize(bytes, byte_count);
      return true;
   }

   bool CurrentCommandLogSize(long &size) const {
      size = 0;
      if (!FileIsExist(CommandsPath(), FILE_COMMON))
         return true;

      ResetLastError();
      int handle = FileOpen(CommandsPath(), BinaryReadFlags());
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not inspect command log size: ", CommandsPath(),
               ", error=", GetLastError());
         return false;
      }
      size = (long)FileSize(handle);
      FileClose(handle);
      return true;
   }

   bool ClearCommandsUnlocked() const {
      ResetLastError();
      int handle = FileOpen(CommandsPath(), BinaryWriteFlags());
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not clear command log: ", CommandsPath(),
               ", error=", GetLastError());
         return false;
      }
      FileFlush(handle);
      FileClose(handle);
      return true;
   }

   bool RepairIncompleteTail(const string relative_path) const {
      if (!FileIsExist(relative_path, FILE_COMMON))
         return true;

      ResetLastError();
      int handle = FileOpen(relative_path, BinaryReadFlags());
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not inspect command log tail: ", relative_path,
               ", error=", GetLastError());
         return false;
      }

      long size = (long)FileSize(handle);
      if (size <= 0) {
         FileClose(handle);
         return true;
      }
      if (size > m_max_command_log_bytes) {
         FileClose(handle);
         Print("OptionX: command log is larger than configured limit: ", size);
         return false;
      }

      uchar tail[];
      ArrayResize(tail, 1);
      FileSeek(handle, size - 1, SEEK_SET);
      uint tail_read = FileReadArray(handle, tail, 0, 1);
      if (tail_read != 1) {
         FileClose(handle);
         Print("OptionX: could not read command log tail byte: ", relative_path,
               ", read=", tail_read);
         return false;
      }
      if (tail[0] == 10) {
         FileClose(handle);
         return true;
      }

      uchar bytes[];
      ArrayResize(bytes, (int)size);
      FileSeek(handle, 0, SEEK_SET);
      uint read = FileReadArray(handle, bytes, 0, (int)size);
      FileClose(handle);
      if (read != (uint)size) {
         Print("OptionX: could not read command log for tail repair: ", relative_path,
               ", read=", read, ", expected=", size);
         return false;
      }

      if (bytes[(int)size - 1] == 10)
         return true;

      int keep = 0;
      for (int i = (int)size - 1; i >= 0; --i) {
         if (bytes[i] == 10) {
            keep = i + 1;
            break;
         }
      }

      ResetLastError();
      handle = FileOpen(relative_path, BinaryWriteFlags());
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not truncate incomplete command log tail: ",
               relative_path, ", error=", GetLastError());
         return false;
      }
      if (keep > 0) {
         uint written = FileWriteArray(handle, bytes, 0, keep);
         if (written != (uint)keep) {
            FileClose(handle);
            Print("OptionX: could not rewrite command log after tail repair: ",
                  relative_path, ", written=", written, ", expected=", keep);
            return false;
         }
      }
      FileFlush(handle);
      FileClose(handle);
      Print("OptionX: repaired incomplete command log tail, kept bytes=", keep);
      return true;
   }

   bool EnsureCommandLogCapacityUnlocked(
         const int append_bytes,
         const long visible_max,
         const long checkpoint) {
      if (append_bytes > m_max_command_log_bytes) {
         Print("OptionX: command line would exceed configured command log byte limit: ",
               append_bytes);
         return false;
      }

      long current_size = 0;
      if (!CurrentCommandLogSize(current_size))
         return false;
      if (current_size > m_max_command_log_bytes) {
         Print("OptionX: command log is larger than configured limit: ", current_size);
         return false;
      }
      if (current_size + append_bytes <= m_max_command_log_bytes)
         return true;

      if (visible_max > 0 && checkpoint >= visible_max) {
         if (!ClearCommandsUnlocked())
            return false;
         current_size = 0;
         if (current_size + append_bytes <= m_max_command_log_bytes)
            return true;
      }

      Print("OptionX: command log would exceed configured byte limit; ",
            "wait for bridge checkpoint or clean up commands.ndjson.");
      return false;
   }

   bool AppendLineBytesUnlocked(
         const string relative_path,
         uchar &payload[],
         const int payload_size) const {
      if (!EnsureClientRoot()) return false;

      ResetLastError();
      int handle = FileOpen(
         relative_path,
         FILE_READ | FILE_WRITE | FILE_BIN | FILE_COMMON |
            FILE_SHARE_READ | FILE_SHARE_WRITE);
      if (handle == INVALID_HANDLE) {
         ResetLastError();
         handle = FileOpen(
            relative_path,
            BinaryWriteFlags());
      }
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not open command log: ", relative_path,
               ", error=", GetLastError());
         return false;
      }

      FileSeek(handle, 0, SEEK_END);
      uint written = FileWriteArray(handle, payload, 0, payload_size);
      if (written != (uint)payload_size) {
         FileClose(handle);
         Print("OptionX: incomplete command log write: written=", written,
               ", expected=", payload_size);
         return false;
      }
      FileFlush(handle);
      FileClose(handle);
      return true;
   }

   bool TryParseLongField(const string text, const string name, long &value) const {
      value = 0;
      int key = StringFind(text, "\"" + name + "\"");
      if (key < 0) return false;
      int colon = StringFind(text, ":", key);
      if (colon < 0) return false;

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
      if (end <= start) return false;
      value = (long)StringToInteger(StringSubstr(text, start, end - start));
      return true;
   }

   bool TryMaxFileSeqInCommands(long &max_seq) const {
      max_seq = 0;
      if (!FileIsExist(CommandsPath(), FILE_COMMON))
         return true;

      ResetLastError();
      int handle = FileOpen(CommandsPath(), TextReadFlags(), 0, CP_UTF8);
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not read command log: ", CommandsPath(),
               ", error=", GetLastError());
         return false;
      }

      while (!FileIsEnding(handle)) {
         string line = FileReadString(handle);
         if (line == "") continue;
         long seq = 0;
         if (!TryParseLongField(line, "file_seq", seq) || seq <= 0) {
            FileClose(handle);
            Print("OptionX: command log contains a malformed line without positive file_seq.");
            return false;
         }
         if (seq > max_seq) max_seq = seq;
      }
      FileClose(handle);
      return true;
   }

   bool TryLastCheckpointSeq(long &checkpoint) const {
      checkpoint = 0;
      if (!FileIsExist(CommandsCheckpointPath(), FILE_COMMON))
         return true;

      ResetLastError();
      int handle = FileOpen(
         CommandsCheckpointPath(),
         TextReadFlags(),
         0,
         CP_UTF8);
      if (handle == INVALID_HANDLE) {
         Print("OptionX: could not read command checkpoint: ",
               CommandsCheckpointPath(), ", error=", GetLastError());
         return false;
      }

      string text = "";
      while (!FileIsEnding(handle))
         text += FileReadString(handle);
      FileClose(handle);

      if (!TryParseLongField(text, "last_file_seq", checkpoint) || checkpoint < 0) {
         Print("OptionX: command checkpoint is malformed or missing last_file_seq.");
         return false;
      }
      return true;
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

   string MakeOperationKey(const long file_seq, const string prefix = "mql") const {
      return prefix + ":" +
         IntegerToString(m_bridge_id) + ":" +
         m_client_id + ":" +
         Base36Encode(file_seq);
   }

   bool RecoverNextFileSeqUnlocked() {
      if (!RepairIncompleteTail(CommandsPath()))
         return false;

      long visible_max = 0;
      long checkpoint = 0;
      if (!TryMaxFileSeqInCommands(visible_max) ||
          !TryLastCheckpointSeq(checkpoint)) {
         Print("OptionX: command sequence recovery failed; command was not written.");
         return false;
      }

      long recovered_next = MathMax(visible_max, checkpoint) + 1;
      if (m_next_file_seq <= 0 || recovered_next > m_next_file_seq)
         m_next_file_seq = recovered_next;
      return true;
   }

   bool AppendPreparedRequestUnlocked(
         const string id,
         const string method,
         const string params,
         const long visible_max,
         const long checkpoint) {
      string line = RequestEnvelope(m_next_file_seq, id, method, params);
      uchar line_bytes[];
      int line_byte_count = 0;
      if (!EncodeUtf8Bytes(line, line_bytes, line_byte_count))
         return false;
      if (line_byte_count > m_max_line_bytes) {
         Print("OptionX: command line exceeds configured byte limit: ",
               line_byte_count);
         return false;
      }

      uchar payload[];
      int payload_size = 0;
      if (!EncodeUtf8Bytes(line + "\n", payload, payload_size))
         return false;
      if (!EnsureCommandLogCapacityUnlocked(payload_size, visible_max, checkpoint))
         return false;
      if (!AppendLineBytesUnlocked(CommandsPath(), payload, payload_size))
         return false;

      Print("OptionX: wrote ", method, " file_seq=", m_next_file_seq,
            " id=", id);
      ++m_next_file_seq;
      return true;
   }

   bool AppendAccountBalanceGetRequest(
         const string account_id,
         string operation_key) {
      if (!m_configured) {
         Print("OptionX: file bridge is not configured.");
         return false;
      }

      int lock_handle = AcquireCommandLock();
      if (lock_handle == INVALID_HANDLE)
         return false;

      bool ok = false;
      do {
         if (!RecoverNextFileSeqUnlocked())
            break;
         if (operation_key == "")
            operation_key = MakeOperationKey(m_next_file_seq);

         long visible_max = 0;
         long checkpoint = 0;
         if (!TryMaxFileSeqInCommands(visible_max) ||
             !TryLastCheckpointSeq(checkpoint))
            break;

         string params = "{}";
         if (account_id != "")
            params = "{\"account_id\":" + JsonString(account_id) + "}";
         ok = AppendPreparedRequestUnlocked(
            operation_key,
            "account.balance.get",
            params,
            visible_max,
            checkpoint);
      } while (false);

      ReleaseCommandLock(lock_handle);
      return ok;
   }

public:
   COptionXFileBridge() {
      m_bridge_id = 1;
      m_client_id = "default";
      m_namespace_subdir = "OptionX\\Bridge\\v1";
      m_default_valid_for_ms = 60000;
      m_max_line_bytes = 65536;
      m_max_command_log_bytes = 8 * 1024 * 1024;
      m_next_file_seq = 0;
      m_configured = true;
   }

   bool Configure(
         const int bridge_id,
         const string client_id,
         const string namespace_subdir = "OptionX\\Bridge\\v1",
         const int default_valid_for_ms = 60000,
         const int max_line_bytes = 65536,
         const int max_command_log_bytes = 8388608) {
      if (bridge_id <= 0) {
         Print("OptionX: bridge_id must be positive.");
         m_configured = false;
         return false;
      }
      if (!IsSafeFileId(client_id)) {
         Print("OptionX: client_id must be a safe [A-Za-z0-9.-]+ identifier.");
         m_configured = false;
         return false;
      }
      if (!IsSafeNamespaceSubdir(namespace_subdir)) {
         Print("OptionX: namespace_subdir must be a safe relative path.");
         m_configured = false;
         return false;
      }
      if (default_valid_for_ms <= 0) {
         Print("OptionX: default_valid_for_ms must be positive.");
         m_configured = false;
         return false;
      }
      if (max_line_bytes <= 0) {
         Print("OptionX: max_line_bytes must be positive.");
         m_configured = false;
         return false;
      }
      if (max_command_log_bytes < max_line_bytes) {
         Print("OptionX: max_command_log_bytes must be at least max_line_bytes.");
         m_configured = false;
         return false;
      }
      m_bridge_id = bridge_id;
      m_client_id = client_id;
      m_namespace_subdir = NormalizePath(namespace_subdir);
      m_default_valid_for_ms = default_valid_for_ms;
      m_max_line_bytes = max_line_bytes;
      m_max_command_log_bytes = max_command_log_bytes;
      m_next_file_seq = 0;
      m_configured = true;
      return true;
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

private:
   bool AppendTradeRequest(
         const string method,
         const string section_name,
         const string symbol,
         const string order_type,
         const double amount,
         const string currency,
         const int duration_ms,
         const string signal_name,
         const string account_id,
         string operation_key,
         const int valid_for_ms) {
      if (!m_configured) {
         Print("OptionX: file bridge is not configured.");
         return false;
      }
      if (operation_key == "") {
         Print("OptionX: trade-affecting commands require an explicit operation_key ",
               "that the caller can reuse on retry.");
         return false;
      }

      int lock_handle = AcquireCommandLock();
      if (lock_handle == INVALID_HANDLE)
         return false;

      bool ok = false;
      do {
         if (!RecoverNextFileSeqUnlocked())
            break;

         long visible_max = 0;
         long checkpoint = 0;
         if (!TryMaxFileSeqInCommands(visible_max) ||
             !TryLastCheckpointSeq(checkpoint))
            break;

         int lifetime = valid_for_ms > 0 ? valid_for_ms : m_default_valid_for_ms;
         long valid_until_ms = UnixTimeMs() + lifetime;
         string params = TradeParams(
            section_name,
            symbol,
            order_type,
            amount,
            currency,
            duration_ms,
            signal_name,
            account_id,
            operation_key,
            valid_until_ms);
         ok = AppendPreparedRequestUnlocked(
            operation_key,
            method,
            params,
            visible_max,
            checkpoint);
      } while (false);

      ReleaseCommandLock(lock_handle);
      return ok;
   }

public:
   bool AccountBalanceGet(const string account_id = "", string operation_key = "") {
      return AppendAccountBalanceGetRequest(account_id, operation_key);
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
      return AppendTradeRequest(
         "signal.submit",
         "signal",
         symbol,
         order_type,
         amount,
         currency,
         duration_ms,
         signal_name,
         account_id,
         operation_key,
         valid_for_ms);
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
      return AppendTradeRequest(
         "trade.open",
         "trade",
         symbol,
         order_type,
         amount,
         currency,
         duration_ms,
         signal_name,
         account_id,
         operation_key,
         valid_for_ms);
   }

   bool CleanupCommandsIfCheckpointCaughtUp() {
      int lock_handle = AcquireCommandLock();
      if (lock_handle == INVALID_HANDLE)
         return false;

      bool ok = false;
      do {
         if (!RepairIncompleteTail(CommandsPath()))
            break;
         long max_seq = 0;
         if (!TryMaxFileSeqInCommands(max_seq))
            break;
         if (max_seq <= 0)
            break;

         long checkpoint = 0;
         if (!TryLastCheckpointSeq(checkpoint))
            break;
         if (checkpoint < max_seq) {
            Print("OptionX: command cleanup skipped, checkpoint=", checkpoint,
                  ", visible_max=", max_seq);
            break;
         }

         if (!ClearCommandsUnlocked())
            break;
         m_next_file_seq = checkpoint + 1;
         Print("OptionX: cleared commands.ndjson after checkpoint ", checkpoint);
         ok = true;
      } while (false);

      ReleaseCommandLock(lock_handle);
      return ok;
   }
};

#endif // OPTIONX_FILE_BRIDGE_MQH
