/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
#include "OgreStableHeaders.h"
#include "OgreShadowCameraSetupCSM.h"

#include "OgreCamera.h"
#include "OgreLight.h"
#include "OgreSceneManager.h"
#include "OgreViewport.h"

namespace Ogre
{
namespace
{
uint32 nextPowerOfTwo(uint32 value)
{
    if (value <= 1)
        return 1;

    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}
}

CSMShadowCameraSetup::CSMShadowCameraSetup()
    : mCascadeCount(0), mSplitPadding(1.0f), mStabilizeCascades(true), mAdaptiveResolution(true),
      mMinResolution(256), mMaxResolution(2048), mResolutionScale(1.0f), mCascadeResolutionFalloff(0.5f)
{
    calculateSplitPoints(4, 1.0f, 1000.0f, 0.95f);
}

CSMShadowCameraSetup::~CSMShadowCameraSetup() {}

void CSMShadowCameraSetup::calculateSplitPoints(uint cascadeCount, Real nearDist, Real farDist, Real lambda)
{
    OgreAssert(cascadeCount >= 1, "Cannot specify less than 1 cascade");
    OgreAssert(farDist > nearDist, "farDist must be greater than nearDist");

    nearDist = std::max(nearDist, (Real)0.001f);
    farDist = std::max(farDist, nearDist + (Real)0.001f);
    lambda = Math::Clamp(lambda, (Real)0, (Real)1);

    mCascadeCount = cascadeCount;
    mSplitPoints.resize(cascadeCount + 1);
    mSplitPoints[0] = nearDist;
    mSplitPoints[cascadeCount] = farDist;

    for (uint i = 1; i < cascadeCount; ++i)
    {
        Real fraction = (Real)i / (Real)cascadeCount;
        Real logSplit = nearDist * std::pow(farDist / nearDist, fraction);
        Real linearSplit = nearDist + fraction * (farDist - nearDist);
        mSplitPoints[i] = linearSplit + lambda * (logSplit - linearSplit);
    }
}

void CSMShadowCameraSetup::calculateSplitPointsFromFirstSplit(uint cascadeCount, Real firstSplitDist,
                                                              Real farDist, Real lambda)
{
    OgreAssert(cascadeCount >= 1, "Cannot specify less than 1 cascade");
    OgreAssert(farDist > firstSplitDist, "farDist must be greater than firstSplitDist");

    firstSplitDist = std::max(firstSplitDist, (Real)0.001f);
    farDist = std::max(farDist, firstSplitDist + (Real)0.001f);
    lambda = Math::Clamp(lambda, (Real)0, (Real)1);

    mCascadeCount = cascadeCount;
    mSplitPoints.resize(cascadeCount + 1);
    mSplitPoints[0] = 0;

    for (uint i = 1; i <= cascadeCount; ++i)
    {
        Real fraction = cascadeCount == 1 ? (Real)1 : (Real)(i - 1) / (Real)(cascadeCount - 1);
        Real logSplit = firstSplitDist * std::pow(farDist / firstSplitDist, fraction);
        Real linearSplit = firstSplitDist + fraction * (farDist - firstSplitDist);
        mSplitPoints[i] = linearSplit + lambda * (logSplit - linearSplit);
    }
}

void CSMShadowCameraSetup::setSplitPoints(const SplitPointList& newSplitPoints)
{
    if (newSplitPoints.size() < 2)
        OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "At least 2 split points are required",
                    "CSMShadowCameraSetup::setSplitPoints");

    for (size_t i = 1; i < newSplitPoints.size(); ++i)
    {
        if (newSplitPoints[i] < newSplitPoints[i - 1])
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Split points must be monotonically increasing",
                        "CSMShadowCameraSetup::setSplitPoints");
    }

    mSplitPoints = newSplitPoints;
    mCascadeCount = static_cast<uint>(newSplitPoints.size() - 1);
}

size_t CSMShadowCameraSetup::getCascadeIndex(Real viewDistance) const
{
    if (mCascadeCount == 0)
        return 0;

    for (size_t i = 0; i < mCascadeCount; ++i)
    {
        if (viewDistance <= mSplitPoints[i + 1])
            return i;
    }

    return mCascadeCount - 1;
}

