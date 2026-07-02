//+------------------------------------------------------------------+
//|                                           legacy-trading-api.mqh |
//|                     Copyright 2022-2024, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+

//+------------------------------------------------------------------+
#ifndef OPTIONX_LEGACY_TRADING_API_MQH
#define OPTIONX_LEGACY_TRADING_API_MQH

#property copyright "Copyright 2022-2024, MegaConnector Software."
#property link      "https://mega-connector.com"
#property strict

#include "..\part\common.mqh"
#include "..\lib\simple-named-pipe-server/named_pipe_client.mqh"
#include "..\lib\json.mqh"
#include "..\lib\legacy-time-utils.mqh"
#include "..\utils\unique_id.mqh"
#include <Tools\DateTime.mqh>

#define McBridgeApiV1 MegaConnectorBridgeApiV1

/** \brief BAPI class for connecting to the MegaConnector program
 * BAPI (Bridge Api) is intended for interaction with brokers:
 * sending signals, receiving payout percentages, account balances, etc.
 */
class MegaConnectorBridgeApiV1 {
private:
    class Config {
    public:
        string pipe_name;
        string app_id;
    };

    Config config;

    NamedPipeClient             pipe;
    MegaConnectorAccountInfo    account_info;
    LegacyTradingTimer               timer;

    bool is_connected;
    bool is_rub_currency;
    bool is_get_account_info;

    string body;         // incoming message body

