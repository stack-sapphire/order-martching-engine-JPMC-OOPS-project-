#pragma once
#include <map>
#include <vector>
#include <string>

class ProfitTracker
{
public:
    void record(double time, int traderId, double profit);
    void exportCSV(const std::string &filename) const;

private:
    std::map<double, std::map<int, double>> data;
};