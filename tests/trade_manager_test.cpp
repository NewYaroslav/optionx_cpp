#define SQLITE_THREADSAFE 1
#define LOGIT_BASE_PATH "E:\\_repoz\\optionx_cpp"
#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2

#ifndef OPTIONX_TIMESTAMP_MS
#define OPTIONX_TIMESTAMP_MS time_shield::timestamp_ms()
#endif

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include "optionx_cpp/parts/modules.hpp" // Main header including all modules
#include "optionx_cpp/parts/platforms.hpp"

namespace optionx::modules {

    /// \class AuthData
    /// \brief Represents authorization data for the Intrade Bar platform.
    class AuthData : public IAuthData {
    public:

        AuthData() {}

        virtual ~AuthData() = default;

        void to_json(nlohmann::json& j) const override {};

        void from_json(const nlohmann::json &j) override {};

        bool check() const override {
            return true;
        }

        /// \brief Clones the authorization data instance to a unique pointer.
        /// \return Unique pointer to a cloned IAuthData instance.
        std::unique_ptr<IAuthData> clone_unique() const override {
            return std::make_unique<AuthData>(*this);
        }

        /// \brief Clones the authorization data instance to a shared pointer.
        /// \return Shared pointer to a cloned IAuthData instance.
        std::shared_ptr<IAuthData> clone_shared() const override {
            return std::make_shared<AuthData>(*this);
        }

        /// \brief Retrieves the API type associated with this authorization data.
        /// \return The type of API used.
        PlatformType platform_type() const override {
            return PlatformType::SIMULATOR;
        }

        AccountType account_type = AccountType::DEMO;
        CurrencyType currency    = CurrencyType::USD;
    }; // AuthData

        /// \class AccountInfoData
    /// \brief Account information data for Intrade Bar platform.
    class AccountInfoData : public IAccountInfoData {
    public:
        // Public member variables
        int64_t         user_id         = 0;                        ///< User ID
        double          balance         = 0.0;                      ///< Account balance
        CurrencyType    currency        = CurrencyType::UNKNOWN;    ///< Account currency
        AccountType     account_type    = AccountType::UNKNOWN;     ///< Account type (DEMO or REAL)
        bool            connect         = false;                    ///< Connection status
        int64_t         open_trades     = 0;                        ///< Number of open trades for the account

        // --- Limits ----------------------------------------------------------

        double min_usd_amount           = 1.0;                      ///< Minimum trade amount for USD accounts
        double max_usd_amount           = 700.0;                    ///< Maximum trade amount for USD accounts
        double max_usd_limit_amount     = 50.0;                     ///< Maximum trade amount during restricted periods for USD
        double high_payout_usd_amount   = 80.0;                     ///< Trade amount threshold for increased payout percentage in USD
        double min_rub_amount           = 100.0;                    ///< Minimum trade amount for RUB accounts
        double max_rub_amount           = 50000.0;                  ///< Maximum trade amount for RUB accounts
        double max_rub_limit_amount     = 3500.0;                   ///< Maximum trade amount during restricted periods for RUB
        double high_payout_rub_amount   = 5000.0;                   ///< Trade amount threshold for increased payout percentage in RUB

        int64_t min_duration            = 5;                        ///< Minimum binary option duration (in seconds)
        int64_t min_btc_duration        = 5;                        ///< Minimum duration for BTCUSDT (in seconds)
        int64_t max_duration            = 30000;                    ///< Maximum duration (in seconds)
        int64_t max_trades              = 5;                        ///< Maximum number of trades
        int64_t max_limit_trades        = 2;                        ///< Maximum number of trades during restricted periods
        int64_t order_queue_timeout     = 10;                       ///< Timeout for pending orders in the queue
        int64_t responce_timeout        = 10;                       ///< Timeout for server response related to opening or closing a trade
        int64_t order_interval_ms       = 1000;                     ///< Minimum time interval required between consecutive orders

        // Trading time for BTCUSDT
        int64_t start_btc_time          = 0;                        ///< Start of trading time for BTCUSDT
        int64_t end_btc_time            = 86400;                    ///< End of trading time for BTCUSDT
        // Trading time for standard pairs
        int64_t start_time              = 0;                        ///< Start of trading time for standard pairs
        int64_t end_time                = 84600;                    ///< End of trading time for standard pairs

