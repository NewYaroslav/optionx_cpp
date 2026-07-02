//+------------------------------------------------------------------+
//|                                                  payout-data.mqh |
//|                     Copyright 2022-2023, MegaConnector Software. |
//|                                https://t.me/BinaryOptionsScience |
//+------------------------------------------------------------------+
// 
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_API_MQH
#define MEGA_CONNECTOR_API_MQH

#property copyright "Copyright 2022-2023, MegaConnector Software."
#property link      "https://t.me/mega_connector"
#property strict

#include "../simple-named-pipe-server/named_pipe_client.mqh"
#include "../mega-connector-utility/mega-connector-hash.mqh"
#include "../mega-connector-utility/mega-connector-json.mqh"
#include <Tools\DateTime.mqh>

/// Состояния сделки
enum MegaConnectorBoStatus {
    MC_BO_UNKNOWN_STATE,        /**< Неопределенное состояние сделки */
    MC_BO_OPENING_ERROR,        /**< Ошибка открытия сделки */
    MC_BO_CHECK_ERROR,          /**< Ошибка проверки результата сделки */
    MC_BO_WAITING_COMPLETION,   /**< Ожидание завершения опциона */
    MC_BO_WIN,
    MC_BO_LOSS
};

/** \brief Перечисление типов опционов
 */
enum MegaConnectorBoType {
    MC_BO_SPRINT = 0,
    MC_BO_CLASSIC = 1,
};

/** \brief Типы контрактов бинарных опционов
 */
enum MegaConnectorBoContractType {
    MC_BO_CONTRACT_UNKNOWN_STATE = 0,
    MC_BO_CONTRACT_BUY = 1,
    MC_BO_CONTRACT_SELL = -1
};

/** \brief Информация об аккаунте
 */
class MegaConnectorAccountInfo {
public:
    string account_name;    /**< Имя аккаунта */
    string broker;          /**< Брокер */
    string user_id;         /**< Уникальный номер юзера */
    string currency;        /**< Валюта счета */
    double balance;         /**< Баланс счета */
    bool is_connect;        /**< Состояние соединения с брокером */
    bool is_demo;           /**< Флаг демо счета */
    
    MegaConnectorAccountInfo() {
        balance = 0;
        is_connect = false;
        is_demo = false;
    };
};

/** \brief Конфигурация бинарного опциона для отправки сигнала
 */
class MegaConnectorBoConfig {
public:
    string symbols;             /**< Символ или список символов. Обязательный параметр */
    string brokers;             /**< Брокер или список брокеров. Можно оставить пустым. Указать, если нужно отправить сигнал на конкретных брокеров */
    string accounts;            /**< Список аккаунтов. Можно оставить пустым. Задать, если нужно отправить сигнал на конкретный аккаунт */
    string signal;              /**< Имя сигнала */
    MegaConnectorBoContractType contract_type;  /**< Направление ставки (BUY/SELL или PUT/CALL). Обязательный параметр */
    int duration;               /**< Длительность опциона для типа SPRINT. Обязательный параметр */
    datetime date_expiration;   /**< Дата экспирации для типа CLASSICAL. Обязательный параметр */
    MegaConnectorBoType type;   /**< Тип опциона: SPRINT или CLASSICAL. Обязательный параметр */
    double amount;              /**< Размер ставки. Указать, если нужен конкретный размер ставки, иначе присвоить 0. Параметр можно опустить, если размер ставки вычисляется ботом */
    double winrate;             /**< Винрейт сигнала. Указать, если нужен автоматический рассчет размера ставки по критерию Келли, иначе присвоить 0. Параметр можно опустить, если размер ставки вычисляется ботом */
    double threshold_payout;    /**< Минимальный размер процентов выплат. Указать, если нужно отсеить сигналы, когда брокер платит меньше заданного уровня */

