#pragma once
#include "OMS.h"
#include <vector>
#include <random>

class Trader
{
public:
    Trader(int id, double initialBankBalance, std::shared_ptr<OMS> oms);
    virtual ~Trader() = default;

    // perform an action
    // called at integer times for regular traders
    virtual void takeAction(double currentTime);

    // initialize trader with random portfolio
    void initializePortfolio(const std::vector<std::string> &symbols);

    // deposit money from bank to trading account if needed
    void depositFromBank(double amount);

    // check if trader can continue trading
    bool canTrade() const;

    // get trader profit/loss at end of day
    double calculateProfitLoss(const std::vector<std::string> &symbols) const;

    int getId() const;

protected:
    int id;
    std::vector<std::string> tradedSymbols;
    // bank account with a fixed amount of balance
    double bankAccountBalance;
    // initial bank balance for profit calculation
    double initialBankBalance;
    // true initial total wealth (bank + cash + stocks at t=0), used as P&L baseline
    double initialTotalWealth;
    // access to order management system
    std::shared_ptr<OMS> oms;
    double initialTotalWealth = 0.0;  

    // random number generator for this trader
    std::mt19937 rng;

    // decide whether to buy or sell
    OrderSide decideBuyOrSell();

    // decide the price at which the buy or sell order is placed
    // randomizes between best bid, best ask, or mid-price
    double calculateOrderPrice(const std::string &symbol, OrderSide side);

    // select which exchange to send order to
    int selectExchange(const std::string &symbol, OrderSide side, double price);

    // get best bid/ask across both exchanges
    Quote getBestBid(const std::string &symbol) const;
    Quote getBestAsk(const std::string &symbol) const;

    // check if trader has sufficient funds/shares for order
    bool canPlaceOrder(const std::string &symbol, OrderSide side, double price, int quantity) const;

    // generate a unique incrementing order id
    static long long generateOrderId();
};
