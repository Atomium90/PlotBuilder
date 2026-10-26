#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DemolitionTypes.h"
#include "Damageable.generated.h"

UINTERFACE(Blueprintable)
class UDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Generic contract for anything a demolition tool can damage (ABuildablePiece, breakable
 *  APhysicsProp furniture...). Tools only ever see this interface - never a concrete class.
 */
class IDamageable
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, Category = "Destruction")
	void ApplyDamage(const FDamageInfo& DamageInfo);
	virtual void ApplyDamage_Implementation(const FDamageInfo& DamageInfo);
};
