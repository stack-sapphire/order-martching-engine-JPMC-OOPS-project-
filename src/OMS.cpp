
#include "OMS.h"
#include <algorithm>
#include <iostream>

// Constructor
OMS::OMS(int traderId, Exchange &exchA, Exchange &exchB)
    : traderId(traderId), cashBalance(0.0), exchangeA(exchA), exchangeB(exchB)
{
}

double OMS::getCashBalance() const
{
    return cashBalance;
}

void OMS::depositCash(double amount)
{
    cashBalance += amount;
}

void OMS::withdrawCash(double amount)
{
    cashBalance -= amount;
}

// Calculate the total net worth of the trader
double OMS::getPortfolioValue() const
{
    double stockValue = 0.0;
    for (const auto &pair : holdings)
    {
        const std::string &sym = pair.first;
        int qty = pair.second;

        if (qty <= 0)
            continue;

        // use best available last traded price across both exchanges
        double priceA = exchangeA.getLastTradedPrice(sym);
        double priceB = exchangeB.getLastTradedPrice(sym);

        double refPrice = 10.0; // Fallback default

        // Use average if both have trades, otherwise use the one that has a price
        if (priceA > 0.0 && priceB > 0.0)
        {
            refPrice = (priceA + priceB) / 2.0;
        }
        else if (priceA > 0.0)
        {
            refPrice = priceA;
        }
        else if (priceB > 0.0)
        {
            refPrice = priceB;
        }

        stockValue += refPrice * qty;
    }
    return cashBalance + stockValue;
}

int OMS::getHoldings(const std::string &symbol) const
{
    auto it = holdings.find(symbol);
    if (it != holdings.end())
        return it->second;
    return 0;
}

const std::map<std::string, int> &OMS::getAllHoldings() const
{
    return holdings;
}

void OMS::sendOrder(const Order &order, int exchangeTarget)
{
    // track pending order
    pendingOrders[order.orderId] = order;

    if (exchangeTarget == 0)
        exchangeA.acceptOrder(order);
    else
        exchangeB.acceptOrder(order);
}

// Update the books after a trade happens
void OMS::onOrderFulfilled(const Trade &trade)
{
    double tradeValue = trade.price * trade.quantity;

    if (trade.buyerId == traderId)
    {
        // we bought: deduct cash, add shares
        cashBalance -= tradeValue;
        holdings[trade.symbol] += trade.quantity;
        // guard against floating point drift
        if (cashBalance < 0.0)
            cashBalance = 0.0;
    }
    else if (trade.sellerId == traderId)
    {
        // we sold: add cash, deduct shares
        cashBalance += tradeValue;
        holdings[trade.symbol] -= trade.quantity;
        // guard against over-selling
        if (holdings[trade.symbol] < 0)
            holdings[trade.symbol] = 0;
    }

    // ADDED — remove fulfilled order from pending to prevent memory bloat
    pendingOrders.erase(trade.orderId);
}

void OMS::onOrderCancelled(const OrderCancellation &cancellation)
{
    // remove from pending orders tracking
    pendingOrders.erase(cancellation.order.orderId);
}

Exchange &OMS::getExchangeA()
{
    return exchangeA;
}

Exchange &OMS::getExchangeB()
{
    return exchangeB;
}

const Exchange &OMS::getExchangeA() const
{
    return exchangeA;
}

const Exchange &OMS::getExchangeB() const
{
    return exchangeB;
}

void OMS::initializePortfolio(double cash, const std::map<std::string, int> &initialHoldings)
{
    cashBalance = cash;
    holdings = initialHoldings;
}
