#ifndef KEYFRAMES_H
#define KEYFRAMES_H

#include <string>
#include <vector>

// One row of keyframes.txt:
//   active_1 u_1 active_2 u_2 ... active_N u_N  extra_1 ... extra_M
// active is 0 or 1, u is the parameter on that trajectory's Bezier curve.
// The extra fields are listed in SceneConfig.h (KeyframeField).
// Lines starting with '#' are comments.
struct Keyframe
{
    std::vector<int> active;
    std::vector<float> u;
    std::vector<float> extra;
};

// The state for one animation frame, produced by interpolation.
struct FrameState
{
    std::vector<bool> active;
    std::vector<float> u;
    std::vector<float> extra;
};

std::vector<Keyframe> loadKeyframes(const std::string& path, int trajectoryCount,
                                    int extraCount);
bool saveKeyframes(const std::string& path, const std::vector<Keyframe>& keyframes,
                   const std::string& headerComment);

// Number of frames for the whole animation with a fixed number of frames
// per keyframe interval.
int totalFrames(int keyframeCount, int framesPerInterval);

// State at a frame. Curve parameters and continuous extra fields are
// interpolated linearly between the two surrounding keyframes. Activity
// flags and discrete fields keep the value of the earlier keyframe and
// switch exactly at the next keyframe, so they never take fractional values.
FrameState interpolateKeyframes(const std::vector<Keyframe>& keyframes, int frame,
                                int framesPerInterval, bool (*isDiscrete)(int field));

#endif
