#include "Exchange.h"
#include "Trader.h"
#include "FastTrader.h"
#include <vector>
#include <iostream>
#include <memory>
#include "ProfitTracker.h"

int main()
{
    // simulation parameters
    const double TIME_STEP = 0.5;
    const double TRADING_DAY_DURATION = 23400.0; // 6.5 hours
    const double MARKET_OPEN = 0.0;
    const double MARKET_CLOSE = TRADING_DAY_DURATION;

    // securities to trade
    std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT", "AMZN", "META"};

    // create 2 identical stock exchanges
    Exchange exchangeA("EXCH_A");
    Exchange exchangeB("EXCH_B");

    // set trading hours for both exchanges
    exchangeA.setTradingHours(MARKET_OPEN, MARKET_CLOSE);
    exchangeB.setTradingHours(MARKET_OPEN, MARKET_CLOSE);

    // initialise traders with non-zero portfolios
    std::vector<std::unique_ptr<Trader>> traders;
    for (int i = 0; i < 5; ++i)
    {
        auto oms = std::make_shared<OMS>(i, exchangeA, exchangeB);

        // register OMS with exchanges
        exchangeA.registerOMS(i, oms.get());
        exchangeB.registerOMS(i, oms.get());

        // create trader with arbitrary bank balance
        auto trader = std::make_unique<Trader>(i, 100000000.0, oms);

        // initialise with random portfolio of stocks
        trader->initializePortfolio(symbols);

        traders.push_back(std::move(trader));
    }

    // initialise fast trader with non-zero portfolio
    auto fastOms = std::make_shared<OMS>(5, exchangeA, exchangeB);
    exchangeA.registerOMS(5, fastOms.get());
    exchangeB.registerOMS(5, fastOms.get());

    FastTrader fastTrader(5, 5000000.0, fastOms);
    fastTrader.initializePortfolio(symbols);
    ProfitTracker tracker;

    // simulation loop
    for (const auto &sym : symbols)
    {
        std::cout << sym << " ";
    }
    std::cout << "\n\n";

    for (double currentTime = 0.0; currentTime <= TRADING_DAY_DURATION; currentTime += TIME_STEP)
    {
        // regular traders take action at integer times
        if (static_cast<int>(currentTime) == currentTime)
        {
            for (auto &trader : traders)
            {
                if (trader->canTrade())
                {
                    trader->takeAction(currentTime);
                }
            }
        }

        // fast trader takes action at half-second points
        if (static_cast<int>(currentTime * 2) % 2 == 1)
        {
            if (fastTrader.canTrade())
            {
                fastTrader.takeAction(currentTime);
            }
        }

        for (auto &trader : traders)
        {
            double profit = trader->calculateProfitLoss(symbols);
            tracker.record(currentTime, trader->getId(), profit);
        }

        double fastProfit = fastTrader.calculateProfitLoss(symbols);
        tracker.record(currentTime, fastTrader.getId(), fastProfit);
    }

    // end of day cleanup
    // cancel all pending orders
    exchangeA.endOfDayCleanup();
    exchangeB.endOfDayCleanup();
    tracker.exportCSV("profits.csv");

    return 0;
}