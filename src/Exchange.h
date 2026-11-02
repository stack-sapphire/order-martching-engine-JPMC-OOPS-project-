#pragma once
#include "Common.h"
#include <map>
#include <list>

class OMS;

class Exchange
{
public:
    Exchange(std::string name);

    // let an oms register itself with the exchange
    void registerOMS(int traderId, OMS *omsPtr);

    // indicates the last traded price of any security
    double getLastTradedPrice(const std::string &symbol) const;

    // indicates the current best bid and best offer
    std::pair<Quote, Quote> getBestBidOffer(const std::string &symbol) const;

    // indicates the top 5 bids and top 5 offers
    std::pair<std::vector<Quote>, std::vector<Quote>> getOrderBookDepth(const std::string &symbol) const;

    // accept bids or offers
    void acceptOrder(const Order &order);

    // trading hours management
    bool isTradingHours(double currentTime) const;
    void setTradingHours(double openTime, double closeTime);

    // end of day cleanup
    void endOfDayCleanup();

    // check if order exists at a specific price level for a symbol
    bool hasOrderAtPrice(const std::string &symbol, OrderSide side, double price) const;

    std::string getName() const;

private:
    std::string exchangeName;
    double marketOpenTime;
    double marketCloseTime;

    // map to link trader id to oms object
    std::map<int, OMS *> omsRegistry;

    // order matching engine
    // must follow price-time priority for all orders
    // implemented as a map of prices to a list of orders (for time priority)
    struct OrderBook
    {
        std::map<double, std::list<Order>, std::greater<double>> bids; // highest price first for bids
        std::map<double, std::list<Order>, std::less<double>> asks;    // lowest price first for asks
        double lastPrice = 0.0;

        // cached total quantity per price level
        // avoids O(n) loops on every quote read
        std::map<double, int, std::greater<double>> bidQty;
        std::map<double, int, std::less<double>> askQty;
    };

    std::map<std::string, OrderBook> symbolBooks;

    // order matching with price-time priority
    void match(const std::string &symbol);

    // cancel orders outside top 5
    void cancelOrdersOutsideTop5(const std::string &symbol);

    // cancel all pending orders for a symbol
    void cancelAllOrders(const std::string &symbol, CancellationReason reason);

    // notify OMS about trade
    void notifyTradeFulfilled(const Trade &trade);

    // notify OMS about order cancellation
    void notifyOrderCancelled(const OrderCancellation &cancellation);
};