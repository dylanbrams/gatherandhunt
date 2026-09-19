#include "TestFramework.h"
#include "Orientation.h"

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

TEST_CASE(Orientation, ForwardVector_LevelHeadings) {
    REQUIRE(Orientation(EHeading::North, EPitch::Level).ForwardVector() == IntVec3(0, +1, 0));
    REQUIRE(Orientation(EHeading::East,  EPitch::Level).ForwardVector() == IntVec3(+1, 0, 0));
    REQUIRE(Orientation(EHeading::South, EPitch::Level).ForwardVector() == IntVec3(0, -1, 0));
    REQUIRE(Orientation(EHeading::West,  EPitch::Level).ForwardVector() == IntVec3(-1, 0, 0));
}

TEST_CASE(Orientation, ForwardVector_PitchOverridesHeading) {
    for (auto H : {EHeading::North, EHeading::East, EHeading::South, EHeading::West}) {
        REQUIRE(Orientation(H, EPitch::Up).ForwardVector()   == IntVec3(0, 0, +1));
        REQUIRE(Orientation(H, EPitch::Down).ForwardVector() == IntVec3(0, 0, -1));
    }
}

TEST_CASE(Orientation, YawCyclesBackAfterFour) {
    EHeading H = EHeading::North;
    for (int i = 0; i < 4; ++i) H = YawLeft(H);
    REQUIRE(H == EHeading::North);
    for (int i = 0; i < 4; ++i) H = YawRight(H);
    REQUIRE(H == EHeading::North);
}

TEST_CASE(Orientation, YawLeftRightAreInverses) {
    for (auto H : {EHeading::North, EHeading::East, EHeading::South, EHeading::West}) {
        REQUIRE(YawRight(YawLeft(H)) == H);
        REQUIRE(YawLeft(YawRight(H)) == H);
    }
}

TEST_CASE(Orientation, PitchUpSaturatesAtUp) {
    auto t1 = TryPitchUp(EPitch::Down);
    REQUIRE(t1.bLegal); REQUIRE(t1.New == EPitch::Level);
    auto t2 = TryPitchUp(EPitch::Level);
    REQUIRE(t2.bLegal); REQUIRE(t2.New == EPitch::Up);
    auto t3 = TryPitchUp(EPitch::Up);
    REQUIRE(!t3.bLegal); REQUIRE(t3.New == EPitch::Up);
}

TEST_CASE(Orientation, PitchDownSaturatesAtDown) {
    auto t1 = TryPitchDown(EPitch::Up);
    REQUIRE(t1.bLegal); REQUIRE(t1.New == EPitch::Level);
    auto t2 = TryPitchDown(EPitch::Level);
    REQUIRE(t2.bLegal); REQUIRE(t2.New == EPitch::Down);
    auto t3 = TryPitchDown(EPitch::Down);
    REQUIRE(!t3.bLegal); REQUIRE(t3.New == EPitch::Down);
}

TEST_CASE(Orientation, MirrorHeadingX) {
    REQUIRE(MirrorHeadingX(EHeading::East)  == EHeading::West);
    REQUIRE(MirrorHeadingX(EHeading::West)  == EHeading::East);
    REQUIRE(MirrorHeadingX(EHeading::North) == EHeading::North);
    REQUIRE(MirrorHeadingX(EHeading::South) == EHeading::South);
}

TEST_CASE(Orientation, AllTwelveStatesDistinct) {
    int Count = 0;
    for (int h = 0; h < 4; ++h)
        for (int p = 0; p < 3; ++p) {
            Orientation O(static_cast<EHeading>(h), static_cast<EPitch>(p));
            (void)O;
            ++Count;
    }
    REQUIRE(Count == 12);
}

#endif
