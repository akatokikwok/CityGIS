// Copyright Epic Games, Inc. All Rights Reserved.

#include "CityGISGameMode.h"
#include "CityGISCharacter.h"
#include "UObject/ConstructorHelpers.h"

ACityGISGameMode::ACityGISGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
