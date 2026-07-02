//+------------------------------------------------------------------+
//|                                                       common.mqh |
//|                     Copyright 2022-2024, MegaConnector Software. |
//|                                      https://mega-connector.com/ |
//+------------------------------------------------------------------+
//
//+------------------------------------------------------------------+
#ifndef MEGA_CONNECTOR_COMMON_MQH
#define MEGA_CONNECTOR_COMMON_MQH

#property copyright "Copyright 2022-2024, MegaConnector Software."
#property link      "https://t.me/mega_connector"
#property strict

#include "../lib/hash.mqh"

const string MC_BROKER_INTRADE_BAR  = "intrade.bar";
const string MC_BROKER_OLYMP_TRADE  = "olymp-trade";
const string MC_BROKER_TURBO_XBT    = "turbo-xbt";

/// Binary option states
enum MegaConnectorBoStatus {
    MC_BO_UNKNOWN_STATE,        // Uncertain state
    MC_BO_OPENING_ERROR,        // Opening error
    MC_BO_CHECK_ERROR,          // Binary option result check error
    MC_BO_LOW_PAYMENT_ERROR,    // Low payout percentage, binary option canceled
    MC_BO_WAITING_COMPLETION,   // Waiting for the binary option to end
    MC_BO_WIN,                  // Binary option completed successfully
    MC_BO_LOSS,                 // Binary option ended with a loss
    MC_BO_STANDOFF,             // Binary option ended in a draw
    MC_BO_UPDATE,               // Binary option state update
    MC_BO_INCORRECT_PARAMETERS, // Invalid binary option parameters
    MC_BO_AUTHORIZATION_ERROR   // No authorization
};

#define McBoStatus MegaConnectorBoStatus

/** \brief Binary Options Types
 */
enum MegaConnectorBoType {
    MC_BO_SPRINT = 0,   // SPRINT
    MC_BO_CLASSIC = 1,  // CLASSIC
};

#define McBoType MegaConnectorBoType

/** \brief Binary Options Contract Types
 */
enum MegaConnectorBoContractType {
    MC_BO_CONTRACT_UNKNOWN_STATE = 0,
    MC_BO_CONTRACT_BUY = 1,
    MC_BO_CONTRACT_SELL = -1
};

#define McBoContractType MegaConnectorBoContractType

/** \brief Entry Type
 */
enum MegaConnectorEntryType {
    MC_BO_ON_NEW_BAR,           // On the new bar entry
    MC_BO_INTRABAR,             // On the intrabar entry
};

#define McEntryType MegaConnectorEntryType

/** \brief Anchor point of a graphical object on the screen
 */
enum MegaConnectorAnchorPoint {
    MC_TOP_LEFT,                // Top Left
    MC_TOP_RIGHT,               // Top Right
    MC_BOTTOM_LEFT,             // Bottom Left
    MC_BOTTOM_RIGHT             // Bottom Right
};

#define McAnchorPoint MegaConnectorAnchorPoint

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
string get_str_broker_type(const string broker_str) {
    if (broker_str == MC_BROKER_INTRADE_BAR) return "Intrade Bar";
    if (broker_str == MC_BROKER_OLYMP_TRADE) return "Olymp Trade";
    return "undefined";
}

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
string get_str_mc_bo_type(const MegaConnectorBoType _bo_type) {
    static const string data[] = {
        "SPRINT",
        "CLASSIC"
    };
    return data[(int)_bo_type];
}

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
string get_str_mc_contract_type(const MegaConnectorBoContractType bo_contract_type) {
    if (bo_contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_UNKNOWN_STATE) return "UNKNOWN";
    if (bo_contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_BUY) return "BUY";
    if (bo_contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_SELL) return "SELL";
    return "UNKNOWN";
}

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
string get_str_mc_contract_type_v2(const MegaConnectorBoContractType bo_contract_type) {
    if (bo_contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_UNKNOWN_STATE) return "UNKNOWN";
    if (bo_contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_BUY) return "PUT";
    if (bo_contract_type == MegaConnectorBoContractType::MC_BO_CONTRACT_SELL) return "CALL";
    return "UNKNOWN";
}

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
string get_str_mc_bo_staus(const MegaConnectorBoStatus status) {
    static const string data[] = {
        "UNKNOWN STATE",        /**< Неопределенное состояние сделки */
        "OPENING ERROR",        /**< Ошибка открытия сделки */
        "CHECK ERROR",          /**< Ошибка проверки результата сделки */
        "LOW PAYMENT ERROR",    /**< Низкий процент выплат, сделка отменена */
        "WAITING",              /**< Ожидание завершения опциона */
        "WIN",                  /**< Сделка завершена удачно */
        "LOSS",                 /**< Сделка завершена с убытком */
        "STANDOFF",             /**< Сделка завершена в ничью */
        "UPDATE",               /**< Обновление состояния сделки */
        "INCORRECT_PARAMETERS", /**< Неверные параметры опциона */
        "AUTHORIZATION_ERROR"   /**< Нет авторизации */
    };
    return data[(int)status];
}

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
const string get_str_connection_status(const bool value) {
    if (value) return "Connected";
    return "Disconnected";
}

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
const string get_str_account_type(const bool is_demo) {
    if (is_demo) return "DEMO";
    return "REAL";
}

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+

