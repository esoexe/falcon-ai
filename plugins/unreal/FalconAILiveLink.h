// Falcon AI v3.0 — Official Unreal Engine 5.4 / 5.5 C++ Live-Link Editor Subsystem (Zero-Python C++)
// Place in `Source/FalconAIEditor/Public/FalconAILiveLink.h` and enable `HTTP`, `Json`, `UnrealEd`, `AssetTools`.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "FalconAILiveLink.generated.h"

UCLASS()
class UFalconAILiveLinkSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "FalconAI")
    void GenerateAndImportAsset(
        const FString& Prompt,
        int32 TargetPolygons = 15000,
        bool bQuadDominant = true,
        bool bAutoRig = true,
        const FString& ServerUrl = TEXT("http://127.0.0.1:8000")
    );
};
