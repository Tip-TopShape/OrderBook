#include "../include/FeedHandler.h"

#include "../include/priceLevel.h"

int main(int argc, char *argv[]) {

    auto feed = std::make_unique<FeedHandler>();
    std::cout << "sizeof(Order) = " << sizeof(Order) << "\n";

    auto print_large_trades = [](const databento::Record record, FeedHandler &fh) {
        const auto& trade_msg = record.Get<databento::MboMsg>();

        if (trade_msg.size > 100) {
            std::cout << trade_msg << '\n';


        }
        return databento::KeepGoing::Continue;
    };

    auto reader0 = databento::DbnFileStore("12152025/XNAS-20251222-CLEAR/xnas-itch-20251215.mbo.dbn.zst");
    auto reader = databento::DbnFileStore("12152025/XNAS-20251221-NASDAQ/xnas-itch-20251215.mbo.dbn.zst");

    // Create instrument_id -> symbol mapping
    // std::unordered_map<uint32_t, std::string> id_to_symbol;
    std::vector<std::basic_string<char>> symbols;//(std::move(reader.GetMetadata().mappings));
    for (const auto& [s, _id] : reader.GetMetadata().mappings) {
        auto id = (_id.begin())->symbol;
        id_to_symbol[std::stoi(id)] = s;
        auto loc = std::lower_bound(symbols.begin(), symbols.end(), s);
        symbols.insert(loc, s);
    }

    feed->prep(symbols);

    auto record = reader0.NextRecord();
    int counter = 0;

    auto loopStart = std::chrono::high_resolution_clock::now();

    while (record != nullptr) {
        const auto& trade_msg = record->Get<databento::MboMsg>();
        auto id = trade_msg.hd.instrument_id;
        auto orderId = trade_msg.order_id;

        // variables
        auto price = trade_msg.price;
        auto qty = trade_msg.size;
        auto side = trade_msg.side;
        std::basic_string<char> symbol = id_to_symbol[id];
        auto action = trade_msg.action;
        auto ts_recv = trade_msg.ts_recv;
        auto ts_in_delta = trade_msg.ts_in_delta;

        // actions
        feed->processOrder(orderId, side, symbol, action, price, qty, ts_recv, ts_in_delta);
        record = reader.NextRecord();
        ++counter;

        if (counter % 100000 == 0) {
            std::cout << "\n[Progress: " << counter << " records processed]\n";
            feed->printMetrics();
            feed->resetMetrics();
        }
    }

    auto loopEnd = std::chrono::high_resolution_clock::now();
    auto loopDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart).count();

    std::cout << "\n============ Overall Statistics ============\n";
    std::cout << "Total messages processed: " << counter << "\n";
    std::cout << "Total processing time:    " << loopDurationMs << " ms\n";
    std::cout << "Throughput:               "
              << (loopDurationMs > 0 ? (counter * 1000.0 / loopDurationMs) : 0)
              << " msgs/sec\n";

    feed->printMetrics();

    return 0;
}