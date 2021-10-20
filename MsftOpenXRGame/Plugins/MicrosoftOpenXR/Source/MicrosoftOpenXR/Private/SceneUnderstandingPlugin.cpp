// Copyright (c) 2021 Microsoft Corporation.
// Licensed under the MIT License.

#include "SceneUnderstandingPlugin.h"

#include "ARBlueprintLibrary.h"
#include "Engine.h"
#include "IOpenXRARModule.h"
#include "IOpenXRARTrackedGeometryHolder.h"
#include "IXRTrackingSystem.h"
#include "MicrosoftOpenXR.h"
#include "Misc/EngineVersionComparison.h"
#include "OpenXRCore.h"
#include "SceneUnderstandingUtility.h"
#include "TrackedGeometryCollision.h"
#include "UniqueHandle.h"

#if SUPPORTS_REMOTING
#include "openxr_msft_holographic_remoting.h"
#endif

namespace MicrosoftOpenXR
{
#if !UE_VERSION_OLDER_THAN(4, 27, 1)
	enum class EScanState
	{
		Idle,
		Waiting,
		Processing,
		AddMeshesToScene,
		Locating
	};

	struct FPlaneData
	{
		FGuid MeshGuid;
	};

	struct FPlaneUpdate
	{
		FGuid MeshGuid;
		EARObjectClassification Type;
		FVector Extent;
		TArray<FVector> Vertices;
		TArray<MRMESH_INDEX_TYPE> Indices;
	};

	struct FSceneUpdate
	{
		FSceneHandle Scene;
		TMap<XrUuidMSFT, FPlaneUpdate> Planes;
		TArray<XrUuidMSFT> PlaneUuids;
		TMap<FGuid, TrackedGeometryCollision> PlaneCollisionInfo;
		TMap<FGuid, TrackedGeometryCollision> MeshCollisionInfo;
	};

	inline FTransform GetPlaneTransform(const XrPosef& Pose, float WorldToMetersScale)
	{
		FTransform transform = ToFTransform(Pose, WorldToMetersScale);
		transform.ConcatenateRotation(FQuat(FVector(0, 1, 0), -HALF_PI));
		return transform;
	}

	TrackedGeometryCollision CreatePlaneGeometryCollision(const FVector& Extent)
	{
		TArray<FVector> Vertices;
		Vertices.Reset(4);
		Vertices.Add(Extent);
		Vertices.Add(FVector(Extent.X, -Extent.Y, Extent.Z));
		Vertices.Add(FVector(-Extent.X, -Extent.Y, Extent.Z));
		Vertices.Add(FVector(-Extent.X, Extent.Y, Extent.Z));

		// Two triangles
		TArray<MRMESH_INDEX_TYPE> Indices{0, 2, 1, 2, 0, 3};
		return TrackedGeometryCollision(MoveTemp(Vertices), MoveTemp(Indices));
	}

