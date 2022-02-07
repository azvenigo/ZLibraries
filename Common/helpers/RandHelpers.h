#pragma once

#include <random>
#include <chrono>

#pragma once

#include <random>
#include <chrono>

//#define THREAD_SAFE_RAND
#ifdef THREAD_SAFE_RAND
#include <mutex>

static std::default_random_engine               gRandGenerator((unsigned int)std::chrono::system_clock::now().time_since_epoch().count()); // seed based on current time

static std::uniform_int_distribution<uint64_t>   gUniformUint64;
static std::uniform_int_distribution<int64_t>    gUniformInt64;
static std::uniform_real_distribution<double>    gUniformDouble;

static std::exponential_distribution<double>     gExponentialDouble;

static std::mutex gRandGeneratorMutex;



inline int64_t getRandI64(int64_t minrand, int64_t maxrand) { const std::lock_guard<std::mutex> lock(gRandGeneratorMutex); int64_t n = minrand + (gUniformInt64(gRandGenerator) % ((maxrand - minrand)));   assert(n >= minrand && n <= maxrand); return n; }
inline uint64_t getRandU64(uint64_t minrand, uint64_t maxrand) { const std::lock_guard<std::mutex> lock(gRandGeneratorMutex); uint64_t n = minrand + (gUniformUint64(gRandGenerator) % ((maxrand - minrand))); assert(n >= minrand && n <= maxrand); return n; }
inline bool getRandBool() { const std::lock_guard<std::mutex> lock(gRandGeneratorMutex); bool b = gUniformUint64(gRandGenerator) % 2 == 0; return b; }
inline double getRandDouble(double minrand, double maxrand) { const std::lock_guard<std::mutex> lock(gRandGeneratorMutex); double f = minrand + (gUniformDouble(gRandGenerator) * (maxrand - minrand)); assert(f >= minrand && f <= maxrand); return f; }
inline double getRandExpDouble(double minrand, double maxrand) { const std::lock_guard<std::mutex> lock(gRandGeneratorMutex); double f = minrand + (gExponentialDouble(gRandGenerator) * (maxrand - minrand)); assert(f >= minrand && f <= maxrand); return f; }
inline double getRandPercent(double percent) { const std::lock_guard<std::mutex> lock(gRandGeneratorMutex); double f = (gUniformDouble(gRandGenerator) * 100.0) < percent; return f; }


#define RANDI64(minrand, maxrand)       getRandI64(minrand, maxrand)    
#define RANDU64(minrand, maxrand)       getRandU64(minrand, maxrand)    

#define RANDBOOL                        getRandBool()    
#define RANDDOUBLE(minrand,maxrand)     getRandDouble(minrand, maxrand)
#define RANDEXP_DOUBLE(minrand,maxrand) getRandExpDouble(minrand, maxrand)

#define RANDPERCENT(percent)            getRandPercent(percent)


#else // no thread safety

static std::default_random_engine               gRandGenerator((unsigned int)std::chrono::system_clock::now().time_since_epoch().count()); // seed based on current time

static std::uniform_int_distribution<uint64_t>   gUniformUint64;
static std::uniform_int_distribution<int64_t>    gUniformInt64;
static std::uniform_real_distribution<double>    gUniformDouble;

static std::exponential_distribution<double>     gExponentialDouble;


#define RANDI64(minrand, maxrand)       minrand + (gUniformInt64(gRandGenerator)%((maxrand - minrand)))
#define RANDU64(minrand, maxrand)       minrand + (gUniformUint64(gRandGenerator)%((maxrand - minrand)))

#define RANDBOOL                        gUniformUint64(gRandGenerator)%2==0
#define RANDDOUBLE(minrand,maxrand)     minrand + (gUniformDouble(gRandGenerator)*(maxrand - minrand))
#define RANDEXP_DOUBLE(minrand,maxrand) minrand + (gExponentialDouble(gRandGenerator)*(maxrand - minrand))

#define RANDPERCENT(percent)            (gUniformDouble(gRandGenerator)*100.0) < percent

#endif