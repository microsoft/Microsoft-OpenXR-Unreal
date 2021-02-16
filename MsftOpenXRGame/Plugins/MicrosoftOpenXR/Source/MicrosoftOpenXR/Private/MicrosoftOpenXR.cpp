// Copyright (c) 2020 Microsoft Corporation.
// Licensed under the MIT License.

#include "MicrosoftOpenXR.h"

#include "CoreMinimal.h"
#include "HolographicWindowAttachmentPlugin.h"
#include "HolographicRemotingPlugin.h"
#include "SpatialAnchorPlugin.h"
#include "Modules/ModuleManager.h"
#include "HandMeshPlugin.h"
#include "QRTrackingPlugin.h"
#include "LocatableCamPlugin.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "SpeechPlugin.h"
#include "SpatialMappingPlugin.h"
#include "SecondaryViewConfiguration.h"

#define LOCTEXT_NAMESPACE "FMicrosoftOpenXRModule"

namespace MicrosoftOpenXR
{
	static class FMicrosoftOpenXRModule * g_MicrosoftOpenXRModule;

	class FMicrosoftOpenXRModule : public IModuleInterface
	{
	public:
		void StartupModule() override
		{
			SpatialAnchorPlugin.Register();
			HandMeshPlugin.Register();
			SecondaryViewConfigurationPlugin.Register();
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
			QRTrackingPlugin.Register();
			LocatableCamPlugin.Register();
			SpeechPlugin.Register();
			SpatialMappingPlugin.Register();
#endif

#if SUPPORTS_REMOTING
			HolographicRemotingPlugin = MakeShared<FHolographicRemotingPlugin>();
			HolographicRemotingPlugin->Register();
#endif

#if PLATFORM_HOLOLENS
			HolographicWindowAttachmentPlugin.Register();
#endif

			g_MicrosoftOpenXRModule = this;
		}

		void ShutdownModule() override
		{
			g_MicrosoftOpenXRModule = nullptr;

			SpatialAnchorPlugin.Unregister();
			HandMeshPlugin.Unregister();
			SecondaryViewConfigurationPlugin.Unregister();
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
			QRTrackingPlugin.Unregister();
			LocatableCamPlugin.Unregister();
			SpeechPlugin.Unregister();
			SpatialMappingPlugin.Unregister();
#endif

#if SUPPORTS_REMOTING
			HolographicRemotingPlugin->Unregister();
#endif

#if PLATFORM_HOLOLENS
			HolographicWindowAttachmentPlugin.Unregister();
#endif
		}

		FSecondaryViewConfigurationPlugin SecondaryViewConfigurationPlugin;
		FHandMeshPlugin HandMeshPlugin;
		FSpatialAnchorPlugin SpatialAnchorPlugin;
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
		FQRTrackingPlugin QRTrackingPlugin;
		FLocatableCamPlugin LocatableCamPlugin;
		FSpeechPlugin SpeechPlugin;
		FSpatialMappingPlugin SpatialMappingPlugin;
#endif

#if SUPPORTS_REMOTING
		TSharedPtr<FHolographicRemotingPlugin> HolographicRemotingPlugin;
#endif

#if PLATFORM_HOLOLENS
		FHolographicWindowAttachmentPlugin HolographicWindowAttachmentPlugin;
#endif
	};
}	 // namespace MicrosoftOpenXR


bool UMicrosoftOpenXRFunctionLibrary::SetUseHandMesh(EHandMeshStatus Mode)
{
	return MicrosoftOpenXR::g_MicrosoftOpenXRModule->HandMeshPlugin.Turn(Mode);
}

bool UMicrosoftOpenXRFunctionLibrary::IsQREnabled()
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	return MicrosoftOpenXR::g_MicrosoftOpenXRModule->QRTrackingPlugin.IsEnabled();
#else
	return false;
#endif
}


FTransform UMicrosoftOpenXRFunctionLibrary::GetPVCameraToWorldTransform()
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	return MicrosoftOpenXR::g_MicrosoftOpenXRModule->LocatableCamPlugin.GetCameraTransform();
#else
	return FTransform::Identity;
#endif
}

bool UMicrosoftOpenXRFunctionLibrary::GetPVCameraIntrinsics(FVector2D& focalLength, int& width, int& height, FVector2D& principalPoint, FVector& radialDistortion, FVector2D& tangentialDistortion)
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	return MicrosoftOpenXR::g_MicrosoftOpenXRModule->LocatableCamPlugin.GetPVCameraIntrinsics(focalLength, width, height, principalPoint, radialDistortion, tangentialDistortion);
#else
	return false;
#endif
}

FVector UMicrosoftOpenXRFunctionLibrary::GetWorldSpaceRayFromCameraPoint(FVector2D pixelCoordinate)
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	return MicrosoftOpenXR::g_MicrosoftOpenXRModule->LocatableCamPlugin.GetWorldSpaceRayFromCameraPoint(pixelCoordinate);
#else
	return FVector::ZeroVector;
#endif
}

bool UMicrosoftOpenXRFunctionLibrary::IsSpeechRecognitionAvailable()
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	return true;
#endif

	return false;
}

void UMicrosoftOpenXRFunctionLibrary::AddKeywords(TArray<FKeywordInput> Keywords)
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	MicrosoftOpenXR::g_MicrosoftOpenXRModule->SpeechPlugin.AddKeywords(Keywords);
#endif
}

void UMicrosoftOpenXRFunctionLibrary::RemoveKeywords(TArray<FString> Keywords)
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	MicrosoftOpenXR::g_MicrosoftOpenXRModule->SpeechPlugin.RemoveKeywords(Keywords);
#endif
}

bool UMicrosoftOpenXRFunctionLibrary::GetPerceptionAnchorFromOpenXRAnchor(void* AnchorID, ::IUnknown** OutPerceptionAnchor)
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	if (MicrosoftOpenXR::g_MicrosoftOpenXRModule == nullptr)
	{
		return false;
	}

	return MicrosoftOpenXR::g_MicrosoftOpenXRModule->SpatialAnchorPlugin.GetPerceptionAnchorFromOpenXRAnchor((XrSpatialAnchorMSFT)AnchorID, OutPerceptionAnchor);
#else
	return false;
#endif
}

bool UMicrosoftOpenXRFunctionLibrary::StorePerceptionAnchor(const FString& InPinId, ::IUnknown* InPerceptionAnchor)
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	if (MicrosoftOpenXR::g_MicrosoftOpenXRModule == nullptr)
	{
		return false;
	}

	return MicrosoftOpenXR::g_MicrosoftOpenXRModule->SpatialAnchorPlugin.StorePerceptionAnchor(InPinId, InPerceptionAnchor);
#else
	return false;
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(MicrosoftOpenXR::FMicrosoftOpenXRModule, MicrosoftOpenXR)
