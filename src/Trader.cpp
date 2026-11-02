#include "Trader.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <utility>

// Constructor
Trader::Trader(int id, double initialBankBalance, std::shared_ptr<OMS> oms)
    : id(id), bankAccountBalance(initialBankBalance), initialBankBalance(initialBankBalance), oms(std::move(oms))
{
    std::random_device rd;
    rng.seed(rd());
}

// Called every tick
void Trader::takeAction(double currentTime)
{
    for (const auto &symbol : tradedSymbols)
    {
        OrderSide side = decideBuyOrSell();

        // calculate price according to rules
        double price = calculateOrderPrice(symbol, side);
        int quantity = 1000;
        if (side == OrderSide::SELL && oms->getHoldings(symbol) < 1000 && oms->getHoldings(symbol) > 0)
        {
            quantity = oms->getHoldings(symbol);
        }

        // check feasibility and transfer funds if necessary
        if (canPlaceOrder(symbol, side, price, quantity))
        {
            if (side == OrderSide::BUY)
            {
                double cost = price * quantity;
                double currentCash = oms->getCashBalance();
                if (currentCash < cost)
                {
                    double deficit = cost - currentCash;

                    if (bankAccountBalance >= deficit)
                    {
                        depositFromBank(deficit);
                    }
                }
            }

            // recheck after potential deposit
            if (side == OrderSide::BUY)
            {
                if (oms->getCashBalance() < price * quantity)
                    continue;
            }

            int exchangeIndex = selectExchange(symbol, side, price); // 0 = A, 1 = B

            Order order;
            // generate a unique incrementing id
            order.orderId = generateOrderId();
            order.traderId = id;
            order.symbol = symbol;
            order.side = side;
            order.price = price;
            order.quantity = quantity;
            order.timestamp = currentTime;

            oms->sendOrder(order, exchangeIndex);
        }
    }
}

void Trader::initializePortfolio(const std::vector<std::string> &symbols)
{
    tradedSymbols = symbols;
    std::map<std::string, int> initialHoldings;
    // random initial stock
    std::uniform_int_distribution<> qtyDist(10000, 50000);

    for (const auto &sym : symbols)
    {
        initialHoldings[sym] = (qtyDist(rng) / 1000) * 1000;
    }

    // move half of bank balance to trading account initially
    double initialTradingCash = initialBankBalance * 0.5;
    if (bankAccountBalance >= initialTradingCash)
    {
        oms->initializePortfolio(initialTradingCash, initialHoldings);
        bankAccountBalance -= initialTradingCash;
    }
    else
    {
        oms->initializePortfolio(0.0, initialHoldings);
    }

    // record true starting wealth after free stocks are allocated
    // getPortfolioValue() includes both oms cash and stock holdings
    initialTotalWealth = bankAccountBalance + oms->getPortfolioValue();
}

void Trader::depositFromBank(double amount)
{
    if (bankAccountBalance >= amount)
    {
        bankAccountBalance -= amount;
        oms->depositCash(amount);
    }
}

bool Trader::canTrade() const
{
    // if they run out of money or stocks to sell, stop taking any actions
    bool haveStocks = false;
    for (const auto &pair : oms->getAllHoldings())
    {
        if (pair.second > 0)
        {
            haveStocks = true;
            break;
        }
    }

    // total cash
    double totalCash = bankAccountBalance + oms->getCashBalance();

    return (totalCash > 1.0) || haveStocks;
}

double Trader::calculateProfitLoss(const std::vector<std::string> &symbols) const
{
    // getPortfolioValue() already includes oms cash + stock value, so we only add bank balance
    double currentAssets = bankAccountBalance + oms->getPortfolioValue();
    return currentAssets - initialTotalWealth;
}

int Trader::getId() const
{
    return id;
}

OrderSide Trader::decideBuyOrSell()
{
    std::uniform_int_distribution<> dist(0, 1);
    return (dist(rng) == 0) ? OrderSide::BUY : OrderSide::SELL;
}