	// This function should be called in a background thread.
	TSharedPtr<FSceneUpdate> LoadPlanes(const ExtensionDispatchTable& Ext, FSceneHandle Scene,
		TMap<XrUuidMSFT, FPlaneData>&& PlaneIdToMeshGuid, const TArray<XrScenePlaneAlignmentTypeMSFT>& PlaneAlignmentFilters,
		float WorldToMetersScale)
	{
		// Get a map of SceneObject UUID to ObjectType.
		// Planes will determine their object classification by looking for their parent's UUID in this map.
		const auto ObjectTypeMap = GetObjectTypeMap(Scene.Handle(), Ext);

		auto SceneUpdate = MakeShared<FSceneUpdate>();
		auto& PlaneUpdates = SceneUpdate->Planes;
		auto& PlaneCollisionInfo = SceneUpdate->PlaneCollisionInfo;
		auto& MeshCollisionInfo = SceneUpdate->MeshCollisionInfo;

		TArray<XrSceneComponentMSFT> SceneComponents;
		TArray<XrScenePlaneMSFT> ScenePlanes;
		GetScenePlanes(Scene.Handle(), Ext, PlaneAlignmentFilters, SceneComponents, ScenePlanes);
		check(SceneComponents.Num() == ScenePlanes.Num());
		const int32_t Count = SceneComponents.Num();
		for (int32_t Index = 0; Index < Count; ++Index)
		{
			// In the SU extension, the plane's mesh is part of the plane scene component so there is one UUID.
			// In Unreal the Plane and the mesh have separate GUIDs.
			// Therefore the plane will use the UUID as its GUID and the mesh will need to generate a GUID.
			const XrSceneComponentMSFT& SceneComponent = SceneComponents[Index];
			const XrScenePlaneMSFT& ScenePlane = ScenePlanes[Index];
			const XrUuidMSFT& PlaneUuid = SceneComponent.id;
			const FGuid PlaneGuid = XrUuidMSFTToFGuid(PlaneUuid);
			const EARObjectClassification ObjectClassification =
				GetObjectClassification(GetObjectType(ObjectTypeMap, SceneComponent.parentId));

			FGuid MeshGuid{};
			// If meshBufferId is zero then the plane doesn't have a mesh. Likely because it wasn't requested.
			if (ScenePlane.meshBufferId != 0)
			{
				const auto* PrevPlaneData = PlaneIdToMeshGuid.Find(PlaneUuid);
				if (PrevPlaneData != nullptr && PrevPlaneData->MeshGuid.IsValid())
				{	 // Updated plane
					MeshGuid = PrevPlaneData->MeshGuid;
				}
				else
				{	 // New plane so generate a new GUID for the mesh
					MeshGuid = FGuid::NewGuid();
				}
			}
			FPlaneUpdate& PlaneUpdate = PlaneUpdates.Add(PlaneUuid);
			PlaneUpdate.MeshGuid = MeshGuid;
			PlaneUpdate.Type = ObjectClassification;
			PlaneUpdate.Extent = FVector(-ScenePlane.size.height, ScenePlane.size.width, 0) * WorldToMetersScale * 0.5f;

			PlaneCollisionInfo.Add(PlaneGuid, CreatePlaneGeometryCollision(PlaneUpdate.Extent));

			if (ScenePlane.meshBufferId != 0)
			{
				ReadMeshBuffers(Scene.Handle(), Ext, ScenePlane.meshBufferId, PlaneUpdate.Vertices, PlaneUpdate.Indices);

				for (FVector& Vertex : PlaneUpdate.Vertices)
				{
					Vertex.Z = -Vertex.Z;
					Vertex *= WorldToMetersScale;
					Vertex = FVector(Vertex.Z, Vertex.X, Vertex.Y);
				}
				MeshCollisionInfo.Add(MeshGuid, TrackedGeometryCollision(PlaneUpdate.Vertices, PlaneUpdate.Indices));
			}
			// The planes and meshes will need to be located on the main thread using the DisplayTime.
		}
		SceneUpdate->Scene = MoveTemp(Scene);
		PlaneUpdates.GetKeys(SceneUpdate->PlaneUuids);
		return SceneUpdate;
	}

	class FSceneUnderstandingPlugin::FImpl
	{
	public:
		void Unregister()
		{
			Stop();
			SceneObserver.Reset();
		}

		bool GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions)
		{
			OutExtensions.Add(XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME);
			return true;
		}

