#include "Utilities.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"
#include "MountManager.h"

Utilities::Utilities()
{

}

Utilities& Utilities::Get()
{
	static TUniquePtr<Utilities> Singleton;
	if (!Singleton)
	{
		Singleton = MakeUnique<Utilities>();
	}
	return *Singleton;
}

void Utilities::PreInit()
{
	PluginCommands = MakeShareable(new FUICommandList);

	mConfigPath = FPaths::GetPath(FPaths::GetProjectFilePath()) / TEXT("Config") / mIniName;
	
	mPluginPath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPaths::GetProjectFilePath())) / TEXT("Plugins") / TEXT("Mount");
	if (!FPaths::DirectoryExists(mPluginPath)) {
		mPluginPath = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir()) / TEXT("Mount");
	}
	
	mPluginConfigPath = mPluginPath / TEXT("Config") / mPluginIniName;
	if (!FPaths::FileExists(mPluginConfigPath)) {
		makeIniFile(mPluginConfigPath);
	}
	
	auto plugin = IPluginManager::Get().FindPlugin(TEXT("Mount"));
	mPluginVersionName = plugin->GetDescriptor().VersionName;
}

void Utilities::Init()
{
	MountManager::Get().Init(mPluginPath);
}

void Utilities::AddUICommand(TSharedPtr< FUICommandInfo > uiCommand, FExecuteAction ExecuteAction)
{
	PluginCommands->MapAction(uiCommand, ExecuteAction);
}

// Util Functions
#include "Developer/DesktopPlatform/Private/Windows/WindowsRegistry.h"
#include "Developer/DesktopPlatform/Private/DesktopPlatformBase.h"
#include "Developer/DesktopPlatform/Private/Windows/DesktopPlatformWindows.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/COMPointer.h"
#include "HAL/FileManager.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <Winver.h>
#include <LM.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
bool Utilities::SelectMultiDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, TArray<FString>& OutFolderNames)
{
	FScopedSystemModalMode SystemModalScope;

	bool bSuccess = false;

	TComPtr<IFileOpenDialog> FileDialog;
	if (SUCCEEDED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&FileDialog))))
	{
		// Set this up as a folder picker
		{
			DWORD dwFlags = 0;
			FileDialog->GetOptions(&dwFlags);
			FileDialog->SetOptions(dwFlags | FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT);
		}

		// Set up common settings
		FileDialog->SetTitle(*DialogTitle);
		if (!DefaultPath.IsEmpty())
		{
			// SHCreateItemFromParsingName requires the given path be absolute and use \ rather than / as our normalized paths do
			FString DefaultWindowsPath = FPaths::ConvertRelativePathToFull(DefaultPath);
			DefaultWindowsPath.ReplaceInline(TEXT("/"), TEXT("\\"), ESearchCase::CaseSensitive);

			TComPtr<IShellItem> DefaultPathItem;
			if (SUCCEEDED(::SHCreateItemFromParsingName(*DefaultWindowsPath, nullptr, IID_PPV_ARGS(&DefaultPathItem))))
			{
				FileDialog->SetFolder(DefaultPathItem);
			}
		}

		// Show the picker
		if (SUCCEEDED(FileDialog->Show((HWND)ParentWindowHandle)))
		{
			IFileOpenDialog* FileOpenDialog = static_cast<IFileOpenDialog*>(FileDialog.Get());

			TComPtr<IShellItemArray> Results;
			if (SUCCEEDED(FileOpenDialog->GetResults(&Results)))
			{
				DWORD NumResults = 0;
				Results->GetCount(&NumResults);

				for (DWORD ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
				{

					TComPtr<IShellItem> Result;
					if (SUCCEEDED(Results->GetItemAt(ResultIndex, &Result)))
					{
						PWSTR pFilePath = nullptr;
						if (SUCCEEDED(Result->GetDisplayName(SIGDN_FILESYSPATH, &pFilePath)))
						{
							bSuccess = true;
							FString OutFolderName = pFilePath;
							FPaths::NormalizeDirectoryName(OutFolderName);

							OutFolderNames.Add(OutFolderName);

							::CoTaskMemFree(pFilePath);
						}
					}
				}
			}
		}
	}

	return bSuccess;
}


void Utilities::makeIniFile(const FString& iniFile)
{
	if (!FPaths::FileExists(iniFile)) {
		FFileHelper::SaveStringToFile(TEXT(" "), *iniFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}
