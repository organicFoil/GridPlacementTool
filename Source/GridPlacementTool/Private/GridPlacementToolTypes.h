// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;

/** Current editor selection (scene actors and/or Content Browser assets) the tool operates on. */
struct FPlacementSelection
{
	TArray<AActor*> Actors;
	TArray<UObject*> Assets;
};
