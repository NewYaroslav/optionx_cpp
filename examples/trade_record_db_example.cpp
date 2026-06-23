#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include <optionx_cpp/storages.hpp>

namespace {

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

optionx::TradeRecord make_open_record(const optionx::TradeRequest& request, std::int64_t timestamp_ms) {
    optionx::TradeRecord record = optionx::TradeRecord::from_trade(request);
    record.place_date = timestamp_ms - 25;
    record.send_date = timestamp_ms - 10;
    record.open_date = timestamp_ms;
    record.close_date = timestamp_ms + request.duration * 1000;
    record.trade_state = optionx::TradeState::IN_PROGRESS;
    record.live_state = optionx::TradeState::IN_PROGRESS;
    record.option_id = 123456;
    record.option_hash = "broker-order-123456";
    record.open_price = 1.12345;
    record.payout = 0.82;
    record.mm_type = optionx::MmSystemType::MARTINGALE_SIGNAL;
    record.mm_step = 1;
    record.mm_group_id = 1001;
    record.mm_group_hash = "eurusd-demo-martingale";
    record.mm_group_name = "EURUSD demo martingale";
    record.mm_params_json = R"({"step":1,"multiplier":2.0})";
    record.decision_params_json = R"({"signalScore":0.74})";
    record.metadata_json = R"({"example":true})";
    return record;
}

optionx::TradeRecord make_closed_record(optionx::TradeRecord record, std::int64_t close_timestamp_ms) {
    record.close_date = close_timestamp_ms;
    record.close_price = 1.12410;
    record.trade_state = optionx::TradeState::WIN;
    record.live_state = optionx::TradeState::WIN;
    record.profit = record.amount * record.payout;
    record.balance = 1012.30;
    return record;
}

} // namespace

int main() {
    mdbxc::Config config = optionx::storage::TradeRecordDB::default_config();
    config.pathname = "data/examples/trade_record_db";
    config.max_dbs = 4;
    config.no_subdir = false;
    config.relative_to_exe = true;

    optionx::storage::TradeRecordDB db(config);
    if (!db.is_open()) {
        std::cerr << "TradeRecordDB is not open" << std::endl;
        return 1;
    }

    optionx::TradeRequest request;
    request.symbol = "EURUSD";
    request.signal_name = "mean-reversion";
    request.comment = "example trade";
    request.account_id = 7001;
    request.account_type = optionx::AccountType::DEMO;
    request.currency = optionx::CurrencyType::USD;
    request.option_type = optionx::OptionType::CLASSIC;
    request.order_type = optionx::OrderType::BUY;
    request.amount = 15.0;
    request.refund = 0.0;
    request.min_payout = 0.75;
    request.duration = 60;

    if (!db.assign_trade_id(request)) {
        std::cerr << "Failed to reserve trade ID" << std::endl;
        return 1;
    }

    const auto open_timestamp = now_ms();
    auto open_record = make_open_record(request, open_timestamp);

    auto open_write = db.upsert(open_record);
    if (!open_write.ok()) {
        std::cerr << "Open record write failed: " << open_write.message << std::endl;
        return 1;
    }

    std::cout << "Reserved trade ID: " << request.trade_id << std::endl;
    std::cout << "Stored trade_id: " << open_write.record.trade_id << std::endl;

    auto found_by_id = db.find_by_trade_id(request.trade_id);
    if (found_by_id.ok()) {
        std::cout << "Found by trade ID, symbol: " << found_by_id.record.symbol << std::endl;
    }

    auto closed_record = make_closed_record(open_write.record, open_timestamp + 60000);
    auto close_write = db.upsert(closed_record);
    if (!close_write.ok()) {
        std::cerr << "Close record update failed: " << close_write.message << std::endl;
        return 1;
    }

    std::cout << "Updated state to WIN, profit: " << close_write.record.profit << std::endl;

    auto records_at_open_ms = db.find_by_timestamp(open_timestamp);
    std::cout << "Records at open millisecond: " << records_at_open_ms.records.size() << std::endl;

    auto records_in_range = db.find_range(open_timestamp - 1000, open_timestamp + 61000);
    std::cout << "Records in range: " << records_in_range.records.size() << std::endl;

    bool callback_called = false;
    db.enqueue_find_by_trade_id(request.trade_id, [&](optionx::storage::TradeRecordDBReadResult result) {
        callback_called = true;
        if (result.ok()) {
            std::cout << "Buffered read callback, trade_id: " << result.record.trade_id << std::endl;
        }
    });
    db.process();
    std::cout << "Buffered callback delivered: " << (callback_called ? "yes" : "no") << std::endl;

    db.shutdown();
    return 0;
}
