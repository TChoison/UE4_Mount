// Copyright Epic Games, Inc. All Rights Reserved.

#include "MountCommands.h"

#define LOCTEXT_NAMESPACE "FMountModule"

void FMountCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "Mount", "Execute Mount action", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(UnMountAction, "UnMount", "Execute UnMount action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