		const void* OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext)
		{
#if SUPPORTS_REMOTING
			bIsRemotingEnabled =
				IOpenXRHMDPlugin::Get().IsExtensionEnabled(XR_MSFT_HOLOGRAPHIC_REMOTING_EXTENSION_NAME);
#endif

			XR_ENSURE(xrGetInstanceProcAddr(
				InInstance, "xrEnumerateSceneComputeFeaturesMSFT", (PFN_xrVoidFunction*) &Ext.xrEnumerateSceneComputeFeaturesMSFT));
			XR_ENSURE(xrGetInstanceProcAddr(
				InInstance, "xrCreateSceneObserverMSFT", (PFN_xrVoidFunction*) &Ext.xrCreateSceneObserverMSFT));
			XR_ENSURE(xrGetInstanceProcAddr(
				InInstance, "xrDestroySceneObserverMSFT", (PFN_xrVoidFunction*) &Ext.xrDestroySceneObserverMSFT));
			XR_ENSURE(xrGetInstanceProcAddr(InInstance, "xrCreateSceneMSFT", (PFN_xrVoidFunction*) &Ext.xrCreateSceneMSFT));
			XR_ENSURE(xrGetInstanceProcAddr(InInstance, "xrDestroySceneMSFT", (PFN_xrVoidFunction*) &Ext.xrDestroySceneMSFT));
			XR_ENSURE(xrGetInstanceProcAddr(InInstance, "xrComputeNewSceneMSFT", (PFN_xrVoidFunction*) &Ext.xrComputeNewSceneMSFT));
			XR_ENSURE(xrGetInstanceProcAddr(
				InInstance, "xrGetSceneComputeStateMSFT", (PFN_xrVoidFunction*) &Ext.xrGetSceneComputeStateMSFT));
			XR_ENSURE(
				xrGetInstanceProcAddr(InInstance, "xrGetSceneComponentsMSFT", (PFN_xrVoidFunction*) &Ext.xrGetSceneComponentsMSFT));
			XR_ENSURE(xrGetInstanceProcAddr(
				InInstance, "xrLocateSceneComponentsMSFT", (PFN_xrVoidFunction*) &Ext.xrLocateSceneComponentsMSFT));
			XR_ENSURE(xrGetInstanceProcAddr(
				InInstance, "xrGetSceneMeshBuffersMSFT", (PFN_xrVoidFunction*) &Ext.xrGetSceneMeshBuffersMSFT));
			return InNext;
		}

		const void* OnBeginSession(XrSession InSession, const void* InNext)
		{
			static FName SystemName(TEXT("OpenXR"));
			if (!GEngine->XRSystem.IsValid() || (GEngine->XRSystem->GetSystemName() != SystemName))
			{
				return InNext;
			}
			XRTrackingSystem = GEngine->XRSystem.Get();

			if (IOpenXRARModule::IsAvailable())
			{
				TrackedMeshHolder = IOpenXRARModule::Get().GetTrackedMeshHolder();
			}
			ViewSpace = CreateViewSpace(InSession);
			return InNext;
		}

		void UpdateDeviceLocations(XrSession InSession, XrTime DisplayTime, XrSpace TrackingSpace)
		{
			// Scene understanding must be special cased for remoting.
			// Short-circuit for now to avoid an exception.
			if (bIsRemotingEnabled)
			{
				return;
			}

			if (bShouldStartSceneUnderstanding && TrackedMeshHolder != nullptr && SceneObserver.Handle() == XR_NULL_HANDLE)
			{
				SceneObserver = CreateSceneObserver(Ext, InSession);
			}
			if (SceneObserver.Handle() == XR_NULL_HANDLE || TrackedMeshHolder == nullptr || XRTrackingSystem == nullptr || !bARSessionStarted)
			{
				return;
			}

			if (ScanState == EScanState::Idle)
			{
				if (bShouldStartSceneUnderstanding)
				{
					ComputeNewScene(DisplayTime);
					ScanState = EScanState::Waiting;
				}
				else
				{
					// Scene Understanding has been stopped, only locate any existing meshes.
					if (UuidsToLocate.Num() != 0 && LocatingScene.Handle() != XR_NULL_HANDLE)
					{
						LocateObjects(LocatingScene.Handle(), Ext, TrackingSpace, DisplayTime, UuidsToLocate, Locations);
						ScanState = EScanState::Locating;
					}
				}
			}
			else if (ScanState == EScanState::Waiting)
			{
				XrSceneComputeStateMSFT SceneComputeState;
				XR_ENSURE(Ext.xrGetSceneComputeStateMSFT(SceneObserver.Handle(), &SceneComputeState));
				if (SceneComputeState == XR_SCENE_COMPUTE_STATE_COMPLETED_WITH_ERROR_MSFT ||
					SceneComputeState == XR_SCENE_COMPUTE_STATE_NONE_MSFT)
				{
					ScanState = EScanState::Idle;
				}
				else if (SceneComputeState == XR_SCENE_COMPUTE_STATE_COMPLETED_MSFT)
				{
					FSceneHandle Scene = CreateScene(Ext, SceneObserver.Handle());
					TPromise<TSharedPtr<FSceneUpdate>> Promise;
					SceneUpdateFuture = Promise.GetFuture();
					AsyncTask(ENamedThreads::AnyThread,
						[Ext = Ext, WorldToMetersScale = XRTrackingSystem->GetWorldToMetersScale(),
							PlaneAlignmentFilters = PlaneAlignmentFilters, Scene = MoveTemp(Scene),
							PlaneIdToMeshGuid = PreviousPlanes, Promise = MoveTemp(Promise)]() mutable {
							Promise.SetValue(LoadPlanes(
								Ext, MoveTemp(Scene), MoveTemp(PlaneIdToMeshGuid), PlaneAlignmentFilters, WorldToMetersScale));
						});
					ScanState = EScanState::Processing;
				}
			}
			else if (ScanState == EScanState::Processing)
			{
				if (SceneUpdateFuture.IsReady())
				{
					ProcessSceneUpdate(MoveTemp(*SceneUpdateFuture.Get()), DisplayTime, TrackingSpace);
					SceneUpdateFuture.Reset();
					UuidToLocateThisFrame = 0;
					// Avoid a frame rate dip by adding meshes over multiple frames after processing
					ScanState = EScanState::AddMeshesToScene;
				}
			}
			else if (ScanState == EScanState::AddMeshesToScene)
			{
				if (UuidsToLocate.Num() == 0 || LocatingScene.Handle() == XR_NULL_HANDLE)
				{
					ScanState = EScanState::Idle;
					return;
				}

				const float WorldToMetersScale = XRTrackingSystem->GetWorldToMetersScale();
				TrackedMeshHolder->StartMeshUpdates();

				for (int i = 0; i < UuidsToLocatePerFrame; i++)
				{
					const XrUuidMSFT& PlaneUuid = UuidsToLocate[UuidToLocateThisFrame];
					const FGuid PlaneGuid = XrUuidMSFTToFGuid(PlaneUuid);
					FPlaneUpdate& Plane = Planes.FindChecked(PlaneUuid);
					const FGuid& MeshGuid = Plane.MeshGuid;
					const auto& Location = Locations[UuidToLocateThisFrame];

					FOpenXRPlaneUpdate* PlaneUpdate = TrackedMeshHolder->AllocatePlaneUpdate(PlaneGuid);
					PlaneUpdate->Type = Plane.Type;
					PlaneUpdate->Extent = Plane.Extent;
					if (IsPoseValid(Location.flags))
					{
						PlaneUpdate->LocalToTrackingTransform = GetPlaneTransform(Location.pose, WorldToMetersScale);
					}
					else
					{
						// A location was not found, hide the mesh until it is located.
						PlaneUpdate->LocalToTrackingTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
					}

					if (MeshGuid.IsValid())
					{
						FOpenXRMeshUpdate* MeshUpdate = TrackedMeshHolder->AllocateMeshUpdate(MeshGuid);
						MeshUpdate->Type = Plane.Type;
						MeshUpdate->Vertices = MoveTemp(Plane.Vertices);
						MeshUpdate->Indices = MoveTemp(Plane.Indices);
						if (IsPoseValid(Location.flags))
						{
							MeshUpdate->LocalToTrackingTransform = ToFTransform(Location.pose, WorldToMetersScale);
						}
						else
						{
							// A location was not found, hide the mesh until it is located.
							MeshUpdate->LocalToTrackingTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
						}
					}

					UuidToLocateThisFrame++;
					if (UuidToLocateThisFrame >= UuidsToLocate.Num())
					{
						UuidToLocateThisFrame = 0;
						ScanState = EScanState::Locating;
						break;
					}
				}

				TrackedMeshHolder->EndMeshUpdates();
			}

			UpdateObjectLocations(DisplayTime, TrackingSpace);
		}

		bool OnToggleARCapture(const bool bOnOff)
		{
			if (bOnOff)
			{
				bShouldStartSceneUnderstanding = true;
			}
			else
			{
				bShouldStartSceneUnderstanding = false;
			}
			return true;
		}

		void OnStartARSession(class UARSessionConfig* SessionConfig)
		{
			float VolumeSize;
			if (GConfig->GetFloat(TEXT("/Script/HoloLensPlatformEditor.HoloLensTargetSettings"), TEXT("SpatialMeshingVolumeSize"),
				VolumeSize, GEngineIni))
			{
				SphereBoundRadius = VolumeSize / 2.0f;
			}

			float VolumeHeight;
			if (GConfig->GetFloat(TEXT("/Script/HoloLensSettings.SceneUnderstanding"), TEXT("SceneUnderstandingVolumeHeight"),
				VolumeHeight, GGameIni))
			{
				BoundHeight = VolumeHeight / 2.0f;
			}

			bool bGenerateSceneMeshData = false;
			GConfig->GetBool(TEXT("/Script/HoloLensSettings.SceneUnderstanding"), TEXT("ShouldDoSceneUnderstandingMeshDetection"),
				bGenerateSceneMeshData, GGameIni);

			//TODO: Restore this block when the session config exposes this flag (UE-126562). Update version when known.
			//#if !UE_VERSION_OLDER_THAN(4, 27, 2)
			//if (!bGenerateSceneMeshData)
			//{
			//	// Game ini does not have mesh detection flag set, check session config
			//	bGenerateSceneMeshData = SessionConfig->ShouldDoSceneUnderstandingMeshDetection();
			//}
			//#endif

			if (bGenerateSceneMeshData)
			{
				ComputeFeatures.AddUnique(XR_SCENE_COMPUTE_FEATURE_PLANE_MESH_MSFT);
			}
			else
			{
				ComputeFeatures.Remove(XR_SCENE_COMPUTE_FEATURE_PLANE_MESH_MSFT);
			}

			PlaneAlignmentFilters.Reset();
			if (SessionConfig->ShouldDoHorizontalPlaneDetection() && !SessionConfig->ShouldDoVerticalPlaneDetection())
			{
				PlaneAlignmentFilters.AddUnique(XR_SCENE_PLANE_ALIGNMENT_TYPE_HORIZONTAL_MSFT);
			}
			else if (SessionConfig->ShouldDoVerticalPlaneDetection() && !SessionConfig->ShouldDoHorizontalPlaneDetection())
			{
				PlaneAlignmentFilters.AddUnique(XR_SCENE_PLANE_ALIGNMENT_TYPE_VERTICAL_MSFT);
			}

			bARSessionStarted = true;
		}

		TArray<FARTraceResult> OnLineTraceTrackedObjects(
			const TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARCompositionComponent, const FVector Start,
			const FVector End, const EARLineTraceChannels TraceChannels)
		{
			// Always hittest meshes, but only hittest planes if PlaneUsingExtent is enabled.
			// This is because some planes may be floating in space, like wall planes through an open door.
			bool HitTestPlanes = ((int32)TraceChannels & (int32)EARLineTraceChannels::PlaneUsingExtent) != 0;

			TArray<FARTraceResult> Results;
			TArray<UARMeshGeometry*> Meshes = UARBlueprintLibrary::GetAllGeometriesByClass<UARMeshGeometry>();
			for (UARMeshGeometry* Mesh : Meshes)
			{
				auto CollisionInfo = MeshCollisionInfo.Find(Mesh->UniqueId);
				if (CollisionInfo != nullptr)
				{
					FVector HitPoint, HitNormal;
					float HitDistance;
					if (CollisionInfo->Collides(Start, End, Mesh->GetLocalToWorldTransform(), HitPoint, HitNormal, HitDistance))
					{
						// Append a hit.  The calling function will then sort by HitDistance.
						Results.Add(FARTraceResult(ARCompositionComponent, HitDistance, TraceChannels,
							FTransform(HitNormal.ToOrientationQuat(), HitPoint), Mesh));
					}
				}
			}
			
			if (HitTestPlanes)
			{
				TArray<UARPlaneGeometry*> TrackedPlanes = UARBlueprintLibrary::GetAllGeometriesByClass<UARPlaneGeometry>();
				for (UARPlaneGeometry* Plane : TrackedPlanes)
				{
					auto CollisionInfo = PlaneCollisionInfo.Find(Plane->UniqueId);
					if (CollisionInfo != nullptr)
					{
						FVector HitPoint, HitNormal;
						float HitDistance;
						if (CollisionInfo->Collides(Start, End, Plane->GetLocalToWorldTransform(), HitPoint, HitNormal, HitDistance))
						{
							// Append a hit.  The calling function will then sort by HitDistance.
							Results.Add(FARTraceResult(ARCompositionComponent, HitDistance, TraceChannels,
								FTransform(HitNormal.ToOrientationQuat(), HitPoint), Plane));
						}
					}
				}
			}
			return Results;
		};

	private:
		void Stop()
		{
			bShouldStartSceneUnderstanding = false;
			LocatingScene.Reset();
		}

		void ComputeNewScene(XrTime DisplayTime)
		{
			SceneComputeInfo.requestedFeatureCount = static_cast<uint32_t>(ComputeFeatures.Num());
			SceneComputeInfo.requestedFeatures = ComputeFeatures.GetData();
			SceneComputeInfo.consistency = XR_SCENE_COMPUTE_CONSISTENCY_SNAPSHOT_COMPLETE_MSFT;
			SceneComputeInfo.bounds.space = ViewSpace.Handle();	  // scene bounds will be relative to view space
			SceneComputeInfo.bounds.time = DisplayTime;

			if (BoundHeight > 0)
			{
				SceneBox.pose = { {0, 0, 0, 1}, {0, 0, 0} };
				SceneBox.extents = { SphereBoundRadius, BoundHeight, SphereBoundRadius };
				SceneComputeInfo.bounds.boxCount = 1;
				SceneComputeInfo.bounds.boxes = &SceneBox;
			}
			else
			{
				SceneSphere.center = { 0, 0, 0 };
				SceneSphere.radius = SphereBoundRadius;
				SceneComputeInfo.bounds.sphereCount = 1;
				SceneComputeInfo.bounds.spheres = &SceneSphere;
			}

			XR_ENSURE(Ext.xrComputeNewSceneMSFT(SceneObserver.Handle(), &SceneComputeInfo));
		}

		void UpdateObjectLocations(XrTime DisplayTime, XrSpace TrackingSpace)
		{
			if (ScanState != EScanState::Locating)
			{
				return;
			}

			if (UuidsToLocate.Num() == 0 || LocatingScene.Handle() == XR_NULL_HANDLE)
			{
				ScanState = EScanState::Idle;
				return;
			}

			static bool ShouldUpdateLocations = true;
			if (ShouldUpdateLocations)
			{
				const float WorldToMetersScale = XRTrackingSystem->GetWorldToMetersScale();

				XrUuidMSFT Uuid = UuidsToLocate[UuidToLocateThisFrame];
				XrSceneComponentLocationMSFT Location = Locations[UuidToLocateThisFrame];

				TrackedMeshHolder->StartMeshUpdates();

				for (int i = 0; i < UuidsToLocatePerFrame; i++)
				{
					const FGuid PlaneGuid = XrUuidMSFTToFGuid(Uuid);
					FPlaneUpdate& Plane = Planes.FindChecked(Uuid);

					auto PlaneUpdate = MakeShared<FOpenXRMeshUpdate>();
					const FGuid Guid = XrUuidMSFTToFGuid(Uuid);
					PlaneUpdate->Id = XrUuidMSFTToFGuid(Uuid);
					PlaneUpdate->SpatialMeshUsageFlags = (EARSpatialMeshUsageFlags)((int32)EARSpatialMeshUsageFlags::Visible);
					if (IsPoseValid(Location.flags))
					{
						PlaneUpdate->TrackingState = EARTrackingState::Tracking;
						PlaneUpdate->LocalToTrackingTransform = GetPlaneTransform(Location.pose, WorldToMetersScale);
					}
					else
					{
						PlaneUpdate->TrackingState = EARTrackingState::NotTracking;
						// EARTrackingState::NotTracking should prevent the mesh from rendering. 
						// However, when ObjectUpdated is called: UARTrackedGeometry::UpdateTrackedGeometry assumes the mesh is being tracked.
						// This can cause a loss of tracking to place every mesh at the origin.
						// Workaround this by scaling the mesh to zero - when it is located again the transform will be corrected.
						PlaneUpdate->LocalToTrackingTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
					}
					TrackedMeshHolder->ObjectUpdated(MoveTemp(PlaneUpdate));

					if (const FPlaneData* PlaneData = PreviousPlanes.Find(Uuid); PlaneData != nullptr)
					{
						auto MeshUpdate = MakeShared<FOpenXRMeshUpdate>();
						const FGuid& MeshGuid = PlaneData->MeshGuid;

						MeshUpdate->Id = MeshGuid;
						MeshUpdate->SpatialMeshUsageFlags =
							(EARSpatialMeshUsageFlags)((int32)EARSpatialMeshUsageFlags::Visible |
								(int32)EARSpatialMeshUsageFlags::Collision);
						if (IsPoseValid(Location.flags))
						{
							MeshUpdate->TrackingState = EARTrackingState::Tracking;
							MeshUpdate->LocalToTrackingTransform = ToFTransform(Location.pose, WorldToMetersScale);
						}
						else
						{
							MeshUpdate->TrackingState = EARTrackingState::NotTracking;
							// EARTrackingState::NotTracking should prevent the mesh from rendering. 
							// However, when ObjectUpdated is called: UARTrackedGeometry::UpdateTrackedGeometry assumes the mesh is being tracked.
							// This can cause a loss of tracking to place every mesh at the origin.
							// Workaround this by scaling the mesh to zero - when it is located again the transform will be corrected.
							MeshUpdate->LocalToTrackingTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
						}

						TrackedMeshHolder->ObjectUpdated(MoveTemp(MeshUpdate));
					}

					UuidToLocateThisFrame++;
					if (UuidToLocateThisFrame >= UuidsToLocate.Num())
					{
						UuidToLocateThisFrame = 0;
						ScanState = EScanState::Idle;
						break;
					}
				}

				TrackedMeshHolder->EndMeshUpdates();
			}
			else // ShouldUpdateLocations == false
			{
				ScanState = EScanState::Idle;
			}
		}

		void ProcessSceneUpdate(FSceneUpdate&& SceneUpdate, XrTime DisplayTime, XrSpace TrackingSpace)
		{
			PlaneCollisionInfo = MoveTemp(SceneUpdate.PlaneCollisionInfo);
			MeshCollisionInfo = MoveTemp(SceneUpdate.MeshCollisionInfo);
			const float WorldToMetersScale = XRTrackingSystem->GetWorldToMetersScale();
			LocateObjects(SceneUpdate.Scene.Handle(), Ext, TrackingSpace, DisplayTime, SceneUpdate.PlaneUuids, Locations);

			// Remove any meshes that are no longer in the scene.
			TrackedMeshHolder->StartMeshUpdates();
			for (const auto& Elem : PreviousPlanes)
			{
				const XrUuidMSFT& PlaneUuid = Elem.Key;
				if (!SceneUpdate.Planes.Contains(PlaneUuid))
				{
					const FGuid& MeshGuid = Elem.Value.MeshGuid;
					if (MeshGuid.IsValid())
					{
						TrackedMeshHolder->RemoveMesh(MeshGuid);
					}
					TrackedMeshHolder->RemovePlane(XrUuidMSFTToFGuid(PlaneUuid));
				}
			}
			TrackedMeshHolder->EndMeshUpdates();

			PreviousPlanes.Reset();
			for (const auto& Elem : SceneUpdate.Planes)
			{
				PreviousPlanes.Add(Elem.Key, {Elem.Value.MeshGuid});
			}

			// Destroying a Scene is unexpectedly slow so destroy it on a background thread.
			AsyncTask(ENamedThreads::AnyThread, [Scene = std::move(LocatingScene), Ext = Ext]() mutable { Scene.Reset(); });

			LocatingScene = MoveTemp(SceneUpdate.Scene);
			UuidsToLocate = MoveTemp(SceneUpdate.PlaneUuids);
			Planes = MoveTemp(SceneUpdate.Planes);
		}

		ExtensionDispatchTable Ext{};

		FSceneObserverHandle SceneObserver;
		FSceneHandle LocatingScene;
		FSpaceHandle ViewSpace;
		EScanState ScanState{EScanState::Idle};

		TArray<XrSceneComputeFeatureMSFT> ComputeFeatures{XR_SCENE_COMPUTE_FEATURE_PLANE_MSFT};
		TArray<XrScenePlaneAlignmentTypeMSFT> PlaneAlignmentFilters;

		// Members for reading scene components
		TArray<XrUuidMSFT> UuidsToLocate;
		TMap<XrUuidMSFT, FPlaneUpdate> Planes;
		TArray<XrSceneComponentLocationMSFT> Locations;
		TMap<XrUuidMSFT, FPlaneData> PreviousPlanes;
		int UuidToLocateThisFrame = 0;
		int UuidsToLocatePerFrame = 5;

		TMap<FGuid, TrackedGeometryCollision> PlaneCollisionInfo;
		TMap<FGuid, TrackedGeometryCollision> MeshCollisionInfo;

		TFuture<TSharedPtr<FSceneUpdate>> SceneUpdateFuture;

		class IXRTrackingSystem* XRTrackingSystem = nullptr;
		IOpenXRARTrackedMeshHolder* TrackedMeshHolder = nullptr;
		XrNewSceneComputeInfoMSFT SceneComputeInfo{ XR_TYPE_NEW_SCENE_COMPUTE_INFO_MSFT };
		XrSceneOrientedBoxBoundMSFT SceneBox;
		XrSceneSphereBoundMSFT SceneSphere;
		float SphereBoundRadius = 10.0f;	// meters
		float BoundHeight = 0.0f;			// meters
		bool bShouldStartSceneUnderstanding = false;
		bool bARSessionStarted = false;
		bool bIsRemotingEnabled = false;
	};

	FSceneUnderstandingPlugin::FSceneUnderstandingPlugin() : Impl(MakePimpl<FImpl>())
	{
	}

	void FSceneUnderstandingPlugin::Register()
	{
		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
	}

	void FSceneUnderstandingPlugin::Unregister()
	{
		Impl->Unregister();
		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	}

	bool FSceneUnderstandingPlugin::GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions)
	{
		return Impl->GetRequiredExtensions(OutExtensions);
	}

	const void* FSceneUnderstandingPlugin::OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext)
	{
		return Impl->OnCreateSession(InInstance, InSystem, InNext);
	}

	const void* FSceneUnderstandingPlugin::OnBeginSession(XrSession InSession, const void* InNext)
	{
		return Impl->OnBeginSession(InSession, InNext);
	}

	void FSceneUnderstandingPlugin::UpdateDeviceLocations(XrSession InSession, XrTime DisplayTime, XrSpace TrackingSpace)
	{
		Impl->UpdateDeviceLocations(InSession, DisplayTime, TrackingSpace);
	}

	IOpenXRCustomCaptureSupport* FSceneUnderstandingPlugin::GetCustomCaptureSupport(const EARCaptureType CaptureType)
	{
		return CaptureType == EARCaptureType::SceneUnderstanding ? this : nullptr;
	}

	bool FSceneUnderstandingPlugin::OnToggleARCapture(const bool bOnOff)
	{
		return Impl->OnToggleARCapture(bOnOff);
	}

	void FSceneUnderstandingPlugin::OnStartARSession(class UARSessionConfig* SessionConfig)
	{
		Impl->OnStartARSession(SessionConfig);
	}

	TArray<FARTraceResult> FSceneUnderstandingPlugin::OnLineTraceTrackedObjects(
		const TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARCompositionComponent, const FVector Start, const FVector End,
		const EARLineTraceChannels TraceChannels)
	{
		return Impl->OnLineTraceTrackedObjects(ARCompositionComponent, Start, End, TraceChannels);
	}

