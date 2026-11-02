#include "Exchange.h"
#include "OMS.h"
#include <iostream>
#include <algorithm>

// Constructor
// Set default starting times to zero
Exchange::Exchange(std::string name) : exchangeName(std::move(name)), marketOpenTime(0.0), marketCloseTime(0.0)
{
}

// Connect a specific trader id to an OMS instance
void Exchange::registerOMS(int traderId, OMS *omsPtr)
{
    omsRegistry[traderId] = omsPtr;
}

// Look up a stock symbol and return the price at which the last trade occurred
double Exchange::getLastTradedPrice(const std::string &symbol) const
{
    auto it = symbolBooks.find(symbol);
    if (it != symbolBooks.end())
    {
        return it->second.lastPrice;
    }
    return 0.0;
}

// Return the top of the book
// The best available buy price and sell price for a symbol
std::pair<Quote, Quote> Exchange::getBestBidOffer(const std::string &symbol) const
{
    Quote bestBid = {0.0, 0};
    Quote bestAsk = {0.0, 0};

    auto it = symbolBooks.find(symbol);
    if (it != symbolBooks.end())
    {
        const auto &book = it->second;

        if (!book.bids.empty())
        {
            double price = book.bids.begin()->first;
            bestBid = {price, book.bidQty.at(price)};
        }

        if (!book.asks.empty())
        {
            double price = book.asks.begin()->first;
            bestAsk = {price, book.askQty.at(price)};
        }
    }

    return {bestBid, bestAsk};
}

std::pair<std::vector<Quote>, std::vector<Quote>> Exchange::getOrderBookDepth(const std::string &symbol) const
{
    std::vector<Quote> bidDepth;
    std::vector<Quote> askDepth;

    auto it = symbolBooks.find(symbol);
    if (it != symbolBooks.end())
    {
        const auto &book = it->second;

        // top 5 bids
        int count = 0;
        for (auto bidIt = book.bids.begin(); bidIt != book.bids.end() && count < 5; ++bidIt)
        {
            bidDepth.push_back({bidIt->first, book.bidQty.at(bidIt->first)});
            count++;
        }

        // top 5 asks
        count = 0;
        for (auto askIt = book.asks.begin(); askIt != book.asks.end() && count < 5; ++askIt)
        {
            askDepth.push_back({askIt->first, book.askQty.at(askIt->first)});
            count++;
        }
    }

    return {bidDepth, askDepth};
}

void Exchange::acceptOrder(const Order &order)
{
    // check trading hours
    if (!isTradingHours(order.timestamp))
    {
        OrderCancellation cancellation;
        cancellation.order = order;
        cancellation.reason = CancellationReason::OUTSIDE_TRADING_HOURS;
        cancellation.timestamp = order.timestamp;
        notifyOrderCancelled(cancellation);
        return;
    }

    // add order to the appropriate book
    OrderBook &book = symbolBooks[order.symbol];
    if (order.side == OrderSide::BUY)
    {
        book.bids[order.price].push_back(order);
        book.bidQty[order.price] += order.quantity;
    }
    else
    {
        book.asks[order.price].push_back(order);
        book.askQty[order.price] += order.quantity;
    }

    // attempt to match orders
    match(order.symbol);

    // after matching, ensure we only keep top 5 price levels
    cancelOrdersOutsideTop5(order.symbol);
}

bool Exchange::isTradingHours(double currentTime) const
{
    return currentTime >= marketOpenTime && currentTime <= marketCloseTime;
}

void Exchange::setTradingHours(double openTime, double closeTime)
{
    marketOpenTime = openTime;
    marketCloseTime = closeTime;
}

void Exchange::endOfDayCleanup()
{
    for (auto &pair : symbolBooks)
    {
        cancelAllOrders(pair.first, CancellationReason::END_OF_DAY);
    }
}

bool Exchange::hasOrderAtPrice(const std::string &symbol, OrderSide side, double price) const
{
    auto it = symbolBooks.find(symbol);
    if (it == symbolBooks.end())
    {
        return false;
    }

    const auto &book = it->second;
    if (side == OrderSide::BUY)
    {
        return book.bids.find(price) != book.bids.end();
    }
    else
    {
        return book.asks.find(price) != book.asks.end();
    }
}

std::string Exchange::getName() const
{
    return exchangeName;
}

