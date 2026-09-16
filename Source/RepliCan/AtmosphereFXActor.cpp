#include "AtmosphereFXActor.h"
#include "NiagaraComponent.h"
#include "Components/BillboardComponent.h"

AAtmosphereFXActor::AAtmosphereFXActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (UNiagaraComponent* FX = GetNiagaraComponent()) { FX->bSelectable = false; }
#if WITH_EDITORONLY_DATA
	if (UBillboardComponent* Icon = GetSpriteComponent()) { Icon->bSelectable = false; }
#endif
}
