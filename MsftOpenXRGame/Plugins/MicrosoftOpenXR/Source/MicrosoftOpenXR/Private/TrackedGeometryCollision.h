// Copyright (c) 2020 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once
#include "OpenXRCommon.h"
#include "OpenXRCore.h"
#include "IOpenXRARModule.h"
#include "IOpenXRARTrackedGeometryHolder.h"

#include "HeadMountedDisplayTypes.h"
#include "ARTypes.h"

namespace MicrosoftOpenXR
{
	class TrackedGeometryCollision
	{
	public:
		TrackedGeometryCollision(TArray<FVector> InVertices, TArray<MRMESH_INDEX_TYPE> InIndices);

		/// <summary>
		/// Hit test a ray against tracked mesh data.
		/// </summary>
		/// <param name="Start">Start of collision ray in world space</param>
		/// <param name="End">End of collision ray in world space</param>
		/// <param name="TrackingToWorld">Transform from mesh local space to world space.  The mesh may not be in tracking space.</param>
		/// <param name="OutHitPoint">Position of hit in world space</param>
		/// <param name="OutHitNormal">Normal of hit in world space</param>
		/// <param name="OutHitDistance">Distance from ray start</param>
		/// <returns>True if the input ray collides with this mesh.</returns>
		bool Collides(const FVector Start, const FVector End, const FTransform MeshToWorld, FVector& OutHitPoint, FVector& OutHitNormal, float& OutHitDistance);

		static void CreateMeshDataForBoundingBox(FVector Center, FVector HalfExtents, TArray<FVector>& OutVertices, TArray<MRMESH_INDEX_TYPE>& OutIndices);

	private:
		TArray<FVector> Vertices;
		TArray<MRMESH_INDEX_TYPE> Indices;

		FBox BoundingBox;
	};
}  // namespace MicrosoftOpenXR
