#pragma once
#include "stdafx.h"

struct Bone;

struct Keyframe 
{
    double     time;
    XMFLOAT4X4 transform;
};

struct BoneAnimation 
{
    std::vector<Keyframe> keys;
};

struct AnimationClip 
{
	std::unordered_map<int, BoneAnimation> boneAnims;
	double duration;
};


class CAnimator {
public:
    std::vector<BoneAnimation> m_BoneAnims;
    double m_CurrentTime = 0.0;

    void Update(double dt);
    void Update(double dt, const std::vector<Bone>& bones, XMFLOAT4X4* outBoneTransforms);
};