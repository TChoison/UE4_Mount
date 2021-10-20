#pragma once
#include "CoreMinimal.h"
#include "LevelEditor.h"

struct MountData
{
	FString RootDir;
	TArray<FString> SubDirs;

	MountData() {} 
	MountData(const FString& dataString)
	{
		FromString(dataString);
	}
	MountData(const FString& rootDir, const FString& subDir)
	{
		FromString(rootDir, subDir);
	}

	void FromString(FString dataString)
	{
		if (dataString.StartsWith(TEXT("("))) {
			dataString.RemoveFromStart(TEXT("("));
			dataString.RemoveFromEnd(TEXT(")"));

			FString root;
			if (FParse::Value(*dataString, TEXT("RootDir="), root)) {
				RootDir = root;
			}

			FString subs;
			if (FParse::Value(*dataString, TEXT("SubDirs="), subs)) {
				subs.ParseIntoArray(SubDirs, TEXT(","));
			}
		}
		else
		{
			// Old data support
			RootDir = dataString;
			SubDirs.Add(dataString);
		}
	}

	void FromString(const FString& rootDir, const FString& subDir)
	{
		RootDir = rootDir;
		subDir.ParseIntoArray(SubDirs, TEXT(","));
	}

	FString ToString()
	{
		FString ret = TEXT("(RootDir=\"{0}\",SubDirs=\"{1}\")");
		FString SubDirStr = FString::Join(SubDirs, TEXT(","));
		ret = FString::Format(*ret, { RootDir, SubDirStr });
		return ret;
	}
};

enum class MountMethod {
	ByLevelConfig,
	ByDirectory,
};

class MountManager
{
public:
	MountManager();
	~MountManager();
	static MountManager& Get();
	void Init(const FString& pluginPath);
	void Shutdown();
	void GenMenu(FMenuBuilder& MenuBuilder);

	TSharedPtr<FExtender> extender;

	// Interface
	TArray<FString> GetLevelMountPath();

	// Menu button events...
	void onMountButtonClick();
	void onUnmountButtonClick(MountData unmountData);
	void onMountRecommendedAsset(FString dir);

	//Mount config
	void loadMountConfigs();
	void mountIniFile(const FString& iniPath, MountMethod by);
	void registerMountPoint(const FString& path, bool isNewAdd = false);

	TArray<MountData> mMountedDatas;
	const FString mAssetSection = TEXT("LevelMountPath");
	MountMethod mByMethod;

private:

	// Mount config
	void addMountedData(const MountData& data);
	void removeMountedData(const MountData& data);
	void getMountedData(TArray<MountData>& datas);
	void resetConfigMountedData(const TArray<MountData>& datas);

	void readonlyFolder(const FString& folder);
	void config2StrArr(TArray<FString>& dataStrs, const TArray<MountData>& inDatas = TArray<MountData>());
	void StrArr2Config(const TArray<FString>& strArr, TArray<MountData>& outDatas);
	void writeMountSign(const FString& dir, const FString& content);
	void writeAssetMountDirs();
	void mountMustMountDirs();

	// Menus...
	// Sub menu for unmount mounted folder
	void createUnmountSubMenu(FMenuBuilder& MenuBuilder);
	// Sub menu for mount recommended assets that user don't need to select in dialog 
	void createOptionalDirSubMenu(FMenuBuilder& MenuBuilder);
	void createUnmountLevelMenu(FMenuBuilder& MenuBuilder);

	// mount level
	void levelRegisterMountPoint(const TArray<FString>& paths, bool isNewAdd = false);
	void registerLevelFromFile(const TArray<FString>& files);
	void getLevelMountPathByLoopAseetPool();
	void onGetLevelMountPathButtonClick();
	void onUnmountLevelButtonClick(FString levelName);
	void onMountLevelClick();
	TArray<FString> keepOuterFolder(TArray<FString>);

	// Mount point register manager
	void AddMountPoint(FString, FString);

	const FString mSectionName = TEXT("MountConfig");
	const FString mMountPoint = TEXT("/Game/");
	FString mMountLogPath;
	TArray<FString> mMountNeedLogDirs;
	TArray<FString> mReadonlyMountPath;
	TMap<FString, FString> mMountRules;
	TMap<FString, FString> mOptionalDirs;
	TMap<FString, FString> mMustMountDirs;
	FString mMakeReadonlyEXEPath;
	TArray<FString> mAssetMountDirs;
	TArray<FString> mMountLevelNames;
	TMap<FString, FString> mMountPoints;

	// Mount Point - Long Mount Full Path
	TMap<FString, FString> mPathToMountPoint;
	TArray<FString> mMountPaths;
};