    MegaConnectorBoConfig() {
        contract_type = MC_BO_CONTRACT_UNKNOWN_STATE;
        duration = 0;
        date_expiration = 0;
        type = MC_BO_SPRINT;
        amount = 0;
        winrate = 0;
        threshold_payout = 0;
    };
};
   
/** \brief Класс для хранения параметров сделки в отчете о ее состоянии
 */
class MegaConnectorBo {
   uint api_id;                 /**< ID сделки внутри API */
   uint broker_id;              /**< ID сделки у брокера */
   string symbol;               /**< Символ */
   string broker;               /**< Брокер */
   string singal;               /**< Имя сигнала */
   MegaConnectorBoContractType contract_type;   /**< Тип контракта BUY или SELL */
   ulong duration;              /**< Длительность контракта в секундах */
   ulong ping;                  /**< Пинг запроса на открытие ставки */
   ulong open_time;             /**< Метка времени начала контракта */
   ulong close_time;            /**< Метка времени конца контракта */
   double amount;               /**< Размер ставки в RUB или USD */
   double profit;               /**< Размер выиграша */
   double payout;               /**< Процент выплат */
   double open_price;           /**< Цена открытия сделки */
   double close_price;
   MegaConnectorBoStatus status;    /**< Состояние сделки */
   
   MegaConnectorBo() {
        api_id = 0;
        broker_id = 0;
        contract_type = MC_BO_CONTRACT_UNKNOWN_STATE;
        duration = 0;
        ping = 0;
        open_time = 0;
        close_time = 0;
        amount = 0;
        profit = 0;
        payout = 0;
        open_price = 0;
        close_price = 0;
   };
};


/** \brief Класс для хранения данных о процентах выплаты в зависимости от размера ставки
 */
class MegaConnectorPayoutDurationInfo {
public:
    uint min_duration; 
    uint max_duration;
    double payout_sprint[2];
    double payout_classic[2];
    
    MegaConnectorPayoutDurationInfo() {
        min_duration = 0;
        max_duration = 0;
        payout_sprint[0] = 0;
        payout_sprint[1] = 0;
        payout_classic[0] = 0;
        payout_classic[1] = 0;
    };
    
    bool check(const uint duration) {
        return duration >= min_duration && duration <= max_duration;
    }
    
    double get(
            const MegaConnectorBoContractType contract_type,
            const MegaConnectorBoType bo_type) {
         if (bo_type == MC_BO_SPRINT) {
             if (contract_type == MC_BO_CONTRACT_BUY) {
                return payout_sprint[0];
             } else
             if (contract_type == MC_BO_CONTRACT_SELL) {
                return payout_sprint[1];
             } else {
                return 0;
             }
         } else {
            if (contract_type == MC_BO_CONTRACT_BUY) {
                return payout_classic[0];
             } else
             if (contract_type == MC_BO_CONTRACT_SELL) {
                return payout_classic[1];
             } else {
                return 0;
             }
         }
    }
    
    void set(
            const MegaConnectorBoContractType contract_type,
            const MegaConnectorBoType bo_type,
            const double payout) {
         if (bo_type == MC_BO_SPRINT) {
             if (contract_type == MC_BO_CONTRACT_BUY) {
                payout_sprint[0] = payout;
             } else
             if (contract_type == MC_BO_CONTRACT_SELL) {
                payout_sprint[1] = payout;
             }
         } else {
            if (contract_type == MC_BO_CONTRACT_BUY) {
                payout_classic[0] = payout;
             } else
             if (contract_type == MC_BO_CONTRACT_SELL) {
                payout_classic[1] = payout;
             }
         }
    }
};

/** \brief Класс для хранения данных о процентах выплаты в зависимости от размера ставки
 */
class MegaConnectorPayoutAmountInfo {
public:
    double min_amount; 
    double max_amount;
    MegaConnectorPayoutDurationInfo* data[];
    
    MegaConnectorPayoutAmountInfo() {
        min_amount = 0;
        max_amount = 0;
    };
    
    ~MegaConnectorPayoutAmountInfo() {
        ArrayFree(data);
    };
    
