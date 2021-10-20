#pragma once

#include "CoreMinimal.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "FMountModule"
enum class AssetRelationType
{
	Dependency,
	Referencer
};

class Utilities
{
public:

	Utilities();
	static Utilities& Get();
	void PreInit();
	void Init();

	static void Log(const FString& log) { UE_LOG(LogTemp, Log, TEXT("CommonUtility: %s"), *log); };
	//static void Log(const FName& log) { UE_LOG(LogTemp, Log, TEXT("CommonUtility: %s"), *log.ToString()); };
	static void CreateUniqueAssetName(const FString& InBasePackageName, const FString& InSuffix, FString& OutPackageName, FString& OutAssetName);

	void AddUICommand(TSharedPtr< FUICommandInfo > uiCommand, FExecuteAction ExecuteAction);
	void RegisterMenuCallbacks(FMenuBuilder& MenuBuilder);
	void RegisterBottomButton(FMenuBuilder& MenuBuilder);
	void RegisterGetMountPathButton(FMenuBuilder& MenuBuilder);
	void RegisterLevelMountButton(FMenuBuilder& MenuBuilder);

	// Util Functions
	bool SelectMultiDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, TArray<FString>& OutFolderNames);
	bool SelectLevelMountConfig(TArray<FString>& OutFileNames);
	bool SelectProjectDialog(FString& OutFolderName);
	bool SelectMultiFilesDialog(const FString& DialogTitle, const FString& FileType, TArray<FString>& OutFileNames);
	static FString ProjContentDir();
	static bool CombineActor(TArray<AActor*> Actors, AStaticMeshActor*& OutActor, FString MeshName, FString Path, bool SpawnMesh, bool DestroyReplacedActors);
	static FString SerializeIndex(int32 inNumber, int32 ZeroNumber);

	// Getters
	TSharedPtr<FUICommandList> GetCommands() { return PluginCommands; };
	FString GetProjectConfigPath() { return mConfigPath; };
	FString GetPluginConfigPath() { return mPluginConfigPath; };
	FString GetPluginPath() { return mPluginPath; };
	FString GetProjIniName() { return mIniName; };
	FString GetPluginVersion() { return mPluginVersionName; };

	// Get asset thumbnail from short path, will cached 
	TSharedPtr<FAssetThumbnail> GetAssetThumbnail(const FString& filePath);
	TSharedRef<SWidget> GetAssetThumbnailWidget(const FString& filePath);
	static void ExportThumbnailJPG(UObject* assetObj, const int32& resolutionX, const int32& resolutionY, const FString& OutputPath);

	// Get FAssetData
	static void GetAssetsUnderPath(TArray<FName> paths, bool isRecursive, TArray<FAssetData>& OutAssetDatas);
	static bool GetAssetDataAt(const FString& dataPath, FAssetData& OutAssetData);

	// Copy from ContentBrowserUtils
	static void GetObjectsInAssetData(const TArray<FAssetData>& AssetList, TArray<UObject*>& OutDroppedObjects);
	static void MoveAssets(const TArray<UObject*>& Assets, const FString& DestPath, TMap<UObject*, FString> NewNameMap, const FString& SourcePath = FString());
	static void CopyMetaData(TMap<FName, FString> SourceAndDestPackages);

	//Separate a Word into char
	static TArray<FString> SeparateWord(FString word);

	// Encrypt and decrypt
	static void ShutdownEngine();

	// AdvancedCopy
	static void AdvancedCopyAssets(const TArray<FAssetData> Assets, const FString& DestPath);

	static FString GetSHA2(FString inPath);

	void checkUpdate();
	void SetLevelName();
	bool IsRefByLevel(const FAssetData& inAsset, TMap<FName, bool> outCachedPackages);
	bool IsRefByLevel(const FAssetData& inAsset);
	void RecursiveGetDependencies(const FName& PackageName, TSet<FName>& AllDependencies) const;
	bool IsInShanghai();
	FString GetAssetPathPrefixWhenUpload(const FAssetData& inAsset, const FString& libraryName);
	FString GetLibNameFromSelectedPath(const FString& SelectedPath);

	static void ImportAbc(TArray<FString> FileNames);
	static void ImportSingleAbc(const FString& InFileName, const FString& OutFilePath);

private:
	void makeIniFile(const FString& iniFile);
	void updatePluginByGogs();
	void updatePluginByDrive();
	void onCloseNotify();
	void onLinkTo(FString url);
	void showPluginContents();
	void updateByGogs();
	void updateByDrive();
	void WriteEngineConfig();

	FDelegateHandle mTickDelegateHandle;
	FTickerDelegate mTickDelegate;
	// CommonUtility tools config Dir
	FString mPluginPath;
	// Project config file
	FString mConfigPath;
	// CommonUtility tools config file
	FString mPluginConfigPath;

	FString mPluginVersionName;

	FString mLevelName;

	// Link configs
	TMap<FString, FString> mLinkMap;
	TWeakPtr<SNotificationItem> mNotifyItem;
	const FString mIniName = TEXT("MountConfig.ini");
	const FString mPluginIniName = TEXT("MountPluginConfig.ini");
	TSharedPtr<class FUICommandList> PluginCommands;

	// Thumbnail cache
	TMap<FString, TSharedPtr<FAssetThumbnail> > mCachedThumbnailMap;
	TSharedPtr<FAssetThumbnailPool> mAssetThumbnailPool;
};
#undef LOCTEXT_NAMESPACE