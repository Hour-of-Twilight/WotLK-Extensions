#pragma once

#include <ClientData/MathTypes.h>
#include <ClientData/ObjectFields.h>
#include <cmath>

namespace ClientData
{
	class CGObject_C
	{
	public:
		template <typename T>
		T& GetValue(uint32_t index) const
		{
			return *((T*)&m_data[index]);
		}

		void SetValueBytes(uint32_t index, uint8_t offset, uint8_t value)
		{
			if (!m_data || offset >= 4)
				return;

			uint32_t& current = m_data[index];
			uint8_t currentByte = static_cast<uint8_t>((current >> (offset * 8)) & 0xFF);
			if (currentByte == value)
				return;

			current &= ~(0xFFu << (offset * 8));
			current |= (uint32_t(value) << (offset * 8));
		}

		virtual ~CGObject_C();
		virtual void Disable();
		virtual void Reenable();
		virtual void PostReenable();
		virtual void HandleOutOfRange();
		virtual void UpdateWorldObject(uint32_t x);
		virtual void ShouldFadeout();
		virtual void UpdateDisplayInfo();
		virtual void GetNamePosition();
		virtual void GetBag();
		virtual void GetBag2();
		virtual C3Vector& GetPosition(C3Vector& pos);
		virtual C3Vector& GetRawPosition(C3Vector& pos);
		virtual float GetFacing();
		virtual float GetRawFacing();
		virtual float GetScale();
		virtual uint64_t GetTransportGUID();
		virtual void GetRotation();
		virtual void SetFrameOfReference();
		virtual bool IsQuestGiver();
		virtual void RefreshInteractIcon();
		virtual void UpdateInteractIcon();
		virtual void UpdateInteractIconAttach();
		virtual void UpdateInteractIconScale();
		virtual bool GetModelFileName(char const** modelFileName);
		virtual void ScaleChangeUpdate();
		virtual void ScaleChangeFinished();
		virtual void RenderTargetSelection();
		virtual void RenderPetTargetSelection();
		virtual void Render();
		virtual void GetSelectionHighlightColor();
		virtual float GetTrueScale();
		virtual void ModelLoaded();
		virtual void ApplyAlpha();
		virtual void PreAnimate();
		virtual void Animate();
		virtual void ShouldRender();
		virtual float GetRenderFacing();
		virtual void OnSpecialMountAnim();
		virtual bool IsSolidSelectable();
		virtual void Dummy40();
		virtual bool CanHighlight();
		virtual bool CanBeTargetted();
		virtual void FloatingTooltip();
		virtual void OnRightClick();
		virtual bool IsHighlightSuppressed();
		virtual void OnSpellEffectClear();
		virtual void GetAppropriateSpellVisual();
		virtual void ConnectToLightningThisFrame();
		virtual void GetMatrix();
		virtual void ObjectNameVisibilityChanged();
		virtual void UpdateObjectNameString();
		virtual void ShouldRenderObjectName();
		virtual void GetObjectModel();
		virtual const char* GetObjectName();
		virtual void GetPageTextID();
		virtual void CleanUpVehicleBoneAnimsBeforeObjectModelChange();
		virtual void ShouldFadeIn();
		virtual float GetBaseAlpha();
		virtual bool IsTransport();
		virtual bool IsPointInside();
		virtual void AddPassenger();
		virtual float GetSpeed();
		virtual void PlaySpellVisualKit_PlayAnims();
		virtual void PlaySpellVisualKit_HandleWeapons();
		virtual void PlaySpellVisualKit_DelayLightningEffects();

		TypeID GetTypeID() const
		{
			return m_typeID;
		}

		float distance(CGObject_C* other)
		{
			if (!other)
				return 0.0f;

			C3Vector a{};
			C3Vector b{};
			a = other->GetPosition(a);
			b = GetPosition(b);

			float dx = b.x - a.x;
			float dy = b.y - a.y;
			float dz = b.z - a.z;
			return std::sqrt(dx * dx + dy * dy + dz * dz);
		}

	private:
		uint32_t m_field4;
		uint32_t* m_data;
		uint32_t m_fieldC;
		uint32_t m_field10;
		TypeID m_typeID;
		uint32_t m_field18[46];
	};
}
