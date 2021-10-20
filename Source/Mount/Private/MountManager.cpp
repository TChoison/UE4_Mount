#include "MountManager.h"
#include "Modules/ModuleManager.h"
#include "Mount.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "MountCommands.h"
#include "Utilities.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "Misc/ScopedSlowTask.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/AssetManager.h"
#include "AssetTools/Private/SDiscoveringAssetsDialog.h"
#include "Runtime/Sockets/Public/SocketSubsystem.h"
#include "IPAddress.h"

#define LOCTEXT_NAMESPACE "FMountModule"
MountManager::MountManager()
{
}
MountManager::~MountManager()
{
}
MountManager& MountManager::Get()
{
	static TUniquePtr<MountManager> Singleton;
	if (!Singleton) {
		Singleton = MakeUnique<MountManager>();
	}
	return *Singleton;
}

void MountManager::Init(const FString& pluginPath)
{
	// Register button callback
	Utilities::Get().AddUICommand(
		FMountCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &MountManager::onMountButtonClick)
	);

	loadMountConfigs();

	// choose mount method
	FString iniPath = Utilities::Get().GetProjectConfigPath();
	TArray<FString> levelMountStrs;
	GConfig->GetArray(*mAssetSection, TEXT("MountedPaths"), levelMountStrs, *iniPath);
	mByMethod = levelMountStrs.Num() > 0 ? MountMethod::ByLevelConfig : MountMethod::ByDirectory;

	// Mount folder from project ini file
	mountIniFile(iniPath, mByMethod);

	// Get make readonly exe
	mMakeReadonlyEXEPath = FPaths::Combine(pluginPath, TEXT("cmd_MakeFolderReadonly.exe"));
}

void MountManager::Shutdown()
{
	TArray<FString> paths;
	FString iniPath = Utilities::Get().GetProjectConfigPath();
	
	switch (mByMethod)
	{
	case MountMethod::ByLevelConfig:
		GConfig->GetArray(*mSectionName, TEXT("MountedPaths"), paths, *iniPath);
		for (TArray<FString>::TIterator iter = paths.CreateIterator(); iter; ++iter)
		{
			FString levelName;
			if (FParse::Value(**iter, TEXT("Level="), levelName))
			{
				writeMountSign(levelName, TEXT("Stop Mount Level"));
			}
		}

		break;
	case MountMethod::ByDirectory:
	default:
		GConfig->GetArray(*mSectionName, TEXT("MountedDirs"), paths, *iniPath);
		for (FString path : paths) {
			writeMountSign(MountData(path).RootDir, TEXT("Stop Mount"));
		}
		break;
	}
}

// Generate menus...
void MountManager::GenMenu(FMenuBuilder& MenuBuilder)
{
	// Unmount menu
	if (mMountedDatas.Num() > 0) {
		MenuBuilder.AddSubMenu(
			LOCTEXT("UnmountDirectory", "Directory"),
			LOCTEXT("UnmountMountedDirectory", "UnMount by clicking at the folder"),
			FNewMenuDelegate::CreateRaw(this, &MountManager::createUnmountSubMenu)
		);
	}
}

void MountManager::createUnmountSubMenu(FMenuBuilder& MenuBuilder)
{
	int32 num = mMountedDatas.Num();
	//FString prefix = LOCTEXT("Unmount", "Unmount").ToString();
	for (int32 i = 0; i < num; ++i) {
		MenuBuilder.AddMenuEntry(
			FText::FromString(FString::Printf(TEXT("%s"), *mMountedDatas[i].RootDir)),
			FText::FromString(TEXT("")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &MountManager::onUnmountButtonClick, mMountedDatas[i]))
		);
	}
}

// Button callback
void MountManager::onMountButtonClick()
{
	// Show dialog
	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const FString Title = TEXT("Choose a directory to mount");
	FString defaultDir;
	TArray<FString> selectedFolders;
	bool bFolderSelected = Utilities::Get().SelectMultiDirectoryDialog(ParentWindowWindowHandle, Title, defaultDir, selectedFolders);

	// Mount folder
	if (bFolderSelected) {
		int32 length = selectedFolders.Num();
		for (int32 i = 0; i < length; i++)
		{
			registerMountPoint(selectedFolders[i], true);
		}
	}
}

void MountManager::onUnmountButtonClick(MountData unmountData)
{
	UE_LOG(LogTemp, Log, TEXT("unmount : %s"), *unmountData.RootDir);
	removeMountedData(unmountData);

	writeMountSign(unmountData.RootDir, TEXT("Remove Mount Point"));
}

