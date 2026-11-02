#pragma once
#include <string>
#include <vector>
#include <memory>

enum class OrderSide
{
    BUY,
    SELL
};

struct Order
{
    long long orderId;
    int traderId;
    std::string symbol;
    OrderSide side;
    double price;
    int quantity;
    double timestamp;
};

struct Trade
{
    long long orderId;  // ADDED
    std::string symbol;
    double price;
    int quantity;
    double timestamp;
    int buyerId;
    int sellerId;
};

struct Quote
{
    double price;
    int quantity;
};

enum class CancellationReason
{
    OUTSIDE_TOP_5,
    END_OF_DAY,
    OUTSIDE_TRADING_HOURS
};

struct OrderCancellation
{
    Order order;
    CancellationReason reason;
    double timestamp;
};
