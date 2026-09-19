// A lamp hanging from the ceiling on a cable, swinging: a slow pendulum whose plane wanders and whose
// amplitude decays, kicked up again by a gust now and then, its shadowed light swinging with it. One
// of these in a room makes every shadow move, which is most of what "something is wrong down here"
// costs to say. The parameters are on the actor so the layout can set them per instance.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwingingLampActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class REPLICAN_API ASwingingLampActor : public AActor
{
	GENERATED_BODY()
public:
	ASwingingLampActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, Category = "Lamp") TObjectPtr<UStaticMesh> LampMesh;
	UPROPERTY(EditAnywhere, Category = "Lamp") float CableLength = 140.0f;
	UPROPERTY(EditAnywhere, Category = "Lamp") float LampYaw = 0.0f;          // the fitting's turn about its cable
	UPROPERTY(EditAnywhere, Category = "Lamp") float AmplitudeDegrees = 12.0f;   // where the swing settles to
	UPROPERTY(EditAnywhere, Category = "Lamp") float PeriodSeconds = 2.6f;
	UPROPERTY(EditAnywhere, Category = "Lamp") float GustEverySeconds = 14.0f;  // a kick to the swing, about this often
	UPROPERTY(EditAnywhere, Category = "Lamp") float GustDegrees = 16.0f;
	UPROPERTY(EditAnywhere, Category = "Lamp") float Intensity = 16.0f;         // candelas
	UPROPERTY(EditAnywhere, Category = "Lamp") float Radius = 800.0f;
	UPROPERTY(EditAnywhere, Category = "Lamp") FLinearColor Colour = FLinearColor(1.0f, 0.9f, 0.78f, 1.0f);
	UPROPERTY(EditAnywhere, Category = "Lamp") bool bShadows = true;

	UPROPERTY(VisibleAnywhere, Category = "Lamp") TObjectPtr<USceneComponent> Hang;      // the ceiling point
	UPROPERTY(VisibleAnywhere, Category = "Lamp") TObjectPtr<USceneComponent> Pivot;     // swings
	UPROPERTY(VisibleAnywhere, Category = "Lamp") TObjectPtr<UStaticMeshComponent> Cable;
	UPROPERTY(VisibleAnywhere, Category = "Lamp") TObjectPtr<UStaticMeshComponent> Lamp;
	UPROPERTY(VisibleAnywhere, Category = "Lamp") TObjectPtr<UPointLightComponent> Light;

private:
	float Clock = 0.0f;
	float Amplitude = 12.0f;
	float Phase = 0.0f;
	float PlaneYaw = 0.0f;        // the swing plane, wandering slowly
	float NextGust = 6.0f;
	void Arrange();
};
