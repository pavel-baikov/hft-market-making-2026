#include "common/Backtester.hpp"
#include "common/Strategy.hpp"

#include "catch2/catch_all.hpp"

using namespace cmf;

namespace
{

BookSnapshot makeBook(double bid, double bidQty, double ask, double askQty, NanoTime ts = 1000)
{
    BookSnapshot book;
    book.timestamp = ts;
    book.bids = {{bid, bidQty}};
    book.asks = {{ask, askQty}};
    return book;
}

} // namespace

TEST_CASE("applyFill updates accounting", "[backtester]")
{
    Metrics metrics;
    applyFill({1, Side::Buy, 0.0100, 5000.0, 1000}, 0.0, metrics);
    REQUIRE(metrics.cash == Catch::Approx(-50.0));
    REQUIRE(metrics.inventory == Catch::Approx(5000.0));
    REQUIRE(metrics.turnover == Catch::Approx(50.0));
    REQUIRE(metrics.fills == 1);

    applyFill({2, Side::Sell, 0.0105, 5000.0, 1001}, 0.0, metrics);
    REQUIRE(metrics.cash == Catch::Approx(2.5));
    REQUIRE(metrics.inventory == Catch::Approx(0.0));
    REQUIRE(metrics.turnover == Catch::Approx(102.5));
    REQUIRE(metrics.fills == 2);
    REQUIRE(metrics.maxAbsInventory == Catch::Approx(5000.0));
}

TEST_CASE("applyFill deducts fees", "[backtester]")
{
    Metrics metrics;
    applyFill({1, Side::Buy, 1.0, 1.0, 1000}, 100.0, metrics);
    REQUIRE(metrics.cash == Catch::Approx(-1.01));
    REQUIRE(metrics.realizedFees == Catch::Approx(0.01));
}

TEST_CASE("crossing orders fill at posted limit price with sufficient depth", "[backtester]")
{
    OrderManager orders;
    orders.place(Side::Buy, 0.0103, 100.0, 1000);
    auto book = makeBook(0.0098, 200.0, 0.0100, 200.0);
    Fill buf[MAX_FILLS_PER_TICK];
    const auto n = orders.matchCrossingOrders(book, buf, MAX_FILLS_PER_TICK);
    REQUIRE(n == 1);
    REQUIRE(buf[0].price == Catch::Approx(0.0103));
}

TEST_CASE("crossing depth check is cumulative", "[backtester]")
{
    OrderManager orders;
    orders.place(Side::Buy, 0.0105, 300.0, 1000);

    BookSnapshot book;
    book.timestamp = 1000;
    book.bids = {{0.0095, 500.0}};
    book.asks = {{0.0100, 100.0}, {0.0103, 200.0}, {0.0107, 500.0}};

    Fill buf[MAX_FILLS_PER_TICK];
    REQUIRE(orders.matchCrossingOrders(book, buf, MAX_FILLS_PER_TICK) == 1);
}

TEST_CASE("crossing depth check skips undersized liquidity", "[backtester]")
{
    OrderManager orders;
    orders.place(Side::Buy, 0.0105, 400.0, 1000);

    BookSnapshot book;
    book.timestamp = 1000;
    book.bids = {{0.0095, 500.0}};
    book.asks = {{0.0100, 100.0}, {0.0103, 200.0}, {0.0107, 500.0}};

    Fill buf[MAX_FILLS_PER_TICK];
    REQUIRE(orders.matchCrossingOrders(book, buf, MAX_FILLS_PER_TICK) == 0);
}

TEST_CASE("microprice follows top-of-book imbalance", "[backtester]")
{
    BookSnapshot book;
    book.bids = {{0.010, 300.0}};
    book.asks = {{0.011, 100.0}};
    REQUIRE(book.microprice() == Catch::Approx(0.01075));
}

TEST_CASE("AS strategy keeps zero-inventory quotes outside the market", "[strategy]")
{
    EngineConfig cfg;
    cfg.useMicropriceExtension = false;
    cfg.micropriceWeight = 0.0;
    cfg.volatilityWindow = 10;

    AvellanedaStoikovStrategy strategy(cfg);
    auto book = makeBook(0.0099, 100.0, 0.0101, 100.0);
    for (int i = 0; i < 20; ++i)
    {
        strategy.observe(book);
    }

    auto quote = strategy.quote(book, 0.0);
    REQUIRE(quote.bid <= book.bestBid());
    REQUIRE(quote.ask >= book.bestAsk());
}

TEST_CASE("rolling volatility evicts old samples", "[strategy]")
{
    RollingVolatility volatility(4);
    for (int i = 0; i < 4; ++i)
    {
        volatility.update(1.0 + 0.001 * (i % 2));
    }
    for (int i = 0; i < 5; ++i)
    {
        volatility.update(1.0);
    }
    REQUIRE(volatility.sigma() == Catch::Approx(0.0).margin(1e-15));
}

TEST_CASE("queue ahead sums visible size at or better than the order price", "[backtester]")
{
    BookSnapshot book;
    book.bids = {{0.0101, 300.0}, {0.0100, 200.0}, {0.0099, 100.0}};
    book.asks = {{0.0102, 100.0}};
    REQUIRE(computeQueueAhead(book, Side::Buy, 0.0100) == Catch::Approx(500.0));

    book.asks = {{0.0100, 150.0}, {0.0101, 250.0}, {0.0102, 400.0}};
    book.bids = {{0.0099, 100.0}};
    REQUIRE(computeQueueAhead(book, Side::Sell, 0.0101) == Catch::Approx(400.0));
}

TEST_CASE("trade-driven queue depletion fills only after queue is exhausted", "[backtester]")
{
    OrderManager orders;
    orders.place(Side::Buy, 0.0103, 100.0, 1000, 300.0);

    Fill buf[MAX_FILLS_PER_TICK];
    REQUIRE(orders.processTrade({1001, Side::Sell, 0.0103, 200.0}, buf, MAX_FILLS_PER_TICK) == 0);

    const auto n =
        orders.processTrade({1002, Side::Sell, 0.0103, 100.0}, buf, MAX_FILLS_PER_TICK);
    REQUIRE(n == 1);
    REQUIRE(buf[0].price == Catch::Approx(0.0103));
    REQUIRE(buf[0].timestamp == 1002);
}

TEST_CASE("roundToTick rounds to nearest tick", "[backtester]")
{
    REQUIRE(roundToTick(0.01034, 0.0001) == Catch::Approx(0.0103));
    REQUIRE(roundToTick(0.01035, 0.0001) == Catch::Approx(0.0104));
}
