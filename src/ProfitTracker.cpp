#include "ProfitTracker.h"
#include <fstream>
#include <set>

// Save a snapshot of the trader's profit at a given time
void ProfitTracker::record(double time, int traderId, double profit)
{
    data[time][traderId] = profit;
}

void ProfitTracker::exportCSV(const std::string &filename) const
{
    std::ofstream file(filename);

    std::set<int> traders;

    for (const auto &t : data)
    {
        for (const auto &p : t.second)
            traders.insert(p.first);
    }

    file << "time";
    for (const auto &trader : traders)
        file << "," << trader;
    file << "\n";

    for (const auto &t : data)
    {
        file << t.first;

        for (const auto &trader : traders)
        {
            auto it = t.second.find(trader);

            if (it != t.second.end())
                file << "," << it->second;
            else
                file << ",0";
        }

        file << "\n";
    }

    file.close();
}