// Copyright (c) 2021 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "OpenXRCommon.h"
#include "Templates/PimplPtr.h"

class IOpenXRARTrackedMeshHolder;

namespace MicrosoftOpenXR
{
	class FSceneUnderstandingPlugin : public IOpenXRExtensionPlugin, public IOpenXRCustomCaptureSupport
	{
	public:
		FSceneUnderstandingPlugin();
		void Register();
		void Unregister();

		bool GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions) override;

		const void* OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext) override;
		const void* OnBeginSession(XrSession InSession, const void* InNext) override;

		void UpdateDeviceLocations(XrSession InSession, XrTime DisplayTime, XrSpace TrackingSpace) override;

		IOpenXRCustomCaptureSupport* GetCustomCaptureSupport(const EARCaptureType CaptureType) override;

		bool OnToggleARCapture(const bool bOnOff) override;
		void OnStartARSession(class UARSessionConfig* SessionConfig) override;

		TArray<FARTraceResult> OnLineTraceTrackedObjects(const TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARCompositionComponent, const FVector Start, const FVector End, const EARLineTraceChannels TraceChannels) override;

	private:

		class FImpl;
		TPimplPtr<FImpl> Impl;
	};
}	 // namespace MicrosoftOpenXR