        /// \brief Retrieves the API type associated with this account data.
        /// \return The type of API used.
        const PlatformType platform_type() const override final {
            return PlatformType::SIMULATOR;
        }

        /// \brief Creates a unique pointer to a clone of this account info data instance.
        /// \return Unique pointer to a cloned IAccountInfoData instance.
        std::unique_ptr<IAccountInfoData> clone_unique() const override final {
            return std::make_unique<AccountInfoData>(*this);
        }

        /// \brief Creates a shared pointer to a clone of this account info data instance.
        /// \return Shared pointer to a cloned IAccountInfoData instance.
        std::shared_ptr<IAccountInfoData> clone_shared() const override final {
            return std::make_shared<AccountInfoData>(*this);
        }

    protected:

        /// \brief Retrieves a boolean account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return Boolean account information.
        bool get_account_info_bool(const AccountInfoRequest& request) const override final {
            switch (request.type) {
            case AccountInfoType::CONNECTION_STATUS:
                return connect;
            case AccountInfoType::SYMBOL_AVAILABILITY: {
                static const std::set<std::string> symbols = {
                    "AUDCAD","AUDCHF","AUDJPY",
                    "AUDNZD","AUDUSD","CADJPY",
                    "EURAUD","EURCAD","EURCHF",
                    "EURGBP","EURJPY","EURUSD",
                    "GBPAUD","GBPCHF","GBPJPY",
                    "GBPNZD","NZDJPY","NZDUSD",
                    "USDCAD","USDCHF","USDJPY",
                    "BTCUSDT"
                };
                return (symbols.find(request.symbol) != symbols.end());
            }
            case AccountInfoType::OPTION_TYPE_AVAILABILITY:
                if (request.option_type == OptionType::CLASSIC &&
                    (request.symbol == "BTCUSDT" || request.symbol == "BTCUSD")) return false;
                return (request.option_type == OptionType::CLASSIC || request.option_type == OptionType::SPRINT);
            case AccountInfoType::ORDER_TYPE_AVAILABILITY:
                return (request.order_type == OrderType::BUY || request.order_type == OrderType::SELL);
            case AccountInfoType::ACCOUNT_TYPE_AVAILABILITY:
                return (account_type != AccountType::UNKNOWN && request.account_type == account_type);
            case AccountInfoType::CURRENCY_AVAILABILITY:
                return (currency != CurrencyType::UNKNOWN && request.currency == currency);
            case AccountInfoType::TRADE_LIMIT_NOT_EXCEEDED:
                return (open_trades < max_trades);
            case AccountInfoType::AMOUNT_BELOW_MAX:
                return request.amount <= get_max_amount(request);
            case AccountInfoType::AMOUNT_ABOVE_MIN:
                return request.amount >= get_min_amount(request);
            case AccountInfoType::REFUND_BELOW_MAX:
                return true;
            case AccountInfoType::REFUND_ABOVE_MIN:
                return true;
            case AccountInfoType::DURATION_AVAILABLE: {
                if (request.option_type == OptionType::CLASSIC) return true;
                const int64_t req_min_duration = (request.symbol == "BTCUSD" || request.symbol == "BTCUSDT") ? min_btc_duration : min_duration;
                return (request.duration >= req_min_duration && request.duration <= max_duration);
            }
            case AccountInfoType::EXPIRATION_DATE_AVAILABLE: {
                if (request.option_type == OptionType::SPRINT) return true;
                const int64_t sec_of_day = time_shield::sec_of_day(request.expiry_time);
                if (sec_of_day < start_time || sec_of_day > end_time) return false;
                if (sec_of_day % (5 * time_shield::SEC_PER_MIN) != 0) return false;
                const int64_t sec_close = request.expiry_time - request.timestamp;
                const int64_t min_sec = 300;
                return (sec_close > min_sec);
            }
            case AccountInfoType::PAYOUT_ABOVE_MIN:
                if (request.min_payout == 0.0) return true;
                return (request.min_payout >= get_payout(request));
            case AccountInfoType::AMOUNT_BELOW_BALANCE:
                return (request.amount <= balance);
            default:
                break;
            }
            return false;
        }

