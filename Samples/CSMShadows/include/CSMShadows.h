// This file is part of the OGRE project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at https://www.ogre3d.org/licensing.
/*  Copyright 2010-2012 Matthew Paul Reid
 */

#include "SdkSample.h"

#include "CSMGpuConstants.h"

using namespace Ogre;
using namespace OgreBites;

class CSMShadows : public SdkSample
{
public:
    CSMShadows()
    {
        mInfo["Title"] = "CSM Shadows";
        mInfo["Description"] =
            "Cascaded shadow maps with stable texel snapping and adaptive resolution. "
            "Use the check boxes to compare against a single shadow map.";
        mInfo["Thumbnail"] = "thumb_csm.png";
        mInfo["Category"] = "Lighting";
    }

    void cleanupContent() override
    {
        mSceneMgr->removeShadowTextureListener(mGpuConstants.get());
        mGpuConstants.reset();
    }

    enum {
        NUM_CASCADES = 4
    };

    void testCapabilities(const Ogre::RenderSystemCapabilities* caps) override
    {
        requireMaterial("CSMShadows/Rockwall");
    }

    void setupContent() override
    {
        mCameraMan->setStyle(CS_ORBIT);

        setupGeometry();
        setupLights();
        setupShadows();
        setupControls();

        mTrayMgr->showCursor();
    }

    void checkBoxToggled(CheckBox* box) override
    {
        if (box->getName() == "AdaptiveResolution")
            mCsm->setAdaptiveShadowMapResolution(box->isChecked());
        else if (box->getName() == "StabilizeCascades")
            mCsm->setStabilizeCascades(box->isChecked());
        else if (box->getName() == "SingleCascade")
            applySplitScheme(box->isChecked());
    }

private:
    void setupControls()
    {
        mTrayMgr->createCheckBox(TL_TOPLEFT, "AdaptiveResolution", "Adaptive resolution", 200)
            ->setChecked(true, false);
        mTrayMgr->createCheckBox(TL_TOPLEFT, "StabilizeCascades", "Stable cascades", 200)
            ->setChecked(true, false);
        mTrayMgr->createCheckBox(TL_TOPLEFT, "SingleCascade", "Single map (compare)", 200)
            ->setChecked(false, false);
    }

    void applySplitScheme(bool singleCascade)
    {
        const Real farClip = 5000.0f;
        if (singleCascade)
        {
            // Keep four texture units for the receiver material, but make the
            // first cascade cover the whole view. That makes the comparison an
            // apples-to-apples single-map vs. cascaded-map test.
            mCsm->setSplitPoints({0.0f, farClip, farClip, farClip, farClip});
        }
        else
        {
            mCsm->calculateSplitPointsFromFirstSplit(NUM_CASCADES, 50.0f, farClip, 0.93f);
        }
    }

    void setupGeometry()
    {
        float scale = 0.5f;
        Vector3 scaleVec(scale, scale, scale);
        {
            SceneNode* node = mSceneMgr->getRootSceneNode()->createChildSceneNode();
            Entity* athene = mSceneMgr->createEntity("athene", "athene.mesh");
            athene->setMaterialName("CSMShadows/Athene");
            node->attachObject(athene);
            node->translate(0, -27 * scale, 0);
            node->yaw(Degree(90));
            node->setScale(scaleVec);
        }

        {
            SceneNode* node = mSceneMgr->getRootSceneNode()->createChildSceneNode();
            Entity* athene = mSceneMgr->createEntity("athene2", "athene.mesh");
            athene->setMaterialName("CSMShadows/Athene");
            node->attachObject(athene);
            node->translate(30, -27 * scale, 22);
            node->yaw(Degree(-45));
            node->setScale(scaleVec);
        }

        Entity* pEnt;
        // Columns
        for (int x = -4; x <= 4; ++x)
        {
            for (int z = -4; z <= 4; ++z)
            {
                if (x != 0 || z != 0)
                {
                    SceneNode* node = mSceneMgr->getRootSceneNode()->createChildSceneNode();
                    pEnt = mSceneMgr->createEntity(StringUtil::format("col%d_%d", x, z), "column.mesh");
                    pEnt->setMaterialName("CSMShadows/Rockwall");
                    node->attachObject(pEnt);
                    node->translate(x * 500, 0, z * 500);
                    node->setScale(scale * 2, scale, scale * 2);
                }
            }
        }

        // Skybox
        mSceneMgr->setSkyBox(true, "Examples/StormySkyBox");

        // Floor plane (use POSM plane def)
        MovablePlane plane("*mPlane");
        plane.normal = Vector3::UNIT_Y;
        plane.d = 107 * scale;
        MeshManager::getSingleton().createPlane(
            "MyPlane", RGN_DEFAULT, plane, 6000, 6000, 50, 50, true,
            1, 25.0f / scale, 25.0f / scale, Vector3::UNIT_Z);
        Entity* pPlaneEnt = mSceneMgr->createEntity("plane", "MyPlane");
        pPlaneEnt->setMaterialName("CSMShadows/Rockwall");
        pPlaneEnt->setCastShadows(false);
        mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(pPlaneEnt);
    }

    void setupLights()
    {
        mSceneMgr->setAmbientLight(ColourValue(0.3, 0.3, 0.3));
        Light* light = mSceneMgr->createLight(Light::LT_DIRECTIONAL);
        Vector3 direction(1, -1, 0.4);
        light->setCastShadows(true);
        light->setShadowFarClipDistance(12000);

        auto node = mSceneMgr->getRootSceneNode()->createChildSceneNode();
        node->attachObject(light);
        node->setDirection(direction.normalisedCopy());
    }

    void setupShadows()
    {
        // Scene manager shadow settings
        mSceneMgr->setShadowTechnique(SHADOWTYPE_TEXTURE_MODULATIVE_INTEGRATED);
        mSceneMgr->setShadowCasterRenderBackFaces(false);

        mSceneMgr->setShadowTextureCount(NUM_CASCADES);
        mSceneMgr->setShadowTextureCountPerLightType(Light::LT_DIRECTIONAL, NUM_CASCADES);

        mGpuConstants = std::make_unique<CSMGpuConstants>(NUM_CASCADES);
        mSceneMgr->addShadowTextureListener(mGpuConstants.get());

        for (int i = 0; i < NUM_CASCADES; i++)
            mSceneMgr->setShadowTextureConfig(i, 1024, 1024, PF_FLOAT32_R);

        mCsm = std::make_shared<CSMShadowCameraSetup>();
        mCsm->calculateSplitPointsFromFirstSplit(NUM_CASCADES, 50.0f, 5000.0f, 0.93f);
        mCsm->setSplitPadding(1.0f);
        mCsm->setShadowMapResolutionRange(512, 2048);
        mCsm->setAdaptiveShadowMapResolution(true);
        mSceneMgr->setShadowCameraSetup(mCsm);
    }

    std::unique_ptr<CSMGpuConstants> mGpuConstants;
    std::shared_ptr<CSMShadowCameraSetup> mCsm;
};
