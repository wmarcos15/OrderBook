#include "workload.hpp"
#include <cstddef>
#include <random>

constexpr int TYPE_ROLL_MAX = 99;
constexpr int GTC_THRESHOLD = 90;
constexpr int MARKET_THRESHOLD = 95;
constexpr int IOC_THRESHOLD = 99;
constexpr double BUY_PROBABILITY = 0.52;
constexpr Price MID_PRICE = 10000;
constexpr int PRICE_OFFSET = 10;
constexpr Quantity MIN_QUANTITY = 1;
constexpr Quantity MAX_QUANTITY = 100;

OrderSpecs generateWorkload(uint64_t count, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> typeDist(0, TYPE_ROLL_MAX);
    std::uniform_int_distribution<int> priceDist(-PRICE_OFFSET, PRICE_OFFSET);
    std::uniform_int_distribution<Quantity> qtyDist(MIN_QUANTITY, MAX_QUANTITY);
    std::uniform_real_distribution<double> sideDist(0.0, 1.0);

    OrderSpecs orderSpecs;
    orderSpecs.reserve(count);

    for (size_t i {0}; i < count; ++i) {
        int typeRoll = typeDist(rng);
        double sideRoll = sideDist(rng);
        int priceOffset = priceDist(rng);
        Quantity qty = qtyDist(rng);

        Side side = (sideRoll < BUY_PROBABILITY) ? Side::buy : Side::sell;
        bool isMarket = typeRoll >= GTC_THRESHOLD && typeRoll < MARKET_THRESHOLD;

        OrderType type = OrderType::GTC;
        Price price = 0;
        if (!isMarket) {
            price = MID_PRICE + priceOffset;
            if (typeRoll < GTC_THRESHOLD) type = OrderType::GTC;
            else if (typeRoll < IOC_THRESHOLD) type = OrderType::IOC;
            else type = OrderType::FOK;
        }

        orderSpecs.push_back({isMarket, type, price, qty, side});
    }
    return orderSpecs;
}