        /// \brief Retrieves integer account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return Integer account information.
        int64_t get_account_info_int64(const AccountInfoRequest& request) const override final {
            switch (request.type) {
                case AccountInfoType::USER_ID: return user_id;
                case AccountInfoType::CONNECTION_STATUS: return static_cast<int64_t>(connect);
                case AccountInfoType::BALANCE: return static_cast<int64_t>(balance);
                case AccountInfoType::PLATFORM_TYPE: return static_cast<int64_t>(platform_type());
                case AccountInfoType::ACCOUNT_TYPE: return static_cast<int64_t>(account_type);
                case AccountInfoType::CURRENCY: return static_cast<int64_t>(currency);
                case AccountInfoType::OPEN_TRADES: return open_trades;
                case AccountInfoType::MAX_TRADES:
                    return check_amount_limits(time_shield::sec_of_day(request.timestamp)) ? max_limit_trades : max_trades;
                case AccountInfoType::PAYOUT:
                    return static_cast<int64_t>(get_payout(request) * 100.0);
                case AccountInfoType::MIN_AMOUNT: return static_cast<int64_t>(get_min_amount(request));
                case AccountInfoType::MAX_AMOUNT: return static_cast<int64_t>(get_max_amount(request));
                case AccountInfoType::MIN_DURATION:
                    return (request.symbol == "BTCUSD" || request.symbol == "BTCUSDT") ? min_btc_duration : min_duration;
                case AccountInfoType::MAX_DURATION:
                    return std::min(time_shield::start_of_min(end_time - time_shield::sec_of_day(request.timestamp)), max_duration);
                case AccountInfoType::START_TIME: return time_shield::start_of_day(request.timestamp) + start_time;
                case AccountInfoType::END_TIME: return time_shield::start_of_day(request.timestamp) + end_time;
                case AccountInfoType::ORDER_QUEUE_TIMEOUT: return order_queue_timeout;
                case AccountInfoType::RESPONSE_TIMEOUT: return responce_timeout;
                case AccountInfoType::ORDER_INTERVAL_MS: return order_interval_ms;
                default: break;
            }
            return 0;
        }

        /// \brief Retrieves floating-point account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return Floating-point account information.
        double get_account_info_f64(const AccountInfoRequest& request) const override final {
            switch (request.type) {
                case AccountInfoType::BALANCE: return balance;
                case AccountInfoType::PAYOUT: return get_payout(request);
                case AccountInfoType::MIN_AMOUNT: return get_min_amount(request);
                case AccountInfoType::MAX_AMOUNT: return get_max_amount(request);
                default: break;
            }
            return 0;
        }

        /// \brief Retrieves string account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return String account information.
        std::string get_account_info_str(const AccountInfoRequest& request) const override final {
            switch (request.type) {
                case AccountInfoType::USER_ID: return std::to_string(user_id);
                case AccountInfoType::BALANCE: return utils::format("%.2f", balance);
                default: break;
            }
            return std::string();
        }

        CurrencyType get_account_currency(const AccountInfoRequest& request) const override final {
            return currency;
        }

        AccountType get_account_type(const AccountInfoRequest& request) const override final {
            return account_type;
        }

        double get_min_amount(const AccountInfoRequest& request) const {
            return request.currency == CurrencyType::USD ?
                min_usd_amount : (request.currency == CurrencyType::RUB ?
                min_rub_amount : (currency == CurrencyType::USD ?
                min_usd_amount : (currency == CurrencyType::RUB ? min_rub_amount : 0.0)));
        }

        double get_max_amount(const AccountInfoRequest& request) const {
            if (check_amount_limits(time_shield::sec_of_day(request.timestamp))) {
                return request.currency == CurrencyType::USD ?
                    max_usd_limit_amount : (request.currency == CurrencyType::RUB ?
                    max_rub_limit_amount : (currency == CurrencyType::USD ?
                    max_usd_limit_amount : (currency == CurrencyType::RUB ? max_rub_limit_amount : 0.0)));
            }
            return request.currency == CurrencyType::USD ?
                    max_usd_amount : (request.currency == CurrencyType::RUB ?
                    max_rub_amount : (currency == CurrencyType::USD ?
                    max_usd_amount : (currency == CurrencyType::RUB ? max_rub_amount : 0.0)));
        }

        /// \brief Checks if the amount limits apply based on the time of day.
        /// \param second_day The time in seconds since the start of the day.
        /// \return True if amount limits are in effect, false otherwise.
        bool check_amount_limits(const int second_day) const {
            // Check if the given time of day falls within specified high-risk periods
            return false;
        }

