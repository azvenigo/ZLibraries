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


    template<typename T>
    T aligned_floor(T offset, T alignment)
    {
        return (offset / alignment) * alignment;
    }

    template<typename T>
    T  aligned_next(T offset, T alignment)
    {
        return ((offset + alignment - 1) / alignment) * alignment;
    }


};