    bool check(const double amount) {
        return amount >= min_amount && amount <= max_amount;
    }
    
    double get(
            const uint duration, 
            const MegaConnectorBoContractType contract_type,
            const MegaConnectorBoType bo_type) {
        const uint data_size = ArraySize(data);
        for (uint i = 0; i < data_size; ++i) {
            if (data[i].check(duration)) return data[i].get(contract_type, bo_type);
        }
        return 0;
    }
    
    void set(
            const uint min_duration, 
            const uint max_duration,
            const MegaConnectorBoContractType contract_type,
            const MegaConnectorBoType bo_type,
            const double payout) {
        const uint data_size = ArraySize(data);
        bool is_del = false;
        bool is_find = false;
        
        for (uint i = 0; i < data_size; ++i) {
            if (data[i].check(min_duration) || data[i].check(max_duration)) {
                if (data[i].min_duration == min_duration && data[i].max_duration == max_duration) {
                    data[i].set(contract_type, bo_type, payout);
                } else {
                    ArrayCopy(data,data,0,0,WHOLE_ARRAY);
                    break;
                }
            }
        }
    }
};

class MegaConnectorPayoutInfo {
public:
    double min_amount, max_amount
    
    
    MegaConnectorPayoutInfo() {};
    
    double get_payout(string symbol) {
        myClass *d = h.hGet(symbol);
    }
    
    void set_payout(string symbol)
};

class MegaConnectorHashPayoutInfo : public HashValue {
    private:
        MegaConnectorPayoutInfo val;
    public:
        MegaConnectorHashPayoutInfo(MegaConnectorPayoutInfo v) { val=v;}
        datetime getVal() { return val; }
};

/** \brief Класс API для работы с брокером
 */
class MegaConnectorApi {
private:
    NamedPipeClient pipe;
    bool is_connected;      // флаг наличия соединения с программой
    int ping_tick;          // тики для подсчета времени отправки ping
    string body;            // тело сообщения
public:

    /** \brief Конструктор класса
     */
    MegaConnectorApi() {
        pipe.set_buffer_size(2048);
        is_connected = false;
        is_broker_connected = false;
        is_broker_prev_connected = false;
        is_rub_currency = false;
        is_demo = false;
        tick = 0;
        balance = 0;
        prev_balance = 0;
    }
   
    /** \brief Деструктор класса
     */
    ~MegaConnectorApi() {
        close();
    }
   
    /** \brief Подключение к программе MegaConnector
     * Данный метод вернет true, если подключение уже есть или было успешно
     * Иначе вернет false. В случае false нужно повторно вызвать этот метод.
     * \return Вернет состояние подключения
     */
    bool connect(string api_pipe_name) {
        if (is_connected) return true;
        return is_connected = pipe.open(api_pipe_name);
    }
   
    /** \brief Состояние подключения
     * \return Вернет состояние подключения
     */
    bool connected() {
        return is_connected;
    }
   
    bool place_bo(MegaConnectorBoConfig &bo_config) {
        if (!is_connected) return false;
    }
   
   /** \brief Открыть сделку
    * \param symbol Имя символа
    * \param direction Направление (BUY или SELL, 1 или -1).
    * \param expiration Экспирация (в минутах)
    * \param type Тип опциона (CLASSICAL или SPRINT)
    * \param amount Размер ставки в валюте счета
    * \return Вернет true в случае успешного отправления
    */
   bool open_deal(string symbol, int direction, datetime expiration, int type, double amount) {
      if(!is_connected || !is_broker_connected) return false;
      if(direction != BUY && direction != SELL) return false;
      if(type != CLASSICAL && type != SPRINT) return false;
      string json_body;
      string str_direction = direction == BUY ? "BUY" : "SELL";
      json_body = StringConcatenate(json_body,"{\"contract\":{\"s\":\"",symbol,"\",\"dir\":\"",str_direction,"\",\"a\":",amount,",");
      if(type == CLASSICAL) {
         if(expiration < 86400) expiration *= 60; // переводим время в секунды
         json_body = StringConcatenate(json_body,"\"exp\":",(long)expiration);
      } else {
         expiration *= 60; // переводим время в секунды
         json_body = StringConcatenate(json_body,"\"dur\":",(long)expiration);
      }
      json_body = StringConcatenate(json_body,"}}");
      if(pipe.write(json_body)) return true;
      close();
      return false;
   }
   
