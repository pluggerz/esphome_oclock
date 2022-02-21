#pragma once

namespace oclock
{
    enum class ForegroundEnum
    {
        First = 0,
        None = First,
        DebugLeds,
        FollowHandles
    };

    enum class BackgroundEnum
    {
        First = 0,
        SolidColor = First,
        WarmWhiteShimmer,
        RandomColorWalk,
        TraditionalColors,
        ColorExplosion,
        Gradient,
        BrightTwinkle,
        Collision,
        Rainbow,
    };

    enum class InBetweenAnimationEnum
    {
        Random,
        None,
        Star,
        Dash,
        Middle1,
        Middle2,
        PacMan,
    };

    enum class HandlesDistanceEnum
    {
        Random,
        Shortest,
        Left,
        Right,
    };

    enum class HandlesAnimationEnum
    {
        Random,
        Swipe,
        Distance,
    };

    enum class ActiveMode
    {
        None,
        TrackHassioTime,
        TrackInternalTime,
        TrackTestTime,
    };

    enum class EditMode
    {
        None = -1,
        Brightness = 0,
        Background,
        BackgroundColor,
        Speed,
        Last = Speed
    };
}