void MountManager::onMountRecommendedAsset(FString dir)
{
	registerMountPoint(dir, true);
}

// Aux...
void MountManager::mountIniFile(const FString& iniPath, MountMethod by)
{
	TArray<FString> Paths;
	switch (by) {
	case MountMethod::ByLevelConfig:
		break;
	case MountMethod::ByDirectory:
	default:
		getMountedData(mMountedDatas);
		GConfig->GetArray(*mSectionName, TEXT("MountedDirs"), Paths, *iniPath);
		for (FString path : Paths) {
			registerMountPoint(MountData(path).RootDir);
		}
		break;
	}
}

void MountManager::registerMountPoint(const FString& path, bool isNewAdd /* = false */)
{
	MountData data;
	data.RootDir = FString(path);
	if (isNewAdd) writeMountSign(path, TEXT("Add Mount Point"));

	readonlyFolder(path);
	bool bInConfig = false;
	for (const TPair<FString, FString> pair : mMountRules) {
		if (path.Contains(pair.Key)) {
			FString left, right;
			if (path.Split(pair.Key, &left, &right, ESearchCase::IgnoreCase, ESearchDir::FromEnd)) {
				// Mount incoming path
				right.RemoveFromStart(TEXT("/"));
				FString mountPoint = FPaths::Combine(mMountPoint, right);
				AddMountPoint(mountPoint, path);
				UE_LOG(LogTemp, Log, TEXT("mount:%s -> %s"), *mountPoint, *path);
				if (isNewAdd) data.SubDirs.Add(path);
				writeMountSign(path, TEXT("Start Mount"));

				// Mount required path
				FString requiredFolders;
				if (FParse::Value(*pair.Value, TEXT("Requires="), requiredFolders)) {
					FString requireStart = FPaths::Combine(left, pair.Key);
					TArray<FString> pathArr;
					requiredFolders.ParseIntoArray(pathArr, TEXT(","));

					for (FString requireFolder : pathArr) {
						mountPoint = FPaths::Combine(mMountPoint, requireFolder);
						FString requirePath = FPaths::Combine(requireStart, requireFolder);
						AddMountPoint(mountPoint, requirePath);
						UE_LOG(LogTemp, Log, TEXT("mount:%s -> %s"), *mountPoint, *requirePath);
						if (isNewAdd)
						{
							MountData requireData(requirePath, requirePath);
							addMountedData(requireData);
						}

						writeMountSign(requireFolder, TEXT("Start Mount"));
					}
				}

				// Search config file to mount
				/*FString configDir = FPaths::Combine(left, TEXT("Config"));
				if (FPaths::DirectoryExists(configDir)) {
					FString configFile = FPaths::Combine(configDir, ToolUtils::Get().GetProjIniName());
					if (FPaths::FileExists(configFile)) {
						mountIniFile(configFile);
					}
				}*/

				bInConfig = true;
				break;
			}
		}
	}

	if (!bInConfig) {
		if (FPaths::GetBaseFilename(path).Equals(TEXT("Content"))) {
			// Mount subfolder of Content
			// Warning: Do not mount "/Game/", or you will not save assets to your disk.
			TArray<FString> files;
			IFileManager::Get().FindFiles(files, *(path + TEXT("/*")), false, true);
			FString absPath;
			for (FString file : files) {
				absPath = FPaths::Combine(path, file);
				if (FPaths::DirectoryExists(absPath)) {
					AddMountPoint(mMountPoint / file, absPath);
					UE_LOG(LogTemp, Log, TEXT("mount:%s -> %s"), *(mMountPoint / file), *absPath);
					if (isNewAdd) data.SubDirs.Add(absPath);
				}
			}
		}
		else
		{
			// Mount to external folder
			FString shortPath = FPaths::Combine(TEXT("/Game/"), FPaths::GetBaseFilename(path));
			AddMountPoint(shortPath, path);
			UE_LOG(LogTemp, Log, TEXT("mount:%s -> %s"), *shortPath, *path);
			if (isNewAdd) data.SubDirs.Add(path);
		}
		writeMountSign(path, TEXT("Start Mount"));
	}

	if (data.SubDirs.Num() > 0)
	{
		addMountedData(data);
	}
}

