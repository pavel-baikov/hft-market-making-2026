#pragma once

#include "common/Backtester.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cmf
{

struct QuotePair
{
    Price bid = 0.0;
    Price ask = 0.0;
    Price reservationPrice = 0.0;
    double halfSpread = 0.0;
    double volatility = 0.0;
};

class RollingVolatility
{
  public:
    explicit RollingVolatility(std::size_t window)
        : window_(std::max<std::size_t>(2, window)),
          buf_(window_, 0.0)
    {
    }

    void update(Price mid)
    {
        if (lastMid_ > 0.0 && mid > 0.0)
        {
            const double r = std::log(mid / lastMid_);
            const double old = buf_[head_];
            buf_[head_] = r;
            head_ = (head_ + 1 == window_) ? 0 : head_ + 1;
            sum_ += r - old;
            sumSq_ += r * r - old * old;
            if (count_ < window_)
            {
                ++count_;
            }
        }
        lastMid_ = mid;
    }

    double sigma() const
    {
        if (count_ < 2)
        {
            return 0.0;
        }
        const double n = static_cast<double>(count_);
        const double mean = sum_ / n;
        const double variance = std::max(0.0, sumSq_ / n - mean * mean);
        return std::sqrt(variance);
    }

  private:
    std::size_t window_;
    std::vector<double> buf_;
    std::size_t head_ = 0;
    std::size_t count_ = 0;
    double sum_ = 0.0;
    double sumSq_ = 0.0;
    Price lastMid_ = 0.0;
};

class AvellanedaStoikovStrategy
{
  public:
    explicit AvellanedaStoikovStrategy(const EngineConfig& cfg)
        : cfg_(cfg),
          volatility_(static_cast<std::size_t>(cfg.volatilityWindow))
    {
    }

    void observe(const BookSnapshot& book)
    {
        volatility_.update(book.mid());
    }

    QuotePair quote(const BookSnapshot& book, Quantity inventory) const
    {
        const double sigma = volatility_.sigma();
        const double sigma2 = sigma * sigma;

        const Price reference = cfg_.useMicropriceExtension
                                    ? (1.0 - cfg_.micropriceWeight) * book.mid() + cfg_.micropriceWeight * book.microprice()
                                    : book.mid();

        // horizon_seconds is used as a forward event count (T in the AS model), not wall-clock time.
        // sigma is per-event log-return std-dev; all terms are dimensionless relative to reference.
        const double horizonEvents = std::max(1.0, cfg_.horizonSeconds);
        const Price reservation = reference * (1.0 - inventory * cfg_.gamma * sigma2 * horizonEvents);
        const double riskSpreadRel = cfg_.gamma * sigma2 * horizonEvents;
        const double liqSpreadRel =
            (2.0 / cfg_.gamma) * std::log(1.0 + cfg_.gamma / std::max(1e-9, cfg_.kappa));
        const double halfSpread =
            std::max(book.spread() * 0.5, 0.5 * (riskSpreadRel + liqSpreadRel) * reference);

        QuotePair q;
        q.reservationPrice = reservation;
        q.halfSpread = halfSpread;
        q.volatility = sigma;
        q.bid = std::min(book.bestBid(), roundToTick(reservation - halfSpread, cfg_.tickSize));
        q.ask = std::max(book.bestAsk(), roundToTick(reservation + halfSpread, cfg_.tickSize));
        return q;
    }

  private:
    EngineConfig cfg_;
    RollingVolatility volatility_;
};

} // namespace cmf
