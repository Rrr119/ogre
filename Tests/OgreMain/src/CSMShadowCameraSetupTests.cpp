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
#include <gtest/gtest.h>

#include "Ogre.h"

using namespace Ogre;

TEST(CSMShadowCameraSetup, PracticalSplitPoints)
{
    CSMShadowCameraSetup csm;
    csm.calculateSplitPoints(4, 1.0f, 1000.0f, 0.95f);

    const auto& points = csm.getSplitPoints();
    ASSERT_EQ(points.size(), 5u);
    EXPECT_FLOAT_EQ(points.front(), 1.0f);
    EXPECT_FLOAT_EQ(points.back(), 1000.0f);
    EXPECT_EQ(csm.getCascadeCount(), 4u);

    for (size_t i = 1; i < points.size(); ++i)
        EXPECT_LT(points[i - 1], points[i]);

    EXPECT_EQ(csm.getCascadeIndex(0.5f), 0u);
    EXPECT_EQ(csm.getCascadeIndex(points[1]), 0u);
    EXPECT_EQ(csm.getCascadeIndex(points[1] + 0.01f), 1u);
    EXPECT_EQ(csm.getCascadeIndex(points[2]), 1u);
    EXPECT_EQ(csm.getCascadeIndex(points[3]), 2u);
    EXPECT_EQ(csm.getCascadeIndex(points[4]), 3u);
    EXPECT_EQ(csm.getCascadeIndex(5000.0f), 3u);
}

TEST(CSMShadowCameraSetup, FirstSplitScheme)
{
    CSMShadowCameraSetup csm;
    csm.calculateSplitPointsFromFirstSplit(4, 50.0f, 5000.0f, 0.93f);

    const auto& points = csm.getSplitPoints();
    ASSERT_EQ(points.size(), 5u);
    EXPECT_FLOAT_EQ(points[0], 0.0f);
    EXPECT_FLOAT_EQ(points[1], 50.0f);
    EXPECT_FLOAT_EQ(points[4], 5000.0f);

    EXPECT_EQ(csm.getCascadeNearDistance(1), points[1]);
    EXPECT_EQ(csm.getCascadeFarDistance(1), points[2]);
}

TEST(CSMShadowCameraSetup, ManualSplitPointsValidateOrder)
{
    CSMShadowCameraSetup csm;
    EXPECT_THROW(csm.setSplitPoints({0.0f, 100.0f, 50.0f}), Exception);

    csm.setSplitPoints({1.0f, 10.0f, 100.0f});
    EXPECT_EQ(csm.getCascadeCount(), 2u);
    EXPECT_EQ(csm.getCascadeIndex(10.0f), 0u);
    EXPECT_EQ(csm.getCascadeIndex(11.0f), 1u);
}

TEST(CSMShadowCameraSetup, AdaptiveResolutionPolicy)
{
    CSMShadowCameraSetup csm;
    csm.setShadowMapResolutionRange(256, 2048);
    csm.setShadowMapResolutionScale(1.0f);
    csm.setCascadeResolutionFalloff(0.5f);

    EXPECT_EQ(csm.getRecommendedShadowMapSize(0, 1080), 2048u);
    EXPECT_EQ(csm.getRecommendedShadowMapSize(1, 1080), 1024u);
    EXPECT_EQ(csm.getRecommendedShadowMapSize(2, 1080), 512u);
    EXPECT_EQ(csm.getRecommendedShadowMapSize(3, 1080), 256u);

    EXPECT_EQ(csm.getRecommendedShadowMapSize(0, 720), 1024u);
    EXPECT_EQ(csm.getRecommendedShadowMapSize(0, 0), 256u);

    csm.setShadowMapResolutionRange(300, 1000);
    EXPECT_EQ(csm.getShadowMapResolutionMin(), 512u);
    EXPECT_EQ(csm.getShadowMapResolutionMax(), 1024u);
}
