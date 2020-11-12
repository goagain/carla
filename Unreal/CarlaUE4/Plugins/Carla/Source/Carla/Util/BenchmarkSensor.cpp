// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.


#include "BenchmarkSensor.h"

ABenchmarkSensor::ABenchmarkSensor(const FObjectInitializer &ObjectInitializer)
  : Super(ObjectInitializer)
{
  PrimaryActorTick.bCanEverTick = true;
}

FActorDefinition ABenchmarkSensor::GetSensorDefinition()
{
  return UActorBlueprintFunctionLibrary::MakeBenchmarkSensorDefinition();
}

void ABenchmarkSensor::Set(const FActorDescription &Description)
{
  Super::Set(Description);
  UActorBlueprintFunctionLibrary::SetBenchmarkSensor(Description, this);
}

void ABenchmarkSensor::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);

  auto DataStream = GetDataStream(*this);
  DataStream.Send(*this, BenchmarkData, DataStream.PopBufferFromPool());
}

ABenchmarkSensor::StatsReturnType ABenchmarkSensor::CollectFrameStats(
  UWorld* World,
  const ABenchmarkSensor::StatsQueriesType& Queries)
{
  StatsReturnType Result;

  // Get the reference to the stats thread
  FStatsThreadStateOverlay& StatsThread = (FStatsThreadStateOverlay&)FStatsThreadState::GetLocalState(); // FStatsThreadState&

  // Get the number of the last processed frame and check if it is valid (just for sure)
  int64 LastGoodGameFrame = StatsThread.GetLastFullFrameProcessed();
  if (StatsThread.IsFrameValid(LastGoodGameFrame) == false)
  {
    return Result;
  }

  if(once)
  {
    once = false;
    // DumpStatGroups(StatsThread);
  }

  UE_LOG(LogCarla, Error, TEXT("=================================================="));
  UE_LOG(LogCarla, Error, TEXT("Frame %d\n"), LastGoodGameFrame);
  for(auto It : Queries)
  {
    FString GroupName(It.first.c_str());

    TArray<FString> StringArray;
    GroupName.ParseIntoArray(StringArray, TEXT("_"), false);
    FString Cmd = StringArray[StringArray.Num() - 1];
    // Enable command to capture it
    World->Exec(World, *Cmd);

    UE_LOG(LogCarla, Error, TEXT("StatGroup %s %s"), *GroupName, *Cmd);

    TSet<FName> StatNamesSet;
    // std::pair<StatsQueriesType::iterator, StatsQueriesType::iterator> StatNames;
    auto StatNames = Queries.equal_range(It.first);
    for (auto It2 = StatNames.first; It2 != StatNames.second; It2++)
    {
      FName StatName(It2->second.c_str());

      UE_LOG(LogCarla, Error, TEXT("\tStat %s"), *StatName.ToString());

      StatNamesSet.Emplace(StatName);
    }
    UE_LOG(LogCarla, Error, TEXT("Num Stats %d"), StatNamesSet.Num());

    //CollectStatsFromGroup(StatsThread, TEXT("STATGROUP_SceneRendering"), LastGoodGameFrame);

    CollectStatsFromGroup(StatsThread, *GroupName, StatNamesSet, LastGoodGameFrame);

    // Disable command to capture it
    //World->Exec(World, *Cmd);
  }

  UE_LOG(LogCarla, Error, TEXT("=================================================="));

  return Result;

}

void ABenchmarkSensor::DumpStatGroups(const FStatsThreadStateOverlay& StatsThread)
{
  const TMultiMap<FName, FName>& Groups = StatsThread.Groups;
  TArray<FName> GroupNames;
  FString Output = "==================================================\n";

  Groups.GenerateKeyArray(GroupNames);

  for(const FName& GroupName : GroupNames)
  {
    FString GroupNameString = GroupName.ToString();
    if(GroupNameString.Contains(TEXT("UObject"))) continue;
    Output += GroupNameString + "\n";

    TArray<FName> GroupItems;
    Groups.MultiFind(GroupName, GroupItems);

    for(const FName& GroupItem : GroupItems)
    {
      Output += "    " + GroupItem.ToString() + "\n";
    }
  }

  Output += "==================================================";
  UE_LOG(LogCarla, Error, TEXT("%s"), *Output);
}

void ABenchmarkSensor::CollectStatsFromGroup(
  const FStatsThreadStateOverlay& StatsThread,
  const FName& GroupName,
  const TSet<FName>& StatNames,
  int64 Frame)
{
  // Gather the names of the stats that are in this group.
  //TArray<FName> GroupItems;
  //StatsThread.Groups.MultiFind(GroupName, GroupItems);

  // Prepare the set of names and raw names of the stats we want to get
  /*
  TSet<FName> EnabledItems;
  for (const FName& ShortName : GroupItems)
  {
    UE_LOG(LogCarla, Error, TEXT("Items %s"), *ShortName.ToString());
    //EnabledItems.Add(ShortName);
    // TODO: RawName is faster

    //const FStatMessage* LongName = StatsThread.ShortNameToLongName.Find(ShortName);
    //if (LongName)
    //{
    //  EnabledItems.Add(LongName->NameAndInfo.GetRawName());
    //}
  }
  */

  // Create a filter (needed by stats gathering function)
  FGroupFilter Filter(StatNames);

  // Create empty stat stack node (needed by stats gathering function)
  FRawStatStackNode HierarchyInclusive;

  // Prepare the array for stat messages
  TArray<FStatMessage> NonStackStats;

  // COLLECT ALL STATS TO THE ARRAY HERE
  StatsThread.UncondenseStackStats(Frame, HierarchyInclusive, &Filter, &NonStackStats);
  //StatsThread.UncondenseStackStats(Frame, HierarchyInclusive, nullptr, &NonStackStats);
  UE_LOG(LogCarla, Error, TEXT("NonStackStats %d"), NonStackStats.Num());

  // Go through all stats
  // There are many ways to display them, dig around the code to display it as you want :)
  for (const FStatMessage& Stat : NonStackStats)
  {
    // Here we are getting the raw name
    FName StatName = Stat.NameAndInfo.GetShortName(); //GetRawName();

    // Here we are getting values
    switch (Stat.NameAndInfo.GetField<EStatDataType>())
    {
      case EStatDataType::ST_int64:
        {
          int64 Value = Stat.GetValue_int64();
          UE_LOG(LogCarla, Error, TEXT("Stat: %s is int64: %lld"), *StatName.ToString(), Value);
        }
        break;
      case EStatDataType::ST_double:
        {
          double Value = Stat.GetValue_double();
          UE_LOG(LogCarla, Error, TEXT("Stat: %s is double: %f"), *StatName.ToString(), Value);
        }
        break;
      case EStatDataType::ST_Ptr:
        {
          uint64 Value = Stat.GetValue_Ptr();
          UE_LOG(LogCarla, Error, TEXT("Stat: %s is uint64: %lld"), *StatName.ToString(), Value);
        }
        break;
      default:
        UE_LOG(LogCarla, Error, TEXT("Stat: %s is %d"), *StatName.ToString(), static_cast<int32>(Stat.NameAndInfo.GetField<EStatDataType>()));
        check(0);
    }
  }
}