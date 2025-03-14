/**********************************************************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#  * Redistributions of code must retain the copyright notice, this list of conditions and the following disclaimer.
#  * Neither the name of NVIDIA CORPORATION nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT
# SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************************/

#include "ShadowDetectionPass.h" 

// Some global vars, used to simplify changing shader location & entry points
namespace {
	// Where is our shader located?
	const char* kFileRayTrace = "shaders\\shadowDetection.rt.hlsl";

	// What are the entry points in that shader for various ray tracing shaders?
	const char* kEntryPointRayGen = "ShadowedDetection";
	const char* kEntryPointMiss0 = "ShadowMiss";
	const char* kEntryAoAnyHit = "ShadowAnyHit";
	const char* kEntryAoClosestHit = "ShadowClosestHit";
};

bool ShadowDetectionPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager so we can get rendering resources
	mpResManager = pResManager;
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse" });

	// Create our wrapper around a ray tracing pass.  Tell it where our ray generation shader and ray-specific shaders are
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss0);
	mpRays->addHitShader(kFileRayTrace, kEntryAoClosestHit, kEntryAoAnyHit);
	mpRays->compileRayProgram();
	if (mpScene) mpRays->setScene(mpScene);
	return true;
}

void ShadowDetectionPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Stash a copy of the scene and pass it to our ray tracer (if initialized)
	mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	if (mpRays) mpRays->setScene(mpScene);
}

void ShadowDetectionPass::execute(RenderContext* pRenderContext)
{
	// Do we have all the resources we need to render?  If not, return
	if (!mpRays || !mpRays->readyToRender()) return;

	// Set our ray tracing shader variables 
	auto rayGenVars = mpRays->getRayGenVars();
	rayGenVars["RayGenCB"]["gMinT"] = mpResManager->getMinTDist();
	rayGenVars["RayGenCB"]["gFrameCount"] = mFrameCount++;

	// Pass our G-buffer textures down to the HLSL so we can shade
	rayGenVars["gPos"] = mpResManager->getTexture("WorldPosition");
	rayGenVars["gNorm"] = mpResManager->getTexture("WorldNormal");
	rayGenVars["gDiffuseMatl"] = mpResManager->getTexture("MaterialDiffuse");

	rayGenVars["sampleIndex"] = mpResManager->getTexture("SampleIndex");
	rayGenVars["toSample"] = mpResManager->getTexture("ToSample");
	rayGenVars["sampleNormalArea"] = mpResManager->getTexture("SampleNormalArea");
	rayGenVars["reservoir"] = mpResManager->getTexture("Reservoir");
	rayGenVars["M"] = mpResManager->getTexture("SamplesSeenSoFar");

	// Shoot our rays and shade our primary hit points
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}


