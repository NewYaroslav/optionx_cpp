//+------------------------------------------------------------------+
//|                                                      bapi-v1.mqh |
//|                                   Copyright 2022, MegaConnector. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
// 
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_BAPI_V2_MQH
#define MEGA_CONNECTOR_BAPI_V2_MQH

#property copyright "Copyright 2022, MegaConnector."
#property link      "https://mega-connector.com/"
#property strict

#include "../part/common.mqh"
#include "../lib/simple-named-pipe-server/named_pipe_client.mqh"
#include "../lib/json.mqh"
#include "../lib/ztime.mqh"
#include <Tools\DateTime.mqh>

/** \brief BAPI class for connecting to the MegaConnector program
 * BAPI (Bridge Api) is intended for interaction with brokers:
 * sending signals, receiving payout percentages, account balances, etc.
 */
class MegaConnectorBridgeApiV2 {
private:
    
    class Config {
    public:
        int api_version;
        int account_limit;
        int payout_limit;
        int open_trades_limit;
        int close_trades_limit;
        int lifetime_closed_bo;
        ulong max_ping_ticks;
        string pipe_name;
    };
    
    Config config;
    
    NamedPipeClient pipe;
    ztime::Timer    timer;

    string body;            // тело сообщения
    bool is_connected;      // состояние подключения
    bool is_signal;         // Флаг наличия сигнала для аккумулятора сигналов
    bool is_use_signal_accumulator; // Флаг использования аккумулятора сигналов
    bool is_last_mt4_connected;
    
    long mt_account_id;
    long mt_soft_id;
    long mt_user_id;
    
    int mt_bo_counter;
    
    // массив аккаунтов
    MegaConnectorAccountInfo    account_list[];

    Hash*                       payout_hash;
    Hash*                       open_trades_hash;
    Hash*                       close_trades_hash;
    
    string get_bet_hash_key(MegaConnectorBoResult &bo_result) {
        if (bo_result.broker_id <= 0) {
            return bo_result.broker + "api" + IntegerToString(mt_bo_counter++);
        }
        return bo_result.broker + IntegerToString(bo_result.broker_id);
    }
    
    void remove_closed_bo(const int delay) {
        string bo_key[];
        MegaConnectorBoResult bo_list[];
        
        ArrayResize(bo_key, open_trades_hash.getCount());
        ArrayResize(bo_list, open_trades_hash.getCount());
        
        int index = 0;
        HashLoop *l = NULL;
        for (l = new HashLoop(open_trades_hash); l.hasNext(); l.next()) {
            bo_key[index] = l.key();
            MegaConnectorBoResult *bo = l.val();
            bo_list[index] = *bo;
            ++index;
        }
        delete l;
        
        const datetime t = TimeGMT();
        for (int i = 0; i < ArraySize(bo_list); ++i) {
            
            if (bo_list[i].status == MegaConnectorBoStatus::MC_BO_WAITING_COMPLETION || 
                    bo_list[i].status == MegaConnectorBoStatus::MC_BO_UPDATE) {
                //Print(bo_list[i].symbol," close_date ", bo_list[i].close_date);   
            }
            
            if (((datetime)bo_list[i].close_date + (datetime)delay) <= t) {
                // если сделка не была закрыта, например из-за обырва связи, добавим ее в список закрытых
                if (bo_list[i].status == MegaConnectorBoStatus::MC_BO_WAITING_COMPLETION || 
                    bo_list[i].status == MegaConnectorBoStatus::MC_BO_UPDATE) {
                    MegaConnectorBoResult *copy_bo = open_trades_hash.hGet(bo_key[i]);
                    MegaConnectorBoResult *temp = new MegaConnectorBoResult;
                    *temp = *copy_bo;
                    close_trades_hash.hPut(bo_key[i], temp);
                }
                // удаляем сделку
                open_trades_hash.hDel(bo_key[i]);
            }
        }
        ArrayFree(bo_key);
        ArrayFree(bo_list);
    }
    
    bool write(string &message) {
        if (!pipe.write(message)) {
            close();
            return false;
        }
        return true;
    }
    
    bool send_ping() {
        string json_body = "{\"e\":0,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{}}";
        return write(json_body);
    }
    
    void update_ping() {
        if (timer.get_elapsed_ms() > config.max_ping_ticks) {
            timer.reset();
            send_ping();
        }
    }
    
    bool is_terminal_connected() {
        return (bool)TerminalInfoInteger(TERMINAL_CONNECTED);
    }
    