   /** \brief Открыть сделку
    * \param symbol Имя символа
    * \param note Заметка. Она будет возвращена в функции обратного вызова состояния сделки
    * \param direction Направление (BUY или SELL, 1 или -1).
    * \param expiration Экспирация (в минутах)
    * \param type Тип опциона (CLASSICAL или SPRINT)
    * \param amount Размер ставки в валюте счета
    * \return Вернет true в случае успешного отправления
    */
   bool open_deal(string symbol, string note, int direction, datetime expiration, int type, double amount) {
      if(!is_connected || !is_broker_connected) return false;
      if(direction != BUY && direction != SELL) return false;
      if(type != CLASSICAL && type != SPRINT) return false;
      string json_body;
      string str_direction = direction == BUY ? "BUY" : "SELL";
      json_body = StringConcatenate(json_body,"{\"contract\":{\"s\":\"",symbol,"\",\"note\":\"",note,"\",\"dir\":\"",str_direction,"\",\"a\":",amount,",");
      if(type == CLASSICAL) {
         if(expiration < 86400) expiration *= 60; // переводим время в секунды
         json_body = StringConcatenate(json_body,"\"exp\":",(long)expiration);
      } else {
         expiration *= 60; // переводим время в секунды
         json_body = StringConcatenate(json_body,"\"dur\":",(long)expiration);
      }
      json_body = StringConcatenate(json_body,"}}");
      if(pipe.write(json_body)) return true;
      close();
      return false;
   }
   
   /** \brief Получить баланс
    * \return Вернет размер баланса
    */
   double get_balance() {
      return balance;
   }
   
   /** \brief Проверить изменение баланса
    * \return Вернет true, если баланс изменился
    */
   bool check_balance_change() {
      if(prev_balance != balance) {
         prev_balance = balance;
         return true;
      }
      return false;
   }
   
   /** \brief Callback-функция для получения состояния бинарных опционов
    * \param new_bet Структура параметров бинарного опциона
    */
   virtual void on_update_bet(IntradeBarBet &new_bet);
   