    void processing_incoming_messages() {
        if(!is_connected) return;
        const ulong MAX_PING_TICKS = 30000;
        if(timer.get_elapsed_ms() > MAX_PING_TICKS) {
            timer.reset();
            string json_body = "{\"ping\":1}";
            if(!pipe.write(json_body)) {
                close();
            }
        }
        while(pipe.get_bytes_read() > 0) {
            body += pipe.read();

            if(StringGetCharacter(body, StringLen(body) - 1) != '}') {
                continue;
            }

            // парсим json сообщение
            JSONParser *parser = new JSONParser();
            JSONValue *jv = parser.parse(body);
            body = "";
            if(jv == NULL) {
                string text = "Error: " +(string)parser.getErrorCode() + (string)parser.getErrorMessage();
                on_error(text);
            } else {
                if(jv.isObject()) {
                    JSONObject *jo = jv;

                    // get the values of the deal status
                    JSONObject *updated_bo = jo.getObject("update_bet");
                    while (updated_bo != NULL) {
                        MegaConnectorBoResult bo;

                        const string str_user_data = updated_bo.getString("note");
                        if (!check_app_id_v3(str_user_data, config.app_id, bo.signal_id, bo.signal_name, bo.user_data)) break;

                        bo.api_id = updated_bo.getInt("aid");
                        bo.broker_id = updated_bo.getInt("id");
                        bo.symbol = updated_bo.getString("s");

                        /*
                        const string str_user_data = updated_bo.getString("note");
                        string user_data_result[];
                        const int n_user_data = StringSplit(str_user_data, StringGetCharacter("&", 0), user_data_result);
                        if (n_user_data >= 2) {
                            bo.signal_name = user_data_result[0];
                            bo.user_data = "";
                            for (int k = 1; k < n_user_data; ++k) {
                                if (k != 1) bo.user_data += "&";
                                bo.user_data += user_data_result[k];
                            }
                            ArrayFree(user_data_result);
                        } else {
                            ArrayFree(user_data_result);
                        }
                        */

                        bo.duration = updated_bo.getLong("dur");
                        bo.status = MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE;
                        bo.send_date = updated_bo.getLong("st");
                        bo.open_date = updated_bo.getLong("ot");
                        bo.close_date = updated_bo.getLong("ct");
                        bo.amount = updated_bo.getDouble("a");
                        bo.profit = updated_bo.getDouble("profit");
                        bo.payout = updated_bo.getDouble("payout");
                        bo.open_price = updated_bo.getDouble("op");
                        updated_bo.getDouble("cp", bo.close_price);
                        //bo.close_price = updated_bo.getDouble("cp");
                        bo.contract_type = 0;
                        string dir_str = updated_bo.getString("dir");
                        if(dir_str == "buy") bo.contract_type = MegaConnectorBoContractType::MC_BO_CONTRACT_BUY;
                        else if(dir_str == "sell") bo.contract_type = MegaConnectorBoContractType::MC_BO_CONTRACT_SELL;
                        //MC_BO_UNKNOWN_STATE
                        //MC_BO_OPENING_ERROR
                        //MC_BO_CHECK_ERROR
                        //MC_BO_WAITING_COMPLETION
                        //MC_BO_WIN
                        //MC_BO_LOSS

                        int step = 0;
                        int max_step = 0;
                        if(updated_bo.getInt("step", step)) bo.step = (uint)step;
                        if(updated_bo.getInt("max_step", max_step)) bo.max_step = (uint)max_step;

                        string status_str = updated_bo.getString("status");
                        if(status_str == "win") bo.status = MegaConnectorBoStatus::MC_BO_WIN;
                        else if(status_str == "loss") bo.status = MegaConnectorBoStatus::MC_BO_LOSS;
                        else if(status_str == "standoff") bo.status = MegaConnectorBoStatus::MC_BO_STANDOFF;
                        else if(status_str == "unknown") bo.status = MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE;
                        else if(status_str == "error") bo.status = MegaConnectorBoStatus::MC_BO_CHECK_ERROR;
                        else if(status_str == "open_error") bo.status = MegaConnectorBoStatus::MC_BO_OPENING_ERROR;
                        else if(status_str == "wait") bo.status = MegaConnectorBoStatus::MC_BO_WAITING_COMPLETION;

                        on_update_bo(bo);
                        break;
                    }

                    //{ get quotes
                    JSONArray *j_array_prices = jo.getArray("prices");
                    if(j_array_prices != NULL) {
                        string intrade_bar_currency_pairs[26] = {
                            "EURUSD","USDJPY","GBPUSD","USDCHF",
                            "USDCAD","EURJPY","AUDUSD","NZDUSD",
                            "EURGBP","EURCHF","AUDJPY","GBPJPY",
                            "CHFJPY","EURCAD","AUDCAD","CADJPY",
                            "NZDJPY","AUDNZD","GBPAUD","EURAUD",
                            "GBPCHF","EURNZD","AUDCHF","GBPNZD",
                            "GBPCAD","XAUUSD"
                        };
                        double array_prices[26];
                        for(int i = 0; i < 26; ++i) {
                            array_prices[i] = j_array_prices.getDouble(i);
                        }
                        on_update_prices(intrade_bar_currency_pairs, array_prices);
                    }
                    //}

                    bool is_change_account_info = false;
                    //{ get a balance
                    double dtemp = 0;
                    if(jo.getDouble("b", dtemp)) {
                        account_info.balance = dtemp;
                        is_change_account_info = true;
                        is_get_account_info = true;
                    }
                    //}

                    //{ get ruble account flag
                    int itemp = 0;
                    if(jo.getInt("rub", itemp)) {
                        is_rub_currency = (itemp == 1);
                        if (is_rub_currency) {
                            account_info.currency = "RUB";
                        } else {
                            account_info.currency = "USD";
                        }
                        is_change_account_info = true;
                    }
                    //}

                    //{ get demo account flag
                    itemp = 0;
                    if(jo.getInt("demo", itemp)) {
                        account_info.is_demo = (itemp == 1);
                        is_change_account_info = true;
                    }
                    //}

                    //{ check the ping message
                    itemp = 0;
                    if(jo.getInt("ping", itemp)) {
                        string json_body = "{\"pong\":1}";
                        if (!pipe.write(json_body)) {
                            close();
                        }
                        on_ping();
                    }
                    //}

                    //{ check connection status
                    itemp = 0;
                    if(jo.getInt("conn", itemp)) {
                        if (itemp == 1) account_info.is_connected = true;
                        else account_info.is_connected = false;
                        is_change_account_info = true;
                    }
                    //}

                    //{ account ID
                    if(jo.getInt("aid", account_info.account_id)) {
                        is_change_account_info = true;
                    }
                    //}

                    if (is_change_account_info &&
                        is_get_account_info) {
                        on_account_info(account_info);
                    }
                }
                delete jv;
            }
            delete parser;
        }
    }

public:

    /** \brief Callback function to handle the connect event
     * \param v Connection status
     */
    virtual void on_connection(const bool status);

