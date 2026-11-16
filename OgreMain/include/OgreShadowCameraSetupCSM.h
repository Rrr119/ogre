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
#ifndef __ShadowCameraSetupCSM_H__
#define __ShadowCameraSetupCSM_H__

#include "OgrePrerequisites.h"
#include "OgreShadowCameraSetup.h"
#include "OgreMath.h"
#include "OgreHeaderPrefix.h"

namespace Ogre
{
    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Scene
    *  @{
    */
    /** Cascaded Shadow Map (CSM) camera setup for directional lights.

        A CSM splits the camera frustum into a number of ranges and renders each
        range into its own shadow map. This keeps a high texel density close to
        the camera while still covering a large outdoor scene. Unlike the
        @ref PSSMShadowCameraSetup, this implementation fits an oriented
        bounding sphere around each split and snaps the light-space translation
        to the shadow map texel grid. Both measures are important for large
        scenes: a moving camera no longer makes the shadow projection crawl or
        flicker from frame to frame.

        The cascade boundaries are ordinary viewing distances. They can be
        queried with getCascadeIndex() and are also exposed through
        getSplitPoints() so custom receiver shaders can select a cascade.

        For each directional light the SceneManager must allocate one shadow
        texture per cascade, e.g.:
        @code
        auto csm = std::make_shared<CSMShadowCameraSetup>();
        csm->calculateSplitPoints(4, 1.0f, 4000.0f, 0.95f);
        mSceneMgr->setShadowTextureCountPerLightType(Light::LT_DIRECTIONAL, 4);
        mSceneMgr->setShadowTextureCount(4);
        mSceneMgr->setShadowCameraSetup(csm);
        @endcode

        Existing material scripts keep working: RTSS shadow_mapping receivers
        pick up the cascade count and split points automatically, while custom
        receivers can use the usual texture_viewproj_matrix auto parameters
        for each cascade. No new shadow caster or receiver material is required.

        When adaptive resolution is enabled (the default), the configured
        shadow texture sizes are tuned to the current viewport height and to the
        position of the cascade. The nearest cascade receives the highest
        resolution and successive cascades are reduced by
        setCascadeResolutionFalloff(). This is done before the shadow textures
        are created, so changing the window size does not require any user code.
    */
    class _OgreExport CSMShadowCameraSetup : public ShadowCameraSetup
    {
    public:
        typedef std::vector<Real> SplitPointList;

        CSMShadowCameraSetup();
        virtual ~CSMShadowCameraSetup();

        static ShadowCameraSetupPtr create()
        {
            return std::make_shared<CSMShadowCameraSetup>();
        }

        /** Calculate a practical split scheme.
        @param cascadeCount The number of cascades. Must be at least 1.
        @param nearDist Distance of the first split start. Usually the camera near clip distance.
        @param farDist Distance of the last split end.
        @param lambda Blend between linear (0) and logarithmic (1) distribution.
        */
        void calculateSplitPoints(uint cascadeCount, Real nearDist, Real farDist, Real lambda = 0.95f);

        /** Calculate a split scheme where the first split has a fixed size.

            This is useful for outdoor scenes where the logarithmic distribution
            would otherwise make the first cascade unnecessarily small.
        @param cascadeCount The number of cascades. Must be at least 1.
        @param firstSplitDist Distance of the first split boundary.
        @param farDist Distance of the last split end.
        @param lambda Blend between linear (0) and logarithmic (1) distribution.
        */
        void calculateSplitPointsFromFirstSplit(uint cascadeCount, Real firstSplitDist, Real farDist,
                                                Real lambda = 0.95f);

        /** Manually configure the splitting scheme.
        @param newSplitPoints A list with cascadeCount + 1 entries. The first
            value is the near distance and the last value is the far distance.
            Intermediate values are shared by two adjacent cascades.
        */
        void setSplitPoints(const SplitPointList& newSplitPoints);

        /// Returns the current split points.
        const SplitPointList& getSplitPoints() const { return mSplitPoints; }

        /// Returns the number of cascades.
        uint getCascadeCount() const { return mCascadeCount; }

        /** Get the cascade containing a distance along the camera view direction.

            Distances before the first split use the first cascade, distances
            beyond the last split use the last cascade.
        */
        size_t getCascadeIndex(Real viewDistance) const;

        /** Get the cascade containing a world-space point for the given camera. */
        size_t getCascadeIndex(const Camera* cam, const Vector3& worldPoint) const;

        /// Get the usable range of a cascade. These are the configured split points.
        Real getCascadeNearDistance(size_t cascade) const;
        Real getCascadeFarDistance(size_t cascade) const;

        /** Set the padding applied to internal split boundaries.

            A small overlap prevents gaps at cascade boundaries when receiver
            geometry is slightly outside the fitted split frustum.
        */
        void setSplitPadding(Real padding) { mSplitPadding = std::max((Real)0, padding); }
        Real getSplitPadding() const { return mSplitPadding; }

        /** Enable or disable texel snapping and stable projection fitting.

            Stabilization is enabled by default. It is the main remedy for
            shadow crawling/flickering when the camera moves through a large
            scene. Disable it only when a custom shadow matrix is required.
        */
        void setStabilizeCascades(bool stabilize) { mStabilizeCascades = stabilize; }
        bool getStabilizeCascades() const { return mStabilizeCascades; }

        /** Enable automatic per-cascade shadow map resolution selection.

            The policy uses the viewport height as its base resolution and
            reduces far cascades according to the cascade resolution falloff.
            The result is clamped to the range configured with
            setShadowMapResolutionRange().
        */
        void setAdaptiveShadowMapResolution(bool enabled) { mAdaptiveResolution = enabled; }
        bool getAdaptiveShadowMapResolution() const { return mAdaptiveResolution; }

        /** Set the minimum and maximum edge length for adaptive shadow maps.

            Values are rounded up to the next power of two. The default range is
            256 to 2048.
        */
        void setShadowMapResolutionRange(uint16 minSize, uint16 maxSize);
        uint16 getShadowMapResolutionMin() const { return mMinResolution; }
        uint16 getShadowMapResolutionMax() const { return mMaxResolution; }

        /** Scale the adaptive base resolution relative to the viewport height.

            A value of 1.0 selects the next power of two above the viewport
            height for the nearest cascade. Lower values reduce memory and
            bandwidth; higher values improve close-up shadow detail.
        */
        void setShadowMapResolutionScale(Real scale) { mResolutionScale = std::max((Real)0.01, scale); }
        Real getShadowMapResolutionScale() const { return mResolutionScale; }

        /** Set the resolution multiplier applied to each successive cascade.

            Defaults to 0.5, so a 2048 first cascade is followed by 1024,
            512 and so on (clamped to the configured minimum).
        */
        void setCascadeResolutionFalloff(Real falloff)
        {
            mCascadeResolutionFalloff = Math::Clamp(falloff, (Real)0.1, (Real)1);
        }
        Real getCascadeResolutionFalloff() const { return mCascadeResolutionFalloff; }

        /** Calculate the adaptive shadow map edge length for one cascade.

            This helper is public so applications can display the selected
            resolutions or reuse the policy for their own shadow textures.
        */
        uint16 getRecommendedShadowMapSize(size_t cascade, uint16 viewportHeight) const;

        /** @copydoc ShadowCameraSetup::getShadowCamera */
        void getShadowCamera(const SceneManager* sm, const Camera* cam, const Viewport* vp,
                             const Light* light, Camera* texCam, size_t iteration) const override;

        /** @copydoc ShadowCameraSetup::prepareShadowTextureConfigs */
        void prepareShadowTextureConfigs(SceneManager* sm, const Camera* cam, const Viewport* vp,
                                         const Light* light, size_t firstShadowTexture,
                                         size_t shadowTextureCount) const override;

    protected:
        /** Fit an oriented bounding sphere around the part of the camera
            frustum used by a cascade. The sphere is invariant under camera
            rotation, which keeps the shadow projection scale stable.
        */
        void getCascadeBoundingSphere(const Camera* cam, Real nearDist, Real farDist,
                                      Vector3& center, Real& radius) const;

        /// Snap the cascade camera translation to its shadow map texel grid.
        void snapCascadeTranslation(Real diameter, uint32 mapWidth, uint32 mapHeight,
                                    Vector3& lightSpacePosition) const;

    private:
        uint mCascadeCount;
        SplitPointList mSplitPoints;
        Real mSplitPadding;
        bool mStabilizeCascades;

        bool mAdaptiveResolution;
        uint16 mMinResolution;
        uint16 mMaxResolution;
        Real mResolutionScale;
        Real mCascadeResolutionFalloff;
    };
    /** @} */
    /** @} */
}

#include "OgreHeaderSuffix.h"

#endif // __ShadowCameraSetupCSM_H__
