#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DemolitionTypes.h"
#include "DurabilityComponent.generated.h"

class UDemolitionRules;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDurabilityDamaged, const FDamageInfo&, DamageInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDurabilityStateChanged, EDurabilityState, OldState, EDurabilityState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDurabilityDestroyed);

/**
 *  Hit points, material and damage state for anything an IDamageable actor owns - see GDD §7.
 *  Agnostic of its owner (no reference to ABuildablePiece or any other concrete class): the owner
 *  calls InitializeFromRules once, then forwards IDamageable::ApplyDamage here and reacts to the
 *  broadcast events. Mirrors UStructuralSupportComponent's "component does the math, owner decides
 *  what to do about it" split.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UDurabilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UDurabilityComponent();

	/** DA_DemolitionRules asset - set once per Blueprint class default, same pass as the owner's Material/PieceClass. */
	UPROPERTY(EditDefaultsOnly, Category = "Destruction")
	TObjectPtr<UDemolitionRules> Rules;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction")
	float CurrentHP = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction")
	float MaxHP = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction")
	EDurabilityState State = EDurabilityState::Intact;

	UPROPERTY(BlueprintAssignable, Category = "Destruction")
	FOnDurabilityDamaged OnDurabilityDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Destruction")
	FOnDurabilityStateChanged OnDurabilityStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Destruction")
	FOnDurabilityDestroyed OnDurabilityDestroyed;

	/** Computes and caches MaxHP from Rules (MaterialBaseHP[Material] x PieceClassFactor[PieceClass]), sets CurrentHP to MaxHP. Call once, typically from the owner's BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Destruction")
	void InitializeFromRules(EDemolitionMaterial InMaterial, EPieceClass InPieceClass);

	/** Subtracts DamageInfo.Amount from CurrentHP (clamped to 0), recomputes State against Rules' thresholds, and broadcasts OnDurabilityDamaged / OnDurabilityStateChanged / OnDurabilityDestroyed as appropriate. No-op once already Destroyed. */
	UFUNCTION(BlueprintCallable, Category = "Destruction")
	void ApplyDamage(const FDamageInfo& DamageInfo);

	float GetHPRatio() const { return MaxHP > 0.f ? CurrentHP / MaxHP : 0.f; }
	EDemolitionMaterial GetMaterial() const { return Material; }
	EDurabilityState GetState() const { return State; }

	/** UDemolitionRules::ComputeStabilityCoupling at GetHPRatio() - GDD §7.6's c(h). Lets UStructuralSupportComponent ask "how much of this piece's stability still gets through" without knowing about DA_DemolitionRules at all - same owner-forwards/component-computes split as everywhere else. */
	float GetStabilityCoupling() const;

	/**
	 *  Forwarders to Rules' collapse-queue constants (GDD §7.7), with the same hardcoded fallback as
	 *  the C++ field defaults - let UStructuralSupportComponent read them off any registered piece's
	 *  durability component without knowing UDemolitionRules exists, same split as GetStabilityCoupling.
	 */
	float GetBaseDelay() const;
	float GetHopDelay() const;
	float GetJitter() const;
	float GetMaxCascadeSpread() const;
	float GetCollapseImpulseStrength() const;
	float GetCompromisedDelayMin() const;
	float GetCompromisedDelayMax() const;

	/** Rules->CompromisedWarningSound, or nullptr if unset - same "missing placeholder is not an error" stance as GetImpactSound. */
	USoundBase* GetCompromisedWarningSound() const;

private:

	EDurabilityState ComputeStateForRatio(float Ratio) const;

	EDemolitionMaterial Material = EDemolitionMaterial::Wood;
};