        double get_payout(const AccountInfoRequest& request) const {
            return 0.8;
        }

        int64_t get_classic_bo_expiration(int64_t timestamp, int64_t closing_timestamp) const {
            const int64_t min_exp = 5 * time_shield::SEC_PER_MIN;
            if ((closing_timestamp % min_exp) != 0) return 0;
            const int64_t diff = closing_timestamp - timestamp;
            const int64_t min_diff = 3 * time_shield::SEC_PER_MIN;
            if (diff <= min_diff) return 0;
            return ((((diff - 1) / time_shield::SEC_PER_MIN - 3) / 5) * 5 + 5) * time_shield::SEC_PER_MIN;
        }

        int64_t get_classic_bo_closing_timestamp(int64_t timestamp, int64_t expiration) const {
            if ((expiration % 5) != 0 || expiration < 5) return 0;
            const int64_t timestamp_future = timestamp + (expiration + 3) * time_shield::SEC_PER_MIN;
            return (timestamp_future - timestamp_future % (5 * time_shield::SEC_PER_MIN));
        }
    }; // AccountInfoData

    /// \class TradeManagerTest
    /// \brief Test implementation of BaseTradeExecutionModule for unit testing.
    class TradeManagerTest : public BaseTradeExecutionModule {
    public:
        TradeManagerTest(utils::EventHub& hub, std::shared_ptr<IAccountInfoData> account_info)
            : BaseTradeExecutionModule(hub, std::move(account_info)) {
            subscribe<events::AuthDataEvent>(this);
        }

        /// \brief Overrides API type retrieval for the test.
        /// \return Dummy ApiType for testing.
        PlatformType platform_type() override {
            return PlatformType::SIMULATOR;
        }
    };

    /// \class TradeTestMediator
    /// \brief Mediator module for handling test events.
    class TradeTestMediator : public utils::EventMediator {
    public:
        explicit TradeTestMediator(utils::EventHub& hub) : utils::EventMediator(hub) {
            subscribe<events::TradeStatusEvent>(this);
            subscribe<events::TradeRequestEvent>(this);
            subscribe<events::OpenTradesEvent>(this);
        }

        /// \brief Handles trade status events.
        /// \param event The trade status event.
        void on_event(const std::shared_ptr<utils::Event>& event) override {}

        void on_event(const utils::Event* const event) {
            if (auto status_event = dynamic_cast<const events::TradeStatusEvent*>(event)) {
                LOGIT_0DEBUG();
                auto request = status_event->request;
                auto result = status_event->result;
                m_task_manager.add_delayed_task(1000, [this, request, result](std::shared_ptr<utils::Task>){
                    LOGIT_0DEBUG();
                    result->trade_state = result->live_state = TradeState::WIN;
                    result->close_price = 1.12360;
                });
            } else
            if (auto request_event = dynamic_cast<const events::TradeRequestEvent*>(event)) {
                LOGIT_0DEBUG();
                auto request = request_event->request;
                auto result = request_event->result;
                m_task_manager.add_delayed_task(1000, [this, request, result](std::shared_ptr<utils::Task>){
                    LOGIT_0DEBUG();
                    result->trade_state = result->live_state = TradeState::OPEN_SUCCESS;
                    result->open_price = result->close_price = 1.12335;
                    result->open_date = OPTIONX_TIMESTAMP_MS;
                    result->close_date = result->open_date + time_shield::sec_to_ms(request->duration);
                });
            } else
            if (auto trades_event = dynamic_cast<const events::OpenTradesEvent*>(event)) {
                LOGIT_INFO(trades_event->open_trades);
            }
        }

        void process() {
            m_task_manager.process();
        }

        void shutdown() {
            m_task_manager.shutdown();
        }

    private:
        utils::TaskManager m_task_manager;
    };