   /** \brief Обновить состояние API
    * \param delay Задержка между вызовами функции
    */
   void update(int delay) {
      if(!is_connected) return;
      const int MAX_TICK = 30000;
      tick += delay;
      if(tick > MAX_TICK) {
         tick = 0;
         string json_body = "{\"ping\":1}";
         if(!pipe.write(json_body)) {
            close();
         }
      }
      while(pipe.get_bytes_read() > 0) {
         body += pipe.read();
         
         //Print("body f: ", body);
          
         if(StringGetChar(body, StringLen(body) - 1) != '}') {
            continue;
         }
         
         //Print("body: ", body);
         
         /* парсим json сообщение */
         JSONParser *parser = new JSONParser();
         JSONValue *jv = parser.parse(body);
         body = "";
         if(jv == NULL) {
            Print("error:"+(string)parser.getErrorCode() + parser.getErrorMessage());
         } else {
            if(jv.isObject()) {
               JSONObject *jo = jv;
               
               /* получаем значения состояния сделок */
               JSONObject *update_bet = jo.getObject("update_bet");
               if(update_bet != NULL){
                  IntradeBarBet bet;
                  bet.api_bet_id = update_bet.getInt("aid");
                  bet.broker_bet_id = update_bet.getInt("id");
                  bet.symbol = update_bet.getString("s");
                  bet.note = update_bet.getString("note"); 
                  bet.duration = update_bet.getLong("dur");
                  bet.bet_status = IntradeBarBetStatus::UNKNOWN_STATE;         
                  bet.send_timestamp = update_bet.getLong("st");  
                  bet.opening_timestamp = update_bet.getLong("ot");
                  bet.closing_timestamp = update_bet.getLong("ct");
                  bet.amount = update_bet.getDouble("a");           
                  bet.profit = update_bet.getDouble("profit");         
                  bet.payout = update_bet.getDouble("payout");           
                  bet.open_price = update_bet.getDouble("op");
                  bet.contract_type = 0;  
                  string dir_str = update_bet.getString("dir");
                  if(dir_str == "buy") bet.contract_type = 1;
                  else if(dir_str == "sell") bet.contract_type = -1;
                  //UNKNOWN_STATE
                  //OPENING_ERROR
                  //CHECK_ERROR     
                  //WAITING_COMPLETION
                  //WIN
                  //LOSS
                  
                  string status_str = update_bet.getString("status"); 
                  if(status_str == "win") bet.bet_status = IntradeBarBetStatus::WIN;
                  else if(status_str == "loss") bet.bet_status = IntradeBarBetStatus::LOSS;
                  else if(status_str == "unknown") bet.bet_status = IntradeBarBetStatus::UNKNOWN_STATE;
                  else if(status_str == "error") bet.bet_status = IntradeBarBetStatus::CHECK_ERROR; 
                  else if(status_str == "open_error") bet.bet_status = IntradeBarBetStatus::OPENING_ERROR; 
                  else if(status_str == "wait") bet.bet_status = IntradeBarBetStatus::WAITING_COMPLETION; 
                  
                  on_update_bet(bet);
                  //Print("update bet, amount ",bet.amount, " status ", status_str);
               }
               
               /* получаем баланс */
               double dtemp = 0;
               if(jo.getDouble("b", dtemp)){
                  balance = dtemp;
               }
               
               /* получить флаг рублевого аккаунта */
               int itemp = 0;
               if(jo.getInt("rub", itemp)){
                  is_rub_currency = (itemp == 1);
               }
               
               /* получить флаг демо аккаунта */
               itemp = 0;
               if(jo.getInt("demo", itemp)){
                  is_demo = (itemp == 1);
               }
               
               /* проверяем сообщение ping */
               itemp = 0;
               if(jo.getInt("ping", itemp)){
                  string json_body = "{\"pong\":1}";
                  if(!pipe.write(json_body)) {
                     close();
                  }
                  Print("ping");
               }
               
               /* проверяем состояние соединения */
               if(jo.getInt("conn", itemp)){
                  if(itemp == 1) is_broker_connected = true;
                  else is_broker_connected = false;
               }
            }
            delete jv;
         }
         delete parser;
      }
   }
   
   /** \brief Получить метку времени закрытия CLASSIC бинарного опциона
    * \param timestamp Метка времени (в секундах)
    * \param expiration Экспирация (в минутах)
    * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
    */
   datetime get_classic_bo_closing_timestamp(const datetime user_timestamp, const ulong user_expiration) {
      if((user_expiration % 5) != 0 || user_expiration < 5) return 0;
      const datetime classic_bet_timestamp_future = (datetime)(user_timestamp + (user_expiration + 3) * 60);
      return (classic_bet_timestamp_future - classic_bet_timestamp_future % (5 * 60));
   }
   
   /** \brief Получить метку времени закрытия CLASSIC бинарного опциона на закрытие бара
    * \param timestamp Метка времени (в секундах)
    * \param expiration Экспирация (в минутах)
    * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
    */
   datetime get_candle_bo_closing_timestamp(const datetime user_timestamp, const ulong user_expiration) {
      if((user_expiration % 5) != 0 || user_expiration < 5) return 0;
      if(user_expiration == 5) return get_classic_bo_closing_timestamp(user_timestamp, user_expiration);
      else {
         datetime end_timestamp = get_classic_bo_closing_timestamp(user_timestamp, user_expiration);
         if(((end_timestamp / 60) % user_expiration) == 0) return end_timestamp;
         else return end_timestamp - 5*60;
      }
      return 0;
   }
   
   
    double get_payout(
            const string broker,
            const string symbol,
            const double amount,
            const uint duration,
            const MegaConnectorBoContractType contract_type,
            const MegaConnectorBoType bo_type){
            
    }
   