#else

	void FSceneUnderstandingPlugin::Register()
	{
	}

	void FSceneUnderstandingPlugin::Unregister()
	{
	}

	bool FSceneUnderstandingPlugin::GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions)
	{
		return false;
	}

	const void* FSceneUnderstandingPlugin::OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext)
	{
		return InNext;
	}

	const void* FSceneUnderstandingPlugin::OnBeginSession(XrSession InSession, const void* InNext)
	{
		return InNext;
	}

	void FSceneUnderstandingPlugin::UpdateDeviceLocations(XrSession InSession, XrTime DisplayTime, XrSpace TrackingSpace)
	{
	}

	IOpenXRCustomCaptureSupport* FSceneUnderstandingPlugin::GetCustomCaptureSupport(const EARCaptureType CaptureType)
	{
		return nullptr;
	}

	bool FSceneUnderstandingPlugin::OnToggleARCapture(const bool bOnOff)
	{
		return false;
	}

	void FSceneUnderstandingPlugin::OnStartARSession(class UARSessionConfig* SessionConfig)
	{
	}

	TArray<FARTraceResult> FSceneUnderstandingPlugin::OnLineTraceTrackedObjects(
		const TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARCompositionComponent, const FVector Start, const FVector End,
		const EARLineTraceChannels TraceChannels)
	{
		return {};
	};

#endif	  // #if !UE_VERSION_OLDER_THAN(4, 27, 1)

}	 // namespace MicrosoftOpenXR