    /// \brief Outputs the details of a TradeRequest.
    /// \param request The TradeRequest instance to output.
    void print_trade_request(const TradeRequest& request) {
        LOGIT_STREAM_TRACE()
            << "TradeRequest Details:\n"
            << "---------------------\n"
            << "Symbol: " << request.symbol << "\n"
            << "Signal Name: " << request.signal_name << "\n"
            << "User Data: " << request.user_data << "\n"
            << "Comment: " << request.comment << "\n"
            << "Unique Hash: " << request.unique_hash << "\n"
            << "Unique ID: " << request.unique_id << "\n"
            << "Account ID: " << request.account_id << "\n"
            << "Option Type: " << to_str(request.option_type) << "\n"
            << "Order Type: " << to_str(request.order_type) << "\n"
            << "Account Type: " << to_str(request.account_type) << "\n"
            << "Currency: " << to_str(request.currency) << "\n"
            << "Amount: " << request.amount << "\n"
            << "Refund: " << request.refund << "\n"
            << "Min Payout: " << request.min_payout << "\n"
            << "Duration: " << request.duration << " seconds\n"
            << "Expiry Time: " << request.expiry_time << " (Unix timestamp)\n";
    }

    /// \brief Outputs the details of a TradeResult.
    /// \param result The TradeResult instance to output.
    void print_trade_result(const TradeResult& result) {
        LOGIT_STREAM_TRACE()
            << "TradeResult Details:\n"
            << "--------------------\n"
            << "Error Code: " << to_str(result.error_code) << "\n"
            << "Error Description: " << result.error_desc << "\n"
            << "Option Hash: " << result.option_hash << "\n"
            << "Option ID: " << result.option_id << "\n"
            << "Amount: " << result.amount << "\n"
            << "Payout: " << result.payout << "\n"
            << "Profit: " << result.profit << "\n"
            << "Balance: " << result.balance << "\n"
            << "Open Price: " << result.open_price << "\n"
            << "Close Price: " << result.close_price << "\n"
            << "Delay: " << result.delay << " ms\n"
            << "Ping: " << result.ping << " ms\n"
            << "Place Date: " << result.place_date << " (Unix ms)\n"
            << "Send Date: " << result.send_date << " (Unix ms)\n"
            << "Open Date: " << result.open_date << " (Unix ms)\n"
            << "Close Date: " << result.close_date << " (Unix ms)\n"
            << "State: " << to_str(result.trade_state) << "\n"
            << "Current State: " << to_str(result.live_state) << "\n"
            << "Account Type: " << to_str(result.account_type) << "\n"
            << "Currency: " << to_str(result.currency) << "\n"
            << "API Type: " << to_str(result.platform_type) << "\n";
    }

} // namespace optionx::modules

using namespace optionx;
using namespace optionx::modules;

/// \brief Test fixture for TradeManager tests.
class TradeManagerTestFixture : public ::testing::Test {
protected:
    /// \brief Set up the test environment.
    void SetUp() override {

    }

    /// \brief Tear down the test environment.
    void TearDown() override {

    }
};

