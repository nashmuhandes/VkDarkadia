
#pragma once

#include "tarray.h"
#include "vectors.h"
#include "hw_collision.h"
#include "bounds.h"
#include "common/utility/matrix.h"
#include <memory>
#include <cstring>
#include "textureid.h"
#include "flatvertices.h"
#include "textures.h"

#include <dp_rect_pack.h>

typedef dp::rect_pack::RectPacker<int> RectPacker;

class LevelSubmesh;
class FMaterial;
class FGameTexture;

class LevelMeshLight
{
public:
	FVector3 Origin;
	FVector3 RelativeOrigin;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	FVector3 SpotDir;
	FVector3 Color;
	int SectorGroup;
};

struct LevelMeshSurface
{
	LevelSubmesh* Submesh = nullptr;

	struct
	{
		unsigned int StartVertIndex = 0;
		int NumVerts = 0;
		unsigned int StartElementIndex = 0;
		unsigned int NumElements = 0;
	} MeshLocation;

	FVector4 Plane = FVector4(0.0f, 0.0f, 1.0f, 0.0f);

	// Surface location in lightmap texture
	struct
	{
		int X = 0;
		int Y = 0;
		int Width = 0;
		int Height = 0;
		int ArrayIndex = 0;
		uint32_t Area() const { return Width * Height; }
	} AtlasTile;

	// True if the surface needs to be rendered into the lightmap texture before it can be used
	bool NeedsUpdate = true;
	bool AlwaysUpdate = false;

	FGameTexture* Texture = nullptr;
	float Alpha = 1.0;
	
	bool IsSky = false;
	int PortalIndex = 0;
	int SectorGroup = 0;

	BBox Bounds;
	uint16_t SampleDimension = 0;

	// Calculate world coordinates to UV coordinates
	struct
	{
		FVector3 TranslateWorldToLocal = { 0.0f, 0.0f, 0.0f };
		FVector3 ProjLocalToU = { 0.0f, 0.0f, 0.0f };
		FVector3 ProjLocalToV = { 0.0f, 0.0f, 0.0f };
	} TileTransform;

	// Surfaces that are visible within the lightmap tile
	TArray<LevelMeshSurface*> TileSurfaces;

	// Light list location in the lightmapper GPU buffers
	struct
	{
		int Pos = -1;
		int Count = 0;
		int ResetCounter = -1;
	} LightList;
};

inline float IsInFrontOfPlane(const FVector4& plane, const FVector3& point)
{
	return (plane.X * point.X + plane.Y * point.Y + plane.Z * point.Z) >= plane.W;
}

struct LevelMeshSmoothingGroup
{
	FVector4 plane = FVector4(0, 0, 1, 0);
	int sectorGroup = 0;
	std::vector<LevelMeshSurface*> surfaces;
};

struct LevelMeshPortal
{
	LevelMeshPortal() { transformation.loadIdentity(); }

	VSMatrix transformation;

	int sourceSectorGroup = 0;
	int targetSectorGroup = 0;

	inline FVector3 TransformPosition(const FVector3& pos) const
	{
		auto v = transformation * FVector4(pos, 1.0);
		return FVector3(v.X, v.Y, v.Z);
	}

	inline FVector3 TransformRotation(const FVector3& dir) const
	{
		auto v = transformation * FVector4(dir, 0.0);
		return FVector3(v.X, v.Y, v.Z);
	}

	// Checks only transformation
	inline bool IsInverseTransformationPortal(const LevelMeshPortal& portal) const
	{
		auto diff = portal.TransformPosition(TransformPosition(FVector3(0, 0, 0)));
		return abs(diff.X) < 0.001 && abs(diff.Y) < 0.001 && abs(diff.Z) < 0.001;
	}