    bool send_mt4_connected() {
        string json_body = "{\"e\":8,\"v\":";
        json_body += IntegerToString(config.api_version);
        if (is_terminal_connected()) json_body += ",\"d\":{\"status\":1}}";
        else json_body += ",\"d\":{\"status\":0}}";
        return write(json_body);
    }
    
    void update_mt4_connected() {
        if (is_last_mt4_connected != is_terminal_connected()) {
            is_last_mt4_connected = is_terminal_connected();
            send_mt4_connected();
        }
    }
    
       /** \brief Обновить состояние API
    * \param delay Задержка между вызовами функции
    */
    void processing_incoming_messages() {
        if (!is_connected) return;
        
        // удаляем закрыте сделки БО
        remove_closed_bo(config.lifetime_closed_bo);

        // отправляем пинг раз в 'config.max_ping_ticks' тиков
        update_ping();

        // отправляем состояние соединения
        update_mt4_connected();
          
        while (pipe.get_bytes_read() > 0) {
            body += pipe.read();
            // проверяем, было ли получено тело сообщения полностью
            if (StringGetCharacter(body, StringLen(body) - 1) != '}') {
                continue;
            }
            
            // парсим json сообщение
            JSONParser *parser = new JSONParser();
            JSONValue *jv = parser.parse(body);
            
            if(jv == NULL) {
                string text = "error: " + (string)parser.getErrorCode() + " " + parser.getErrorMessage();
                text += " body: ";
                text += body;
                on_error(900, text);
                body = "";
            } else {
                body = "";
                if (jv.isObject())
                while (true) {
                    JSONObject *jo = jv;

                    // получаем версию API
                    int api_version = -1;
                    if (!jo.getInt("v", api_version)) {
                        // ошибка API
                        on_error(-1000);
                        break;
                    }
                    
                    if (api_version != config.api_version) {
                        // ошибка API
                        on_error(-1001);
                        break;
                    }
                    
                    // получаем код события
                    int event_code = -1;
                    if (!jo.getInt("e", event_code)) {
                        // ошибка API
                        on_error(-1002);
                        break;
                    }
                    
                    // получаем сигнал пинг
                    if (event_code == 0) {
                        string json_body = "{\"e\":0,\"v\":";
                        json_body += IntegerToString(config.api_version);
                        json_body += ",\"d\":{}}";
                        if (!pipe.write(json_body)) {
                            close();
                            // ошибка API
                            on_error(-1003);
                            break;
                        }
                        on_ping();
                    } else
                    // получаем информацию об аккаунтах
                    if (event_code == 1) {
                        JSONArray *data_array = jo.getArray("d");
                        if (data_array != NULL) {
                            
                            int account_index[];
                            const int accounts = data_array.size();
                            for (int i = 0; i < accounts; ++i) {
                                JSONObject *account_data = data_array.getObject(i);
                                if (account_data == NULL) continue;
                                
                                MegaConnectorAccountInfo account_info;
                                string value;
                                if (account_data.getString("an", value)) account_info.account_name = value;
                                if (account_data.getString("bn", value)) account_info.broker = value;
                                if (account_data.getString("uid", value)) account_info.user_id = value;
                                if (account_data.getString("cur", value)) account_info.currency = value;
                                
                                /*
                                account_info.account_name = account_data.getString("an");
                                account_info.broker = account_data.getString("bn");
                                account_info.user_id = account_data.getString("uid");
                                account_info.currency = account_data.getString("cur");
                                */
                                
                                account_info.balance = account_data.getDouble("b");
                                
                                const int connect_status = account_data.getInt("con");
                                account_info.is_connected = (connect_status == 1);
                                
                                const int demo_status = account_data.getInt("d");
                                account_info.is_demo = (demo_status == 1);
                                
                                const int account_id = account_data.getInt("i");

                                if (account_id < config.account_limit) {
                                    const int account_list_size = ArraySize(account_list);
                                    if (account_id >= account_list_size) {
                                        ArrayResize(account_list, account_id + 1);
                                    }
                                    account_list[account_id] = account_info;

                                    ArrayResize(account_index, ArraySize(account_index) + 1);
                                    account_index[ArraySize(account_index)-1] = account_id;
                                } else {
                                    // ошибка API
                                    on_error(-1004);
                                    break;
                                }
                            } // for i
                            for (int i = 0; i < ArraySize(account_index); ++i) {
                                on_account_info(account_list[account_index[i]]); // Callback
                            }
                        } else {
                            // ошибка API
                            on_error(-1005);
                            break;
                        }
                    } else
                    // получаем проценты выплат по подписке
                    if (event_code == 5) {
                        JSONArray *data_array = jo.getArray("d");
                        if (data_array != NULL) {
                            const int payouts = data_array.size();
                            for (int i = 0; i < payouts; ++i) {
                                JSONObject *data_payout = data_array.getObject(i);
                                if (data_payout == NULL) continue;
                                
                                const double payout = data_payout.getDouble("p");
                                const string subscription_key = data_payout.getString("key");
                                
                                MegaConnectorPayoutSubscription *payout_temp = payout_hash.hGet(subscription_key);
                                if (payout_temp == NULL) continue;
                                payout_temp.payout = payout; 
                                
                                on_payout_info(payout_temp); // Callback
                            } // for i
                        } // if (data_array != NULL)
                    } else
                    // получаем значения состояния сделок
                    if (event_code == 3) {
                        JSONArray *data_array = jo.getArray("d");
                        if (data_array != NULL) {
   
                            const int bo_bets = data_array.size();
                            for (int i = 0; i < bo_bets; ++i) {
                                JSONObject *data_bo = data_array.getObject(i);
                                if (data_bo == NULL) continue;
   
                                MegaConnectorBoResult bo_data;
                                bo_data.api_id = data_bo.getInt("aid");
                                bo_data.broker_id = data_bo.getInt("id");
                                bo_data.broker = data_bo.getString("b");
                                bo_data.symbol = data_bo.getString("s");
                                bo_data.signal_name = data_bo.getString("sn");
                                bo_data.user_data = data_bo.getString("ud");
                                bo_data.currency = data_bo.getString("c");
                                
                                bo_data.duration = data_bo.getLong("d");
                                bo_data.ping = data_bo.getLong("p"); 
                                bo_data.open_date = data_bo.getLong("od");
                                bo_data.close_date = data_bo.getLong("cd");
  
                                bo_data.amount = data_bo.getDouble("a");           
                                bo_data.profit = data_bo.getDouble("profit");         
                                bo_data.payout = data_bo.getDouble("payout"); 
      
                                bo_data.open_price = data_bo.getDouble("op");
                                bo_data.close_price = data_bo.getDouble("cp");
                                
                                bo_data.contract_type = MegaConnectorBoContractType::MC_BO_CONTRACT_UNKNOWN_STATE; 
                                string dir_str = data_bo.getString("dir");
                                if(dir_str == "buy") bo_data.contract_type = MegaConnectorBoContractType::MC_BO_CONTRACT_BUY;
                                else if(dir_str == "sell") bo_data.contract_type = MegaConnectorBoContractType::MC_BO_CONTRACT_SELL;
                                
                                string bo_type_str = data_bo.getString("bt");
                                if(bo_type_str == "s") bo_data.bo_type = MegaConnectorBoType::MC_BO_SPRINT;
                                else if(bo_type_str == "c") bo_data.bo_type = MegaConnectorBoType::MC_BO_CLASSIC;
                                
                                /*
                                UNKNOWN_STATE
                                OPENING_ERROR
                                CHECK_ERROR
                                LOW_PAYMENT_ERROR
                                WAITING_COMPLETION
                                WIN
                                LOSS
                                STANDOFF
                                UPDATE
                                INCORRECT_PARAMETERS
                                AUTHORIZATION_ERROR
                                */
                                
                                bo_data.status = MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE;
                                string status_str = data_bo.getString("status"); 
                                if (status_str == "update")          bo_data.status = MegaConnectorBoStatus::MC_BO_UPDATE;
                                else if (status_str == "wait")       bo_data.status = MegaConnectorBoStatus::MC_BO_WAITING_COMPLETION;
                                else if (status_str == "win")        bo_data.status = MegaConnectorBoStatus::MC_BO_WIN;
                                else if (status_str == "loss")       bo_data.status = MegaConnectorBoStatus::MC_BO_LOSS;
                                else if (status_str == "standoff")   bo_data.status = MegaConnectorBoStatus::MC_BO_STANDOFF;
                                else if (status_str == "unknown")    bo_data.status = MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE;
                                else if (status_str == "c-error")    bo_data.status = MegaConnectorBoStatus::MC_BO_CHECK_ERROR; 
                                else if (status_str == "o-error")    bo_data.status = MegaConnectorBoStatus::MC_BO_OPENING_ERROR;
                                else if (status_str == "p-error")    bo_data.status = MegaConnectorBoStatus::MC_BO_LOW_PAYMENT_ERROR;
                                else if (status_str == "i-error")    bo_data.status = MegaConnectorBoStatus::MC_BO_INCORRECT_PARAMETERS;
                                else if (status_str == "a-error")    bo_data.status = MegaConnectorBoStatus::MC_BO_AUTHORIZATION_ERROR; 
                                
                                bo_data.raw_status = MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE;
                                string r_status_str = data_bo.getString("r-status"); 
                                if (r_status_str == "update")          bo_data.raw_status = MegaConnectorBoStatus::MC_BO_UPDATE;
                                else if (r_status_str == "wait")       bo_data.raw_status = MegaConnectorBoStatus::MC_BO_WAITING_COMPLETION;
                                else if (r_status_str == "win")        bo_data.raw_status = MegaConnectorBoStatus::MC_BO_WIN;
                                else if (r_status_str == "loss")       bo_data.raw_status = MegaConnectorBoStatus::MC_BO_LOSS;
                                else if (r_status_str == "standoff")   bo_data.raw_status = MegaConnectorBoStatus::MC_BO_STANDOFF;
                                else if (r_status_str == "unknown")    bo_data.raw_status = MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE;
                                else if (r_status_str == "c-error")    bo_data.raw_status = MegaConnectorBoStatus::MC_BO_CHECK_ERROR; 
                                else if (r_status_str == "o-error")    bo_data.raw_status = MegaConnectorBoStatus::MC_BO_OPENING_ERROR;
                                else if (r_status_str == "p-error")    bo_data.raw_status = MegaConnectorBoStatus::MC_BO_LOW_PAYMENT_ERROR;
                                else if (r_status_str == "i-error")    bo_data.raw_status = MegaConnectorBoStatus::MC_BO_INCORRECT_PARAMETERS;
                                else if (r_status_str == "a-error")    bo_data.raw_status = MegaConnectorBoStatus::MC_BO_AUTHORIZATION_ERROR; 
                                
                                // если неопределенный статус, пропускаем результаты
                                if (bo_data.status != MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE) {
                                    
                                    // получаем хеш
                                    const string bet_hash_id = get_bet_hash_key(bo_data);
                                    if (StringLen(bet_hash_id) == 0) continue;
                                    // добавляем сделку в массив
                                    
                                    if (open_trades_hash.getCount() == 0) {
                                        MegaConnectorBoResult *temp = new MegaConnectorBoResult();
                                        *temp = bo_data;
                                        open_trades_hash.hPut(bet_hash_id, temp);
                                    } else {
                                        if (open_trades_hash.hContainsKey(bet_hash_id)) {
                                            MegaConnectorBoResult *bo_update = open_trades_hash.hGet(bet_hash_id);
                                            *bo_update = bo_data;
                                        } else 
                                        if (open_trades_hash.getCount() < config.open_trades_limit) {
                                            MegaConnectorBoResult *temp = new MegaConnectorBoResult();
                                            *temp = bo_data;
                                            open_trades_hash.hPut(bet_hash_id, temp);
                                        }
                                    }
                                    
                                    // Добавляем закрытую сделку
                                    if (bo_data.status != MegaConnectorBoStatus::MC_BO_WAITING_COMPLETION && 
                                        bo_data.status != MegaConnectorBoStatus::MC_BO_UPDATE) {
                                        if (close_trades_hash.getCount() == 0) {
                                            MegaConnectorBoResult *temp = new MegaConnectorBoResult();
                                            *temp = bo_data;
                                            close_trades_hash.hPut(bet_hash_id, temp);
                                        } else {
                                            if (close_trades_hash.hContainsKey(bet_hash_id)) {
                                                MegaConnectorBoResult *bo_update_data = close_trades_hash.hGet(bet_hash_id);
                                                *bo_update_data = bo_data;
                                            } else 
                                            if (close_trades_hash.getCount() < config.close_trades_limit) {
                                                MegaConnectorBoResult *temp = new MegaConnectorBoResult();
                                                *temp = bo_data;
                                                close_trades_hash.hPut(bet_hash_id, temp);
                                            } else 
                                            if (close_trades_hash.getCount() >= config.close_trades_limit) {
                                                // удаляем первый элемент
                                                HashLoop *l = new HashLoop(close_trades_hash);
                                                string del_bet_hash_id = l.key();
                                                delete l;
                                                close_trades_hash.hDel(del_bet_hash_id);
                                                // добавляем новый
                                                MegaConnectorBoResult *temp = new MegaConnectorBoResult();
                                                *temp = bo_data;
                                                close_trades_hash.hPut(bet_hash_id, temp);
                                            }
                                        }
                                    }
                                } // if (bo_data.status != MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE)
                                
                                on_update_bo(bo_data);
                            } // for i
                            
                        } // if (data_array != NULL)
                    } // if (event_code == 3)
                    break;
                } // while
                delete jv;
            }
            delete parser;
        }
    };
    
public:

    /** \brief Callback function to handle the connect event
     */
    virtual void on_connect();
    
    /** \brief Callback function to handle the disconnect event
     */
    virtual void on_disconnect();

    /** \brief Callback-функция для получения состояния бинарных опционов
     * \param bo_data Класс параметров бинарного опциона
     */
    virtual void on_update_bo(const MegaConnectorBoResult &bo_result);
    
    /** \brief Callback-функция для получения состояния аккаунта
     * \param account_info Класс параметров аккаунта
     */
    virtual void on_account_info(const MegaConnectorAccountInfo &account_info);
    
    /** \brief Callback-функция для получения процентов выплат
     * \param payout_info Класс данных процентов выплат
     */
    virtual void on_payout_info(const MegaConnectorPayoutSubscription &payout_info);
    
    /** \brief Callback-функция для обработки события ping
     */
    virtual void on_ping();
    
    /** \brief Callback-функция для обработки ошибок
     * \param error_code Код ошибки
     */
    virtual void on_error(const int error_code, const string message = "");
    
    /** \brief Callback-функция для запроса кода защиты
     */
    virtual ulong on_security_code();

    /** \brief Конструктор класса
     */
    MegaConnectorBridgeApiV2() {
        is_connected = false;
        is_signal = false;
        is_use_signal_accumulator = false;
        is_last_mt4_connected = false;
        
        config.pipe_name = "mega_connector_bapi_v1";
        config.api_version          = 1;
        config.account_limit        = 256;
        config.payout_limit         = 256;
        config.open_trades_limit    = 32; // максимальное количество сделок в списке открытых сделок  
        config.close_trades_limit   = 32; // максимальное количество сделок в списке закрытых сделок
        config.lifetime_closed_bo   = 10; // время жизни сделки в списке открытх сделок после ее завершения
        config.max_ping_ticks       = 30000;
        
        payout_hash = new Hash(config.payout_limit, true); 
        open_trades_hash = new Hash(config.open_trades_limit, true);
        close_trades_hash = new Hash(config.close_trades_limit, true);
        
        pipe.set_buffer_size(4096);

        //mt_account_id = AccountNumber();
        mt_account_id = AccountInfoInteger(ACCOUNT_LOGIN);
        mt_user_id = 0;
        mt_soft_id = 0;
        
        mt_bo_counter = 0;
    }
   