/// \brief Tests that a valid trade request is processed successfully.
///
/// The test places a BUY trade for "EURUSD" and then simulates a price update
/// event. The immediate mediator updates the trade state so that the final state
/// is expected to be WIN.
TEST_F(TradeManagerTestFixture, ValidTradeTest) {
    // Initialize the event hub.
    utils::EventHub hub;

    // Create shared account info.
    auto account_info = std::make_shared<AccountInfoData>();
    // Set dummy account info.
    account_info->user_id      = 12345;
    account_info->balance      = 1000.0;
    account_info->currency     = CurrencyType::USD;
    account_info->account_type = AccountType::DEMO;
    account_info->connect      = true;

    // Create the test TradeManager.
    TradeManagerTest trade_manager(hub, account_info);

    // Create the test mediator to handle events
    TradeTestMediator test_mediator(hub);

    // Atomic flag to control the processing loop
    std::atomic<bool> processing{true};

    // Start processing in a separate thread
    std::thread processor([&processing, &hub, &trade_manager, &test_mediator]() {
        while (processing) {
            trade_manager.process();
            test_mediator.process();
            hub.process();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Finalize all trades
        LOGIT_STREAM_TRACE() << "Finalizing all trades...\n";
        trade_manager.shutdown();
        test_mediator.shutdown();
    });

    std::atomic<int>  callback_step{0};

    // Create a valid trade request.
    auto trade_request = std::make_unique<TradeRequest>();
    trade_request->symbol      = "EURUSD";
    trade_request->amount      = 100.0;
    trade_request->option_type = OptionType::SPRINT;
    trade_request->order_type  = OrderType::BUY;
    trade_request->duration    = 10;
    trade_request->add_callback([&callback_step](
            std::unique_ptr<TradeRequest> req,
            std::unique_ptr<TradeResult> res) {
        LOGIT_STREAM_TRACE() << "State: " << to_str(res->trade_state);
        switch (res->trade_state) {
        case TradeState::WAITING_OPEN:
            callback_step = 1;
            break;
        case TradeState::OPEN_SUCCESS:
            if (callback_step == 1) callback_step = 2;
            break;
        case TradeState::IN_PROGRESS:
            if (callback_step == 2) callback_step = 3;
            break;
        case TradeState::WAITING_CLOSE:
            if (callback_step == 3) callback_step = 4;
            break;
        case TradeState::WIN:
            if (callback_step == 4) callback_step = 5;
            break;
        default:
            callback_step = 0;
            break;
        }
    });

    // Place the trade.
    bool placed = trade_manager.place_trade(std::move(trade_request));
    ASSERT_TRUE(placed);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // Simulate a price update event to trigger closing.
    TickData tick;
    // Set tick price data so that mid_price > open_price.
    // For example, for BUY order if mid_price > 1.12335 then trade is WIN.
    tick.tick         = { 1.12350, 1.12340, 0.1, 1695483030000, 1695483031000, 0 };
    tick.symbol       = "EURUSD";
    tick.price_digits = 5;
    tick.flags        = optionx::TickStatusFlags::REALTIME | optionx::TickStatusFlags::INITIALIZED;
    std::vector<TickData> ticks = { tick };

    hub.notify_async(std::make_unique<events::PriceUpdateEvent>(ticks));

    // Wait a short time for the callback to be invoked.
    int wait_iterations = 0;
    while (callback_step != 5 && wait_iterations < 30) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        wait_iterations++;
    }

    // Stop processing and join the thread
    processing = false;
    processor.join();

    EXPECT_EQ(callback_step, 5);

    LOGIT_WAIT();
}

TEST_F(TradeManagerTestFixture, InvalidTradeTest) {
    // Initialize the event hub.
    utils::EventHub hub;

    // Create shared account info.
    auto account_info = std::make_shared<AccountInfoData>();
    // Set dummy account info.
    account_info->user_id      = 12345;
    account_info->balance      = 1000.0;
    account_info->currency     = CurrencyType::USD;
    account_info->account_type = AccountType::DEMO;
    account_info->connect      = true;

    // Create the test TradeManager.
    TradeManagerTest trade_manager(hub, account_info);

    // Create the test mediator to handle events
    TradeTestMediator test_mediator(hub);

    // Atomic flag to control the processing loop
    std::atomic<bool> processing{true};

    // Start processing in a separate thread
    std::thread processor([&processing, &hub, &trade_manager, &test_mediator]() {
        while (processing) {
            trade_manager.process();
            test_mediator.process();
            hub.process();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Finalize all trades
        LOGIT_STREAM_TRACE() << "Finalizing all trades...\n";
        trade_manager.shutdown();
        test_mediator.shutdown();
    });

    std::atomic<int> callback_step{0};
    TradeErrorCode final_error = TradeErrorCode::SUCCESS;

    // Create a valid trade request.
    auto trade_request = std::make_unique<TradeRequest>();
    trade_request->symbol      = "";
    trade_request->amount      = 100.0;
    trade_request->option_type = OptionType::SPRINT;
    trade_request->order_type  = OrderType::BUY;
    trade_request->duration    = 10;
    trade_request->add_callback([&callback_step, &final_error](
            std::unique_ptr<TradeRequest> req,
            std::unique_ptr<TradeResult> res) {
        LOGIT_STREAM_TRACE() << "State: " << to_str(res->trade_state);
        switch (res->trade_state) {
        case TradeState::OPEN_ERROR:
            callback_step = 1;
            final_error = res->error_code;
            break;
        default:
            callback_step = 0;
            break;
        }
    });

    // Place the trade.
    bool placed = trade_manager.place_trade(std::move(trade_request));
    ASSERT_TRUE(placed);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // Simulate a price update event to trigger closing.
    TickData tick;
    // Set tick price data so that mid_price > open_price.
    // For example, for BUY order if mid_price > 1.12335 then trade is WIN.
    tick.tick         = { 1.12350, 1.12340, 0.1, 1695483030000, 1695483031000, 0 };
    tick.symbol       = "EURUSD";
    tick.price_digits = 5;
    tick.flags        = optionx::TickStatusFlags::REALTIME | optionx::TickStatusFlags::INITIALIZED;
    std::vector<TickData> ticks = { tick };

    hub.notify_async(std::make_unique<events::PriceUpdateEvent>(ticks));

    // Wait a short time for the callback to be invoked.
    int wait_iterations = 0;
    while (callback_step != 1 && wait_iterations < 30) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        wait_iterations++;
    }

    // Stop processing and join the thread
    processing = false;
    processor.join();

    EXPECT_EQ(callback_step, 1);
    // Expect that an empty symbol leads to INVALID_SYMBOL error.
    EXPECT_EQ(final_error, TradeErrorCode::INVALID_SYMBOL);

    LOGIT_WAIT();
}

