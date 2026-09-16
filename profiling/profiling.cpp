// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 - Present Romain Augier
// All rights reserved.

#include <icy.hpp>

#include <print>
#include <chrono>

constexpr auto primes = icy::set<int, 512>::make({
    2ull, 3ull, 5ull, 7ull, 11ull, 13ull, 17ull, 19ull,
    23ull, 29ull, 31ull, 37ull, 41ull, 43ull, 47ull, 53ull,
    59ull, 61ull, 67ull, 71ull, 73ull, 79ull, 83ull, 89ull,
    97ull, 101ull, 103ull, 107ull, 109ull, 113ull, 127ull, 131ull,
    137ull, 139ull, 149ull, 151ull, 157ull, 163ull, 167ull, 173ull,
    179ull, 181ull, 191ull, 193ull, 197ull, 199ull, 211ull, 223ull,
    227ull, 229ull, 233ull, 239ull, 241ull, 251ull, 257ull, 263ull,
    269ull, 271ull, 277ull, 281ull, 283ull, 293ull, 307ull, 311ull,
    313ull, 317ull, 331ull, 337ull, 347ull, 349ull, 353ull, 359ull,
    367ull, 373ull, 379ull, 383ull, 389ull, 397ull, 401ull, 409ull,
    419ull, 421ull, 431ull, 433ull, 439ull, 443ull, 449ull, 457ull,
    461ull, 463ull, 467ull, 479ull, 487ull, 491ull, 499ull, 503ull,
    509ull, 521ull, 523ull, 541ull, 547ull, 557ull, 563ull, 569ull,
    571ull, 577ull, 587ull, 593ull, 599ull, 601ull, 607ull, 613ull,
    617ull, 619ull, 631ull, 641ull, 643ull, 647ull, 653ull, 659ull,
    661ull, 673ull, 677ull, 683ull, 691ull, 701ull, 709ull, 719ull,
    727ull, 733ull, 739ull, 743ull, 751ull, 757ull, 761ull, 769ull,
    773ull, 787ull, 797ull, 809ull, 811ull, 821ull, 823ull, 827ull,
    829ull, 839ull, 853ull, 857ull, 859ull, 863ull, 877ull, 881ull,
    883ull, 887ull, 907ull, 911ull, 919ull, 929ull, 937ull, 941ull,
    947ull, 953ull, 967ull, 971ull, 977ull, 983ull, 991ull, 997ull,
    1009ull, 1013ull, 1019ull, 1021ull, 1031ull, 1033ull, 1039ull, 1049ull,
    1051ull, 1061ull, 1063ull, 1069ull, 1087ull, 1091ull, 1093ull, 1097ull,
    1103ull, 1109ull, 1117ull, 1123ull, 1129ull, 1151ull, 1153ull, 1163ull,
    1171ull, 1181ull, 1187ull, 1193ull, 1201ull, 1213ull, 1217ull, 1223ull,
    1229ull, 1231ull, 1237ull, 1249ull, 1259ull, 1277ull, 1279ull, 1283ull,
    1289ull, 1291ull, 1297ull, 1301ull, 1303ull, 1307ull, 1319ull, 1321ull,
    1327ull, 1361ull, 1367ull, 1373ull, 1381ull, 1399ull, 1409ull, 1423ull,
    1427ull, 1429ull, 1433ull, 1439ull, 1447ull, 1451ull, 1453ull, 1459ull,
    1471ull, 1481ull, 1483ull, 1487ull, 1489ull, 1493ull, 1499ull, 1511ull,
    1523ull, 1531ull, 1543ull, 1549ull, 1553ull, 1559ull, 1567ull, 1571ull,
    1579ull, 1583ull, 1597ull, 1601ull, 1607ull, 1609ull, 1613ull, 1619ull,
    1621ull, 1627ull, 1637ull, 1657ull, 1663ull, 1667ull, 1669ull, 1693ull,
    1697ull, 1699ull, 1709ull, 1721ull, 1723ull, 1733ull, 1741ull, 1747ull,
    1753ull, 1759ull, 1777ull, 1783ull, 1787ull, 1789ull, 1801ull, 1811ull,
    1823ull, 1831ull, 1847ull, 1861ull, 1867ull, 1871ull, 1873ull, 1877ull,
    1879ull, 1889ull, 1901ull, 1907ull, 1913ull, 1931ull, 1933ull, 1949ull,
    1951ull, 1973ull, 1979ull, 1987ull, 1993ull, 1997ull, 1999ull, 2003ull,
    2011ull, 2017ull, 2027ull, 2029ull, 2039ull, 2053ull, 2063ull, 2069ull,
    2081ull, 2083ull, 2087ull, 2089ull, 2099ull, 2111ull, 2113ull, 2129ull,
    2131ull, 2137ull, 2141ull, 2143ull, 2153ull, 2161ull, 2179ull, 2203ull,
    2207ull, 2213ull, 2221ull, 2237ull, 2239ull, 2243ull, 2251ull, 2267ull,
    2269ull, 2273ull, 2281ull, 2287ull, 2293ull, 2297ull, 2309ull, 2311ull,
    2333ull, 2339ull, 2341ull, 2347ull, 2351ull, 2357ull, 2371ull, 2377ull,
    2381ull, 2383ull, 2389ull, 2393ull, 2399ull, 2411ull, 2417ull, 2423ull,
    2437ull, 2441ull, 2447ull, 2459ull, 2467ull, 2473ull, 2477ull, 2503ull,
    2521ull, 2531ull, 2539ull, 2543ull, 2549ull, 2551ull, 2557ull, 2579ull,
    2591ull, 2593ull, 2609ull, 2617ull, 2621ull, 2633ull, 2647ull, 2657ull,
    2659ull, 2663ull, 2671ull, 2677ull, 2683ull, 2687ull, 2689ull, 2693ull,
    2699ull, 2707ull, 2711ull, 2713ull, 2719ull, 2729ull, 2731ull, 2741ull,
    2749ull, 2753ull, 2767ull, 2777ull, 2789ull, 2791ull, 2797ull, 2801ull,
    2803ull, 2819ull, 2833ull, 2837ull, 2843ull, 2851ull, 2857ull, 2861ull,
    2879ull, 2887ull, 2897ull, 2903ull, 2909ull, 2917ull, 2927ull, 2939ull,
    2953ull, 2957ull, 2963ull, 2969ull, 2971ull, 2999ull, 3001ull, 3011ull,
    3019ull, 3023ull, 3037ull, 3041ull, 3049ull, 3061ull, 3067ull, 3079ull,
    3083ull, 3089ull, 3109ull, 3119ull, 3121ull, 3137ull, 3163ull, 3167ull,
    3169ull, 3181ull, 3187ull, 3191ull, 3203ull, 3209ull, 3217ull, 3221ull,
    3229ull, 3251ull, 3253ull, 3257ull, 3259ull, 3271ull, 3299ull, 3301ull,
    3307ull, 3313ull, 3319ull, 3323ull, 3329ull, 3331ull, 3343ull, 3347ull,
    3359ull, 3361ull, 3371ull, 3373ull, 3389ull, 3391ull, 3407ull, 3413ull,
    3433ull, 3449ull, 3457ull, 3461ull, 3463ull, 3467ull, 3469ull, 3491ull,
    3499ull, 3511ull, 3517ull, 3527ull, 3529ull, 3533ull, 3539ull, 3541ull,
    3547ull, 3557ull, 3559ull, 3571ull, 3581ull, 3583ull, 3593ull, 3607ull,
    3613ull, 3617ull, 3623ull, 3631ull, 3637ull, 3643ull, 3659ull, 3671ull
});


int main(void)
{
    auto start = std::chrono::steady_clock::now();

    volatile std::size_t total = 0;

    for(std::size_t i = 0; i < 1'000'000'000ull; i++)
    {
        if(primes.contains(i))
            total++;
    }

    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::println("We found {} primes in {} ms", static_cast<std::size_t>(total), elapsed);

    return 0;
}