size_t CSMShadowCameraSetup::getCascadeIndex(const Camera* cam, const Vector3& worldPoint) const
{
    OgreAssert(cam != NULL, "Camera is NULL");

    Real viewDistance = (worldPoint - cam->getDerivedPosition()).dotProduct(cam->getDerivedDirection());
    return getCascadeIndex(std::max(viewDistance, (Real)0));
}

Real CSMShadowCameraSetup::getCascadeNearDistance(size_t cascade) const
{
    if (mCascadeCount == 0)
        return 0;

    cascade = std::min(cascade, (size_t)mCascadeCount - 1);
    return mSplitPoints[cascade];
}

Real CSMShadowCameraSetup::getCascadeFarDistance(size_t cascade) const
{
    if (mCascadeCount == 0)
        return 0;

    cascade = std::min(cascade, (size_t)mCascadeCount - 1);
    return mSplitPoints[cascade + 1];
}

void CSMShadowCameraSetup::setShadowMapResolutionRange(uint16 minSize, uint16 maxSize)
{
    uint32 minPow2 = std::min(nextPowerOfTwo(std::max<uint16>(minSize, 1)), (uint32)32768);
    uint32 maxPow2 = std::min(nextPowerOfTwo(std::max<uint16>(maxSize, 1)), (uint32)32768);
    mMinResolution = static_cast<uint16>(minPow2);
    mMaxResolution = static_cast<uint16>(maxPow2);

    if (mMinResolution > mMaxResolution)
        std::swap(mMinResolution, mMaxResolution);
}

uint16 CSMShadowCameraSetup::getRecommendedShadowMapSize(size_t cascade, uint16 viewportHeight) const
{
    if (viewportHeight == 0)
        return mMinResolution;

    Real desired = (Real)viewportHeight * mResolutionScale * std::pow(mCascadeResolutionFalloff, (Real)cascade);
    uint32 requested = static_cast<uint32>(std::ceil(Math::Clamp(desired, (Real)1, (Real)65535)));
    uint32 rounded = nextPowerOfTwo(requested);

    return static_cast<uint16>(Math::Clamp(rounded, (uint32)mMinResolution, (uint32)mMaxResolution));
}

void CSMShadowCameraSetup::getCascadeBoundingSphere(const Camera* cam, Real nearDist, Real farDist,
                                                    Vector3& center, Real& radius) const
{
    OgreAssert(cam != NULL, "Camera is NULL");

    nearDist = std::max(nearDist, cam->getNearClipDistance());
    farDist = std::max(farDist, nearDist + (Real)0.001f);

    Real halfFov = Math::Tan(cam->getFOVy() * 0.5f);
    Real halfHeight = halfFov * farDist;
    Real halfWidth = halfHeight * cam->getAspectRatio();
    Real halfDepth = (farDist - nearDist) * 0.5f;

    center = cam->getDerivedPosition() + cam->getDerivedDirection() * ((nearDist + farDist) * 0.5f);
    radius = Math::Sqrt(halfWidth * halfWidth + halfHeight * halfHeight + halfDepth * halfDepth);
    radius = std::max(radius, (Real)0.01f);
}

void CSMShadowCameraSetup::snapCascadeTranslation(Real diameter, uint32 mapWidth, uint32 mapHeight,
                                                  Vector3& lightSpacePosition) const
{
    if (!mStabilizeCascades || mapWidth == 0 || mapHeight == 0)
        return;

    Real texelWidth = diameter / (Real)mapWidth;
    Real texelHeight = diameter / (Real)mapHeight;

    lightSpacePosition.x = Math::Floor(lightSpacePosition.x / texelWidth + (Real)0.5f) * texelWidth;
    lightSpacePosition.y = Math::Floor(lightSpacePosition.y / texelHeight + (Real)0.5f) * texelHeight;
}