/** \brief Account Information
 */
class MegaConnectorAccountInfo {
private:
    double prev_balance;
    bool is_prev_connected;
public:
    int     account_id;     /**< ID аккаунта */
    string  account_name;   /**< Имя аккаунта */
    string  broker;         /**< Брокер */
    string  user_id;        /**< Уникальный номер юзера */
    string  currency;       /**< Валюта счета */
    double  balance;        /**< Баланс счета */
    bool is_connected;      /**< Состояние соединения с брокером */
    bool is_demo;           /**< Флаг демо счета */

    MegaConnectorAccountInfo() {
        account_id          = 0;
        balance             = 0;
        prev_balance        = 0;
        is_connected        = false;
        is_demo             = false;
        is_prev_connected   = false;
    };

    bool check_balance_change() {
        const bool temp = prev_balance != balance;
        prev_balance = balance;
        return temp;
    }

    bool check_connection_change() {
        const bool temp = is_connected != is_prev_connected;
        is_prev_connected = is_connected;
        return temp;
    }
    
    void update(const MegaConnectorAccountInfo &o) {
        account_name = o.account_name;
        broker = o.broker;
        user_id = o.user_id;
        currency = o.currency;
        balance = o.balance;
        is_connected = o.is_connected;
        is_demo = o.is_demo;
    }

    void reset() {
        prev_balance        = 0;
        is_prev_connected   = false;
    }
};

#define McAccountInfo MegaConnectorAccountInfo

/** \brief Binary option request configuration
 */
class MegaConnectorBoRequest {
public:
    string symbols;             /**< Символ или список символов. Обязательный параметр */
    string brokers;             /**< Брокер или список брокеров. Можно оставить пустым. Указать, если нужно отправить сигнал на конкретных брокеров */
    string accounts;            /**< Список аккаунтов. Можно оставить пустым. Задать, если нужно отправить сигнал на конкретный аккаунт */
    string signal_name;         /**< Имя сигнала */
    string user_data;           /**< Пользовательские данные, которые будут представлены в Callback */

    MegaConnectorBoContractType contract_type;  /**< Направление ставки (BUY/SELL или PUT/CALL). Обязательный параметр */
    MegaConnectorBoType bo_type;                /**< Тип опциона: SPRINT или CLASSIC. Обязательный параметр */

    uint step;                  /**< Шаг СКУ */
    uint max_step;              /**< Максимальный шаг СКУ */

    uint duration;              /**< Длительность опциона для типа SPRINT. Обязательный параметр */
    datetime end_date;          /**< Дата экспирации для типа CLASSIC. Обязательный параметр */

    double rate;
    double amount;              /**< Размер ставки. Указать, если нужен конкретный размер ставки, иначе присвоить 0. Параметр можно опустить, если размер ставки вычисляется ботом */
    double winrate;             /**< Винрейт сигнала. Указать, если нужен автоматический рассчет размера ставки по критерию Келли, иначе присвоить 0. Параметр можно опустить, если размер ставки вычисляется ботом */
    double threshold_payout;    /**< Минимальный размер процентов выплат. Указать, если нужно отсеить сигналы, когда брокер платит меньше заданного уровня */
    double score;               /**< Оценка сигнала */
    uint amount_digits;         /**< Количество разрядов для размера ставки. По умолчанию 2 */

