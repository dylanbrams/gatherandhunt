#include "TestFramework.h"
#include "RandomPCG.h"

#include <unordered_set>
#include <vector>

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

TEST_CASE(RandomPCG, SameSeedSameSequence) {
    PCG32 A(42), B(42);
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(A.NextUInt32() == B.NextUInt32());
    }
}

TEST_CASE(RandomPCG, DifferentSeedsDifferentSequences) {
    PCG32 A(1), B(2);
    bool AnyDiff = false;
    for (int i = 0; i < 10; ++i) {
        if (A.NextUInt32() != B.NextUInt32()) { AnyDiff = true; break; }
    }
    REQUIRE(AnyDiff);
}

TEST_CASE(RandomPCG, RangeInt32_StaysInBounds) {
    PCG32 R(123);
    for (int i = 0; i < 10000; ++i) {
        std::int32_t v = R.RangeInt32(-5, 7);
        REQUIRE(v >= -5);
        REQUIRE(v <= 7);
    }
}

TEST_CASE(RandomPCG, RangeInt32_DegenerateRange) {
    PCG32 R(7);
    for (int i = 0; i < 100; ++i) {
        REQUIRE(R.RangeInt32(3, 3) == 3);
        REQUIRE(R.RangeInt32(10, 5) == 10); // Max <= Min returns Min.
    }
}

TEST_CASE(RandomPCG, RangeInt32_HitsEveryValueInSmallRange) {
    PCG32 R(2026);
    std::unordered_set<std::int32_t> Seen;
    for (int i = 0; i < 200; ++i) Seen.insert(R.RangeInt32(0, 5));
    // 6 possible values; 200 draws should hit them all with overwhelming probability.
    REQUIRE(Seen.size() == 6);
}

TEST_CASE(RandomPCG, ReseedRestartsSequence) {
    PCG32 R(99);
    std::vector<std::uint32_t> First;
    for (int i = 0; i < 16; ++i) First.push_back(R.NextUInt32());

    R.Reseed(99);
    for (std::size_t i = 0; i < First.size(); ++i) {
        REQUIRE(R.NextUInt32() == First[i]);
    }
}

#endif
