// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "LyraEmoteSoundInterface.generated.h"

#define UE_API LYRAGAME_API

class UAudioComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class ULyraEmoteSoundInterface : public UInterface
{
    GENERATED_BODY()
};

class UE_API ILyraEmoteSoundInterface
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Emote")
    void SetEmoteAudioComponent(UAudioComponent* InAudioComponent);
};

#undef UE_API