    /** \brief Callback function for the balance receipt event
     * \param b Current account balance
     */
    virtual void on_account_info(const MegaConnectorAccountInfo &info);

    /** \brief Callback function for getting the state of binary options
     * \param bo_result Binary option parameter structure
     */
    virtual void on_update_bo(MegaConnectorBoResult &bo_result);

    /** \brief Callback function for getting quotes of currency pairs
     * \param symbols   Character array
     * \param prices    Price array
     */
    virtual void on_update_prices(string &symbols[], double &prices[]);

    /** \brief Callback function to handle the ping event
     */
    virtual void on_ping();

    /** \brief Callback function to handle the error event
     */
    virtual void on_error(const string &message);

    /** \brief Конструктор класса
     */
    MegaConnectorBridgeApiV1() {
        pipe.set_buffer_size(4096);
        account_info.broker = MC_BROKER_INTRADE_BAR;
        is_connected                = false;
        is_rub_currency             = false;
        is_get_account_info         = false;
        config.app_id = get_str_unique_id();
    }

    /** \brief Деструктор класса
     */
    ~MegaConnectorBridgeApiV1() {
        close();
    }

    /** \brief Set named pipe name
     * \param arg_pipe_name Name of the named pipe
     */
    void set_pipe_name(const string &arg_pipe_name) {
        config.pipe_name = arg_pipe_name;
    }

    /** \brief Set the application ID
     * \param arg_app_id    Application ID
     */
    void set_app_id(const string &arg_app_id) {
        config.app_id = arg_app_id;
    }

    /** \brief Get the application ID
     * \return Application ID
     */
    inline string get_app_id() {
        return config.app_id;
    }

    /** \brief Connect to the MegaConnector program
     * This method will return true if the connection already exists or was successful
     * If false, you need to call this method again.
     * \return Return the connection status
     */
    bool connect() {
        if (is_connected) return true;
        is_connected = pipe.open(config.pipe_name);
        if (is_connected) {
            on_connection(true);
        }
        return is_connected;
    }

    /** \brief Connection status
     * \return Return the connection state
     */
    bool connected() {
        return is_connected;
    }

    datetime get_classic_timestamp(const int arg_expiration) {
	// проверяем кратность экспирации 5-ти минутам
	if ((arg_expiration % 5) != 0 || arg_expiration < 5) return 0;
	// находим смещение UTC времени
	datetime server_time = TimeCurrent();
	int gmt_offset = (int)(server_time - TimeGMT());
	int temp = gmt_offset % 300;
	if (temp > 150) gmt_offset += (300 - temp);
	else gmt_offset -= temp;
	// вычисляем верное время GMT
	server_time -= gmt_offset;
	server_time += (arg_expiration + 3) * 60;
	return (server_time - server_time % 300);
    }

    /** \brief Place binary option
     * \param arg_signal_id     Signal ID
     * \param arg_symbol        Symbol name
     * \param arg_signal_name   Signal name
     * \param arg_user_data     User data (The string will be returned in the deal state callback function)
     * \param arg_direction     Direction (BUY or SELL, 1 or -1).
     * \param arg_expiration    Expiration (in minutes)
     * \param arg_type          Option type (CLASSICAL or SPRINT)
     * \param arg_amount        The amount of the bet in the account currency
     * \param arg_step          The step number of the Loss Compensation System (LCS) request. Ranges from 0 to max_step.
     * \param arg_max_step      The maximum number of steps in the LCS.
     * \return Returns true if the submission was successful
     */
    bool place_bo(
            string                              &arg_signal_id,
            const string                        &arg_symbol,
            const string                        &arg_signal_name,
            const string                        &arg_user_data,
            const MegaConnectorBoContractType   arg_contract_type,
            const datetime                      arg_expiration,
            const MegaConnectorBoType           arg_type,
            const double                        arg_amount,
            const uint                          arg_step = 0,
            const uint                          arg_max_step = 1) {
        if (!is_connected || !account_info.is_connected) return false;

        if (arg_contract_type != MC_BO_CONTRACT_BUY &&
            arg_contract_type != MC_BO_CONTRACT_SELL) return false;

        if (arg_type != MC_BO_CLASSIC &&
            arg_type != MC_BO_SPRINT) return false;

        string json_body;
        string str_direction = arg_contract_type == MC_BO_CONTRACT_BUY ? "BUY" : "SELL";
        const string str_user_data = get_str_signal_id_v3(config.app_id, arg_signal_name, arg_user_data, arg_signal_id);

        StringConcatenate(json_body, json_body,
            "{\"contract\":{\"s\":\"", arg_symbol,
            "\",\"note\":\"", str_user_data,
            "\",\"dir\":\"", str_direction,
            "\",\"a\":", arg_amount,
            ",\"step\":", arg_step,
            ",\"max_step\":", arg_max_step,",");

        if(arg_type == MC_BO_CLASSIC) {
            datetime end_date = arg_expiration;
            //if(end_date < 86400) end_date *= 60; // convert time to seconds
            if (end_date < 86400) {
                end_date = get_classic_timestamp((int)arg_expiration);
            }
            StringConcatenate(json_body, json_body,
                "\"exp\":", (long)end_date);
        } else {
            datetime end_date = arg_expiration * 60; // convert time to seconds
            StringConcatenate(json_body, json_body,
                "\"dur\":", (long)end_date);
        }

        StringConcatenate(json_body, json_body, "}}");
        if(pipe.write(json_body)) return true;
        close();
        return false;
    }

