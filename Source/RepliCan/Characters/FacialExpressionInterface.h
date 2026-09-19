// Reusable interface for anything that can display a facial expression.
// Any actor (a character, a standalone controller like AFaceController,
// or a future component) can implement this to become a valid target for
// expression-selection input, without callers needing to know the concrete
// implementation -- callers just call SetMouthExpression() through the
// interface.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FacialExpressionInterface.generated.h"

UINTERFACE(BlueprintType)
class REPLICAN_API UFacialExpressionInterface : public UInterface
{
	GENERATED_BODY()
};

class REPLICAN_API IFacialExpressionInterface
{
	GENERATED_BODY()

public:

	// Selects a named expression (e.g. "Neutral", "Happy", "Angry"). What
	// names are valid is entirely up to the implementer -- this interface
	// only defines the selection contract, not the available set.
	UFUNCTION(BlueprintNativeEvent, Category = "Expression")
	void SetMouthExpression(FName ExpressionName);
};
