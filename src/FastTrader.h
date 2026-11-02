#pragma once
#include "Trader.h"

class FastTrader : public Trader
{
public:
    using Trader::Trader;

    // performs action at half-second intervals
    void takeAction(double currentTime) override;

private:
    // look for arbitrage opportunity and execute if possible
    // buy low at one exchange and sell high at another simultaneously
    void executeArbitrage(const std::string &symbol, double currentTime);
};