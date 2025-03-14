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

// Some shared Falcor stuff for talking between CPU and GPU code
#include "HostDeviceSharedMacros.h"
#include "HostDeviceData.h"           

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

// A separate file with some simple utility functions: getPerpendicularVector(), initRand(), nextRand()
#include "diffusePlus1ShadowUtils.hlsli"

// Include shader entries, data structures, and utility function to spawn shadow rays
#include "standardShadowRay.hlsli"

// A constant buffer we'll populate from our C++ code 
cbuffer RayGenCB
{
	float gMinT;        // Min distance to start a ray to avoid self-occlusion
	uint  gFrameCount;  // Frame counter, used to perturb random seed each frame
}

// Input and out textures that need to be set by the C++ code
Texture2D<float4>   gPos;                   // G-buffer world-space position
Texture2D<float4>   gNorm;                  // G-buffer world-space normal
Texture2D<float4>   gDiffuseMatl;           // G-buffer diffuse material (RGB) and opacity (A)
RWTexture2D<float4> gOutput;                // Output to store shaded result

// Reservoir texture
RWTexture2D<int> sampleIndex;
RWTexture2D<float4> emittedLight; // xyz: light color
RWTexture2D<float4> toSample; // xyz: hit to sample // w: distToLight
RWTexture2D<float4> sampleNormalArea; // xyz: sample noraml // w: area of light
RWTexture2D<float4> reservoir; // x: W // y: Wsum // zw: not used
RWTexture2D<int> M;

// TODO : shadowed detection
void ShadowedDetection() {
	// Get our pixel's position on the screen
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	// Load g-buffer data:  world-space position, normal, and diffuse color
	float4 worldPos = gPos[launchIndex];
	float4 worldNorm = gNorm[launchIndex];
	float4 rsv = reservoir[launchIndex];

	// If we don't hit any geometry, our difuse material contains our background color.
	float weight = rsv.x;
	
	// Our camera sees the background if worldPos.w is 0, only do diffuse shading elsewhere
	if (worldPos.w != 0.0f)
	{
		// We need to query our scene to find info about the current light
		float distToLight = toSample[launchIndex].w;      // How far away is it?
		float3 lightIntensity = emittedLight[launchIndex].xyz;  // What color is it?
		float3 toLight = toSample[launchIndex].xyz;         // What direction is it from our current pixel?

		// Shoot our ray
		int isVisible = shadowRayVisibility(worldPos.xyz, toLight, gMinT, distToLight);
		if (isVisible == 0) {
			weight = 0.0;
		}
	}
	
	// Save out our final shaded
	reservoir[launchIndex] = float4(weight, rsv.y, rsv.z, rsv.w);
}