    MegaConnectorBoRequest() {
        contract_type = MegaConnectorBoContractType::MC_BO_CONTRACT_UNKNOWN_STATE;
        bo_type = MegaConnectorBoType::MC_BO_SPRINT;
        step = 0;
        max_step = 0;
        duration = 0;
        end_date = 0;
        amount = 0;
        rate = 0;
        winrate = 0;
        score = 0;
        threshold_payout = 0;
        amount_digits = 2;
    };
};

#define McBoRequest MegaConnectorBoRequest

/** \brief Класс для хранения параметров сделки в отчете о ее состоянии
 */
class MegaConnectorBoResult : public HashValue {
public:
    uint api_id;                    /**< ID сделки внутри API */
    uint broker_id;                 /**< ID сделки у брокера */
    string symbol;                  /**< Символ */
    string broker;                  /**< Брокер */
    string signal_name;             /**< Имя сигнала */
    string signal_id;               /**< ID сигнала */
    string user_data;               /**< Пользовательские данные, которые будут представлены в Callback */
    string currency;                /**< Валюта счета */
    
    uint step;                      /**< Шаг СКУ */
    uint max_step;                  /**< Максимальный шаг СКУ */

    ulong duration;                 /**< Длительность контракта в секундах */
    ulong ping;                     /**< Пинг запроса на открытие ставки */
    ulong send_date;                /**< Метка времени запроса на контракт */
    ulong open_date;                /**< Метка времени начала контракта */
    ulong close_date;               /**< Метка времени конца контракта */
    double amount;                  /**< Размер ставки в RUB или USD */
    double profit;                  /**< Размер выиграша */
    double payout;                  /**< Процент выплат */
    double open_price;              /**< Цена открытия сделки */
    double close_price;             /**< Цена закрытия сделки */
    MegaConnectorBoStatus       status;         /**< Состояние сделки */
    MegaConnectorBoStatus       raw_status;     /**< Предварительное состояние сделки */
    MegaConnectorBoContractType contract_type;  /**< Тип контракта BUY или SELL */
    MegaConnectorBoType         bo_type;        /**< Тип опциона: SPRINT или CLASSIC. Обязательный параметр */

    MegaConnectorBoResult() {
        api_id = 0;
        broker_id = 0;

        contract_type = MegaConnectorBoContractType::MC_BO_CONTRACT_UNKNOWN_STATE;
        bo_type = MegaConnectorBoType::MC_BO_SPRINT;
        
        step = 0;
        max_step = 0;

        duration = 0;
        ping = 0;
        open_date = 0;
        close_date = 0;
        amount = 0;
        profit = 0;
        payout = 0;
        open_price = 0;
        close_price = 0;
        status = MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE;
        raw_status = MegaConnectorBoStatus::MC_BO_UNKNOWN_STATE;
    };
};

#define McBoResult MegaConnectorBoResult

/** \brief Класс для хранения параметров процентов выплат
 */
class MegaConnectorPayoutConfig {
public:
    string subscribe_key;           /**< Ключ подписки. Обязательный параметр! */
    uint   account_id;              /**< ID аккаунта.  */
    string broker;                  /**< Имя брокера. Обязательный параметр! */
    string symbol;                  /**< Имя символа. Обязательный параметр! */
    double amount;                  /**< Размер ставки. Обязательный параметр! */
    datetime end_date;              /**< Дата экспирации для типа CLASSIC. Обязательный параметр! */
    uint duration;                  /**< Длительность контракта в секундах. Обязательный параметр! */
    uint amount_digits;             /**< Количество разрядов для размера ставки. По умолчанию 2 */
    MegaConnectorBoContractType contract_type;  /**< Тип контракта (BUY/SELL). Обязательный параметр! */
    MegaConnectorBoType bo_type;    /**< Тип опциона. Обязательный параметр! */

    MegaConnectorPayoutConfig() {
        account_id = 0;
        amount = 0;
        duration = 0;
        end_date = 0;
        amount_digits = 2;
        contract_type = MegaConnectorBoContractType::MC_BO_CONTRACT_UNKNOWN_STATE;
        bo_type = MegaConnectorBoType::MC_BO_SPRINT;
    };
};

#define McPayoutConfig MegaConnectorPayoutConfig

/** \brief Класс для хранения подписок на процент выплат
 */
class MegaConnectorPayoutSubscription : public HashValue {
public:
    MegaConnectorPayoutConfig config;
    double payout;

    MegaConnectorPayoutSubscription() {
        payout = 0;
    };
};

#define McPayoutSubscription MegaConnectorPayoutSubscription

#endif
//+------------------------------------------------------------------+
