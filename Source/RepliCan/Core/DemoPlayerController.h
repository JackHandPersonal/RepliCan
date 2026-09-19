// Sandbox input: binds number keys 1-9 to select a facial expression on
// every actor in the level implementing IFacialExpressionInterface. This is
// deliberately not routed through any specific character class -- it works
// against the interface, so it applies uniformly to however many
// expression-capable actors happen to be in the level (the three demo
// knights today, anything else implementing the interface later).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DemoPlayerController.generated.h"

UCLASS()
class REPLICAN_API ADemoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	// Ordered 1-9; index 0 corresponds to key "1". Edit in the editor to
	// remap without touching code.
	UPROPERTY(EditAnywhere, Category = "Expression")
	TArray<FName> ExpressionKeyMap = {
		TEXT("Neutral"), TEXT("Sad"), TEXT("Happy"), TEXT("Surprised"), TEXT("Smirk")
	};

	// Full expression set Q cycles through, independent of ExpressionKeyMap
	// (which only covers 9 slots) -- every registered expression, so Q is
	// how you reach anything not bound to a number key.
	UPROPERTY(EditAnywhere, Category = "Expression")
	TArray<FName> CycleExpressions = {
		TEXT("Neutral"), TEXT("Sad"), TEXT("Happy"), TEXT("Surprised"), TEXT("Smirk")
	};

protected:

	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;

private:

	void SelectExpression(int32 KeyIndex);
	void CycleToNextExpression();
	void ApplyExpression(FName ExpressionName);

	void OnKey1() { SelectExpression(0); }
	void OnKey2() { SelectExpression(1); }
	void OnKey3() { SelectExpression(2); }
	void OnKey4() { SelectExpression(3); }
	void OnKey5() { SelectExpression(4); }
	void OnKey6() { SelectExpression(5); }
	void OnKey7() { SelectExpression(6); }
	void OnKey8() { SelectExpression(7); }
	void OnKey9() { SelectExpression(8); }

	int32 CycleIndex = 0;
};
