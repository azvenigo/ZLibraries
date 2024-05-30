#pragma once

namespace MH
{
    // Variadic MAX
    template<typename T>
    T Max(T arg) { return arg; }

    template<typename T, typename... Args>
    T Max(T first, Args... args)
    {
        T maxRest = Max(args...);
        if (first > Max(args...))
            return first;
        return maxRest;
    }

    // Variadic MIN
    template<typename T>
    T Min(T arg) { return arg; }

    template<typename T, typename... Args>
    T Min(T first, Args... args)
    {
        T minRest = Min(args...);
        if (first < Min(args...))
            return first;
        return minRest;
    }

};