double Trader::calculateOrderPrice(const std::string &symbol, OrderSide side)
{
    auto [bidA, askA] = oms->getExchangeA().getBestBidOffer(symbol);
    auto [bidB, askB] = oms->getExchangeB().getBestBidOffer(symbol);

    Quote bestBid = {0.0, 0};
    if (bidA.quantity > 0 && bidB.quantity > 0)
        bestBid = (bidA.price >= bidB.price) ? bidA : bidB;
    else if (bidA.quantity > 0) bestBid = bidA;
    else if (bidB.quantity > 0) bestBid = bidB;

    Quote bestAsk = {0.0, 0};
    if (askA.quantity > 0 && askB.quantity > 0)
        bestAsk = (askA.price <= askB.price) ? askA : askB;
    else if (askA.quantity > 0) bestAsk = askA;
    else if (askB.quantity > 0) bestAsk = askB;

    bool hasBid = (bestBid.quantity > 0);
    bool hasAsk = (bestAsk.quantity > 0);

    if (!hasBid && !hasAsk)
    {
        double pA = oms->getExchangeA().getLastTradedPrice(symbol);
        double pB = oms->getExchangeB().getLastTradedPrice(symbol);
        double refPrice = (pA > 0 && pB > 0) ? (pA + pB) / 2.0 : std::max(pA, pB);
        if (refPrice <= 0.0001) refPrice = 10.0;
        std::uniform_int_distribution<> dist(0, 1);
        return (dist(rng) == 0) ? refPrice * 1.05 : refPrice * 0.95;
    }

    std::vector<double> options;
    if (hasBid) options.push_back(bestBid.price);
    if (hasAsk) options.push_back(bestAsk.price);
    if (hasBid && hasAsk) options.push_back((bestBid.price + bestAsk.price) / 2.0);

    std::uniform_int_distribution<> dist(0, (int)options.size() - 1);
    return options[dist(rng)];
}
int Trader::selectExchange(const std::string &symbol, OrderSide side, double price)
{
    bool existsInA = oms->getExchangeA().hasOrderAtPrice(symbol, side, price);
    bool existsInB = oms->getExchangeB().hasOrderAtPrice(symbol, side, price);

    if (existsInA && !existsInB)
        return 1; // Send to Exchange B
    if (existsInB && !existsInA)
        return 0; // Send to Exchange A

    // If exists in both or neither, choice is random
    std::uniform_int_distribution<> dist(0, 1);
    return dist(rng);
}

Quote Trader::getBestBid(const std::string &symbol) const
{
    auto resultA = oms->getExchangeA().getBestBidOffer(symbol);
    auto resultB = oms->getExchangeB().getBestBidOffer(symbol);

    Quote bidA = resultA.first;
    Quote bidB = resultB.first;

    if (bidA.quantity == 0 && bidB.quantity == 0)
        return {0.0, 0};
    if (bidA.quantity == 0)
        return bidB;
    if (bidB.quantity == 0)
        return bidA;

    // highest bid is best
    if (bidA.price > bidB.price)
        return bidA;
    if (bidB.price > bidA.price)
        return bidB;
    // tiebreaker
    return bidA;
}

Quote Trader::getBestAsk(const std::string &symbol) const
{
    auto resultA = oms->getExchangeA().getBestBidOffer(symbol);
    auto resultB = oms->getExchangeB().getBestBidOffer(symbol);

    Quote askA = resultA.second;
    Quote askB = resultB.second;

    if (askA.quantity == 0 && askB.quantity == 0)
        return {0.0, 0};
    if (askA.quantity == 0)
        return askB;
    if (askB.quantity == 0)
        return askA;

    // lowest ask is best
    if (askA.price < askB.price)
        return askA;
    if (askB.price < askA.price)
        return askB;
    // tiebreaker
    return askA;
}

bool Trader::canPlaceOrder(const std::string &symbol, OrderSide side, double price, int quantity) const
{
    if (side == OrderSide::BUY)
    {
        double cost = price * quantity;
        double currentTotal = oms->getCashBalance() + bankAccountBalance;
        return currentTotal >= cost;
    }
    else
    {
        return oms->getHoldings(symbol) >= quantity;
    }
}

long long Trader::generateOrderId()
{
    static long long counter = 0;
    return ++counter;
}