    /** \brief Get balance
     * \return Returns the balance
     */
    double get_balance() {
        return account_info.balance;
    }

    /** \brief Check broker connection status
     * \return Will return true if there is a connection to the broker
     */
    bool check_broker_connection() {
        return account_info.is_connected;
    }

    /** \brief Update API state
     * This method must be called with each tick or by timer
     */
    void update() {
        if (connect()) {
            processing_incoming_messages();
        }
    }

    /** \brief Get the CLASSIC closing timestamp of a binary option
     * \param arg_timestamp     Timestamp (in seconds)
     * \param arg_expiration    Expiration (in minutes)
     * \return Returns the CLASSIC closing timestamp of the binary option, or 0 if an error occurred.
     */
    datetime get_classic_bo_closing_timestamp(const datetime arg_timestamp, const ulong arg_expiration) {
        if((arg_expiration % 5) != 0 || arg_expiration < 5) return 0;
        const datetime classic_bet_timestamp_future = (datetime)(arg_timestamp + (arg_expiration + 3) * 60);
        return (classic_bet_timestamp_future - classic_bet_timestamp_future % (5 * 60));
    }

    /** \brief Get CLASSIC closing timestamp of a binary option to close a bar
     * \param arg_timestamp     Timestamp (in seconds)
     * \param arg_expiration    Expiration (in minutes)
     * \return Returns the CLASSIC closing timestamp of the binary option, or 0 if an error occurred.
     */
    datetime get_candle_bo_closing_timestamp(const datetime arg_timestamp, const ulong arg_expiration) {
        if((arg_expiration % 5) != 0 || arg_expiration < 5) return 0;
        if(arg_expiration == 5) return get_classic_bo_closing_timestamp(arg_timestamp, arg_expiration);
        else {
            datetime end_timestamp = get_classic_bo_closing_timestamp(arg_timestamp, arg_expiration);
            if(((end_timestamp / 60) % arg_expiration) == 0) return end_timestamp;
            else return end_timestamp - 5*60;
        }
        return 0;
    }

