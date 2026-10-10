#pragma once

#include <Defines.h>

#include <Macros.h>
#include <ClientData/GxBatch.h>
#include <ClientData/GxTypes.h>
#include <ClientData/MathTypes.h>

#include <cstdint>

namespace ClientData
{
	class CGxDevice;
	CLIENT_FUNCTION(CGxDevice_Push, 0x409670, __thiscall, void, (CGxDevice * device))
	CLIENT_FUNCTION(CGxDevice_Pop, 0x685FB0, __thiscall, void, (CGxDevice * device))

	struct CGxFormat
	{
		uint32 apiSpecificModeID;
		bool hwTnL;
		bool hwCursor;
		int8 fixLag;
		int8 window;
		bool aspect;
		int32 maximize;
		CGxFormat__Format depthFormat;
		C2iVector size;
		uint32 backBufferCount;
		uint32 multisampleCount;
		float multisampleQuality;
		CGxFormat__Format colorFormat;
		uint32 refreshRate;
		uint32 vsync;
		bool stereoEnabled;
		int32 fixedFunction[6];
		C2iVector pos;
	};

	struct CGxCaps
	{
		int32 m_numTmus;
		int32 m_pixelCenterOnEdge;
		int32 m_texelCenterOnEdge;
		int32 m_numStreams;
		int32 int10;
		EGxColorFormat m_colorFormat;
		uint32 unk18;
		uint32 m_maxIndex;
		int32 m_generateMipMaps;
		int32 m_texFmt[13];
		int32 m_texTarget[4];
		uint32 m_texNonPow2;
		uint32 m_texMaxSize[4];
		int32 m_rttFormat[13];
		uint32 unkB0;
		int32 m_shaderTargets[6];
		uint32 m_shaderConstants[6];
		int32 m_texFilterTrilinear;
		int32 m_texFilterAnisotropic;
		uint32 m_maxTexAnisotropy;
		int32 m_depthBias;
		int32 m_colorWrite;
		int32 m_maxClipPlanes;
		int32 m_hwCursor;
		int32 m_occlusionQuery;
		int32 m_pointParameters;
		float m_pointScaleMax;
		int32 m_pointSprite;
		uint32 m_blendFactor;
		uint32 unk114[5];
		int32 m_oglRtAlpha;
		int32 m_stereoAvailable;
		int32 int130;
		int32 int134;
		int32 int138;
	};

	struct CGxGammaRamp
	{
		uint16 red[256];
		uint16 green[256];
		uint16 blue[256];
	};

	struct CGxMatrixStack
	{
		uint32 m_level;
		int8 m_dirty;
		C44Matrix m_mtx[4];
		uint32 m_flags[4];
	};

	class CGxDevice
	{
	public:
		static CGxDevice* Get()
		{
			return *reinterpret_cast<CGxDevice**>(0x00C5DF88);
		}

		virtual void unk0();
		virtual void unk1();
		virtual void unk2();
		virtual void unk3();
		virtual void unk4();
		virtual void unk5();
		virtual void unk6();
		virtual void unk7();
		virtual void unk8();
		virtual void unk9();
		virtual void unk10();
		virtual void unk11();
		virtual void unk12();
		virtual void unk13();
		virtual void unk14();
		virtual void unk15();
		virtual void unk16();
		virtual void unk17();
		virtual void unk18();
		virtual void unk19();
		virtual void unk20();
		virtual void unk21();
		virtual void unk22();
		virtual void unk23();
		virtual void unk24();
		virtual void unk25();
		virtual void unk26();
		virtual void unk27();
		virtual void unk28();
		virtual void unk29();
		virtual void unk30();
		virtual void unk31();
		virtual void unk32();
		virtual void unk33();
		virtual void unk34();
		virtual void unk35();
		virtual void unk36();
		virtual void unk37();
		virtual void ScenePresent();
		virtual void unk39();
		virtual void XformSetProjection(C44Matrix* matrix);
		virtual void XformSetView(C44Matrix* matrix);
		virtual void Draw(CGxBatch* batch, int32 indexed);

		void Push()
		{
			CGxDevice_Push(this);
		}
		void Pop()
		{
			CGxDevice_Pop(this);
		}

		char m_pushedStates[0x10];
		char m_stackOffsets[0x10];
		char m_dirtyStates[0x10];
		EGxPrim m_primType;
		int32 m_indexLocked;
		int32 m_vertexLocked;
		int32 m_inBeginEnd;
		C3Vector m_primVertex;
		C2Vector m_primTexCoord[8];
		C3Vector m_primNormal;
		CImVector m_primColor;
		char m_primVertexArray[0x10];
		char m_primTexCoordArray[8][0x10];
		char m_primNormalArray[0x10];
		char m_primColorArray[0x10];
		char m_primIndexArray[0x10];
		uint32 m_primMask;
		CRect m_defWindowRect;
		CRect m_curWindowRect;
		char m_deviceRestoredCallbacks[0x10];
		char m_textureRecreationCallbacks[0x10];
		char m_stereoChangedCallbacks[0x10];
		EGxApi m_api;
		uint32 m_cpuFeatures;
		CGxFormat m_format;
		CGxCaps m_caps;
		uint32 m_baseMipLevel;
		CGxGammaRamp m_gammaRamp;
		CGxGammaRamp m_systemGammaRamp;
		int32(__stdcall* m_windowProc)(void* window, uint32 message, uintptr_t wparam, intptr_t lparam);
		int32 m_context;
		int32 intF5C;
		int32 m_windowVisible;
		int32 m_windowFocus;
		int32 m_frameCount;
		int32 m_viewportDirty;
		CBoundingBox m_viewport;
		C44Matrix m_projection;
		C44Matrix m_projNative;
		CGxMatrixStack m_xforms[11];
		CGxMatrixStack m_texGen[8];
		uint32 m_clipPlaneMask;
	};
}