// Price-Time Priority
void Exchange::match(const std::string &symbol)
{
    auto it = symbolBooks.find(symbol);
    if (it == symbolBooks.end())
        return;

    OrderBook &book = it->second;

    while (!book.bids.empty() && !book.asks.empty())
    {
        // Get the highest bid (bids sorted descending, so begin() = best/highest)
        auto bestBidIt = book.bids.begin();
        // Get the lowest ask (asks sorted ascending, so begin() = best/lowest)
        auto bestAskIt = book.asks.begin();

        double bidPrice = bestBidIt->first;
        double askPrice = bestAskIt->first;

        // check if there is a match (bid price >= ask price)
        if (bidPrice >= askPrice)
        {
            // get the orders at the front of the list (time priority)
            std::list<Order> &bidOrders = bestBidIt->second;
            std::list<Order> &askOrders = bestAskIt->second;

            if (bidOrders.empty() || askOrders.empty())
            {
                // safety check
                if (bidOrders.empty())
                {
                    book.bids.erase(bestBidIt);
                    book.bidQty.erase(bidPrice);
                }
                if (askOrders.empty())
                {
                    book.asks.erase(bestAskIt);
                    book.askQty.erase(askPrice);
                }
                continue;
            }

            Order &bidOrder = bidOrders.front();
            Order &askOrder = askOrders.front();

            // determine execution price based on the older order
            double tradePrice;
            if (bidOrder.timestamp < askOrder.timestamp)
            {
                tradePrice = bidPrice;
            }
            else
            {
                tradePrice = askPrice;
            }

            // determine trade quantity
            int tradeQuantity = std::min(bidOrder.quantity, askOrder.quantity);

           // create trade record
            Trade trade;
            trade.orderId = bidOrder.orderId;  
            trade.symbol = symbol;
            trade.price = tradePrice;
            trade.quantity = tradeQuantity;
            trade.timestamp = std::max(bidOrder.timestamp, askOrder.timestamp); // trade happens at the time of the newer order
            trade.buyerId = bidOrder.traderId;
            trade.sellerId = askOrder.traderId;
            // notify oms
            notifyTradeFulfilled(trade);

            // updates last traded price
            book.lastPrice = tradePrice;

            // decrement cached quantities
            book.bidQty[bidPrice] -= tradeQuantity;
            book.askQty[askPrice] -= tradeQuantity;

            // update quantities or remove filled orders
            bidOrder.quantity -= tradeQuantity;
            askOrder.quantity -= tradeQuantity;

            if (bidOrder.quantity == 0)
            {
                bidOrders.pop_front();
                if (bidOrders.empty())
                {
                    book.bids.erase(bestBidIt);
                    book.bidQty.erase(bidPrice);
                }
            }

            if (askOrder.quantity == 0)
            {
                askOrders.pop_front();
                if (askOrders.empty())
                {
                    book.asks.erase(bestAskIt);
                    book.askQty.erase(askPrice);
                }
            }
            // std::cout << "TRADE executed: " << symbol << " Qty: " << tradeQuantity << " Price: " << tradePrice << " Time: " << trade.timestamp << std::endl;
        }
        else
        {
            // no more matches possible
            break;
        }
    }
}

void Exchange::cancelOrdersOutsideTop5(const std::string &symbol)
{
    auto it = symbolBooks.find(symbol);
    if (it == symbolBooks.end())
        return;

    OrderBook &book = it->second;

    // For bids, keep the 5 highest prices.
    // bids map is sorted descending (std::greater), so begin() = highest (best).
    // To discard the lowest (worst) price levels, remove from prev(end()).
    while (book.bids.size() > 5)
    {
        auto lowestBidIt = std::prev(book.bids.end());
        double price = lowestBidIt->first;

        for (const auto &order : lowestBidIt->second)
        {
            OrderCancellation cancellation;
            cancellation.order = order;
            cancellation.reason = CancellationReason::OUTSIDE_TOP_5;
            cancellation.timestamp = order.timestamp;
            notifyOrderCancelled(cancellation);
        }

        book.bidQty.erase(price);
        book.bids.erase(lowestBidIt);
    }

    // For asks, keep the 5 lowest prices
    // Since map is sorted ascending, the top asks are at the beginning
    // We remove from the end (highest prices)
    while (book.asks.size() > 5)
    {
        auto highestAskIt = std::prev(book.asks.end());
        double price = highestAskIt->first;

        for (const auto &order : highestAskIt->second)
        {
            OrderCancellation cancellation;
            cancellation.order = order;
            cancellation.reason = CancellationReason::OUTSIDE_TOP_5;
            cancellation.timestamp = order.timestamp;
            notifyOrderCancelled(cancellation);
        }

        book.askQty.erase(price);
        book.asks.erase(highestAskIt);
    }
}

void Exchange::cancelAllOrders(const std::string &symbol, CancellationReason reason)
{
    auto it = symbolBooks.find(symbol);
    if (it == symbolBooks.end())
        return;

    OrderBook &book = it->second;

    // cancel all bids
    // at the end of the day
    for (auto &pair : book.bids)
    {
        for (const auto &order : pair.second)
        {
            OrderCancellation cancellation;
            cancellation.order = order;
            cancellation.reason = reason;
            cancellation.timestamp = marketCloseTime;
            notifyOrderCancelled(cancellation);
        }
    }
    book.bids.clear();
    book.bidQty.clear();

    // cancel all asks
    for (auto &pair : book.asks)
    {
        for (const auto &order : pair.second)
        {
            OrderCancellation cancellation;
            cancellation.order = order;
            cancellation.reason = reason;
            cancellation.timestamp = marketCloseTime;
            notifyOrderCancelled(cancellation);
        }
    }
    book.asks.clear();
    book.askQty.clear();
}

void Exchange::notifyTradeFulfilled(const Trade &trade)
{
    // notify buyer
    if (omsRegistry.count(trade.buyerId))
    {
        omsRegistry[trade.buyerId]->onOrderFulfilled(trade);
    }

    // notify seller
    if (omsRegistry.count(trade.sellerId))
    {
        omsRegistry[trade.sellerId]->onOrderFulfilled(trade);
    }
}

void Exchange::notifyOrderCancelled(const OrderCancellation &cancellation)
{
    if (omsRegistry.count(cancellation.order.traderId))
    {
        omsRegistry[cancellation.order.traderId]->onOrderCancelled(cancellation);
    }
}
