#include "DurabilityComponent.h"
#include "DemolitionRules.h"
#include "PlotBuilder.h"

UDurabilityComponent::UDurabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDurabilityComponent::InitializeFromRules(EDemolitionMaterial InMaterial, EPieceClass InPieceClass)
{
	Material = InMaterial;

	if (!Rules)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UDurabilityComponent::InitializeFromRules: Rules is unset on %s, falling back to 100 HP"), *GetOwner()->GetName());
		MaxHP = 100.f;
	}
	else
	{
		MaxHP = Rules->ComputeMaxHP(InMaterial, InPieceClass);
	}

	CurrentHP = MaxHP;
	State = EDurabilityState::Intact;
}

void UDurabilityComponent::ApplyDamage(const FDamageInfo& DamageInfo)
{
	if (State == EDurabilityState::Destroyed)
	{
		return;
	}

	CurrentHP = FMath::Max(0.f, CurrentHP - DamageInfo.Amount);

	OnDurabilityDamaged.Broadcast(DamageInfo);

	const EDurabilityState OldState = State;
	const EDurabilityState NewState = ComputeStateForRatio(GetHPRatio());

	if (NewState != OldState)
	{
		State = NewState;
		OnDurabilityStateChanged.Broadcast(OldState, NewState);
	}

	if (State == EDurabilityState::Destroyed)
	{
		OnDurabilityDestroyed.Broadcast();
	}
}

float UDurabilityComponent::GetStabilityCoupling() const
{
	return Rules ? Rules->ComputeStabilityCoupling(GetHPRatio()) : 1.f;
}

float UDurabilityComponent::GetBaseDelay() const
{
	return Rules ? Rules->BaseDelay : 0.25f;
}

float UDurabilityComponent::GetHopDelay() const
{
	return Rules ? Rules->HopDelay : 0.12f;
}

float UDurabilityComponent::GetJitter() const
{
	return Rules ? Rules->Jitter : 0.08f;
}

float UDurabilityComponent::GetMaxCascadeSpread() const
{
	return Rules ? Rules->MaxCascadeSpread : 3.f;
}

float UDurabilityComponent::GetCollapseImpulseStrength() const
{
	return Rules ? Rules->CollapseImpulseStrength : 400.f;
}

float UDurabilityComponent::GetCompromisedDelayMin() const
{
	return Rules ? Rules->CompromisedDelayMin : 0.5f;
}

float UDurabilityComponent::GetCompromisedDelayMax() const
{
	return Rules ? Rules->CompromisedDelayMax : 3.f;
}

USoundBase* UDurabilityComponent::GetCompromisedWarningSound() const
{
	return Rules ? Rules->CompromisedWarningSound : nullptr;
}

EDurabilityState UDurabilityComponent::ComputeStateForRatio(float Ratio) const
{
	if (Ratio <= 0.f)
	{
		return EDurabilityState::Destroyed;
	}

	const float DamagedThreshold = Rules ? Rules->DamagedThreshold : 0.75f;
	const float CriticalThreshold = Rules ? Rules->CriticalThreshold : 0.35f;

	if (Ratio < CriticalThreshold)
	{
		return EDurabilityState::Critical;
	}

	if (Ratio < DamagedThreshold)
	{
		return EDurabilityState::Damaged;
	}

	return EDurabilityState::Intact;
}
