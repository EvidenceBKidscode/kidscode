#pragma once

#include <irrlicht.h>
#include "client.h"

using namespace irr;

class WaterSceneNode: public scene::ISceneNode, video::IShaderConstantSetCallBack
{
public:
	WaterSceneNode(scene::ISceneManager *smgr, Client *client, f32 width, f32 height,
			scene::ISceneNode *parent = nullptr,
			core::dimension2du renderTargetSize = core::dimension2du(512, 512),
			s32 id = -1);

	virtual ~WaterSceneNode();

	// frame
	virtual void OnRegisterSceneNode();

	virtual void OnAnimate(u32 timeMs);

	// renders terrain
	virtual void render();
    
	virtual const aabb3f &getBoundingBox() const
	{
		return m_box;
	}

	virtual void OnSetConstants(video::IMaterialRendererServices *services, s32 userData);

	void setWindForce(f32 windForce);
	void setWindDirection(const v2f &windDirection);
	void setWaveHeight(f32 waveHeight);

	void setWaterColor(const video::SColorf &waterColor);
	void setColorBlendFactor(f32 colorBlendFactor);

private:
	video::IVideoDriver*      driver;
	u32			  _time;
	core::dimension2d<f32>	  _size;
	scene::ISceneManager*	  _smgr;
	video::ITexture*	  _refractionMap;
	video::ITexture*	  _reflectionMap;
	f32			  _windForce;
	v2f		          _windDirection;
	f32			  _waveHeight;
	video::SColorf		  _waterColor;
	f32			  _colorBlendFactor;
	scene::ICameraSceneNode*  _camera;
	scene::ISceneNode*	  _waterSceneNode;
	s32			  _shaderMaterial;
	scene::IAnimatedMesh*	  _waterMesh;

	aabb3f m_box = aabb3f(-BS * 1000000, -BS * 1000000, -BS * 1000000,
		BS * 1000000, BS * 1000000, BS * 1000000);
};