void MountManager::getMountedData(TArray<MountData>& datas)
{
	TArray<FString> dataStrings;
	GConfig->GetArray(*mSectionName, TEXT("MountedDirs"), dataStrings, Utilities::Get().GetProjectConfigPath());
	for (FString str : dataStrings)
	{
		datas.Add(MountData(str));
	}
}

void MountManager::addMountedData(const MountData& inData)
{
	// Dupcheck
	TArray<MountData> datas;
	getMountedData(datas);
	for (MountData data : datas)
	{
		if (data.RootDir.Equals(inData.RootDir))
		{
			return;
		}
	}

	// Cache
	datas.Add(inData);
	resetConfigMountedData(datas);
}

void MountManager::removeMountedData(const MountData& inData)
{
	TArray<MountData> datas;
	getMountedData(datas);
	int32 len = datas.Num();
	for (int32 i = 0; i < len; ++i)
	{
		if (datas[i].RootDir.Equals(inData.RootDir))
		{
			datas.RemoveAt(i);
			break;
		}
	}
	resetConfigMountedData(datas);
}

void MountManager::resetConfigMountedData(const TArray<MountData>& datas)
{
	FString configPath = Utilities::Get().GetProjectConfigPath();
	TArray<FString> dataStrs;
	config2StrArr(dataStrs, datas);
	GConfig->SetArray(*mSectionName, TEXT("MountedDirs"), dataStrs, *configPath);
	GConfig->Flush(false, *configPath);
	mMountedDatas = datas;
}

void MountManager::readonlyFolder(const FString& folder)
{
	// Make folder readonly
	bool needReadOnly = false;
	for (FString readonlyWord : mReadonlyMountPath)
	{
		if (folder.Contains(readonlyWord)) {
			needReadOnly = true;
			break;
		}
	}

	if (needReadOnly && FPaths::FileExists(mMakeReadonlyEXEPath))
	{
		FString params = TEXT("-readonly ") + folder;
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*mMakeReadonlyEXEPath, *params);
	}
}

void MountManager::config2StrArr(TArray<FString>& dataStrs, const TArray<MountData>& inDatas /* = TArray<MountData>() */)
{
	TArray<MountData> datas;
	if (inDatas.Num() < 1)
		getMountedData(datas);
	else
		datas = inDatas;

	for (MountData data : inDatas)
	{
		dataStrs.Add(data.ToString());
	}
}

void MountManager::StrArr2Config(const TArray<FString>& strArr, TArray<MountData>& outDatas)
{
	for (FString str : strArr)
	{
		outDatas.Add(MountData(str));
	}
}

void MountManager::loadMountConfigs()
{
	FString configFile = Utilities::Get().GetPluginConfigPath();
	// Mount rules
	TMap<FString, FString> rules;
	TArray<FString> values;
	if (GConfig->GetArray(TEXT("MountRule"), TEXT("Rule"), values, *configFile)) {
		for (FString value : values) {
			FString entry = FString(value);
			entry.RemoveFromStart(TEXT("("));
			entry.RemoveFromEnd(TEXT(")"));

			FString subDir;
			if (FParse::Value(*entry, TEXT("SubDir="), subDir)) {
				rules.Add(subDir, entry);
				UE_LOG(LogTemp, Log, TEXT("Mount rule:%s -> %s"), *subDir, *entry);
			}
		}
	}
	mMountRules = rules;

	mReadonlyMountPath.Empty();
	TArray<FString> readonlyPaths;
	if (GConfig->GetArray(TEXT("ReadonlyMountPath"), TEXT("Path"), readonlyPaths, *configFile)) {
		for (FString path : readonlyPaths) {
			FString readonlyPath = path;
			mReadonlyMountPath.Add(readonlyPath);
		}
	}

	// Mount log path
	//GConfig->GetString(TEXT("MountLogRootPath"), TEXT("Path"), mMountLogPath, *configFile);
	TArray<FString> mountPathConfigs;
	if (GConfig->GetArray(TEXT("MountLogRootPath"), TEXT("Path"), mountPathConfigs, *configFile)) {
		for (FString path : mountPathConfigs) {
			if (FPaths::DirectoryExists(path))
			{
				mMountLogPath = path;
				break;
			}
		}
	}

	mMountNeedLogDirs.Empty();
	TArray<FString> needLogDirs;
	if (GConfig->GetArray(TEXT("MountNeedLogDir"), TEXT("Path"), needLogDirs, *configFile))
	{
		for (FString path : needLogDirs) {
			mMountNeedLogDirs.Add(FString(path));
		}
	}

	// Optional asset folder
	FKeyValueSink visitor;
	visitor.BindLambda([=](const TCHAR *Key, const TCHAR* Value) {
		mOptionalDirs.Add(Key, Value);
	});
	GConfig->ForEachEntry(visitor, TEXT("OptionalMountList"), *configFile);

	// must mount dirs
	FKeyValueSink MustMountVisitor;
	MustMountVisitor.BindLambda([=](const TCHAR* Key, const TCHAR* Value) {
		mMustMountDirs.Add(Key, Value);
	});
	GConfig->ForEachEntry(MustMountVisitor, TEXT("Server"), *configFile);
}

