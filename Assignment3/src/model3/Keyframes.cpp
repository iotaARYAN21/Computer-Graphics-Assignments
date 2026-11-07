#include "Keyframes.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

std::vector<Keyframe> loadKeyframes(const std::string& path, int trajectoryCount,
                                    int extraCount)
{
    std::vector<Keyframe> result;
    std::ifstream file(path);
    if (!file)
        return result;

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos || line[start] == '#')
            continue;

        std::istringstream in(line);
        std::vector<float> values;
        float v;
        while (in >> v)
            values.push_back(v);

        if (static_cast<int>(values.size()) < 2 * trajectoryCount)
        {
            std::cerr << path << ":" << lineNumber << ": expected at least "
                      << 2 * trajectoryCount << " values, found " << values.size()
                      << " (row skipped)\n";
            continue;
        }

        Keyframe key;
        for (int i = 0; i < trajectoryCount; ++i)
        {
            key.active.push_back(values[2 * i] >= 0.5f ? 1 : 0);
            key.u.push_back(std::clamp(values[2 * i + 1], 0.0f, 1.0f));
        }
        // Missing extra fields default to 0.
        for (int i = 0; i < extraCount; ++i)
        {
            size_t index = 2 * trajectoryCount + i;
            key.extra.push_back(index < values.size() ? values[index] : 0.0f);
        }
        result.push_back(key);
    }
    return result;
}

bool saveKeyframes(const std::string& path, const std::vector<Keyframe>& keyframes,
                   const std::string& headerComment)
{
    std::ofstream file(path);
    if (!file)
        return false;

    file << headerComment;
    for (const Keyframe& key : keyframes)
    {
        for (size_t i = 0; i < key.active.size(); ++i)
            file << key.active[i] << ' ' << std::fixed << std::setprecision(5)
                 << key.u[i] << "  ";
        for (size_t i = 0; i < key.extra.size(); ++i)
        {
            float v = key.extra[i];
            if (v == static_cast<float>(static_cast<int>(v)))
                file << ' ' << static_cast<int>(v); // whole numbers (discrete fields)
            else
                file << ' ' << std::setprecision(3) << v;
        }
        file << '\n';
    }
    return static_cast<bool>(file);
}

int totalFrames(int keyframeCount, int framesPerInterval)
{
    if (keyframeCount <= 0)
        return 0;
    return (keyframeCount - 1) * framesPerInterval + 1;
}

FrameState interpolateKeyframes(const std::vector<Keyframe>& keyframes, int frame,
                                int framesPerInterval, bool (*isDiscrete)(int field))
{
    FrameState state;
    if (keyframes.empty())
        return state;

    int last = static_cast<int>(keyframes.size()) - 1;
    frame = std::clamp(frame, 0, last * framesPerInterval);

    int k = std::min(frame / framesPerInterval, last);
    int next = std::min(k + 1, last);
    float s = (k == last) ? 0.0f
                          : static_cast<float>(frame - k * framesPerInterval) / framesPerInterval;

    const Keyframe& a = keyframes[k];
    const Keyframe& b = keyframes[next];

    for (size_t i = 0; i < a.active.size(); ++i)
    {
        state.active.push_back(a.active[i] != 0);         // discrete
        state.u.push_back(a.u[i] + (b.u[i] - a.u[i]) * s); // continuous
    }
    for (size_t i = 0; i < a.extra.size(); ++i)
    {
        if (isDiscrete(static_cast<int>(i)))
            state.extra.push_back(a.extra[i]);
        else
            state.extra.push_back(a.extra[i] + (b.extra[i] - a.extra[i]) * s);
    }
    return state;
}
