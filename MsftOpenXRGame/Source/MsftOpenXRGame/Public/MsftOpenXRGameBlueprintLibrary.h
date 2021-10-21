#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Misc/EngineVersionComparison.h"

#include "MsftOpenXRGameBlueprintLibrary.generated.h"

UCLASS()
class MSFTOPENXRGAME_API UMsftOpenXRGameBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintPure, Category = "MsftOpenXRGame")
	static bool AtLeast4_27_1();
};
