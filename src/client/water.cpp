#include "water.h"
#include "filesys.h"
#include <string>

WaterSceneNode::WaterSceneNode(
		scene::ISceneManager *smgr, Client *client, f32 width, f32 height, 
		scene::ISceneNode *parent, core::dimension2du renderTargetSize, s32 id):
	scene::ISceneNode(parent, smgr, id), _time(0),
	_size(width, height), _smgr(smgr), _refractionMap(nullptr), _reflectionMap(nullptr),
	_windForce(10.0f), _windDirection(1, 1), _waveHeight(0.3f), _waterColor(0.1f, 0.1f, 0.6f, 1.f),
	_colorBlendFactor(0.2f), _camera(nullptr)
{
	driver = smgr->getVideoDriver();

	m_box = aabb3f(-BS * 1000000, -BS * 1000000, -BS * 1000000,
			BS * 1000000,  BS * 1000000,  BS * 1000000);
	
	//create new camera
	_camera = smgr->addCameraSceneNode(0, v3f(0), v3f(0), -1, false);
/*
	_waterMesh = smgr->addHillPlaneMesh("Water", _size, core::dimension2d<u32>(1, 1));

	_waterSceneNode = smgr->addMeshSceneNode(_waterMesh->getMesh(0), this);

	video::IGPUProgrammingServices *GPUProgrammingServices = driver->getGPUProgrammingServices();

	std::string pathShader = porting::path_share + DIR_DELIM + "client/shaders/water_shader/";

	std::string waterPixelShader = pathShader + "opengl_fragment.glsl";
	std::string waterVertexShader = pathShader + "opengl_vertex.glsl";

	_shaderMaterial = GPUProgrammingServices->addHighLevelShaderMaterialFromFiles(
		waterVertexShader.c_str(), "main", video::EVST_VS_1_1,
		waterPixelShader.c_str(), "main", video::EPST_PS_1_1,
		this);

	_waterSceneNode->setMaterialType((video::E_MATERIAL_TYPE)_shaderMaterial);

	video::ITexture *bumpTexture = client->getTextureSource()->getTexture("waterbump.png");
	_waterSceneNode->setMaterialTexture(0, bumpTexture);
*/
	_refractionMap = driver->addRenderTargetTexture(renderTargetSize);
	_reflectionMap = driver->addRenderTargetTexture(renderTargetSize);

//	_waterSceneNode->setMaterialTexture(1, _refractionMap);
//	_waterSceneNode->setMaterialTexture(2, _reflectionMap);
}

WaterSceneNode::~WaterSceneNode()
{
	if (_camera) {
		_camera->drop();
		_camera = nullptr;
	}

	if (_refractionMap) {
		_refractionMap->drop();
		_refractionMap = nullptr;
	}

	if (_reflectionMap) {
		_reflectionMap->drop();
		_reflectionMap = nullptr;
	}

	if (_waterSceneNode) {
		_waterSceneNode->drop();
		_waterSceneNode = nullptr;
	}

	if (_waterMesh) {
		_waterMesh->drop();
		_waterMesh = nullptr;
	}
}

void WaterSceneNode::OnRegisterSceneNode()
{
	ISceneNode::OnRegisterSceneNode();

	if (IsVisible)
		_smgr->registerNodeForRendering(this);
}

void WaterSceneNode::OnAnimate(u32 timeMs)
{
	ISceneNode::OnAnimate(timeMs);

	_time = timeMs;

	//fixes glitches with incomplete refraction
	const f32 CLIP_PLANE_OFFSET_Y = 250.0f;

	if (IsVisible) {
		setVisible(false); //hide the water

		//refraction
		driver->setRenderTarget(_refractionMap, true, true); //render to refraction

		//refraction clipping plane
		core::plane3d<f32> refractionClipPlane(0, RelativeTranslation.Y + CLIP_PLANE_OFFSET_Y, 0, 0, -1, 0); //refraction clip plane
		driver->setClipPlane(0, refractionClipPlane, true);

		_smgr->drawAll(); //draw the scene

		//reflection
		driver->setRenderTarget(_reflectionMap, true, true); //render to reflection

		//get current camera
		scene::ICameraSceneNode *currentCamera = _smgr->getActiveCamera();

		//set FOV anf far value from current camera
		_camera->setFarValue(currentCamera->getFarValue());
		_camera->setFOV(currentCamera->getFOV());

		v3f position = currentCamera->getAbsolutePosition();
		position.Y = -position.Y + 2 * RelativeTranslation.Y; //position of the water
		_camera->setPosition(position);

		v3f target = currentCamera->getTarget();

		//invert Y position of current camera
		target.Y = -target.Y + 2 * RelativeTranslation.Y;
		_camera->setTarget(target);

		//set the reflection camera
		_smgr->setActiveCamera(_camera);

		//reflection clipping plane
		core::plane3d<f32> reflectionClipPlane(0, RelativeTranslation.Y - CLIP_PLANE_OFFSET_Y, 0, 0, 1, 0);
		driver->setClipPlane(0, reflectionClipPlane, true);

		_smgr->drawAll(); //draw the scene

		//disable clip plane
		driver->enableClipPlane(0, false);

		//set back old render target
		driver->setRenderTarget(0);

		//set back the active camera
		_smgr->setActiveCamera(currentCamera);

		setVisible(true); //show it again
	}
}

