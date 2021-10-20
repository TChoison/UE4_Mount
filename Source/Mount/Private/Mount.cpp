// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mount.h"
#include "MountStyle.h"
#include "MountCommands.h"
#include "MountManager.h"
#include "LevelEditor.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Utilities.h"

static const FName MountTabName("Mount");

#define LOCTEXT_NAMESPACE "FMountModule"

void FMountModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	Utilities::Get().PreInit();

	FMountStyle::Initialize();
	FMountStyle::ReloadTextures();

	FMountCommands::Register();
	Utilities::Get().Init();

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	{
		TSharedPtr<FExtender> mExtender = MountManager::Get().extender;
		mExtender = MakeShareable(new FExtender());
		mExtender->AddToolBarExtension("Settings", EExtensionHook::After, Utilities::Get().GetCommands(), FToolBarExtensionDelegate::CreateRaw(this, &FMountModule::ExtendUI));
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(mExtender);
	}

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMountModule::RegisterMenus));
}

void FMountModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FMountStyle::Shutdown();

	FMountCommands::Unregister();
}

TSharedRef< SWidget > FMountModule::GenerateMenu()
{
	FMenuBuilder MenuBuilder(true, Utilities::Get().GetCommands());

	// Menu Lists...
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("St_funcs", ""));
	//MenuBuilder.AddMenuEntry(FMountCommands::Get().PluginAction, NAME_None, LOCTEXT("StartMount", "Mount External Dictory"));
	MountManager::Get().GenMenu(MenuBuilder);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FMountModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FMountCommands::Get().PluginAction, Utilities::Get().GetCommands());
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FMountCommands::Get().PluginAction));
				Entry.SetCommandList(Utilities::Get().GetCommands());
			}
		}
	}
}

void FMountModule::ExtendUI(FToolBarBuilder& Builder)
{
	Builder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FMountModule::GenerateMenu),
		LOCTEXT("MountButton", "UnMount"),
		LOCTEXT("MountButtonTips", "UnMount"),
		FSlateIcon(FMountStyle::GetStyleSetName(), "Mount.UnMountAction")
	);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMountModule, Mount)