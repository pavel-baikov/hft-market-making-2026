#pragma once

#include "common/BasicTypes.hpp"
#include "common/Csv.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cmf
{

struct Level
{
    Price price = 0.0;
    Quantity quantity = 0.0;
};

struct BookSnapshot
{
    NanoTime timestamp = 0;
    std::vector<Level> asks;
    std::vector<Level> bids;

    Price bestAsk() const
    {
        return asks.empty() ? std::numeric_limits<Price>::quiet_NaN() : asks.front().price;
    }

    Price bestBid() const
    {
        return bids.empty() ? std::numeric_limits<Price>::quiet_NaN() : bids.front().price;
    }

    Price mid() const
    {
        return 0.5 * (bestBid() + bestAsk());
    }

    Price spread() const
    {
        return bestAsk() - bestBid();
    }

    Price microprice() const
    {
        if (asks.empty() || bids.empty())
        {
            return mid();
        }
        const Quantity bidQty = std::max(0.0, bids.front().quantity);
        const Quantity askQty = std::max(0.0, asks.front().quantity);
        const Quantity denom = bidQty + askQty;
        if (denom <= 0.0)
        {
            return mid();
        }
        return (bestAsk() * bidQty + bestBid() * askQty) / denom;
    }
};

struct Order
{
    int id = 0;
    Side side = Side::Buy;
    Price price = 0.0;
    Quantity quantity = 0.0;
    NanoTime placedTs = 0;
    Quantity queueAhead = 0.0;
};

struct Fill
{
    int orderId = 0;
    Side side = Side::Buy;
    Price price = 0.0;
    Quantity quantity = 0.0;
    NanoTime timestamp = 0;
};

struct TradeRecord
{
    NanoTime timestamp = 0;
    Side side = Side::Buy;
    Price price = 0.0;
    Quantity amount = 0.0;
};

struct Metrics
{
    double cash = 0.0;
    double inventory = 0.0;
    double turnover = 0.0;
    double realizedFees = 0.0;
    double lastMid = 0.0;
    double maxAbsInventory = 0.0;
    std::size_t fills = 0;
    std::size_t snapshots = 0;
    std::size_t ordersPlaced = 0;
    std::size_t cancels = 0;

    double markToMarketPnl() const
    {
        return cash + inventory * lastMid;
    }
};

struct EngineConfig
{
    std::string lobPath = "lob.csv";
    std::string tradePath = "";
    std::string reportPath = "reports/performance.md";
    std::size_t levels = 25;
    std::size_t maxEvents = 200000;
    std::size_t warmupEvents = 1000;
    std::size_t quoteIntervalEvents = 25;
    double tickSize = 0.0000001;
    double orderQuantity = 5000.0;
    double feeBps = 0.0;
    double gamma = 0.08;
    double kappa = 50000.0;
    double volatilityWindow = 500.0;
    double horizonSeconds = 60.0;
    double micropriceWeight = 0.65;
    double inventoryLimit = 50000.0;
    bool useMicropriceExtension = true;
};

inline std::string trim(std::string s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

inline bool parseBool(const std::string& value)
{
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

inline EngineConfig loadConfig(const std::string& path)
{
    EngineConfig cfg;
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("cannot open config: " + path);
    }

    std::string line;
    while (std::getline(in, line))
    {
        line = trim(line.substr(0, line.find('#')));
        if (line.empty())
        {
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            throw std::runtime_error("invalid config line: " + line);
        }

        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key == "lob_path")
            cfg.lobPath = value;
        else if (key == "trade_path")
            cfg.tradePath = value;
        else if (key == "report_path")
            cfg.reportPath = value;
        else if (key == "levels")
            cfg.levels = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_events")
            cfg.maxEvents = static_cast<std::size_t>(std::stoull(value));
        else if (key == "warmup_events")
            cfg.warmupEvents = static_cast<std::size_t>(std::stoull(value));
        else if (key == "quote_interval_events")
            cfg.quoteIntervalEvents = static_cast<std::size_t>(std::stoull(value));
        else if (key == "tick_size")
            cfg.tickSize = std::stod(value);
        else if (key == "order_quantity")
            cfg.orderQuantity = std::stod(value);
        else if (key == "fee_bps")
            cfg.feeBps = std::stod(value);
        else if (key == "gamma")
            cfg.gamma = std::stod(value);
        else if (key == "kappa")
            cfg.kappa = std::stod(value);
        else if (key == "volatility_window")
            cfg.volatilityWindow = std::stod(value);
        else if (key == "horizon_seconds")
            cfg.horizonSeconds = std::stod(value);
        else if (key == "microprice_weight")
            cfg.micropriceWeight = std::stod(value);
        else if (key == "inventory_limit")
            cfg.inventoryLimit = std::stod(value);
        else if (key == "use_microprice_extension")
            cfg.useMicropriceExtension = parseBool(value);
        else
            throw std::runtime_error("unknown config key: " + key);
    }
    return cfg;
}

inline void validateConfig(const EngineConfig& cfg)
{
    if (cfg.tickSize <= 0.0)
        throw std::runtime_error("tick_size must be > 0");
    if (cfg.gamma <= 0.0)
        throw std::runtime_error("gamma must be > 0");
    if (cfg.kappa <= 0.0)
        throw std::runtime_error("kappa must be > 0");
    if (cfg.orderQuantity <= 0.0)
        throw std::runtime_error("order_quantity must be > 0");
    if (cfg.inventoryLimit <= 0.0)
        throw std::runtime_error("inventory_limit must be > 0");
    if (cfg.micropriceWeight < 0.0 || cfg.micropriceWeight > 1.0)
        throw std::runtime_error("microprice_weight must be in [0, 1]");
    if (cfg.volatilityWindow < 2.0)
        throw std::runtime_error("volatility_window must be >= 2");
    if (cfg.horizonSeconds <= 0.0)
        throw std::runtime_error("horizon_seconds must be > 0");
    if (cfg.quoteIntervalEvents == 0)
        throw std::runtime_error("quote_interval_events must be > 0");
    if (cfg.levels == 0)
        throw std::runtime_error("levels must be > 0");
    if (2 + cfg.levels * 4 > MAX_CSV_FIELDS)
        throw std::runtime_error("levels exceeds MAX_CSV_FIELDS; increase MAX_CSV_FIELDS");
}

static constexpr std::size_t MAX_OPEN_ORDERS = 8;
static constexpr std::size_t MAX_FILLS_PER_TICK = MAX_OPEN_ORDERS;

inline Quantity computeQueueAhead(const BookSnapshot& book, Side side, Price price)
{
    Quantity total = 0.0;
    if (side == Side::Buy)
    {
        for (const auto& lvl : book.bids)
        {
            if (lvl.price < price - 1e-12)
            {
                break;
            }
            total += lvl.quantity;
        }
    }
    else
    {
        for (const auto& lvl : book.asks)
        {
            if (lvl.price > price + 1e-12)
            {
                break;
            }
            total += lvl.quantity;
        }
    }
    return total;
}

class OrderManager
{
    struct Slot
    {
        Order order{};
        bool used = false;
    };

  public:
    int place(Side side, Price price, Quantity quantity, NanoTime ts, Quantity queueAhead = 0.0)
    {
        for (auto& slot : slots_)
        {
            if (!slot.used)
            {
                slot.order = {++nextId_, side, price, quantity, ts, queueAhead};
                slot.used = true;
                return slot.order.id;
            }
        }
        throw std::runtime_error("OrderManager: slot overflow (increase MAX_OPEN_ORDERS)");
    }

    std::size_t cancelAll()
    {
        std::size_t count = 0;
        for (auto& slot : slots_)
        {
            if (slot.used)
            {
                slot.used = false;
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] std::size_t processTrade(const TradeRecord& trade, Fill* out, std::size_t cap)
    {
        std::size_t n = 0;
        for (auto& slot : slots_)
        {
            if (!slot.used)
            {
                continue;
            }
            auto& order = slot.order;
            const bool relevant =
                (order.side == Side::Buy && trade.side == Side::Sell && trade.price <= order.price) ||
                (order.side == Side::Sell && trade.side == Side::Buy && trade.price >= order.price);
            if (!relevant)
            {
                continue;
            }

            order.queueAhead -= trade.amount;
            if (order.queueAhead > 0.0)
            {
                continue;
            }
            if (n >= cap)
            {
                throw std::runtime_error("processTrade: fill buffer overflow");
            }
            out[n++] = {order.id, order.side, order.price, order.quantity, trade.timestamp};
            slot.used = false;
        }
        return n;
    }

    [[nodiscard]] std::size_t matchCrossingOrders(const BookSnapshot& book,
                                                  Fill* out,
                                                  std::size_t cap)
    {
        std::size_t n = 0;
        for (auto& slot : slots_)
        {
            if (!slot.used)
            {
                continue;
            }

            const auto& order = slot.order;
            const bool crosses = order.side == Side::Buy ? book.bestAsk() <= order.price
                                                         : book.bestBid() >= order.price;
            if (!crosses)
            {
                continue;
            }

            Quantity avail = 0.0;
            if (order.side == Side::Buy)
            {
                for (const auto& lvl : book.asks)
                {
                    if (lvl.price > order.price)
                    {
                        break;
                    }
                    avail += lvl.quantity;
                    if (avail >= order.quantity)
                    {
                        break;
                    }
                }
            }
            else
            {
                for (const auto& lvl : book.bids)
                {
                    if (lvl.price < order.price)
                    {
                        break;
                    }
                    avail += lvl.quantity;
                    if (avail >= order.quantity)
                    {
                        break;
                    }
                }
            }

            if (avail < order.quantity)
            {
                continue;
            }
            if (n >= cap)
            {
                throw std::runtime_error("matchCrossingOrders: fill buffer overflow");
            }
            out[n++] = {order.id, order.side, order.price, order.quantity, book.timestamp};
            slot.used = false;
        }
        return n;
    }

  private:
    int nextId_ = 0;
    std::array<Slot, MAX_OPEN_ORDERS> slots_{};
};

class CsvBookReader
{
    static constexpr std::size_t IO_BUF_BYTES = 1u << 20;

  public:
    explicit CsvBookReader(const std::string& path, std::size_t levels)
        : levels_(levels)
    {
        ioBuf_.resize(IO_BUF_BYTES);
        in_.rdbuf()->pubsetbuf(ioBuf_.data(), static_cast<std::streamsize>(IO_BUF_BYTES));
        in_.open(path);
        if (!in_)
        {
            throw std::runtime_error("cannot open LOB file: " + path);
        }
        line_.reserve(2048);
        std::getline(in_, line_);
    }

    void prepare(BookSnapshot& snap) const
    {
        snap.asks.resize(levels_);
        snap.bids.resize(levels_);
    }

    bool next(BookSnapshot& out)
    {
        if (!std::getline(in_, line_))
        {
            return false;
        }

        if (out.asks.size() != levels_)
        {
            out.asks.resize(levels_);
            out.bids.resize(levels_);
        }

        const auto n = splitCsvLine(std::string_view(line_), fields_);
        const std::size_t needed = 2 + levels_ * 4;
        if (n < needed)
        {
            throw std::runtime_error("LOB row has too few fields");
        }

        out.timestamp = parseI64(fields_[1]);
        for (std::size_t i = 0; i < levels_; ++i)
        {
            const std::size_t base = 2 + i * 4;
            out.asks[i] = {parseDouble(fields_[base]), parseDouble(fields_[base + 1])};
            out.bids[i] = {parseDouble(fields_[base + 2]), parseDouble(fields_[base + 3])};
        }
        return true;
    }

  private:
    std::vector<char> ioBuf_;
    std::ifstream in_;
    std::string line_;
    std::array<std::string_view, MAX_CSV_FIELDS> fields_;
    std::size_t levels_ = 0;
};

class CsvTradeReader
{
    static constexpr std::size_t IO_BUF_BYTES = 1u << 20;

  public:
    explicit CsvTradeReader(const std::string& path)
    {
        ioBuf_.resize(IO_BUF_BYTES);
        in_.rdbuf()->pubsetbuf(ioBuf_.data(), static_cast<std::streamsize>(IO_BUF_BYTES));
        in_.open(path);
        if (!in_)
        {
            throw std::runtime_error("cannot open trade file: " + path);
        }
        line_.reserve(256);
        std::getline(in_, line_);
    }

    bool next(TradeRecord& out)
    {
        if (!std::getline(in_, line_))
        {
            return false;
        }
        const auto n = splitCsvLine(std::string_view(line_), fields_);
        if (n < 5)
        {
            throw std::runtime_error("trade row has too few fields");
        }
        out.timestamp = parseI64(fields_[1]);
        out.side = (fields_[2][0] == 's') ? Side::Sell : Side::Buy;
        out.price = parseDouble(fields_[3]);
        out.amount = parseDouble(fields_[4]);
        return true;
    }

  private:
    std::vector<char> ioBuf_;
    std::ifstream in_;
    std::string line_;
    std::array<std::string_view, MAX_CSV_FIELDS> fields_;
};

inline Price roundToTick(Price price, double tickSize)
{
    return std::round(price / tickSize) * tickSize;
}

inline void applyFill(const Fill& fill, double feeBps, Metrics& metrics)
{
    const double notional = fill.price * fill.quantity;
    const double fee = notional * feeBps / 10000.0;
    if (fill.side == Side::Buy)
    {
        metrics.cash -= notional + fee;
        metrics.inventory += fill.quantity;
    }
    else
    {
        metrics.cash += notional - fee;
        metrics.inventory -= fill.quantity;
    }
    metrics.realizedFees += fee;
    metrics.turnover += notional;
    metrics.maxAbsInventory = std::max(metrics.maxAbsInventory, std::abs(metrics.inventory));
    ++metrics.fills;
}

} // namespace cmf