void MountManager::writeMountSign(const FString& dir, const FString& content)
{
	// Init failed
	if (!FPaths::DirectoryExists(mMountLogPath))
		return;

	// Not in check, ignore 
	bool needWrite = false;
	for (auto checkStr : mMountNeedLogDirs)
	{
		if (dir.Contains(checkStr)) {
			needWrite = true;
			break;
		}
	}
	if (!needWrite) return;

	FString userName = FPlatformProcess::UserName();
	FString comName = FPlatformProcess::ComputerName();
	FString IpName = TEXT("");
	bool bBindAll = false;
	TSharedRef<FInternetAddr> localIp = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bBindAll);
	if (localIp->IsValid())
	{
		IpName = localIp->ToString(false);
	}

	FDateTime date = FDateTime::Now();
	FString logContent = FString::Printf(TEXT("%s  -  %s  -  %s  -  %s  -  %s"), *date.ToString(), *comName, *userName, *IpName, *content);
	// Construct log path
	FString middlePath;
	if (dir.Contains(TEXT(":/")))
	{
		FString l, r;
		dir.Split(TEXT(":/"), &l, &r);
		middlePath = l + TEXT("/") + r;
	}
	else if (dir.Contains(TEXT("//")))
	{
		middlePath = FString(dir);
		middlePath.RemoveFromStart(TEXT("//"));
	}

	FString logDir = FPaths::Combine(mMountLogPath, middlePath);

	// Find log file for me
	FString tempLogFileName = TEXT("");
	FString newLogFileName = logContent + TEXT(".txt");
	TArray<FString> txtList;
	IFileManager& fileMgr = IFileManager::Get();
	fileMgr.FindFilesRecursive(txtList, *logDir, TEXT("*.txt"), true, false, false);
	for (auto txt : txtList)
	{
		if (txt.Contains(userName))
		{
			tempLogFileName = FString(txt);
			break;
		}
	}

	FString newLogPath = FPaths::Combine(logDir, newLogFileName);
	if (FPaths::FileExists(tempLogFileName)) {
		TArray<FString> strArr;
		FFileHelper::LoadFileToStringArray(strArr, *tempLogFileName);
		strArr.Add(logContent);
		FFileHelper::SaveStringArrayToFile(strArr, *tempLogFileName);
		fileMgr.Move(*newLogPath, *tempLogFileName);
	}
	else {
		FFileHelper::SaveStringToFile(logContent, *newLogPath);
	}
}

void MountManager::writeAssetMountDirs()
{
	FString ret = TEXT("(Level=\"{0}\",Dirs=\"{1}\")");
	FString Dirs = FString::Join(mAssetMountDirs, TEXT(","));
	auto World = GEditor->GetAllViewportClients()[0]->GetWorld();
	FString Level = World->GetName();
	ret = FString::Format(*ret, { Level, Dirs });
	FString configPath = Utilities::Get().GetProjectConfigPath();
	GConfig->SetString(*mAssetSection, TEXT("MountedPaths"), *ret, *configPath);
	GConfig->Flush(false, *configPath);
	mAssetMountDirs.Empty();
}

void MountManager::AddMountPoint(FString Point, FString Path)
{
	FString StrictPoint = Point;
	if (!StrictPoint.EndsWith(TEXT("/")))
	{
		StrictPoint += TEXT("/");
	}

	mMountPoints.Add(StrictPoint, Path);
	if (StrictPoint != mMountPoint)	mMountPaths.AddUnique(StrictPoint);
	FPackageName::RegisterMountPoint(StrictPoint, Path);
	if (mPathToMountPoint.Contains(Path))
	{
		mPathToMountPoint[Path] = StrictPoint;
	}
	else
	{
		mPathToMountPoint.Add(Path, StrictPoint);
	}
}

#undef LOCTEXT_NAMESPACE 