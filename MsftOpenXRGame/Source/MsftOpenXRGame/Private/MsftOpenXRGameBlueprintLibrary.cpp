#include "MsftOpenXRGameBlueprintLibrary.h"

bool UMsftOpenXRGameBlueprintLibrary::AtLeast4_27_1()
{
#if !UE_VERSION_OLDER_THAN(4, 27, 1)
    return true;
#endif
    return false;
}