#include "ShadePixelPass.h"
#include <malloc.h>


// Some global vars, used to simplify changing shader location & entry points
namespace {
	// Where is our shader located?
	const char* kShadePixelShader = "shaders\\shadePixel.hlsl";
	// What environment map should we load?
	const char* kEnvironmentMap = "shaders\\MonValley_G_DirtRoad_3k.hdr";
};

bool ShadePixelPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;

	// Request textures
	mpResManager->requestTextureResource("FinalShadedImage");
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse" });

	mpResManager->requestTextureResource("EmittedLight");
	mpResManager->requestTextureResource("ToSample");
	mpResManager->requestTextureResource("Reservoir");
	mpResManager->requestTextureResource("SamplesSeenSoFar", ResourceFormat::R32Int, ResourceManager::kDefaultFlags);

	mpResManager->requestTextureResource("TestBuffer"); // Debug

	mpResManager->updateEnvironmentMap(kEnvironmentMap);

	// Use the default gfx pipeline state
	mpGfxState = GraphicsState::create();

	// Create our shader
	mpShadePixelPass = FullscreenLaunch::create(kShadePixelShader);

	return true; 
}

void ShadePixelPass::execute(RenderContext* pRenderContext)
{
	auto myFBO = mpResManager->createManagedFbo({ "WorldPosition" });

	auto shaderVars = mpShadePixelPass->getVars();
	
	shaderVars["shadedImage"] = mpResManager->getTexture("FinalShadedImage");
	shaderVars["gWsPos"] = myFBO->getColorTexture(0);
	shaderVars["gWsNorm"] = mpResManager->getTexture("WorldNormal");
	shaderVars["gMatDif"] = mpResManager->getTexture("MaterialDiffuse");

	shaderVars["emittedLight"] = mpResManager->getTexture("EmittedLight");
	shaderVars["toSample"] = mpResManager->getTexture("ToSample");
	shaderVars["reservoir"] = mpResManager->getTexture("Reservoir");
	shaderVars["M"] = mpResManager->getTexture("SamplesSeenSoFar");

	shaderVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);
	
	mpGfxState->setFbo(myFBO);
	mpShadePixelPass->execute(pRenderContext, mpGfxState); // Shade the pixel
}