void WaterSceneNode::render()
{
	//driver->draw2DImage(_reflectionMap, core::position2d<s32>(0, 0));
	//driver->draw2DImage(_refractionMap, core::position2d<s32>(550, 0));
}

void WaterSceneNode::OnSetConstants(video::IMaterialRendererServices *services, s32 userData)
{
	video::IVideoDriver *driver = services->getVideoDriver();

	core::matrix4 projection = driver->getTransform(video::ETS_PROJECTION);
	core::matrix4 view = driver->getTransform(video::ETS_VIEW);
	core::matrix4 world = driver->getTransform(video::ETS_WORLD);

	core::matrix4 cameraView = _camera->getViewMatrix();

	//vertex shader constants
	//services->setVertexShaderConstant("View", view.pointer(), 16);
	
	core::matrix4 worldViewProj = projection;			
	worldViewProj *= view;
	worldViewProj *= world;
	
	core::matrix4 worldReflectionViewProj = projection;
	worldReflectionViewProj *= cameraView;
	worldReflectionViewProj *= world;
	
	f32 waveLength = 0.1f;
	f32 time = _time / 100000.0f;
	v3f cameraPosition = _smgr->getActiveCamera()->getPosition();
	
	bool fogEnabled = getMaterial(0).getFlag(video::EMF_FOG_ENABLE);
	video::SColor color;
	video::E_FOG_TYPE fogType;
	f32 start;
	f32 end;
	f32 density;
	bool pixelFog;
	bool rangeFog;
	driver->getFog(color, fogType, start, end, density, pixelFog, rangeFog);

	services->setVertexShaderConstant("WorldViewProj", worldViewProj.pointer(), 16);
	services->setVertexShaderConstant("WorldReflectionViewProj", worldReflectionViewProj.pointer(), 16);
	services->setVertexShaderConstant("WaveLength", &waveLength, 1);
	services->setVertexShaderConstant("Time", &time, 1);
	services->setVertexShaderConstant("WindForce", &_windForce, 1);
	services->setVertexShaderConstant("WindDirection", &_windDirection.X, 2);
	services->setPixelShaderConstant("CameraPosition", &cameraPosition.X, 3);
	services->setPixelShaderConstant("WaveHeight", &_waveHeight, 1);
	services->setPixelShaderConstant("WaterColor", &_waterColor.r, 4);
	services->setPixelShaderConstant("ColorBlendFactor", &_colorBlendFactor, 1);

	//texture constants for GLSL
	int var0 = 0;
	int var1 = 1;
	int var2 = 2;

	services->setPixelShaderConstant("WaterBump", &var0, 1);
	services->setPixelShaderConstant("RefractionMap", &var1, 1);
	services->setPixelShaderConstant("ReflectionMap", &var2, 1);
	
	services->setPixelShaderConstant("FogEnabled", (int*)&fogEnabled, 1);
	services->setPixelShaderConstant("FogMode", (int*)&fogType, 1);
}

void WaterSceneNode::setWindForce(const f32 windForce)
{
	_windForce = windForce;
}

void WaterSceneNode::setWindDirection(const v2f &windDirection)
{
	_windDirection = windDirection;
	_windDirection.normalize();
}

void WaterSceneNode::setWaveHeight(const f32 waveHeight)
{
	_waveHeight = waveHeight;
}

void WaterSceneNode::setWaterColor(const video::SColorf& waterColor)
{
	_waterColor = waterColor;
}

void WaterSceneNode::setColorBlendFactor(const f32 colorBlendFactor)
{
	_colorBlendFactor = colorBlendFactor;
}
