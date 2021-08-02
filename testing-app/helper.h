#pragma once

#include <iomanip>
#include <chrono>

std::string log_time()
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    struct tm timeinfo;
    localtime_s(&timeinfo, &now);

    char buf[100] = { 0 };
    std::strftime(buf, sizeof(buf), "%T", &timeinfo);
    return buf;
}
