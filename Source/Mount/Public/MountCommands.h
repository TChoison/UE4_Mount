// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "MountStyle.h"

class FMountCommands : public TCommands<FMountCommands>
{
public:

	FMountCommands()
		: TCommands<FMountCommands>(TEXT("Mount"), NSLOCTEXT("Contexts", "Mount", "Mount Plugin"), NAME_None, FMountStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
	TSharedPtr< FUICommandInfo > UnMountAction;
};
