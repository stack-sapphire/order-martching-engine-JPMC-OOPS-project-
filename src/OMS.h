#pragma once
#include "Common.h"
#include "Exchange.h"
#include <map>

class OMS
{
public:
    OMS(int traderId, Exchange &exchA, Exchange &exchB);

    // track the trader's cash in the trading account
    double getCashBalance() const;

    // add and withdraw cash from the trading account
    void depositCash(double amount);
    void withdrawCash(double amount);

    // indicates the current value of the trader's portfolio (cash + stock holdings)
    double getPortfolioValue() const;

    // get holdings for a specific symbol
    int getHoldings(const std::string &symbol) const;

    // get all holdings
    const std::map<std::string, int> &getAllHoldings() const;

    // place buy or sell orders
    // 0 = exchangeA, 1 = exchangeB
    void sendOrder(const Order &order, int exchangeTarget);

    // callback for when exchange matches a trade
    void onOrderFulfilled(const Trade &trade);

    // callback for when exchange cancels an order
    void onOrderCancelled(const OrderCancellation &cancellation);

    // get references to exchanges for querying market data
    Exchange &getExchangeA();
    Exchange &getExchangeB();
    const Exchange &getExchangeA() const;
    const Exchange &getExchangeB() const;

    // initialize starting portfolio
    void initializePortfolio(double cash, const std::map<std::string, int> &initialHoldings);

private:
    int traderId;
    double cashBalance;

    // a map to track stocks
    // key: stock symbol
    // value: quantity of shares held
    std::map<std::string, int> holdings;

    // pending orders sent to exchange, key: orderId
    std::map<long long, Order> pendingOrders;

    // references to the two identical exchanges
    Exchange &exchangeA;
    Exchange &exchangeB;
};