   /** \brief Получить проценты выплаты брокера
    * \symbol Символ
    * \duration Длительность опциона
    * \param timestamp Метка времени
    * \param amount Размер ставки
    * \return Вернет процент выплаты брокера в виде дробного числа
    */
   double calc_payout(string symbol, const ulong duration, const datetime timestamp, const double amount) {
      string intrade_bar_currency_pairs[21] = {
           "EURUSD","USDJPY","USDCHF",
           "USDCAD","EURJPY","AUDUSD",
           "NZDUSD","EURGBP","EURCHF",
           "AUDJPY","GBPJPY","EURCAD",
           "AUDCAD","CADJPY","NZDJPY",
           "AUDNZD","GBPAUD","EURAUD",
           "GBPCHF","AUDCHF","GBPNZD"
      };
      bool is_found = false;
      for(int i = 0; i < 21; ++i) {
         if(symbol == intrade_bar_currency_pairs[i]) {
            is_found = true;
            break;
         }
      }
      if(!is_found) return 0.0;
      if(duration == 120) return 0.0;
      const ulong max_duration = 3600*24*360*10;
      if(duration > 30000 && duration < max_duration) return 0.0;
      if((!is_rub_currency && amount < 1)|| (is_rub_currency && amount < 50)) return 0.0;
      CDateTime date_time;
      date_time.DateTime(timestamp);
      
      /* пропускаем выходные дни */
      if(date_time.day_of_week == 0 || date_time.day_of_week == 6) return 0.0;
      /* пропускаем 0 час */
      if(date_time.hour == 0) return 0.0;
      if(date_time.hour >= 21) return 0.0;
      /* с 4 часа по МСК до 9 утра по МСК процент выполат 60%
       * с 17 часов по МСК  процент выплат в течении 3 минут в начале часа и конце часа также составляет 60%
       */
      if(date_time.hour <= 6 || date_time.hour >= 14) {
          /* с 4 часа по МСК до 9 утра по МСК процент выполат 60%
           * с 17 часов по МСК  процент выплат в течении 3 минут в начале часа и конце часа также составляет 60%
           */
          if(date_time.min >= 57 || date_time.min <= 2) {
              return 0.6;
          }
      }
      if(date_time.hour == 13 && date_time.min >= 57) {
          return 0.6;
      }
      /* Если счет в долларах и ставка больше 80 долларов или счет в рублях и ставка больше 5000 рублей */
      if((!is_rub_currency && amount >= 80)||
          (is_rub_currency && amount >= 5000)) {
          if(duration == 60) return 0.63;
          return 0.85;
      } else {
          /* Если счет в долларах и ставка меньше 80 долларов или счет в рублях и ставка меньше 5000 рублей */
          if(duration == 60) {
              /* Если продолжительность экспирации 3 минуты
               * Процент выплат составит 82 (0,82)
               */
              return 0.6;
          } else
          if(duration == 180) {
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
   
   /** \brief Проверить на демо аккаунт
    * \return Вернет true если демо аккаунт
    */
   bool check_demo() {
      return is_demo;
   }
   
   /** \brief Проверить на рублевый аккаунт
    * \return Вернет true если аккаунт рублевый
    */
   bool check_rub() {
      return is_rub_currency;
   }
   
   /** \brief Закрыть соединение
    */
   void close() {
      if(is_connected) pipe.close();
      is_connected = false;
      is_broker_connected = false;
      is_broker_prev_connected = false;
      is_rub_currency = false;
      is_demo = false;
      tick = 0;
      balance = 0;
      prev_balance = 0;
   }
};