    /** \brief Get payout percentage
      * \symbol         Symbol
      * \duration       Option duration
      * \param amount   Amount
      * \return Returns the broker's payout percentage as a fractional number
      */
    double get_payout(const string &arg_symbol, const ulong arg_duration, const double arg_amount) {
        const datetime timestamp = TimeGMT();
        const ulong sec_duration = arg_duration * 60;
        string intrade_bar_currency_pairs[23] = {
            "EURUSD","USDJPY","USDCHF",
            "USDCAD","EURJPY","AUDUSD",
            "NZDUSD","EURGBP","EURCHF",
            "AUDJPY","GBPJPY","EURCAD",
            "AUDCAD","CADJPY","NZDJPY",
            "AUDNZD","GBPAUD","EURAUD",
            "GBPCHF","AUDCHF","GBPNZD",
            "BTCUSD","BTCUSDT"
        };

        const int BTCUSD_INDEX = 21;
        const int BTCUSDT_INDEX = 22;

        int symbol_index = 0;
        bool is_found = false;
        for (int i = 0; i < 23; ++i) {
            const int len = StringLen(intrade_bar_currency_pairs[i]);
            if (StringSubstr(arg_symbol, 0, len) == intrade_bar_currency_pairs[i]) {
                is_found = true;
                symbol_index = i;
                break;
            }
        }
        if (!is_found) return 0.0;
        if ((sec_duration % 60) != 0) return 0.0;

        const ulong max_duration = 3600*24*360*10;
        if (sec_duration > 30000 && sec_duration < max_duration) return 0.0;

        if ((!is_rub_currency && arg_amount < 1.0) ||
            (is_rub_currency && arg_amount < 50.0)) return 0.0;

        if (symbol_index == BTCUSD_INDEX || symbol_index == BTCUSDT_INDEX) {
            if (sec_duration < 600) return 0.0;
            if ((!is_rub_currency && arg_amount >= 80)||
                    (is_rub_currency && arg_amount >= 5000)) {
                return 0.85;
            }
            return 0.79;
        }

        if (sec_duration < 180) return 0.0;

        const uint second_day = legacy_trading_sec_of_day(timestamp);
        if (second_day < 3600) return 0.0;
        if (second_day >= 75600) return 0.0;

        if (// первая половина дня 4-9
            (second_day < 3780) ||                          // 4
            (second_day >= 7020 && second_day < 7380) ||    // 5
            (second_day >= 10620 && second_day < 10980) ||  // 6
            (second_day >= 14220 && second_day < 14580) ||  // 7
            (second_day >= 17820 && second_day < 18180) ||  // 8
            (second_day >= 21420 && second_day < 21780) ||  // 9
            // вторая половина дня 17-23
            (second_day >= 50100 && second_day < 50700) || // 17
            (second_day >= 53700 && second_day < 54300) || // 18
            (second_day >= 57300 && second_day < 57900) || // 19
            (second_day >= 60900 && second_day < 61500) || // 20
            (second_day >= 64500 && second_day < 65100) || // 21
            (second_day >= 68100 && second_day < 68700) || // 22
            (second_day >= 71700 && second_day < 72300) || // 23
            //(second_day >= 73500 && second_day < 74100) || // 23:30 73800
            (second_day >= 75300)) {
            return 0.6;
        }

        // Если счет в долларах и ставка больше 80 долларов или счет в рублях и ставка больше 5000 рублей
        if ((!is_rub_currency && arg_amount >= 80)||
                (is_rub_currency && arg_amount >= 5000)) {
            return 0.85;
        } else {
            if(sec_duration == 180) {
                /* Если продолжительность экспирации 3 минуты
                 * Процент выплат составит 82 (0,82)
                 */
                return 0.82;
            } else {
                /* Если продолжительность экспирации от 4 до 500 минут */
                return 0.79;
            }
        }
        return 0.0;
    }

    /** \brief Get account type
     * \return Will return true if demo account
     */
    bool get_account_type() {
        return account_info.is_demo;
    }

    /** \brief Get account currency
     * \return Returns the account currency name
     */
    string get_account_currency() {
        return account_info.currency;
    }

    /** \brief Get account ID
     * \return Account ID
     */
    inline int get_account_id() {
        return account_info.account_id;
    }

    /** \brief Close connection with MegaConnector program
     */
    void close() {
        is_rub_currency             = false;
        account_info.is_demo        = false;
        account_info.balance        = 0;
        if (is_connected) {
            pipe.close();
            is_connected = false;
            on_connection(false);
            if (account_info.is_connected) {
                account_info.is_connected = false;
                on_account_info(account_info);
                is_get_account_info = false;
            }
        }
    }
}; // MegaConnectorBridgeApiV1

void NamedPipeClient::on_open   (NamedPipeClient *pointer) {};
void NamedPipeClient::on_close  (NamedPipeClient *pointer) {};
void NamedPipeClient::on_message(NamedPipeClient *pointer, const string &message) {};
void NamedPipeClient::on_error  (NamedPipeClient *pointer, const string &error_message) {};

#endif // OPTIONX_LEGACY_TRADING_API_MQH
//+------------------------------------------------------------------+