TEST_F(TradeManagerTestFixture, ShutdownTest) {
    // Initialize the event hub.
    utils::EventHub hub;

    // Create shared account info.
    auto account_info = std::make_shared<AccountInfoData>();
    // Set dummy account info.
    account_info->user_id      = 12345;
    account_info->balance      = 1000.0;
    account_info->currency     = CurrencyType::USD;
    account_info->account_type = AccountType::DEMO;
    account_info->connect      = true;

    // Create the test TradeManager.
    TradeManagerTest trade_manager(hub, account_info);

    // Create the test mediator to handle events
    TradeTestMediator test_mediator(hub);

    // Atomic flag to control the processing loop
    std::atomic<bool> processing{true};

    // Start processing in a separate thread
    std::thread processor([&processing, &hub, &trade_manager, &test_mediator]() {
        while (processing) {
            trade_manager.process();
            test_mediator.process();
            hub.process();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Finalize all trades
        LOGIT_STREAM_TRACE() << "Finalizing all trades...\n";
        trade_manager.shutdown();
        test_mediator.shutdown();
    });

    std::atomic<int> callback_step{0};
    TradeErrorCode final_error = TradeErrorCode::SUCCESS;

    // Create a valid trade request.
    auto trade_request = std::make_unique<TradeRequest>();
    trade_request->symbol      = "EURUSD";
    trade_request->amount      = 100.0;
    trade_request->option_type = OptionType::SPRINT;
    trade_request->order_type  = OrderType::BUY;
    trade_request->duration    = 10;
    trade_request->add_callback([&callback_step, &final_error](
            std::unique_ptr<TradeRequest> req,
            std::unique_ptr<TradeResult> res) {
        LOGIT_STREAM_TRACE() << "State: " << to_str(res->trade_state);
        switch (res->trade_state) {
        case TradeState::CHECK_ERROR:
            callback_step = 1;
            final_error = res->error_code;
            break;
        default:
            callback_step = 0;
            break;
        }
    });

    // Place the trade.
    bool placed = trade_manager.place_trade(std::move(trade_request));
    ASSERT_TRUE(placed);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // Simulate a price update event to trigger closing.
    TickData tick;
    // Set tick price data so that mid_price > open_price.
    // For example, for BUY order if mid_price > 1.12335 then trade is WIN.
    tick.tick         = { 1.12350, 1.12340, 0.1, 1695483030000, 1695483031000, 0 };
    tick.symbol       = "EURUSD";
    tick.price_digits = 5;
    tick.flags        = optionx::TickStatusFlags::REALTIME | optionx::TickStatusFlags::INITIALIZED;
    std::vector<TickData> ticks = { tick };

    hub.notify_async(std::make_unique<events::PriceUpdateEvent>(ticks));

    // Wait a short time for the callback to be invoked.
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // Stop processing and join the thread
    processing = false;
    processor.join();

    EXPECT_EQ(callback_step, 1);
    // Expect that an empty symbol leads to INVALID_SYMBOL error.
    EXPECT_EQ(final_error, TradeErrorCode::CLIENT_FORCED_CLOSE);

    LOGIT_WAIT();
}


/// \brief Main entry point for GoogleTest.
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_ADD_FILE_LOGGER_DEFAULT();
    LOGIT_ADD_UNIQUE_FILE_LOGGER_DEFAULT_SINGLE_MODE();
    return RUN_ALL_TESTS();
}
