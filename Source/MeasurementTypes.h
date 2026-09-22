#pragma once
#include <vector>

struct GainCurvePoint
{
    float inputDb  = 0.0f;
    float outputDb = 0.0f;
};

struct TauPoint
{
    float deltaGrDb  = 0.0f;
    float attackMs   = 0.0f;
    float releaseMs  = 0.0f;
};

struct KnobSpec
{
    bool isStepped = false;
    std::vector<float> values;
};
