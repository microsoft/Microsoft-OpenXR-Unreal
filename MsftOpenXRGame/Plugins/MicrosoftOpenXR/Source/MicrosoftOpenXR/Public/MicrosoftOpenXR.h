// Copyright (c) 2020 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/InputComponent.h"

#include "MicrosoftOpenXR.generated.h"

// Currently remoting only supports x64 Windows: Editor and Packaged Exe
#define SUPPORTS_REMOTING (PLATFORM_WINDOWS && PLATFORM_64BITS)

USTRUCT(BlueprintType, Category = "MicrosoftOpenXR|OpenXR")
struct FKeywordInput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MicrosoftOpenXR|OpenXR")
	FString Keyword;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MicrosoftOpenXR|OpenXR")
	FInputActionHandlerDynamicSignature Callback;
};

UENUM(BlueprintType, Category = "MicrosoftOpenXR|OpenXR")
enum class EHandMeshStatus : uint8
{
	NotInitialised = 0  UMETA(Hidden),
	Disabled = 1, 
	EnabledTrackingGeometry = 2, 
	EnabledXRVisualization = 3
};

UCLASS(ClassGroup = OpenXR)
class MICROSOFTOPENXR_API UMicrosoftOpenXRFunctionLibrary :
	public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	Turn Hand Mesh

	@param On true if enable
	@return true if the command successes
	*/
	UFUNCTION(BlueprintCallable, Category = "MicrosoftOpenXR|OpenXR")
	static bool SetUseHandMesh(EHandMeshStatus Mode);

	/**
	Is QR Tracking enabled

	@return true if the command successes
	*/
	UFUNCTION(BlueprintPure, Category = "MicrosoftOpenXR|OpenXR")
	static bool IsQREnabled();

	/**
	 * Get the transform from PV camera space to Unreal world space.
	 */
	UFUNCTION(BlueprintPure, Category = "MicrosoftOpenXR|OpenXR")
	static FTransform GetPVCameraToWorldTransform();

	/**
	 * Get the PV Camera intrinsics.
	 */
	UFUNCTION(BlueprintPure, Category = "MicrosoftOpenXR|OpenXR")
	static bool GetPVCameraIntrinsics(FVector2D& focalLength, int& width, int& height, FVector2D& principalPoint, FVector& radialDistortion, FVector2D& tangentialDistortion);

	/**
	 * Get a ray into the scene from a camera point.
	 * X is left/right
	 * Y is up/down
	 */
	UFUNCTION(BlueprintPure, Category = "MicrosoftOpenXR|OpenXR")
	static FVector GetWorldSpaceRayFromCameraPoint(FVector2D pixelCoordinate);

	/**
	Check if the current platform supports speech recognition.
	*/
	UFUNCTION(BlueprintPure, Category = "MicrosoftOpenXR|OpenXR")
	static bool IsSpeechRecognitionAvailable();

	/**
	Add new speech keywords with associated callbacks.

	@param Keywords list of keyword and callbacks to add.
	*/
	UFUNCTION(BlueprintCallable, Category = "MicrosoftOpenXR|OpenXR")
	static void AddKeywords(TArray<FKeywordInput> Keywords);

	/**
	Remove speech keywords.

	@param Keywords list of keyword to remove.
	*/
	UFUNCTION(BlueprintCallable, Category = "MicrosoftOpenXR|OpenXR")
	static void RemoveKeywords(TArray<FString> Keywords);

	// Helper function to use OpenXR functions from the AzureSpatialAnchors module.
	static bool GetPerceptionAnchorFromOpenXRAnchor(void* AnchorID, void** OutPerceptionAnchor);
	static bool StorePerceptionAnchor(const FString& InPinId, void* InPerceptionAnchor);

	UFUNCTION(BlueprintPure, Category = "MicrosoftOpenXR|OpenXR")
	static bool IsRemoting();

	UFUNCTION(BlueprintPure, Category = "MicrosoftOpenXR|OpenXR")
	static bool CanDetectPlanes();
};

