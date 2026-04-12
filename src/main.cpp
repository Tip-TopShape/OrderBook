#include "../include/FeedHandler.h"
#include <memory>
#include <filesystem>
#include <ctime>

static constexpr int64_t DATABENTO_TO_CENTS = 10'000'000;
static const std::string REF_PRICES_FILE = "ref_prices.bin";
static constexpr int64_t TICK_SIZE = 10;
static constexpr int64_t HALF_RANGE = 5000;
static constexpr int REPORT_INTERVAL = 1000000;

std::string timestamp() {
    auto t = std::time(nullptr);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));
    return buf;
}

void saveRefPrices(const std::unordered_map<uint32_t, int64_t>& prices, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    uint32_t count = prices.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& [id, price] : prices) {
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        out.write(reinterpret_cast<const char*>(&price), sizeof(price));
    }
}

bool loadRefPrices(std::unordered_map<uint32_t, int64_t>& prices, const std::string& path) {
    if (!std::filesystem::exists(path)) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    uint32_t count;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t id;
        int64_t price;
        in.read(reinterpret_cast<char*>(&id), sizeof(id));
        in.read(reinterpret_cast<char*>(&price), sizeof(price));
        prices[id] = price;
    }
    return true;
}

int main(int argc, char *argv[]) {
    bool csv_mode = false;
    int64_t max_records = INT64_MAX;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--csv") {
            csv_mode = true;
        } else if (arg.substr(0, 2) == "--" && arg.size() > 2) {
            max_records = std::stoll(arg.substr(2)) * 1'000'000;
        }
    }

    auto feed = std::make_unique<FeedHandler>();

    auto reader0 = databento::DbnFileStore("12152025/XNAS-20251222-CLEAR/xnas-itch-20251215.mbo.dbn.zst");
    auto reader = databento::DbnFileStore("12152025/XNAS-20251221-NASDAQ/xnas-itch-20251215.mbo.dbn.zst");

    const auto& metadata = reader.GetMetadata();
    for (const auto& mapping : metadata.mappings) {
        if (mapping.intervals.empty()) continue;
        uint32_t id = std::stoul(mapping.intervals[0].symbol);
        feed->registerSymbol(id, mapping.raw_symbol);
    }

    std::unordered_map<uint32_t, int64_t> ref_prices;
    if (!loadRefPrices(ref_prices, REF_PRICES_FILE)) {
        auto prepass_reader = databento::DbnFileStore("12152025/XNAS-20251221-NASDAQ/xnas-itch-20251215.mbo.dbn.zst");
        auto record = prepass_reader.NextRecord();
        while (record != nullptr) {
            if (record->RType() == databento::RType::Mbo) {
                const auto& msg = record->Get<databento::MboMsg>();
                uint32_t id = msg.hd.instrument_id;
                if (ref_prices.find(id) == ref_prices.end() && msg.price > 0) {
                    ref_prices[id] = msg.price / DATABENTO_TO_CENTS;
                }
            }
            record = prepass_reader.NextRecord();
        }
        saveRefPrices(ref_prices, REF_PRICES_FILE);
    }

    feed->reserveBooks(metadata.mappings.size());
    for (const auto& mapping : metadata.mappings) {
        if (mapping.intervals.empty()) continue;
        uint32_t id = std::stoul(mapping.intervals[0].symbol);
        int64_t ref_price = 10000;
        auto it = ref_prices.find(id);
        if (it != ref_prices.end()) ref_price = it->second;
        int64_t low = std::max(int64_t{0}, ref_price - HALF_RANGE);
        int64_t high = ref_price + HALF_RANGE;
        feed->prep(mapping.raw_symbol, low, high, TICK_SIZE);
    }
    feed->finalizePrep();

    std::ofstream csv_file;
    if (csv_mode) {
        std::filesystem::create_directories("runs");
        std::string csv_path = "runs/" + timestamp() + ".csv";
        csv_file.open(csv_path);
        MatchingMetrics::printCSVHeader(csv_file);
        std::cout << "Saving to " << csv_path << "\n";
    }

    std::cout << "records   throughput |   p50      p99    p99.9  |  memory\n";
    std::cout << "----------|----------|--------------------------|--------\n";

    auto record = reader0.NextRecord();
    int64_t counter = 0;
    feed->getMetrics().startWindow();

    while (record != nullptr && counter < max_records) {
        if (record->RType() != databento::RType::Mbo) {
            record = reader.NextRecord();
            continue;
        }

        const auto& msg = record->Get<databento::MboMsg>();
        auto orderId = msg.order_id;
        int64_t price = msg.price / DATABENTO_TO_CENTS;
        auto qty = msg.size;
        auto side = msg.side;
        auto action = msg.action;
        auto ts_recv = msg.ts_recv;
        auto ts_in_delta = msg.ts_in_delta;

        feed->processOrder(orderId, side, msg.hd.instrument_id, action, price, qty, ts_recv, ts_in_delta);
        record = reader.NextRecord();
        ++counter;

        if (counter % REPORT_INTERVAL == 0) {
            feed->printMetrics(std::cout, counter);
            if (csv_mode) {
                feed->printMetricsCSV(csv_file, counter);
            }
            feed->resetMetrics();
            feed->getMetrics().startWindow();
        }
    }

    if (counter % REPORT_INTERVAL != 0) {
        feed->printMetrics(std::cout, counter);
        if (csv_mode) {
            feed->printMetricsCSV(csv_file, counter);
        }
    }

    std::cout << "----------|----------|--------------------------|--------\n";
    std::cout << "Done. " << counter << " records processed.\n";

    return 0;
}