	// Checks only transformation
	inline bool IsEqualTransformationPortal(const LevelMeshPortal& portal) const
	{
		auto diff = portal.TransformPosition(FVector3(0, 0, 0)) - TransformPosition(FVector3(0, 0, 0));
		return (abs(diff.X) < 0.001 && abs(diff.Y) < 0.001 && abs(diff.Z) < 0.001);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsEqualPortal(const LevelMeshPortal& portal) const
	{
		return sourceSectorGroup == portal.sourceSectorGroup && targetSectorGroup == portal.targetSectorGroup && IsEqualTransformationPortal(portal);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsInversePortal(const LevelMeshPortal& portal) const
	{
		return sourceSectorGroup == portal.targetSectorGroup && targetSectorGroup == portal.sourceSectorGroup && IsInverseTransformationPortal(portal);
	}
};

// for use with std::set to recursively go through portals and skip returning portals
struct RecursivePortalComparator
{
	bool operator()(const LevelMeshPortal& a, const LevelMeshPortal& b) const
	{
		return !a.IsInversePortal(b) && std::memcmp(&a.transformation, &b.transformation, sizeof(VSMatrix)) < 0;
	}
};

// for use with std::map to reject portals which have the same effect for light rays
struct IdenticalPortalComparator
{
	bool operator()(const LevelMeshPortal& a, const LevelMeshPortal& b) const
	{
		return !a.IsEqualPortal(b) && std::memcmp(&a.transformation, &b.transformation, sizeof(VSMatrix)) < 0;
	}
};

struct LevelMeshSurfaceStats
{
	struct Stats
	{
		uint32_t total = 0, dirty = 0, sky = 0;
	};

	Stats surfaces, pixels;
};

struct SurfaceUniforms
{
	FVector4 uObjectColor;
	FVector4 uObjectColor2;
	FVector4 uDynLightColor;
	FVector4 uAddColor;
	FVector4 uTextureAddColor;
	FVector4 uTextureModulateColor;
	FVector4 uTextureBlendColor;
	FVector4 uFogColor;
	float uDesaturationFactor; // HWDrawInfo::SetColor
	float uInterpolationFactor;
	float timer;
	int useVertexData;
	FVector4 uVertexColor; // HWDrawInfo::SetColor
	FVector4 uVertexNormal;

	FVector4 uGlowTopPlane;
	FVector4 uGlowTopColor;
	FVector4 uGlowBottomPlane;
	FVector4 uGlowBottomColor;

	FVector4 uGradientTopPlane;
	FVector4 uGradientBottomPlane;

	FVector4 uSplitTopPlane;
	FVector4 uSplitBottomPlane;

	FVector4 uDetailParms;
	FVector4 uNpotEmulation;

	FVector2 uClipSplit;
	FVector2 uSpecularMaterial;

	float uLightLevel; // HWDrawInfo::SetColor
	float uFogDensity;
	float uLightFactor;
	float uLightDist;

	float uAlphaThreshold;
	int uTextureIndex;
	float padding2;
	float padding3;
};

struct FMaterialState
{
	FMaterial* mMaterial = nullptr;
	int mClampMode;
	int mTranslation;
	int mOverrideShader;
	bool mChanged;

	void Reset()
	{
		mMaterial = nullptr;
		mTranslation = 0;
		mClampMode = CLAMP_NONE;
		mOverrideShader = -1;
		mChanged = false;
	}
};

struct LevelSubmeshDrawRange
{
	int PipelineID;
	int Start;
	int Count;
};

class LevelSubmesh
{
public:
	LevelSubmesh();

	virtual ~LevelSubmesh() = default;

	virtual LevelMeshSurface* GetSurface(int index) { return nullptr; }
	virtual unsigned int GetSurfaceIndex(const LevelMeshSurface* surface) const { return 0xffffffff; }
	virtual int GetSurfaceCount() { return 0; }

	struct
	{
		TArray<FFlatVertex> Vertices;
		TArray<uint32_t> Indexes;
		TArray<int> SurfaceIndexes;
		TArray<int> UniformIndexes;
		TArray<SurfaceUniforms> Uniforms;
		TArray<FMaterialState> Materials;
	} Mesh;

	std::unique_ptr<TriangleMeshShape> Collision;

	TArray<LevelSubmeshDrawRange> DrawList;
	TArray<LevelSubmeshDrawRange> PortalList;

	// Lightmap atlas
	int LMTextureCount = 0;
	int LMTextureSize = 0;
	TArray<uint16_t> LMTextureData;

	uint16_t LightmapSampleDistance = 16;

	uint32_t AtlasPixelCount() const { return uint32_t(LMTextureCount * LMTextureSize * LMTextureSize); }

	void UpdateCollision();
	void GatherSurfacePixelStats(LevelMeshSurfaceStats& stats);
	void BuildTileSurfaceLists();

private:
	FVector2 ToUV(const FVector3& vert, const LevelMeshSurface* targetSurface);
};

class LevelMesh
{
public:
	LevelMesh();
	virtual ~LevelMesh() = default;

	std::unique_ptr<LevelSubmesh> StaticMesh = std::make_unique<LevelSubmesh>();
	std::unique_ptr<LevelSubmesh> DynamicMesh = std::make_unique<LevelSubmesh>();

	virtual int AddSurfaceLights(const LevelMeshSurface* surface, LevelMeshLight* list, int listMaxSize) { return 0; }

	LevelMeshSurface* Trace(const FVector3& start, FVector3 direction, float maxDist);

	LevelMeshSurfaceStats GatherSurfacePixelStats();

	// Map defaults
	FVector3 SunDirection = FVector3(0.0f, 0.0f, -1.0f);
	FVector3 SunColor = FVector3(0.0f, 0.0f, 0.0f);

	TArray<LevelMeshPortal> Portals;
};
