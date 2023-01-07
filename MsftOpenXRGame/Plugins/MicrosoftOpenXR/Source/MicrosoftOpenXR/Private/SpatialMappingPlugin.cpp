// Copyright (c) 2021 Microsoft Corporation.
// Licensed under the MIT License.

#include "SpatialMappingPlugin.h"

namespace MicrosoftOpenXR
{
	FSpatialMappingPlugin::FSpatialMappingPlugin()
	{
	}

	IOpenXRCustomCaptureSupport* FSpatialMappingPlugin::GetCustomCaptureSupport(const EARCaptureType CaptureType)
	{
		return CaptureType == EARCaptureType::SpatialMapping ? this : nullptr;
	}

	XrSceneComputeConsistencyMSFT FSpatialMappingPlugin::GetSceneComputeConsistency()
	{
		bool bShouldDoHighQualityMeshing = false;
		GConfig->GetBool(TEXT("/Script/HoloLensSettings.SceneUnderstanding"), TEXT("ShouldDoSceneUnderstandingHighQualityMeshing"),
			bShouldDoHighQualityMeshing, GGameIni);

		return bShouldDoHighQualityMeshing ? XR_SCENE_COMPUTE_CONSISTENCY_SNAPSHOT_INCOMPLETE_FAST_MSFT : XR_SCENE_COMPUTE_CONSISTENCY_OCCLUSION_OPTIMIZED_MSFT;
	}

	TArray<XrSceneComputeFeatureMSFT> FSpatialMappingPlugin::GetSceneComputeFeatures(class UARSessionConfig* SessionConfig)
	{
		TArray<XrSceneComputeFeatureMSFT> SceneComputeFeatures;
		SceneComputeFeatures.AddUnique(XR_SCENE_COMPUTE_FEATURE_VISUAL_MESH_MSFT);

		return SceneComputeFeatures;
	}

}	 // namespace MicrosoftOpenXR