    /** \brief Деструктор класса
     */
    ~MegaConnectorBridgeApiV2() {
        close();
        if (payout_hash != NULL) delete payout_hash;
        if (open_trades_hash != NULL) delete open_trades_hash;
        if (close_trades_hash != NULL) delete close_trades_hash;
    }
    
    /** \brief Set named pipe name
     * \param pipe_name Name of the named pipe
     */
    void set_pipe_name(const string &pipe_name) {
        config.pipe_name = pipe_name;
    }
    
    /** \brief Установить флаг использования аккумулятора сигналов
     * \param value Включить/выключить аккумулятор сигналов
     */
    void set_use_signal_accumulator(const bool value) {
        is_use_signal_accumulator = value;
    }
    
    /** \brief Установить user id
     * \param value Значение user id
     */
    void set_user_id(const int value) {
        mt_user_id = value;
    }
    
    /** \brief Установить soft id
     * \param value Значение soft id
     */
    void set_soft_id(const int value) {
        mt_soft_id = value;
    }
    
    void set_lifetime_closed_bo(const int value) {
        config.lifetime_closed_bo = value;
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
            on_connect();
            send_mt4_connected();
            is_last_mt4_connected = is_terminal_connected();
        }
        return is_connected;
    }

    /** \brief Состояние подключения
     * \return Вернет состояние подключения
     */
    bool connected() {
        return is_connected;
    }
    
    /** \brief Отправить информацию про аккаунт МТ4 и советник
     * \return Вернет true в случае успешного отправления
     */
    bool send_mt_info() {
        if (!is_connected) return false;
        
        string json_body = "{\"e\":1000,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"mtid\":";
        json_body += IntegerToString(mt_account_id);
        json_body += ",\"sid\":";
        json_body += IntegerToString(mt_soft_id);
        json_body += ",\"uid\":";
        json_body += IntegerToString(mt_user_id);
        json_body += ",\"sc\":";
        json_body += IntegerToString(on_security_code());
        json_body += "}}";
        
        return write(json_body);
    }
    
    /** \brief Отправить сообщение на email
     * \param to_email  Куда отправлять
     * \param theme     Тема сообщения
     * \param message   Сообщение
     * \return Вернет true в случае успешного отправления
     */
    bool send_email(string to_email, string theme, string message) {
        if (!is_connected) return false;
        
        string json_body = "{\"e\":10,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"te\":\"";
        json_body += to_email;
        json_body += "\",\"t\":\"";
        json_body += theme;
        json_body += "\",\"m\":\"";
        json_body += message;
        json_body += "\",\"st\":\"e\"}}";
        
        return write(json_body);
    }
    
    /** \brief Отправить сообщение на свой email
     * \param theme     Тема сообщения
     * \param message   Сообщение
     * \return Вернет true в случае успешного отправления
     */
    bool send_email(string theme, string message) {
        if (!is_connected) return false;
        
        string json_body = "{\"e\":10,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"t\":\"";
        json_body += theme;
        json_body += "\",\"m\":\"";
        json_body += message;
        json_body += "\",\"st\":\"e\"}}";
        
        return write(json_body);
    }
    
    /** \brief Отправить сообщение в телеграм
     * \param chat_name Имя чата
     * \param message   Сообщение
     * \return Вернет true в случае успешного отправления
     */
    bool send_telegram(string chat_name, string message) {
        if (!is_connected) return false;
        
        string json_body = "{\"e\":10,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"cn\":\"";
        json_body += chat_name;
        json_body += "\",\"m\":\"";
        json_body += message;
        json_body += "\",\"st\":\"t\"}}";
        
        return write(json_body);
    }
    
    /** \brief Отправить сообщение в телеграм
     * \param message   Сообщение
     * \return Вернет true в случае успешного отправления
     */
    bool send_telegram(string message) {
        if (!is_connected) return false;
        
        string json_body = "{\"e\":10,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"m\":\"";
        json_body += message;
        json_body += "\",\"st\":\"t\"}}";
        
        return write(json_body);
    }
    
    /** \brief Начать аккумулировать сигналы
     * \return Вернет true в случае успешного отправления
     */
    bool begin_signal_accumulator() {
        if (!is_connected) return false;
        
        string json_body = "{\"e\":9,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"status\":1}}";
        
        return write(json_body);
    }
    
    /** \brief Остановить аккумулирование сигналов
     * Во время завершения аккумулирования сигналов коннектор произведет анализ массива сделок
     * \return Вернет true в случае успешного отправления
     */
    bool end_signal_accumulator() {
        if (!is_connected) return false;
        
        string json_body = "{\"e\":9,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"status\":0}}";
        
        return write(json_body);
    }
    
    /** \brief Обноить аккумулятор сигналов
     * При вызове данного метода все ранее аккумулированные сделки начнут обрабатываться коннектором
     * Метод работает только если был включен аккумулятор сигналов
     * \return Вернет true в случае успешного отправления
     */
    bool update_signal_accumulator() {
        if (!is_use_signal_accumulator) return false;
        if (is_signal) {
            if (!end_signal_accumulator()) return false;
            is_signal = false;
        }
        return true;
    }
   
    /** \brief Открыть сделку
     * \param bo_config Параметры опциона
     * \return Вернет true в случае успешного отправления
     */
    bool place_bo(MegaConnectorBoRequest &bo_request) {
        if (!is_connected) return false;
        if (is_use_signal_accumulator) {
            if (!begin_signal_accumulator()) return false;
            is_signal = true;
        }
        
        string json_body = "{\"e\":4,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"s\":\"";
        
        json_body += bo_request.symbols;
        
        if (StringLen(bo_request.brokers) > 0) {
            json_body += "\",\"b\":\"";
            json_body += bo_request.brokers;
        }
        if (StringLen(bo_request.accounts) > 0) {
            json_body += "\",\"ac\":\"";
            json_body += bo_request.accounts;
        }
        if (StringLen(bo_request.signal_name) > 0) {
            json_body += "\",\"sn\":\"";
            json_body += bo_request.signal_name;
        }
        if (StringLen(bo_request.user_data) > 0) {
            json_body += "\",\"ud\":\"";
            json_body += bo_request.user_data;
        }
        
        if (bo_request.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_BUY) {
            json_body += "\",\"ct\":\"buy";
        } else 
        if (bo_request.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_SELL) {
            json_body += "\",\"ct\":\"sell";
        } else return false;
        
        if (bo_request.bo_type == MegaConnectorBoType::MC_BO_SPRINT) {
            json_body += "\",\"t\":\"s";
        } else 
        if (bo_request.bo_type == MegaConnectorBoType::MC_BO_CLASSIC) {
            json_body += "\",\"t\":\"c";
        } else return false;
        
        if (bo_request.bo_type == MegaConnectorBoType::MC_BO_SPRINT) {
            json_body += "\",\"d\":";
            json_body += IntegerToString(bo_request.duration);
        } else 
        if (bo_request.bo_type == MegaConnectorBoType::MC_BO_CLASSIC) {
            json_body += "\",\"e\":";
            json_body += IntegerToString(bo_request.end_date);
        }
        
        if (bo_request.threshold_payout > 0) {
            json_body += ",\"tp\":";
            json_body += DoubleToString(bo_request.threshold_payout, 3);
        }
        
        if (bo_request.amount > 0) {
            json_body += ",\"a\":";
            json_body += DoubleToString(bo_request.amount, bo_request.amount_digits);
        }
        if (bo_request.winrate > 0) {
            json_body += ",\"w\":";
            json_body += DoubleToString(bo_request.winrate, 3);
        }
        
        json_body += "}}";
        
        return write(json_body);
    }
    
    bool subscribe_payout(MegaConnectorPayoutConfig &payout_config) {
        if (!is_connected) return false;
        if (payout_hash.getCount() >= config.payout_limit) return false;
        
        string json_body = "{\"e\":5,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"s\":\"";
        json_body += payout_config.symbol;
        
        if (StringLen(payout_config.broker) > 0) {
            json_body += "\",\"b\":\"";
            json_body += payout_config.broker;
            json_body += "\",\"a\":";
        } else {
            json_body += "\",\"id\":";
            json_body += IntegerToString(payout_config.account_id);
            json_body += ",\"a\":";
        }
        json_body += DoubleToString(payout_config.amount, payout_config.amount_digits);
        
        if (payout_config.bo_type == MegaConnectorBoType::MC_BO_SPRINT) {
            json_body += ",\"d\":";
            json_body += IntegerToString(payout_config.duration);
        } else 
        if (payout_config.bo_type == MegaConnectorBoType::MC_BO_CLASSIC) {
            json_body += ",\"e\":";
            json_body += IntegerToString(payout_config.end_date);
        } else return false;
        
        if (payout_config.bo_type == MegaConnectorBoType::MC_BO_SPRINT) {
            json_body += ",\"t\":\"s\"";
        } else 
        if (payout_config.bo_type == MegaConnectorBoType::MC_BO_CLASSIC) {
            json_body += ",\"t\":\"c\"";
        } else return false;
        
        if (payout_config.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_BUY) {
            json_body += ",\"ct\":\"buy\"";
        } else 
        if (payout_config.contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_SELL) {
            json_body += ",\"ct\":\"sell\"";
        } else return false;
        
        // добавляем в массив подписок на проценты выплат
        if (StringLen(payout_config.subscribe_key) == 0) {
            payout_config.subscribe_key = IntegerToString(payout_hash.getCount() + 1);
        }

        if (payout_hash.hContainsKey(payout_config.subscribe_key)) {
            // если процент выплат уже инициализирован
            
            //---payout_hash.hDel(payout_config.subscribe_key);
            MegaConnectorPayoutSubscription *payout_find = payout_hash.hGet(payout_config.subscribe_key);
            payout_find.config = payout_config;
            payout_find.payout = 0;
        } else {
            MegaConnectorPayoutSubscription *payout_temp = new MegaConnectorPayoutSubscription();
            payout_temp.config = payout_config;
            payout_temp.payout = 0;
            payout_hash.hPut(payout_config.subscribe_key, payout_temp);
        }
        
        json_body += ",\"key\":\"";
        json_body += payout_config.subscribe_key;
        json_body += "\"}}";
        
        return write(json_body);
    }
    
    bool unsubscribe_payout(const string subscribe_key) {
        if (!is_connected) return false;
        
        if (!payout_hash.hContainsKey(subscribe_key)) return false;
        
        string json_body = "{\"e\":6,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{\"key\":\"";
        
        json_body += subscribe_key;
        json_body += "\"}}";
        
        payout_hash.hDel(subscribe_key);
        
        return write(json_body);
    }
    
    bool unsubscribe_payout_all() {
        if (!is_connected) return false;
        
        string json_body = "{\"e\":7,\"v\":";
        json_body += IntegerToString(config.api_version);
        json_body += ",\"d\":{}}";
        
        delete payout_hash;
        payout_hash = new Hash(config.payout_limit, true);
        
        return write(json_body);
    }

    /** \brief Обновить состояние API
     * Данная версия функции сама проверяет наличие подключения к API
     * \param delay Задержка между вызовами функции
     */
    void update() {
        if (connect()) {
            processing_incoming_messages();
        }
    }
    
    /** \brief Получить актуальные данные по всем аккаунтам
     * \param new_account_list Ссылка на массив аккаунтов
     */
    void get_account_list(MegaConnectorAccountInfo &new_account_list[]) {
        ArrayResize(new_account_list, ArraySize(account_list));
        const int len = ArraySize(new_account_list);
        for (int i = 0; i < len; ++i) {
            new_account_list[i] = account_list[i];
        }
    }
   
    /** \brief Получить метку времени закрытия CLASSIC бинарного опциона
     * \param timestamp Метка времени (в секундах)
     * \param expiration Экспирация (в минутах)
     * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
     */
    datetime get_classic_bo_closing_timestamp(const datetime user_timestamp, const ulong user_expiration) {
        if ((user_expiration % 5) != 0 || user_expiration < 5) return 0;
        const datetime classic_bet_timestamp_future = (datetime)(user_timestamp + (user_expiration + 3) * 60);
        return (classic_bet_timestamp_future - classic_bet_timestamp_future % (5 * 60));
    }
   
    /** \brief Получить метку времени закрытия CLASSIC бинарного опциона на закрытие бара
     * \param timestamp Метка времени (в секундах)
     * \param expiration Экспирация (в минутах)
     * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
     */
    datetime get_candle_bo_closing_timestamp(const datetime user_timestamp, const ulong user_expiration) {
        if ((user_expiration % 5) != 0 || user_expiration < 5) return 0;
        if (user_expiration == 5) return get_classic_bo_closing_timestamp(user_timestamp, user_expiration);
        else {
            datetime end_timestamp = get_classic_bo_closing_timestamp(user_timestamp, user_expiration);
            if (((end_timestamp / 60) % user_expiration) == 0) return end_timestamp;
            else return end_timestamp - 5*60;
        }
        return 0;
    }
   
    /** \brief Получить процент выплат
     * \param subscribe_key Ключ подписки на процент выплат
     * \return Вернет процент выплат или 0, если данных нет или брокер в данный момент не платит при заданных параметрах
     */
    double get_payout(const string subscribe_key) {
        if (!is_connected) return 0;
        MegaConnectorPayoutSubscription *payout_find = payout_hash.hGet(subscribe_key);
        if (payout_find == NULL) return 0;
        return payout_find.payout;
    };
    
    void clear_open_trades() {
        HashLoop *l = NULL;
        for (l = new HashLoop(open_trades_hash); l.hasNext(); l.next()) {
            string key = l.key();
            open_trades_hash.hDel(key);
        }
        delete l;
    }
    
    void get_open_trades_list(MegaConnectorBoResult &bo_list[]) {
        ArrayResize(bo_list, open_trades_hash.getCount());
        if (open_trades_hash.getCount() == 0) return;
        uint index = open_trades_hash.getCount() - 1;
        HashLoop *l = NULL;
        for (l = new HashLoop(open_trades_hash); l.hasNext(); l.next()) {
            MegaConnectorBoResult *bo = l.val();
            bo_list[index--] = *bo;
        }
        delete l;
    };
    
    void get_close_trades_list(MegaConnectorBoResult &bo_list[]) {
        ArrayResize(bo_list, close_trades_hash.getCount());
        if (close_trades_hash.getCount() == 0) return;
        uint index = close_trades_hash.getCount() - 1;
        HashLoop *l = NULL;
        for (l = new HashLoop(close_trades_hash); l.hasNext(); l.next()) {
            MegaConnectorBoResult *bo = l.val();
            bo_list[index--] = *bo;
        }
        delete l;
    };
    
    /** \brief Close connection with MegaConnector program
    */
    void close() {
        if (is_connected) {
            pipe.close();
            is_connected = false;
            // меняем статусы аккаунтов
            const int len = ArraySize(account_list);
            for (int i = 0; i < len; ++i) {
                account_list[i].is_connected = false;
            }
            on_disconnect();
        }
    };
};

void NamedPipeClient::on_open   (NamedPipeClient *pointer) {};
void NamedPipeClient::on_close  (NamedPipeClient *pointer) {};
void NamedPipeClient::on_message(NamedPipeClient *pointer, const string &message) {};
void NamedPipeClient::on_error  (NamedPipeClient *pointer, const string &error_message) {};

#endif
