#include "FastTrader.h"
#include <algorithm>

void FastTrader::takeAction(double currentTime)
{
    for (const auto &symbol : tradedSymbols)
    {
        executeArbitrage(symbol, currentTime);
    }
}

void FastTrader::executeArbitrage(const std::string &symbol, double currentTime)
{
    auto [bidA, askA] = oms->getExchangeA().getBestBidOffer(symbol);
    auto [bidB, askB] = oms->getExchangeB().getBestBidOffer(symbol);

    // Case 1
    // Best bid on Exchange A > Best ask on Exchange B
    // Buy cheap on B, sell high on A
    if (bidA.quantity > 0 && askB.quantity > 0 && bidA.price > askB.price)
    {
        int qty = std::min({1000, bidA.quantity, askB.quantity});
        double cost = askB.price * qty;

        // need cash to buy and shares to sell simultaneously
        if (oms->getCashBalance() < cost)
            return;
        if (oms->getHoldings(symbol) < qty)
            return;

        Order buyOrder;
        buyOrder.orderId = generateOrderId();
        buyOrder.traderId = getId();
        buyOrder.symbol = symbol;
        buyOrder.side = OrderSide::BUY;
        buyOrder.price = askB.price; // buy at the ask on B
        buyOrder.quantity = qty;
        buyOrder.timestamp = currentTime;

        Order sellOrder;
        sellOrder.orderId = generateOrderId();
        sellOrder.traderId = getId();
        sellOrder.symbol = symbol;
        sellOrder.side = OrderSide::SELL;
        sellOrder.price = bidA.price; // sell at the bid on A
        sellOrder.quantity = qty;
        sellOrder.timestamp = currentTime;

        // buy on Exchange B
        oms->sendOrder(buyOrder, 1);
        // sell on Exchange A
        oms->sendOrder(sellOrder, 0);
    }

    // Case 2
    // Best bid on Exchange B > Best ask on Exchange A
    // Buy cheap on A, sell high on B
    else if (bidB.quantity > 0 && askA.quantity > 0 && bidB.price > askA.price)
    {
        int qty = std::min({1000, bidB.quantity, askA.quantity});
        double cost = askA.price * qty;

        if (oms->getCashBalance() < cost)
            return;
        if (oms->getHoldings(symbol) < qty)
            return;

        Order buyOrder;
        buyOrder.orderId = generateOrderId();
        buyOrder.traderId = getId();
        buyOrder.symbol = symbol;
        buyOrder.side = OrderSide::BUY;
        buyOrder.price = askA.price; // buy at the ask on A
        buyOrder.quantity = qty;
        buyOrder.timestamp = currentTime;

        Order sellOrder;
        sellOrder.orderId = generateOrderId();
        sellOrder.traderId = getId();
        sellOrder.symbol = symbol;
        sellOrder.side = OrderSide::SELL;
        sellOrder.price = bidB.price; // sell at the bid on B
        sellOrder.quantity = qty;
        sellOrder.timestamp = currentTime;

        // buy on Exchange A
        oms->sendOrder(buyOrder, 0);
        // sell on Exchange B
        oms->sendOrder(sellOrder, 1);
    }
}