void CSMShadowCameraSetup::getShadowCamera(const SceneManager* sm, const Camera* cam, const Viewport* vp,
                                           const Light* light, Camera* texCam, size_t iteration) const
{
    OgreAssert(sm != NULL, "SceneManager is NULL");
    OgreAssert(cam != NULL, "Camera is NULL");
    OgreAssert(light != NULL, "Light is NULL");
    OgreAssert(texCam != NULL, "Shadow camera is NULL");
    OgreAssert(light->getType() == Light::LT_DIRECTIONAL,
               "CSMShadowCameraSetup only supports directional lights");
    OgreAssert(iteration < mCascadeCount, "Cascade index out of range");

    Real splitNear = mSplitPoints[iteration];
    Real splitFar = mSplitPoints[iteration + 1];

    if (iteration > 0)
        splitNear -= mSplitPadding;
    if (iteration + 1 < mCascadeCount)
        splitFar += mSplitPadding;

    Vector3 center;
    Real radius;
    getCascadeBoundingSphere(cam, splitNear, splitFar, center, radius);

    if (mStabilizeCascades)
    {
        Real quantum = std::max(radius * (Real)1e-4f, (Real)1e-3f);
        radius = std::ceil(radius / quantum) * quantum;
    }

    Vector3 lightDirection = -light->getDerivedDirection();
    lightDirection.normalise();

    Vector3 up = Vector3::UNIT_Y;
    if (Math::Abs(up.dotProduct(lightDirection)) >= (Real)0.99f)
        up = Vector3::UNIT_Z;

    Matrix3 lightRotation = Math::lookRotation(lightDirection, up);
    Real diameter = radius * 2.0f;

    uint32 mapWidth = 1024;
    uint32 mapHeight = 1024;
    const auto& configs = sm->getShadowTextureConfigList();
    if (iteration < configs.size())
    {
        mapWidth = std::max<uint32>(configs[iteration].width, 1);
        mapHeight = std::max<uint32>(configs[iteration].height, 1);
    }
    if (texCam->getViewport())
    {
        mapWidth = std::max<uint32>(texCam->getViewport()->getActualWidth(), 1);
        mapHeight = std::max<uint32>(texCam->getViewport()->getActualHeight(), 1);
    }

    Vector3 lightSpacePosition = lightRotation.transpose() * center;
    snapCascadeTranslation(diameter, mapWidth, mapHeight, lightSpacePosition);

    Real shadowFar = light->getShadowFarDistance();
    if (shadowFar <= 0)
        shadowFar = light->_deriveShadowFarClipDistance();
    if (shadowFar <= 0)
        shadowFar = mSplitPoints.back();

    // Use the same depth range for every cascade. Besides making the ortho
    // projection stable, receivers that derive one cascade from another (as
    // the classic CSM sample does) can rely on a shared depth scale.
    shadowFar = std::max(shadowFar, mSplitPoints.back());
    if (shadowFar <= 0)
        shadowFar = std::max(radius * 2.0f, (Real)1.0f);
    Real backDistance = shadowFar * 0.5f;
    lightSpacePosition.z = backDistance;

    Vector3 position = lightRotation * lightSpacePosition;

    texCam->setCustomViewMatrix(false);
    texCam->setCustomProjectionMatrix(false);
    texCam->setProjectionType(PT_ORTHOGRAPHIC);
    texCam->setFOVy(Degree(90));
    texCam->setNearClipDistance(0.05f);
    texCam->setFarClipDistance(shadowFar * 2.0f);
    texCam->setOrthoWindow(diameter, diameter);
    texCam->setLodBias(0.25f);

    texCam->getParentNode()->setPosition(position);
    texCam->getParentNode()->setOrientation(lightRotation);

    // The ortho window only depends on the cascade sphere; the viewport is
    // part of the signature for consistency with other camera setups.
    (void)vp;
}

void CSMShadowCameraSetup::prepareShadowTextureConfigs(SceneManager* sm, const Camera* cam,
                                                       const Viewport* vp, const Light* light,
                                                       size_t firstShadowTexture,
                                                       size_t shadowTextureCount) const
{
    if (!mAdaptiveResolution || sm == NULL || light == NULL || vp == NULL)
        return;
    if (light->getType() != Light::LT_DIRECTIONAL)
        return;

    uint16 viewportHeight = static_cast<uint16>(vp->getActualHeight());
    if (viewportHeight == 0)
        return;

    const auto& configs = sm->getShadowTextureConfigList();
    if (firstShadowTexture >= configs.size())
        return;

    size_t available = configs.size() - firstShadowTexture;
    size_t cascadeCount = std::min((size_t)mCascadeCount, std::min(shadowTextureCount, available));

    for (size_t i = 0; i < cascadeCount; ++i)
    {
        uint16 size = getRecommendedShadowMapSize(i, viewportHeight);
        const ShadowTextureConfig& current = configs[firstShadowTexture + i];
        if (current.width == size && current.height == size)
            continue;

        ShadowTextureConfig updated = current;
        updated.width = size;
        updated.height = size;
        sm->setShadowTextureConfig(firstShadowTexture + i, updated);
    }
}

}
