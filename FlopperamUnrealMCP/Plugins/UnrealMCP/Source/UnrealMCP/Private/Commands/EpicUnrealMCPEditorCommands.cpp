#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPErrorCodes.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetExportTask.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "ReferenceSkeleton.h"
#include "RetargetEditor/IKRetargetFactory.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargetChainMapping.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetSettings.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigController.h"
#include "RigEditor/IKRigDefinitionFactory.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "UObject/UnrealType.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/SkeletonFactory.h"
#include "Factories/AnimSequenceFactory.h"
#include "Exporters/AnimSequenceExporterFBX.h"
#include "Exporters/Exporter.h"
#include "Exporters/FbxExportOption.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
// Phase 1: Editor lifecycle
#include "Misc/EngineVersion.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "AI/NavigationSystemBase.h"
// Phase 1: Level management
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
// Phase 1: Viewport
#include "LevelEditor.h"
#include "Slate/SceneViewport.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ============================================================================
// Anonymous namespace: reflection helper functions (merged from standalone)
// ============================================================================
namespace
{
    static bool JsonValueToVector(const TSharedPtr<FJsonValue>& JsonValue, FVector& OutVector)
    {
        if (!JsonValue.IsValid())
        {
            return false;
        }

        if (JsonValue->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Values = JsonValue->AsArray();
            if (Values.Num() < 3)
            {
                return false;
            }

            OutVector = FVector(
                static_cast<float>(Values[0]->AsNumber()),
                static_cast<float>(Values[1]->AsNumber()),
                static_cast<float>(Values[2]->AsNumber())
            );
            return true;
        }

        if (JsonValue->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
            if (!Obj.IsValid())
            {
                return false;
            }

            double X = 0.0;
            double Y = 0.0;
            double Z = 0.0;
            bool bHasXYZ = Obj->TryGetNumberField(TEXT("x"), X) &&
                           Obj->TryGetNumberField(TEXT("y"), Y) &&
                           Obj->TryGetNumberField(TEXT("z"), Z);
            if (!bHasXYZ)
            {
                bHasXYZ = Obj->TryGetNumberField(TEXT("X"), X) &&
                          Obj->TryGetNumberField(TEXT("Y"), Y) &&
                          Obj->TryGetNumberField(TEXT("Z"), Z);
            }

            if (bHasXYZ)
            {
                OutVector = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
                return true;
            }
        }

        return false;
    }

    static bool JsonValueToRotator(const TSharedPtr<FJsonValue>& JsonValue, FRotator& OutRotator)
    {
        if (!JsonValue.IsValid())
        {
            return false;
        }

        if (JsonValue->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Values = JsonValue->AsArray();
            if (Values.Num() < 3)
            {
                return false;
            }

            OutRotator = FRotator(
                static_cast<float>(Values[0]->AsNumber()),
                static_cast<float>(Values[1]->AsNumber()),
                static_cast<float>(Values[2]->AsNumber())
            );
            return true;
        }

        if (JsonValue->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
            if (!Obj.IsValid())
            {
                return false;
            }

            double Pitch = 0.0;
            double Yaw = 0.0;
            double Roll = 0.0;
            bool bHasPYR = Obj->TryGetNumberField(TEXT("pitch"), Pitch) &&
                           Obj->TryGetNumberField(TEXT("yaw"), Yaw) &&
                           Obj->TryGetNumberField(TEXT("roll"), Roll);
            if (!bHasPYR)
            {
                bHasPYR = Obj->TryGetNumberField(TEXT("Pitch"), Pitch) &&
                          Obj->TryGetNumberField(TEXT("Yaw"), Yaw) &&
                          Obj->TryGetNumberField(TEXT("Roll"), Roll);
            }

            if (bHasPYR)
            {
                OutRotator = FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw), static_cast<float>(Roll));
                return true;
            }
        }

        return false;
    }

    static UObject* ResolveTargetObject(const TSharedPtr<FJsonObject>& Params, FString& OutError)
    {
        if (!Params.IsValid())
        {
            OutError = TEXT("Missing params object");
            return nullptr;
        }

        FString ObjectPath;
        if (Params->TryGetStringField(TEXT("object_path"), ObjectPath))
        {
            UObject* TargetObject = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
            if (!TargetObject)
            {
                TargetObject = LoadObject<UObject>(nullptr, *ObjectPath);
            }
            if (!TargetObject && ObjectPath.StartsWith(TEXT("/")) && !ObjectPath.Contains(TEXT(".")))
            {
                FString AssetName = FPaths::GetBaseFilename(ObjectPath);
                FString ExpandedPath = FString::Printf(TEXT("%s.%s"), *ObjectPath, *AssetName);
                TargetObject = StaticFindObject(UObject::StaticClass(), nullptr, *ExpandedPath);
                if (!TargetObject)
                {
                    TargetObject = LoadObject<UObject>(nullptr, *ExpandedPath);
                }
            }

            if (TargetObject)
            {
                return TargetObject;
            }

            OutError = FString::Printf(TEXT("Failed to resolve object_path: %s"), *ObjectPath);
            return nullptr;
        }

        FString ActorName;
        if (Params->TryGetStringField(TEXT("actor_name"), ActorName))
        {
            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
            if (!World)
            {
                OutError = TEXT("Failed to resolve editor world for actor lookup");
                return nullptr;
            }

            TArray<AActor*> AllActors;
            UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
            for (AActor* Actor : AllActors)
            {
                if (Actor && Actor->GetName() == ActorName)
                {
                    return Actor;
                }
            }

            OutError = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
            return nullptr;
        }

        OutError = TEXT("Missing target object. Provide 'object_path' or 'actor_name'");
        return nullptr;
    }

    static bool SetPropertyFromJsonValue(FProperty* Property, void* ValuePtr, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
    {
        if (!Property || !ValuePtr || !JsonValue.IsValid())
        {
            OutError = TEXT("Invalid property/value input");
            return false;
        }

        if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
        {
            if (JsonValue->Type == EJson::Boolean)
            {
                BoolProp->SetPropertyValue(ValuePtr, JsonValue->AsBool());
                return true;
            }
            if (JsonValue->Type == EJson::Number)
            {
                BoolProp->SetPropertyValue(ValuePtr, JsonValue->AsNumber() != 0.0);
                return true;
            }
            OutError = TEXT("Expected bool or number");
            return false;
        }

        if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
        {
            IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(JsonValue->AsNumber()));
            return true;
        }

        if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
        {
            Int64Prop->SetPropertyValue(ValuePtr, static_cast<int64>(JsonValue->AsNumber()));
            return true;
        }

        if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
        {
            FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(JsonValue->AsNumber()));
            return true;
        }

        if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
        {
            DoubleProp->SetPropertyValue(ValuePtr, JsonValue->AsNumber());
            return true;
        }

        if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
        {
            StrProp->SetPropertyValue(ValuePtr, JsonValue->AsString());
            return true;
        }

        if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
        {
            NameProp->SetPropertyValue(ValuePtr, FName(*JsonValue->AsString()));
            return true;
        }

        if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
        {
            TextProp->SetPropertyValue(ValuePtr, FText::FromString(JsonValue->AsString()));
            return true;
        }

        if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
        {
            UEnum* EnumDef = ByteProp->GetIntPropertyEnum();
            if (EnumDef && JsonValue->Type == EJson::String)
            {
                FString EnumValueName = JsonValue->AsString();
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    OutError = FString::Printf(TEXT("Enum value not found: %s"), *EnumValueName);
                    return false;
                }
                ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumValue));
                return true;
            }
            ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(JsonValue->AsNumber()));
            return true;
        }

        if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            UEnum* EnumDef = EnumProp->GetEnum();
            FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
            if (!EnumDef || !Underlying)
            {
                OutError = TEXT("Invalid enum property");
                return false;
            }
            if (JsonValue->Type == EJson::String)
            {
                FString EnumValueName = JsonValue->AsString();
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    OutError = FString::Printf(TEXT("Enum value not found: %s"), *EnumValueName);
                    return false;
                }
                Underlying->SetIntPropertyValue(ValuePtr, EnumValue);
                return true;
            }
            Underlying->SetIntPropertyValue(ValuePtr, static_cast<int64>(JsonValue->AsNumber()));
            return true;
        }

        if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
        {
            if (JsonValue->Type == EJson::Null)
            {
                ObjectProp->SetObjectPropertyValue(ValuePtr, nullptr);
                return true;
            }
            FString ObjectPath = JsonValue->AsString();
            UClass* ExpectedClass = ObjectProp->PropertyClass ? ObjectProp->PropertyClass.Get() : UObject::StaticClass();
            UObject* ReferencedObject = StaticFindObject(ExpectedClass, nullptr, *ObjectPath);
            if (!ReferencedObject)
            {
                ReferencedObject = LoadObject<UObject>(nullptr, *ObjectPath);
            }
            if (!ReferencedObject && ObjectPath.StartsWith(TEXT("/")) && !ObjectPath.Contains(TEXT(".")))
            {
                FString AssetName = FPaths::GetBaseFilename(ObjectPath);
                FString ExpandedPath = FString::Printf(TEXT("%s.%s"), *ObjectPath, *AssetName);
                ReferencedObject = StaticFindObject(ExpectedClass, nullptr, *ExpandedPath);
                if (!ReferencedObject)
                {
                    ReferencedObject = LoadObject<UObject>(nullptr, *ExpandedPath);
                }
            }
            if (!ReferencedObject)
            {
                OutError = FString::Printf(TEXT("Failed to resolve object reference: %s"), *ObjectPath);
                return false;
            }
            if (!ReferencedObject->IsA(ExpectedClass))
            {
                OutError = FString::Printf(TEXT("Object '%s' is not of expected class '%s'"), *ObjectPath, *ExpectedClass->GetName());
                return false;
            }
            ObjectProp->SetObjectPropertyValue(ValuePtr, ReferencedObject);
            return true;
        }

        if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            if (StructProp->Struct == TBaseStructure<FVector>::Get())
            {
                FVector ParsedVector;
                if (!JsonValueToVector(JsonValue, ParsedVector))
                {
                    OutError = TEXT("Invalid FVector value");
                    return false;
                }
                *reinterpret_cast<FVector*>(ValuePtr) = ParsedVector;
                return true;
            }
            if (StructProp->Struct == TBaseStructure<FRotator>::Get())
            {
                FRotator ParsedRotator;
                if (!JsonValueToRotator(JsonValue, ParsedRotator))
                {
                    OutError = TEXT("Invalid FRotator value");
                    return false;
                }
                *reinterpret_cast<FRotator*>(ValuePtr) = ParsedRotator;
                return true;
            }
            if (StructProp->Struct == TBaseStructure<FTransform>::Get())
            {
                if (JsonValue->Type != EJson::Object)
                {
                    OutError = TEXT("FTransform expects object with location/rotation/scale");
                    return false;
                }
                TSharedPtr<FJsonObject> TransformObj = JsonValue->AsObject();
                if (!TransformObj.IsValid())
                {
                    OutError = TEXT("Invalid FTransform object");
                    return false;
                }
                FTransform ParsedTransform = *reinterpret_cast<FTransform*>(ValuePtr);
                if (TransformObj->HasField(TEXT("location")))
                {
                    FVector ParsedLocation;
                    if (!JsonValueToVector(TransformObj->TryGetField(TEXT("location")), ParsedLocation))
                    {
                        OutError = TEXT("Invalid transform.location");
                        return false;
                    }
                    ParsedTransform.SetLocation(ParsedLocation);
                }
                if (TransformObj->HasField(TEXT("rotation")))
                {
                    FRotator ParsedRotation;
                    if (!JsonValueToRotator(TransformObj->TryGetField(TEXT("rotation")), ParsedRotation))
                    {
                        OutError = TEXT("Invalid transform.rotation");
                        return false;
                    }
                    ParsedTransform.SetRotation(ParsedRotation.Quaternion());
                }
                if (TransformObj->HasField(TEXT("scale")))
                {
                    FVector ParsedScale;
                    if (!JsonValueToVector(TransformObj->TryGetField(TEXT("scale")), ParsedScale))
                    {
                        OutError = TEXT("Invalid transform.scale");
                        return false;
                    }
                    ParsedTransform.SetScale3D(ParsedScale);
                }
                *reinterpret_cast<FTransform*>(ValuePtr) = ParsedTransform;
                return true;
            }
        }

        OutError = FString::Printf(TEXT("Unsupported property type: %s"), *Property->GetClass()->GetName());
        return false;
    }

    static TSharedPtr<FJsonValue> PropertyValueToJson(FProperty* Property, const void* ValuePtr)
    {
        if (!Property || !ValuePtr)
        {
            return MakeShared<FJsonValueNull>();
        }

        if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
        {
            return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
        }
        if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
        {
            return MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue(ValuePtr));
        }
        if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
        {
            return MakeShared<FJsonValueNumber>(static_cast<double>(Int64Prop->GetPropertyValue(ValuePtr)));
        }
        if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
        {
            return MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue(ValuePtr));
        }
        if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
        {
            return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(ValuePtr));
        }
        if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
        {
            return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
        }
        if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
        {
            return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
        }
        if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
        {
            return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
        }
        if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
        {
            uint8 ByteValue = ByteProp->GetPropertyValue(ValuePtr);
            if (UEnum* EnumDef = ByteProp->GetIntPropertyEnum())
            {
                return MakeShared<FJsonValueString>(EnumDef->GetNameStringByValue(ByteValue));
            }
            return MakeShared<FJsonValueNumber>(ByteValue);
        }
        if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
            int64 EnumValue = Underlying ? Underlying->GetSignedIntPropertyValue(ValuePtr) : 0;
            if (UEnum* EnumDef = EnumProp->GetEnum())
            {
                return MakeShared<FJsonValueString>(EnumDef->GetNameStringByValue(EnumValue));
            }
            return MakeShared<FJsonValueNumber>(static_cast<double>(EnumValue));
        }
        if (const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
        {
            UObject* ReferencedObject = ObjectProp->GetObjectPropertyValue(ValuePtr);
            if (!ReferencedObject)
            {
                return MakeShared<FJsonValueNull>();
            }
            return MakeShared<FJsonValueString>(ReferencedObject->GetPathName());
        }
        if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            if (StructProp->Struct == TBaseStructure<FVector>::Get())
            {
                FVector Value = *reinterpret_cast<const FVector*>(ValuePtr);
                TArray<TSharedPtr<FJsonValue>> Arr;
                Arr.Add(MakeShared<FJsonValueNumber>(Value.X));
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Y));
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Z));
                return MakeShared<FJsonValueArray>(Arr);
            }
            if (StructProp->Struct == TBaseStructure<FRotator>::Get())
            {
                FRotator Value = *reinterpret_cast<const FRotator*>(ValuePtr);
                TArray<TSharedPtr<FJsonValue>> Arr;
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Pitch));
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Yaw));
                Arr.Add(MakeShared<FJsonValueNumber>(Value.Roll));
                return MakeShared<FJsonValueArray>(Arr);
            }
            if (StructProp->Struct == TBaseStructure<FTransform>::Get())
            {
                FTransform Value = *reinterpret_cast<const FTransform*>(ValuePtr);
                TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
                FVector Location = Value.GetLocation();
                FRotator Rotation = Value.GetRotation().Rotator();
                FVector Scale = Value.GetScale3D();

                TArray<TSharedPtr<FJsonValue>> LocationArr;
                LocationArr.Add(MakeShared<FJsonValueNumber>(Location.X));
                LocationArr.Add(MakeShared<FJsonValueNumber>(Location.Y));
                LocationArr.Add(MakeShared<FJsonValueNumber>(Location.Z));
                TransformObj->SetArrayField(TEXT("location"), LocationArr);

                TArray<TSharedPtr<FJsonValue>> RotationArr;
                RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
                RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
                RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
                TransformObj->SetArrayField(TEXT("rotation"), RotationArr);

                TArray<TSharedPtr<FJsonValue>> ScaleArr;
                ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.X));
                ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Y));
                ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Z));
                TransformObj->SetArrayField(TEXT("scale"), ScaleArr);

                return MakeShared<FJsonValueObject>(TransformObj);
            }
        }

        return MakeShared<FJsonValueString>(FString::Printf(TEXT("<unsupported_property_type:%s>"), *Property->GetClass()->GetName()));
    }
}

// ============================================================================
// Constructor
// ============================================================================
FEpicUnrealMCPEditorCommands::FEpicUnrealMCPEditorCommands()
{
}

// ============================================================================
// Command dispatch
// ============================================================================
TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation
    if (CommandType == TEXT("get_actors_in_level")) { return HandleGetActorsInLevel(Params); }
    else if (CommandType == TEXT("find_actors_by_name")) { return HandleFindActorsByName(Params); }
    else if (CommandType == TEXT("spawn_actor")) { return HandleSpawnActor(Params); }
    else if (CommandType == TEXT("delete_actor")) { return HandleDeleteActor(Params); }
    else if (CommandType == TEXT("set_actor_transform")) { return HandleSetActorTransform(Params); }
    else if (CommandType == TEXT("execute_unreal_python")) { return HandleExecuteUnrealPython(Params); }
    else if (CommandType == TEXT("export_retargeted_animations")) { return HandleExportRetargetedAnimations(Params); }
    else if (CommandType == TEXT("create_reference_pose_animation_sequence")) { return HandleCreateReferencePoseAnimationSequence(Params); }
    else if (CommandType == TEXT("export_animation_sequences")) { return HandleExportAnimationSequences(Params); }
    else if (CommandType == TEXT("validate_animation_export_config")) { return HandleValidateAnimationExportConfig(Params); }
    else if (CommandType == TEXT("import_skeletal_mesh_asset")) { return HandleImportSkeletalMeshAsset(Params); }
    else if (CommandType == TEXT("copy_content_tree_from_disk")) { return HandleCopyContentTreeFromDisk(Params); }
    else if (CommandType == TEXT("create_ik_retargeter_assets")) { return HandleCreateIKRetargeterAssets(Params); }
    else if (CommandType == TEXT("ensure_animation_skeletons")) { return HandleEnsureAnimationSkeletons(Params); }
    else if (CommandType == TEXT("list_animation_assets")) { return HandleListAnimationAssets(Params); }
    else if (CommandType == TEXT("spawn_blueprint_actor")) { return HandleSpawnBlueprintActor(Params); }
    // Editor lifecycle
    else if (CommandType == TEXT("request_editor_exit")) { return HandleRequestEditorExit(Params); }
    else if (CommandType == TEXT("restart_editor")) { return HandleRestartEditor(Params); }
    else if (CommandType == TEXT("get_editor_state")) { return HandleGetEditorState(Params); }
    else if (CommandType == TEXT("get_engine_version")) { return HandleGetEngineVersion(Params); }
    else if (CommandType == TEXT("save_all")) { return HandleSaveAll(Params); }
    // PIE / Simulation
    else if (CommandType == TEXT("play_in_editor_start")) { return HandlePlayInEditorStart(Params); }
    else if (CommandType == TEXT("play_in_editor_stop")) { return HandlePlayInEditorStop(Params); }
    else if (CommandType == TEXT("simulate_start")) { return HandleSimulateStart(Params); }
    else if (CommandType == TEXT("simulate_stop")) { return HandleSimulateStop(Params); }
    else if (CommandType == TEXT("pause_game")) { return HandlePauseGame(Params); }
    // Level lifecycle
    else if (CommandType == TEXT("list_levels")) { return HandleListLevels(Params); }
    else if (CommandType == TEXT("open_level")) { return HandleOpenLevel(Params); }
    else if (CommandType == TEXT("save_level")) { return HandleSaveLevel(Params); }
    else if (CommandType == TEXT("create_level")) { return HandleCreateLevel(Params); }
    else if (CommandType == TEXT("duplicate_level")) { return HandleDuplicateLevel(Params); }
    else if (CommandType == TEXT("delete_level")) { return HandleDeleteLevel(Params); }
    else if (CommandType == TEXT("set_current_world")) { return HandleSetCurrentWorld(Params); }
    else if (CommandType == TEXT("add_sublevel")) { return HandleAddSublevel(Params); }
    else if (CommandType == TEXT("remove_sublevel")) { return HandleRemoveSublevel(Params); }
    else if (CommandType == TEXT("toggle_sublevel_visibility")) { return HandleToggleSublevelVisibility(Params); }
    // Selection and viewport
    else if (CommandType == TEXT("get_selected_actors")) { return HandleGetSelectedActors(Params); }
    else if (CommandType == TEXT("select_actors")) { return HandleSelectActors(Params); }
    else if (CommandType == TEXT("focus_viewport_on_selection")) { return HandleFocusViewportOnSelection(Params); }
    else if (CommandType == TEXT("set_viewport_camera")) { return HandleSetViewportCamera(Params); }
    else if (CommandType == TEXT("capture_viewport_screenshot")) { return HandleCaptureViewportScreenshot(Params); }
    else if (CommandType == TEXT("get_viewport_stats")) { return HandleGetViewportStats(Params); }
    // Generic reflection
    else if (CommandType == TEXT("get_object_properties")) { return HandleGetObjectProperties(Params); }
    else if (CommandType == TEXT("set_object_properties")) { return HandleSetObjectProperties(Params); }
    else if (CommandType == TEXT("call_uobject_function")) { return HandleCallUObjectFunction(Params); }
    else if (CommandType == TEXT("build_navmesh")) { return HandleBuildNavMesh(Params); }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        MCPErrorCodes::UNKNOWN_COMMAND,
        FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

// ============================================================================
// Actor manipulation commands (existing)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location"))) { Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")); }
    if (Params->HasField(TEXT("rotation"))) { Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")); }
    if (Params->HasField(TEXT("scale")))    { Scale = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")); }

    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world")); }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* NewMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (NewMeshActor)
        {
            FString MeshPath;
            if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
            {
                UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                if (Mesh) { NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh); }
            }
        }
        NewActor = NewMeshActor;
    }
    else if (ActorType == TEXT("PointLight")) { NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams); }
    else if (ActorType == TEXT("SpotLight")) { NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams); }
    else if (ActorType == TEXT("DirectionalLight")) { NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams); }
    else if (ActorType == TEXT("CameraActor")) { NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams); }
    else { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType)); }

    if (NewActor)
    {
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);
        return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TSharedPtr<FJsonObject> ActorInfo = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            Actor->Destroy();
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName) { TargetActor = Actor; break; }
    }

    if (!TargetActor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName)); }

    FTransform NewTransform = TargetActor->GetTransform();
    if (Params->HasField(TEXT("location"))) { NewTransform.SetLocation(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"))); }
    if (Params->HasField(TEXT("rotation"))) { NewTransform.SetRotation(FQuat(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")))); }
    if (Params->HasField(TEXT("scale")))    { NewTransform.SetScale3D(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"))); }

    TargetActor->SetActorTransform(NewTransform);
    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleExportRetargetedAnimations(const TSharedPtr<FJsonObject>& Params)
{
    FString ConfigPath;
    if (!Params.IsValid() || (!Params->TryGetStringField(TEXT("config_path"), ConfigPath) &&
                              !Params->TryGetStringField(TEXT("configPath"), ConfigPath)))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'config_path' parameter"));
    }

    FString ScriptPath;
    if (!Params->TryGetStringField(TEXT("script_path"), ScriptPath) &&
        !Params->TryGetStringField(TEXT("scriptPath"), ScriptPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'script_path' parameter"));
    }

    TSharedPtr<FJsonObject> ArgsObject = MakeShared<FJsonObject>();
    ArgsObject->SetStringField(TEXT("config_path"), ConfigPath);

    FString ArgsJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArgsJson);
    FJsonSerializer::Serialize(ArgsObject.ToSharedRef(), Writer);

    TSharedPtr<FJsonObject> PythonParams = MakeShared<FJsonObject>();
    PythonParams->SetStringField(TEXT("script_path"), ScriptPath);
    PythonParams->SetStringField(TEXT("args_json"), ArgsJson);
    return HandleExecuteUnrealPython(PythonParams);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateReferencePoseAnimationSequence(const TSharedPtr<FJsonObject>& Params)
{
    FString SkeletalMeshPath;
    if (!Params.IsValid() || (!Params->TryGetStringField(TEXT("skeletal_mesh_path"), SkeletalMeshPath) &&
                              !Params->TryGetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath)))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'skeletal_mesh_path' parameter"));
    }

    FString DestinationPath;
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath) &&
        !Params->TryGetStringField(TEXT("destinationPath"), DestinationPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'destination_path' parameter"));
    }

    bool bReplaceExisting = false;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
    Params->TryGetBoolField(TEXT("replaceExisting"), bReplaceExisting);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    auto LoadAssetExpanded = [](FString AssetPath) -> UObject*
    {
        AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset && AssetPath.StartsWith(TEXT("/")) && !AssetPath.Contains(TEXT(".")))
        {
            const FString AssetName = FPaths::GetBaseFilename(AssetPath);
            const FString ExpandedPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
            Asset = UEditorAssetLibrary::LoadAsset(ExpandedPath);
        }
        return Asset;
    };

    auto SplitGameAssetPath = [](FString RawPath, FString& OutPackagePath, FString& OutAssetName, FString& OutAssetPath, FString& OutObjectPath, FString& OutError) -> bool
    {
        RawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
        FString AssetPath = RawPath;
        if (RawPath.Contains(TEXT(".")))
        {
            RawPath.Split(TEXT("."), &AssetPath, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        }
        while (AssetPath.EndsWith(TEXT("/")) && AssetPath.Len() > 5)
        {
            AssetPath.LeftChopInline(1);
        }
        if (!AssetPath.StartsWith(TEXT("/Game/")))
        {
            OutError = FString::Printf(TEXT("Asset path must be under /Game: %s"), *RawPath);
            return false;
        }
        OutAssetName = FPaths::GetBaseFilename(AssetPath);
        OutPackagePath = FPaths::GetPath(AssetPath);
        if (OutAssetName.IsEmpty() || OutPackagePath.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Asset path must include a package and asset name: %s"), *RawPath);
            return false;
        }
        OutAssetPath = AssetPath;
        OutObjectPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *OutAssetName);
        return true;
    };

    USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadAssetExpanded(SkeletalMeshPath));
    if (!SkeletalMesh)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::NOT_FOUND,
            FString::Printf(TEXT("Could not load SkeletalMesh: %s"), *SkeletalMeshPath));
    }

    USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::INVALID_PARAM,
            FString::Printf(TEXT("SkeletalMesh has no USkeleton. Run ensure_animation_skeletons first: %s"), *SkeletalMeshPath));
    }

    FString PackagePath;
    FString AssetName;
    FString AssetPath;
    FString ObjectPath;
    FString PathError;
    if (!SplitGameAssetPath(DestinationPath, PackagePath, AssetName, AssetPath, ObjectPath, PathError))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, PathError);
    }

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        if (!bReplaceExisting)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::INVALID_PARAM,
                FString::Printf(TEXT("AnimSequence already exists and replace_existing is false: %s"), *AssetPath));
        }
        if (!UEditorAssetLibrary::DeleteAsset(AssetPath))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::UNKNOWN_ERROR,
                FString::Printf(TEXT("Failed to delete existing AnimSequence: %s"), *AssetPath));
        }
    }

    UEditorAssetLibrary::MakeDirectory(PackagePath);

    UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
    Factory->TargetSkeleton = Skeleton;
    Factory->PreviewSkeletalMesh = SkeletalMesh;

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(
        AssetName,
        PackagePath,
        UAnimSequence::StaticClass(),
        Factory);

    UAnimSequence* AnimSequence = Cast<UAnimSequence>(CreatedAsset);
    if (!AnimSequence)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Failed to create AnimSequence asset: %s"), *AssetPath));
    }

    AnimSequence->SetSkeleton(Skeleton);
    AnimSequence->SetPreviewMesh(SkeletalMesh);
    if (!AnimSequence->CreateAnimation(SkeletalMesh))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Failed to populate AnimSequence from reference pose: %s"), *AssetPath));
    }

    AnimSequence->MarkPackageDirty();
    AnimSequence->PostEditChange();
    const bool bSaved = bSave ? UEditorAssetLibrary::SaveLoadedAsset(AnimSequence, false) : false;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("skeletal_mesh_path"), SkeletalMesh->GetPathName());
    ResultObj->SetStringField(TEXT("skeleton_path"), Skeleton->GetPathName());
    ResultObj->SetStringField(TEXT("animation_path"), AnimSequence->GetPathName());
    ResultObj->SetStringField(TEXT("package_path"), AssetPath);
    ResultObj->SetBoolField(TEXT("replace_existing"), bReplaceExisting);
    ResultObj->SetBoolField(TEXT("saved"), bSaved);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleExportAnimationSequences(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing params object"));
    }

    const TArray<TSharedPtr<FJsonValue>>* AnimationPathValues = nullptr;
    if (!Params->TryGetArrayField(TEXT("animation_paths"), AnimationPathValues) || !AnimationPathValues || AnimationPathValues->Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'animation_paths' entries"));
    }

    FString FbxOutputDir;
    if (!Params->TryGetStringField(TEXT("fbx_output_dir"), FbxOutputDir) &&
        !Params->TryGetStringField(TEXT("fbxOutputDir"), FbxOutputDir))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'fbx_output_dir' parameter"));
    }

    FString SourceId;
    if (!Params->TryGetStringField(TEXT("source_id"), SourceId) &&
        !Params->TryGetStringField(TEXT("sourceId"), SourceId))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'source_id' parameter"));
    }

    FString ClipKind;
    if (!Params->TryGetStringField(TEXT("clip_kind"), ClipKind) &&
        !Params->TryGetStringField(TEXT("clipKind"), ClipKind))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'clip_kind' parameter"));
    }

    bool bLoop = false;
    if (!Params->TryGetBoolField(TEXT("loop"), bLoop))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'loop' parameter"));
    }

    bool bReplaceExisting = true;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
    Params->TryGetBoolField(TEXT("overwrite"), bReplaceExisting);

    bool bWriteMetadata = true;
    Params->TryGetBoolField(TEXT("write_metadata"), bWriteMetadata);

    bool bExportPreviewMesh = false;
    Params->TryGetBoolField(TEXT("export_preview_mesh"), bExportPreviewMesh);

    FString LicenseClass;
    Params->TryGetStringField(TEXT("license_class"), LicenseClass);
    Params->TryGetStringField(TEXT("licenseClass"), LicenseClass);

    FString AbsoluteOutputDir = FPaths::ConvertRelativePathToFull(FbxOutputDir);
    FPaths::NormalizeFilename(AbsoluteOutputDir);
    if (!IFileManager::Get().MakeDirectory(*AbsoluteOutputDir, true))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Failed to create FBX output directory: %s"), *AbsoluteOutputDir));
    }

    auto LoadAnimSequence = [](const FString& RawPath) -> UAnimSequence*
    {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(RawPath);
        if (!Asset && RawPath.StartsWith(TEXT("/")) && !RawPath.Contains(TEXT(".")))
        {
            const FString AssetName = FPaths::GetBaseFilename(RawPath);
            const FString ExpandedPath = FString::Printf(TEXT("%s.%s"), *RawPath, *AssetName);
            Asset = LoadObject<UObject>(nullptr, *ExpandedPath);
        }
        return Cast<UAnimSequence>(Asset);
    };

    TArray<TSharedPtr<FJsonValue>> Exported;
    for (const TSharedPtr<FJsonValue>& PathValue : *AnimationPathValues)
    {
        if (!PathValue.IsValid() || PathValue->Type != EJson::String)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::INVALID_PARAM, TEXT("Each 'animation_paths' entry must be a string"));
        }

        const FString AnimationPath = PathValue->AsString();
        UAnimSequence* AnimSequence = LoadAnimSequence(AnimationPath);
        if (!AnimSequence)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::NOT_FOUND,
                FString::Printf(TEXT("Could not load AnimSequence: %s"), *AnimationPath));
        }

        const FString ClipId = AnimSequence->GetName();
        const FString FbxPath = FPaths::Combine(AbsoluteOutputDir, FString::Printf(TEXT("%s.fbx"), *ClipId));
        if (IFileManager::Get().FileExists(*FbxPath) && !bReplaceExisting)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::INVALID_PARAM,
                FString::Printf(TEXT("FBX already exists and replace_existing is false: %s"), *FbxPath));
        }

        UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
        ExportTask->Object = AnimSequence;
        ExportTask->Filename = FbxPath;
        ExportTask->bAutomated = true;
        ExportTask->bPrompt = false;
        ExportTask->bReplaceIdentical = bReplaceExisting;
        ExportTask->bSelected = false;
        ExportTask->bUseFileArchive = false;
        ExportTask->bWriteEmptyFiles = false;
        ExportTask->Exporter = NewObject<UAnimSequenceExporterFBX>();

        UFbxExportOption* ExportOptions = NewObject<UFbxExportOption>();
        ExportOptions->bASCII = false;
        ExportOptions->Collision = false;
        ExportOptions->bExportPreviewMesh = bExportPreviewMesh;
        ExportOptions->bExportSourceMesh = false;
        ExportOptions->bForceFrontXAxis = false;
        ExportTask->Options = ExportOptions;

        if (!UExporter::RunAssetExportTask(ExportTask))
        {
            const FString ExportErrors = ExportTask->Errors.Num() > 0
                ? FString::Join(ExportTask->Errors, TEXT("; "))
                : TEXT("unknown exporter error");
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::UNKNOWN_ERROR,
                FString::Printf(TEXT("FBX export failed for %s: %s"), *AnimationPath, *ExportErrors));
        }

        FString MetadataPath;
        if (bWriteMetadata)
        {
            const FString MetadataFileName = FString::Printf(TEXT("%s.%s"), *ClipId, TEXT("sen_clip_metadata.json"));
            MetadataPath = FPaths::Combine(AbsoluteOutputDir, MetadataFileName);
            if (IFileManager::Get().FileExists(*MetadataPath) && !bReplaceExisting)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    MCPErrorCodes::INVALID_PARAM,
                    FString::Printf(TEXT("Metadata already exists and replace_existing is false: %s"), *MetadataPath));
            }

            TSharedPtr<FJsonObject> MetadataObj = MakeShared<FJsonObject>();
            MetadataObj->SetStringField(TEXT("clipId"), ClipId);
            MetadataObj->SetStringField(TEXT("sourceId"), SourceId);
            MetadataObj->SetStringField(TEXT("kind"), ClipKind);
            MetadataObj->SetBoolField(TEXT("loop"), bLoop);
            if (!LicenseClass.IsEmpty())
            {
                MetadataObj->SetStringField(TEXT("licenseClass"), LicenseClass);
            }

            FString MetadataJson;
            TSharedRef<TJsonWriter<>> MetadataWriter = TJsonWriterFactory<>::Create(&MetadataJson);
            FJsonSerializer::Serialize(MetadataObj.ToSharedRef(), MetadataWriter);
            MetadataJson += LINE_TERMINATOR;
            if (!FFileHelper::SaveStringToFile(MetadataJson, *MetadataPath))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    MCPErrorCodes::UNKNOWN_ERROR,
                    FString::Printf(TEXT("Failed to write Sen clip metadata sidecar: %s"), *MetadataPath));
            }
        }

        TSharedPtr<FJsonObject> ExportObj = MakeShared<FJsonObject>();
        ExportObj->SetStringField(TEXT("animation_path"), AnimSequence->GetPathName());
        ExportObj->SetStringField(TEXT("clip_id"), ClipId);
        ExportObj->SetStringField(TEXT("fbx_path"), FbxPath);
        if (!MetadataPath.IsEmpty())
        {
            ExportObj->SetStringField(TEXT("metadata_path"), MetadataPath);
        }
        Exported.Add(MakeShared<FJsonValueObject>(ExportObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("fbx_output_dir"), AbsoluteOutputDir);
    ResultObj->SetStringField(TEXT("source_id"), SourceId);
    ResultObj->SetStringField(TEXT("clip_kind"), ClipKind);
    ResultObj->SetBoolField(TEXT("loop"), bLoop);
    ResultObj->SetBoolField(TEXT("write_metadata"), bWriteMetadata);
    ResultObj->SetArrayField(TEXT("exported"), Exported);
    ResultObj->SetNumberField(TEXT("count"), Exported.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleExecuteUnrealPython(const TSharedPtr<FJsonObject>& Params)
{
    FString ScriptPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("script_path"), ScriptPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'script_path' parameter"));
    }

    FString ArgsJson;
    Params->TryGetStringField(TEXT("args_json"), ArgsJson);

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR, TEXT("PythonScriptPlugin is not loaded"));
    }

    if (!PythonPlugin->IsPythonInitialized())
    {
        PythonPlugin->ForceEnablePythonAtRuntime();
    }
    if (!PythonPlugin->IsPythonAvailable() || !PythonPlugin->IsPythonInitialized())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR, TEXT("PythonScriptPlugin is not ready"));
    }

    FPythonCommandEx PythonCommand;
    PythonCommand.Flags |= EPythonCommandFlags::Unattended;
    PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
    PythonCommand.Command = FString::Printf(
        TEXT("import base64, os, runpy\n")
        TEXT("os.environ['UNREAL_MCP_ARGS_JSON'] = base64.b64decode('%s').decode('utf-8')\n")
        TEXT("runpy.run_path(base64.b64decode('%s').decode('utf-8'), run_name='__main__')"),
        *FBase64::Encode(ArgsJson),
        *FBase64::Encode(ScriptPath)
    );
    const bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    ResultObj->SetStringField(TEXT("script_path"), ScriptPath);
    ResultObj->SetStringField(TEXT("command_result"), PythonCommand.CommandResult);

    TArray<TSharedPtr<FJsonValue>> LogOutput;
    for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
    {
        TSharedPtr<FJsonObject> LogEntry = MakeShared<FJsonObject>();
        LogEntry->SetStringField(TEXT("type"), LexToString(Entry.Type));
        LogEntry->SetStringField(TEXT("output"), Entry.Output);
        LogOutput.Add(MakeShared<FJsonValueObject>(LogEntry));
    }
    ResultObj->SetArrayField(TEXT("log_output"), LogOutput);

    if (!bSuccess)
    {
        ResultObj->SetStringField(TEXT("error"), PythonCommand.CommandResult);
        ResultObj->SetStringField(TEXT("error_code"), MCPErrorCodes::UNKNOWN_ERROR);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleValidateAnimationExportConfig(const TSharedPtr<FJsonObject>& Params)
{
    FString ConfigPath;
    if (!Params.IsValid() || (!Params->TryGetStringField(TEXT("config_path"), ConfigPath) &&
                              !Params->TryGetStringField(TEXT("configPath"), ConfigPath)))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'config_path' parameter"));
    }

    FString ConfigJson;
    if (!FFileHelper::LoadFileToString(ConfigJson, *ConfigPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::NOT_FOUND,
            FString::Printf(TEXT("Failed to read animation export config: %s"), *ConfigPath));
    }

    TSharedPtr<FJsonObject> Config;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ConfigJson);
    if (!FJsonSerializer::Deserialize(Reader, Config) || !Config.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::INVALID_PARAM,
            FString::Printf(TEXT("Animation export config is not valid JSON: %s"), *ConfigPath));
    }

    TArray<TSharedPtr<FJsonValue>> MissingConfigKeys;
    auto RequireField = [&Config, &MissingConfigKeys](const FString& FieldName)
    {
        if (!Config->HasField(FieldName))
        {
            MissingConfigKeys.Add(MakeShared<FJsonValueString>(FieldName));
        }
    };

    RequireField(TEXT("source_mesh"));
    RequireField(TEXT("target_mesh"));
    RequireField(TEXT("ik_retargeter"));
    RequireField(TEXT("source_animation_paths"));
    RequireField(TEXT("retarget_output_path"));
    RequireField(TEXT("fbx_output_dir"));
    RequireField(TEXT("licenseClass"));
    RequireField(TEXT("sourceId"));
    RequireField(TEXT("clipKind"));
    RequireField(TEXT("loop"));

    const TArray<TSharedPtr<FJsonValue>>* SourceAnimationValues = nullptr;
    if (!Config->TryGetArrayField(TEXT("source_animation_paths"), SourceAnimationValues) ||
        !SourceAnimationValues ||
        SourceAnimationValues->Num() == 0)
    {
        MissingConfigKeys.Add(MakeShared<FJsonValueString>(TEXT("source_animation_paths")));
    }

    FString LicenseClass;
    Config->TryGetStringField(TEXT("licenseClass"), LicenseClass);
    const bool bBlockedLicense = LicenseClass.Equals(TEXT("ue_only_blocked"), ESearchCase::IgnoreCase);

    TArray<TSharedPtr<FJsonValue>> AvailableAssets;
    TArray<TSharedPtr<FJsonValue>> MissingAssets;

    auto TryLoadAssetWithExpandedPath = [](const FString& AssetPath) -> UObject*
    {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset && AssetPath.StartsWith(TEXT("/")) && !AssetPath.Contains(TEXT(".")))
        {
            const FString AssetName = FPaths::GetBaseFilename(AssetPath);
            const FString ExpandedPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
            Asset = UEditorAssetLibrary::LoadAsset(ExpandedPath);
        }
        return Asset;
    };

    auto CheckAsset = [&AvailableAssets, &MissingAssets, &TryLoadAssetWithExpandedPath](const FString& Label, const FString& AssetPath)
    {
        if (AssetPath.IsEmpty())
        {
            TSharedPtr<FJsonObject> MissingObj = MakeShared<FJsonObject>();
            MissingObj->SetStringField(TEXT("label"), Label);
            MissingObj->SetStringField(TEXT("path"), AssetPath);
            MissingObj->SetStringField(TEXT("reason"), TEXT("empty_path"));
            MissingAssets.Add(MakeShared<FJsonValueObject>(MissingObj));
            return;
        }

        UObject* Asset = TryLoadAssetWithExpandedPath(AssetPath);
        TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
        AssetObj->SetStringField(TEXT("label"), Label);
        AssetObj->SetStringField(TEXT("path"), AssetPath);

        if (Asset)
        {
            AssetObj->SetStringField(TEXT("name"), Asset->GetName());
            AssetObj->SetStringField(TEXT("resolved_path"), Asset->GetPathName());
            AssetObj->SetStringField(TEXT("class"), Asset->GetClass() ? Asset->GetClass()->GetName() : TEXT(""));
            AvailableAssets.Add(MakeShared<FJsonValueObject>(AssetObj));
        }
        else
        {
            AssetObj->SetStringField(TEXT("reason"), TEXT("not_found"));
            MissingAssets.Add(MakeShared<FJsonValueObject>(AssetObj));
        }
    };

    FString SourceMeshPath;
    if (Config->TryGetStringField(TEXT("source_mesh"), SourceMeshPath))
    {
        CheckAsset(TEXT("source_mesh"), SourceMeshPath);
    }

    FString TargetMeshPath;
    if (Config->TryGetStringField(TEXT("target_mesh"), TargetMeshPath))
    {
        CheckAsset(TEXT("target_mesh"), TargetMeshPath);
    }

    FString RetargeterPath;
    if (Config->TryGetStringField(TEXT("ik_retargeter"), RetargeterPath))
    {
        CheckAsset(TEXT("ik_retargeter"), RetargeterPath);
    }

    if (SourceAnimationValues)
    {
        for (int32 Index = 0; Index < SourceAnimationValues->Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>& Value = (*SourceAnimationValues)[Index];
            if (!Value.IsValid() || Value->Type != EJson::String)
            {
                MissingConfigKeys.Add(MakeShared<FJsonValueString>(
                    FString::Printf(TEXT("source_animation_paths[%d]"), Index)));
                continue;
            }
            CheckAsset(FString::Printf(TEXT("source_animation_paths[%d]"), Index), Value->AsString());
        }
    }

    const bool bValid = MissingConfigKeys.Num() == 0 && MissingAssets.Num() == 0 && !bBlockedLicense;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("valid"), bValid);
    ResultObj->SetStringField(TEXT("config_path"), ConfigPath);
    ResultObj->SetArrayField(TEXT("missing_config_keys"), MissingConfigKeys);
    ResultObj->SetArrayField(TEXT("missing_assets"), MissingAssets);
    ResultObj->SetArrayField(TEXT("available_assets"), AvailableAssets);
    ResultObj->SetBoolField(TEXT("blocked_license"), bBlockedLicense);
    ResultObj->SetStringField(TEXT("licenseClass"), LicenseClass);
    ResultObj->SetNumberField(TEXT("missing_config_key_count"), MissingConfigKeys.Num());
    ResultObj->SetNumberField(TEXT("missing_asset_count"), MissingAssets.Num());
    ResultObj->SetNumberField(TEXT("available_asset_count"), AvailableAssets.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleImportSkeletalMeshAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("source_path"), SourcePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'source_path' parameter"));
    }

    FString DestinationPath;
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'destination_path' parameter"));
    }

    FString DestinationName;
    if (!Params->TryGetStringField(TEXT("destination_name"), DestinationName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'destination_name' parameter"));
    }

    if (!FPaths::FileExists(SourcePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::NOT_FOUND,
            FString::Printf(TEXT("Skeletal mesh source file does not exist: %s"), *SourcePath));
    }

    if (!DestinationPath.StartsWith(TEXT("/Game/")) && DestinationPath != TEXT("/Game"))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::INVALID_PARAM,
            TEXT("destination_path must be an Unreal game content path such as /Game/Senko"));
    }

    bool bReplaceExisting = true;
    bool bSave = true;
    bool bImportMaterials = true;
    bool bImportTextures = true;
    bool bCreatePhysicsAsset = false;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
    Params->TryGetBoolField(TEXT("save"), bSave);
    Params->TryGetBoolField(TEXT("import_materials"), bImportMaterials);
    Params->TryGetBoolField(TEXT("import_textures"), bImportTextures);
    Params->TryGetBoolField(TEXT("create_physics_asset"), bCreatePhysicsAsset);

    UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
    FbxFactory->AddToRoot();
    FbxFactory->SetDetectImportTypeOnImport(false);

    if (!FbxFactory->ImportUI)
    {
        FbxFactory->ImportUI = NewObject<UFbxImportUI>(FbxFactory);
    }

    UFbxImportUI* ImportUI = FbxFactory->ImportUI;
    ImportUI->MeshTypeToImport = FBXIT_SkeletalMesh;
    ImportUI->OriginalImportType = FBXIT_SkeletalMesh;
    ImportUI->bAutomatedImportShouldDetectType = false;
    ImportUI->bImportAsSkeletal = true;
    ImportUI->bImportMesh = true;
    ImportUI->bImportAnimations = false;
    ImportUI->bCreatePhysicsAsset = bCreatePhysicsAsset;
    ImportUI->bImportMaterials = bImportMaterials;
    ImportUI->bImportTextures = bImportTextures;

    if (ImportUI->SkeletalMeshImportData)
    {
        ImportUI->SkeletalMeshImportData->bImportMeshLODs = false;
        ImportUI->SkeletalMeshImportData->bImportMorphTargets = false;
        ImportUI->SkeletalMeshImportData->bImportMeshesInBoneHierarchy = true;
        ImportUI->SkeletalMeshImportData->ImportContentType = EFBXImportContentType::FBXICT_All;
    }

    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->AddToRoot();
    Task->Filename = SourcePath;
    Task->DestinationPath = DestinationPath;
    Task->DestinationName = DestinationName;
    Task->bAutomated = true;
    Task->bReplaceExisting = bReplaceExisting;
    Task->bReplaceExistingSettings = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = FbxFactory;
    Task->Options = ImportUI;
    FbxFactory->SetAssetImportTask(Task);

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    TArray<UAssetImportTask*> Tasks;
    Tasks.Add(Task);
    AssetToolsModule.Get().ImportAssetTasks(Tasks);

    TArray<TSharedPtr<FJsonValue>> ImportedObjectPaths;
    for (const FString& ImportedPath : Task->ImportedObjectPaths)
    {
        ImportedObjectPaths.Add(MakeShared<FJsonValueString>(ImportedPath));
    }

    TArray<TSharedPtr<FJsonValue>> ImportedObjects;
    TArray<UObject*> ImportedAssetObjects;
    FString PrimaryAssetPath;
    for (UObject* ImportedObject : Task->GetObjects())
    {
        if (!ImportedObject)
        {
            continue;
        }

        if (PrimaryAssetPath.IsEmpty())
        {
            PrimaryAssetPath = ImportedObject->GetPathName();
        }

        ImportedAssetObjects.Add(ImportedObject);

        TSharedPtr<FJsonObject> ObjectObj = MakeShared<FJsonObject>();
        ObjectObj->SetStringField(TEXT("name"), ImportedObject->GetName());
        ObjectObj->SetStringField(TEXT("path"), ImportedObject->GetPathName());
        ObjectObj->SetStringField(TEXT("class"), ImportedObject->GetClass() ? ImportedObject->GetClass()->GetName() : TEXT(""));
        ImportedObjects.Add(MakeShared<FJsonValueObject>(ObjectObj));
    }

    if (ImportedObjectPaths.Num() == 0 && ImportedObjects.Num() == 0)
    {
        Task->RemoveFromRoot();
        FbxFactory->RemoveFromRoot();
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Unreal imported no assets from %s"), *SourcePath));
    }

    TArray<TSharedPtr<FJsonValue>> SavedAssetPaths;
    if (bSave)
    {
        TArray<TSharedPtr<FJsonValue>> FailedSaveAssetPaths;
        for (UObject* ImportedObject : ImportedAssetObjects)
        {
            if (!ImportedObject)
            {
                continue;
            }

            const FString ObjectPath = ImportedObject->GetPathName();
            if (UEditorAssetLibrary::SaveLoadedAsset(ImportedObject, false))
            {
                SavedAssetPaths.Add(MakeShared<FJsonValueString>(ObjectPath));
            }
            else
            {
                FailedSaveAssetPaths.Add(MakeShared<FJsonValueString>(ObjectPath));
            }
        }

        if (FailedSaveAssetPaths.Num() > 0)
        {
            Task->RemoveFromRoot();
            FbxFactory->RemoveFromRoot();
            FString FailedPathsString;
            for (const TSharedPtr<FJsonValue>& FailedPathValue : FailedSaveAssetPaths)
            {
                if (!FailedPathsString.IsEmpty())
                {
                    FailedPathsString += TEXT(", ");
                }
                FailedPathsString += FailedPathValue.IsValid() ? FailedPathValue->AsString() : TEXT("<unknown>");
            }
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::UNKNOWN_ERROR,
                FString::Printf(TEXT("Imported skeletal mesh, but failed to save imported assets: %s"), *FailedPathsString));
        }
    }

    Task->RemoveFromRoot();
    FbxFactory->RemoveFromRoot();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_path"), SourcePath);
    ResultObj->SetStringField(TEXT("destination_path"), DestinationPath);
    ResultObj->SetStringField(TEXT("destination_name"), DestinationName);
    ResultObj->SetStringField(TEXT("asset_path"), PrimaryAssetPath);
    ResultObj->SetArrayField(TEXT("imported_object_paths"), ImportedObjectPaths);
    ResultObj->SetArrayField(TEXT("imported_objects"), ImportedObjects);
    ResultObj->SetNumberField(TEXT("imported_object_count"), ImportedObjects.Num());
    ResultObj->SetBoolField(TEXT("save_requested"), bSave);
    ResultObj->SetArrayField(TEXT("saved_asset_paths"), SavedAssetPaths);
    ResultObj->SetNumberField(TEXT("saved_asset_count"), SavedAssetPaths.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCopyContentTreeFromDisk(const TSharedPtr<FJsonObject>& Params)
{
    FString SourceContentDir;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("source_content_dir"), SourceContentDir))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'source_content_dir' parameter"));
    }

    FString DestinationPath;
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'destination_path' parameter"));
    }

    bool bReplaceExisting = false;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

    SourceContentDir = FPaths::ConvertRelativePathToFull(SourceContentDir);
    FPaths::NormalizeFilename(SourceContentDir);
    FPaths::NormalizeDirectoryName(SourceContentDir);
    if (!SourceContentDir.EndsWith(TEXT("/")))
    {
        SourceContentDir += TEXT("/");
    }

    if (!IFileManager::Get().DirectoryExists(*SourceContentDir))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::NOT_FOUND,
            FString::Printf(TEXT("Source content directory does not exist: %s"), *SourceContentDir));
    }

    DestinationPath.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (DestinationPath.EndsWith(TEXT("/")) && DestinationPath.Len() > 5)
    {
        DestinationPath.LeftChopInline(1);
    }

    if (!DestinationPath.StartsWith(TEXT("/Game")) ||
        (DestinationPath.Len() > 5 && !DestinationPath.StartsWith(TEXT("/Game/"))))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::INVALID_PARAM,
            TEXT("destination_path must be an Unreal game content path such as /Game/Characters/Mannequins"));
    }

    FString DestinationPhysicalDir;
    if (!FPackageName::TryConvertLongPackageNameToFilename(DestinationPath, DestinationPhysicalDir))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::INVALID_PARAM,
            FString::Printf(TEXT("Could not convert destination_path to a content directory: %s"), *DestinationPath));
    }
    DestinationPhysicalDir = FPaths::ConvertRelativePathToFull(DestinationPhysicalDir);
    FPaths::NormalizeFilename(DestinationPhysicalDir);
    FPaths::NormalizeDirectoryName(DestinationPhysicalDir);

    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(SourceFiles, *SourceContentDir, TEXT("*.*"), true, false);
    SourceFiles.Sort();

    TArray<TSharedPtr<FJsonValue>> CopiedFiles;
    TArray<TSharedPtr<FJsonValue>> SkippedFiles;
    TArray<TSharedPtr<FJsonValue>> FailedFiles;
    TArray<FString> CopiedAssetPathStrings;

    auto IsUnrealPackagePayload = [](const FString& FilePath) -> bool
    {
        const FString Extension = FPaths::GetExtension(FilePath).ToLower();
        return Extension == TEXT("uasset") ||
               Extension == TEXT("uexp") ||
               Extension == TEXT("ubulk") ||
               Extension == TEXT("uptnl");
    };

    for (const FString& SourceFile : SourceFiles)
    {
        if (!IsUnrealPackagePayload(SourceFile))
        {
            continue;
        }

        FString NormalizedSourceFile = SourceFile;
        FPaths::NormalizeFilename(NormalizedSourceFile);
        FString RelativePath = NormalizedSourceFile;
        if (!FPaths::MakePathRelativeTo(RelativePath, *SourceContentDir))
        {
            FailedFiles.Add(MakeShared<FJsonValueString>(NormalizedSourceFile));
            continue;
        }
        FPaths::NormalizeFilename(RelativePath);

        const FString DestinationFile = FPaths::Combine(DestinationPhysicalDir, RelativePath);
        if (IFileManager::Get().FileExists(*DestinationFile) && !bReplaceExisting)
        {
            SkippedFiles.Add(MakeShared<FJsonValueString>(DestinationFile));
            continue;
        }

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(DestinationFile), true);
        const uint32 CopyResult = IFileManager::Get().Copy(*DestinationFile, *NormalizedSourceFile, bReplaceExisting, true);
        if (CopyResult != COPY_OK)
        {
            FailedFiles.Add(MakeShared<FJsonValueString>(NormalizedSourceFile));
            continue;
        }

        CopiedFiles.Add(MakeShared<FJsonValueString>(DestinationFile));

        if (FPaths::GetExtension(RelativePath).Equals(TEXT("uasset"), ESearchCase::IgnoreCase))
        {
            const FString RelativePackageDir = FPaths::GetPath(RelativePath);
            const FString RelativePackageName = FPaths::GetBaseFilename(RelativePath);
            FString RelativePackagePath = RelativePackageDir.IsEmpty()
                ? RelativePackageName
                : FPaths::Combine(RelativePackageDir, RelativePackageName);
            FPaths::NormalizeFilename(RelativePackagePath);
            FString AssetPath = FPaths::Combine(DestinationPath, RelativePackagePath);
            AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
            CopiedAssetPathStrings.Add(AssetPath);
        }
    }

    if (FailedFiles.Num() > 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Failed to copy %d Unreal content file(s) from %s"), FailedFiles.Num(), *SourceContentDir));
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FString> ScanPaths;
    ScanPaths.Add(DestinationPath);
    AssetRegistryModule.Get().ScanPathsSynchronous(ScanPaths, true);

    TArray<TSharedPtr<FJsonValue>> CopiedAssetPaths;
    TArray<TSharedPtr<FJsonValue>> LoadedAssetPaths;
    TArray<TSharedPtr<FJsonValue>> FailedAssetPaths;
    for (const FString& AssetPath : CopiedAssetPathStrings)
    {
        CopiedAssetPaths.Add(MakeShared<FJsonValueString>(AssetPath));
        if (UEditorAssetLibrary::LoadAsset(AssetPath))
        {
            LoadedAssetPaths.Add(MakeShared<FJsonValueString>(AssetPath));
        }
        else
        {
            FailedAssetPaths.Add(MakeShared<FJsonValueString>(AssetPath));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_content_dir"), SourceContentDir);
    ResultObj->SetStringField(TEXT("destination_path"), DestinationPath);
    ResultObj->SetStringField(TEXT("destination_physical_dir"), DestinationPhysicalDir);
    ResultObj->SetBoolField(TEXT("replace_existing"), bReplaceExisting);
    ResultObj->SetArrayField(TEXT("copied_files"), CopiedFiles);
    ResultObj->SetArrayField(TEXT("skipped_files"), SkippedFiles);
    ResultObj->SetArrayField(TEXT("copied_asset_paths"), CopiedAssetPaths);
    ResultObj->SetArrayField(TEXT("loaded_asset_paths"), LoadedAssetPaths);
    ResultObj->SetArrayField(TEXT("failed_asset_paths"), FailedAssetPaths);
    ResultObj->SetNumberField(TEXT("copied_file_count"), CopiedFiles.Num());
    ResultObj->SetNumberField(TEXT("skipped_file_count"), SkippedFiles.Num());
    ResultObj->SetNumberField(TEXT("copied_asset_count"), CopiedAssetPaths.Num());
    ResultObj->SetNumberField(TEXT("loaded_asset_count"), LoadedAssetPaths.Num());
    ResultObj->SetNumberField(TEXT("failed_asset_count"), FailedAssetPaths.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleEnsureAnimationSkeletons(const TSharedPtr<FJsonObject>& Params)
{
    const TArray<TSharedPtr<FJsonValue>>* BindingValues = nullptr;
    if (!Params.IsValid() ||
        !Params->TryGetArrayField(TEXT("mesh_skeleton_bindings"), BindingValues) ||
        !BindingValues ||
        BindingValues->Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM,
            TEXT("Missing 'mesh_skeleton_bindings' entries"));
    }

    bool bReplaceExisting = false;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

    auto LoadExpanded = [](const FString& AssetPath) -> UObject*
    {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset && AssetPath.StartsWith(TEXT("/")) && !AssetPath.Contains(TEXT(".")))
        {
            const FString AssetName = FPaths::GetBaseFilename(AssetPath);
            Asset = UEditorAssetLibrary::LoadAsset(FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName));
        }
        return Asset;
    };

    auto SplitObjectPath = [](const FString& ObjectPath, FString& OutPackagePath, FString& OutAssetName) -> bool
    {
        FString PackageName = ObjectPath;
        if (ObjectPath.Contains(TEXT(".")))
        {
            FString Ignored;
            ObjectPath.Split(TEXT("."), &PackageName, &Ignored, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        }
        if (!PackageName.StartsWith(TEXT("/Game/")))
        {
            return false;
        }
        OutPackagePath = FPackageName::GetLongPackagePath(PackageName);
        OutAssetName = FPaths::GetBaseFilename(PackageName);
        return !OutPackagePath.IsEmpty() && !OutAssetName.IsEmpty();
    };

    auto CreateOrLoadSkeleton = [&](USkeletalMesh* SkeletalMesh, const FString& SkeletonPath, bool& bOutCreated) -> USkeleton*
    {
        bOutCreated = false;
        if (bReplaceExisting && UEditorAssetLibrary::DoesAssetExist(SkeletonPath))
        {
            UEditorAssetLibrary::DeleteAsset(SkeletonPath);
        }

        if (UObject* Existing = LoadExpanded(SkeletonPath))
        {
            return Cast<USkeleton>(Existing);
        }

        FString PackagePath;
        FString AssetName;
        if (!SplitObjectPath(SkeletonPath, PackagePath, AssetName))
        {
            return nullptr;
        }

        UEditorAssetLibrary::MakeDirectory(PackagePath);
        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
        USkeletonFactory* SkeletonFactory = NewObject<USkeletonFactory>();
        SkeletonFactory->TargetSkeletalMesh = SkeletalMesh;
        UObject* CreatedAsset = AssetTools.CreateAsset(AssetName, PackagePath, USkeleton::StaticClass(), SkeletonFactory);
        USkeleton* Skeleton = Cast<USkeleton>(CreatedAsset);
        if (Skeleton && !Skeleton->MergeAllBonesToBoneTree(SkeletalMesh))
        {
            return nullptr;
        }
        bOutCreated = Skeleton != nullptr;
        return Skeleton;
    };

    TArray<TSharedPtr<FJsonValue>> BindingResults;
    int32 CreatedSkeletonCount = 0;
    int32 MeshUpdatedCount = 0;
    int32 AnimationUpdatedCount = 0;

    for (const TSharedPtr<FJsonValue>& BindingValue : *BindingValues)
    {
        const TSharedPtr<FJsonObject>* BindingObjectPtr = nullptr;
        if (!BindingValue.IsValid() || !BindingValue->TryGetObject(BindingObjectPtr) || !BindingObjectPtr || !BindingObjectPtr->IsValid())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::INVALID_PARAM,
                TEXT("Each mesh_skeleton_bindings entry must be an object"));
        }

        const TSharedPtr<FJsonObject>& BindingObject = *BindingObjectPtr;
        FString SkeletalMeshPath;
        FString SkeletonPath;
        if (!BindingObject->TryGetStringField(TEXT("skeletal_mesh_path"), SkeletalMeshPath) || SkeletalMeshPath.IsEmpty() ||
            !BindingObject->TryGetStringField(TEXT("skeleton_path"), SkeletonPath) || SkeletonPath.IsEmpty())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::MISSING_PARAM,
                TEXT("Each binding requires skeletal_mesh_path and skeleton_path"));
        }

        USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadExpanded(SkeletalMeshPath));
        if (!SkeletalMesh)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::NOT_FOUND,
                FString::Printf(TEXT("Could not load SkeletalMesh: %s"), *SkeletalMeshPath));
        }

        bool bCreatedSkeleton = false;
        USkeleton* Skeleton = CreateOrLoadSkeleton(SkeletalMesh, SkeletonPath, bCreatedSkeleton);
        if (!Skeleton)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::UNKNOWN_ERROR,
                FString::Printf(TEXT("Could not create or load Skeleton: %s"), *SkeletonPath));
        }
        if (bCreatedSkeleton)
        {
            CreatedSkeletonCount++;
        }

        bool bMeshUpdated = false;
        if (SkeletalMesh->GetSkeleton() != Skeleton)
        {
            SkeletalMesh->Modify();
            SkeletalMesh->SetSkeleton(Skeleton);
            SkeletalMesh->MarkPackageDirty();
            bMeshUpdated = true;
            MeshUpdatedCount++;
        }
        UEditorAssetLibrary::SaveLoadedAsset(SkeletalMesh);
        UEditorAssetLibrary::SaveLoadedAsset(Skeleton);

        TArray<TSharedPtr<FJsonValue>> AnimationResults;
        const TArray<TSharedPtr<FJsonValue>>* AnimationValues = nullptr;
        if (BindingObject->TryGetArrayField(TEXT("animation_paths"), AnimationValues) && AnimationValues)
        {
            for (const TSharedPtr<FJsonValue>& AnimationValue : *AnimationValues)
            {
                FString AnimationPath;
                if (!AnimationValue.IsValid() || !AnimationValue->TryGetString(AnimationPath) || AnimationPath.IsEmpty())
                {
                    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                        MCPErrorCodes::INVALID_PARAM,
                        TEXT("animation_paths entries must be non-empty strings"));
                }

                UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(LoadExpanded(AnimationPath));
                if (!AnimationAsset)
                {
                    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                        MCPErrorCodes::NOT_FOUND,
                        FString::Printf(TEXT("Could not load AnimationAsset: %s"), *AnimationPath));
                }

                const bool bAnimationUpdated = AnimationAsset->GetSkeleton() != Skeleton;
                if (bAnimationUpdated)
                {
                    AnimationAsset->Modify();
                    AnimationAsset->SetSkeleton(Skeleton);
                    AnimationAsset->MarkPackageDirty();
                    AnimationUpdatedCount++;
                }
                UEditorAssetLibrary::SaveLoadedAsset(AnimationAsset);

                TSharedPtr<FJsonObject> AnimationObj = MakeShared<FJsonObject>();
                AnimationObj->SetStringField(TEXT("animation_path"), AnimationPath);
                AnimationObj->SetStringField(TEXT("resolved_path"), AnimationAsset->GetPathName());
                AnimationObj->SetBoolField(TEXT("updated"), bAnimationUpdated);
                AnimationObj->SetStringField(TEXT("skeleton_path"), Skeleton->GetPathName());
                AnimationResults.Add(MakeShared<FJsonValueObject>(AnimationObj));
            }
        }

        TSharedPtr<FJsonObject> BindingResult = MakeShared<FJsonObject>();
        BindingResult->SetStringField(TEXT("skeletal_mesh_path"), SkeletalMeshPath);
        BindingResult->SetStringField(TEXT("resolved_skeletal_mesh_path"), SkeletalMesh->GetPathName());
        BindingResult->SetStringField(TEXT("skeleton_path"), SkeletonPath);
        BindingResult->SetStringField(TEXT("resolved_skeleton_path"), Skeleton->GetPathName());
        BindingResult->SetBoolField(TEXT("created_skeleton"), bCreatedSkeleton);
        BindingResult->SetBoolField(TEXT("mesh_updated"), bMeshUpdated);
        BindingResult->SetArrayField(TEXT("animations"), AnimationResults);
        BindingResults.Add(MakeShared<FJsonValueObject>(BindingResult));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("replace_existing"), bReplaceExisting);
    ResultObj->SetArrayField(TEXT("bindings"), BindingResults);
    ResultObj->SetNumberField(TEXT("binding_count"), BindingResults.Num());
    ResultObj->SetNumberField(TEXT("created_skeleton_count"), CreatedSkeletonCount);
    ResultObj->SetNumberField(TEXT("mesh_updated_count"), MeshUpdatedCount);
    ResultObj->SetNumberField(TEXT("animation_updated_count"), AnimationUpdatedCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateIKRetargeterAssets(const TSharedPtr<FJsonObject>& Params)
{
    auto RequireString = [&Params](const TCHAR* FieldName, FString& OutValue) -> bool
    {
        return Params.IsValid() && Params->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
    };

    FString SourceIKRigPath;
    FString SourceMeshPath;
    FString TargetMeshPath;
    FString TargetIKRigPath;
    FString RetargeterPath;
    FString TargetRetargetRoot;
    if (!RequireString(TEXT("source_ik_rig_path"), SourceIKRigPath) ||
        !RequireString(TEXT("source_mesh_path"), SourceMeshPath) ||
        !RequireString(TEXT("target_mesh_path"), TargetMeshPath) ||
        !RequireString(TEXT("target_ik_rig_path"), TargetIKRigPath) ||
        !RequireString(TEXT("retargeter_path"), RetargeterPath) ||
        !RequireString(TEXT("target_retarget_root"), TargetRetargetRoot))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM,
            TEXT("Missing one of required parameters: source_ik_rig_path, source_mesh_path, target_mesh_path, target_ik_rig_path, retargeter_path, target_retarget_root"));
    }

    const TArray<TSharedPtr<FJsonValue>>* TargetChainValues = nullptr;
    if (!Params->TryGetArrayField(TEXT("target_chains"), TargetChainValues) || !TargetChainValues || TargetChainValues->Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::MISSING_PARAM, TEXT("Missing non-empty 'target_chains' array"));
    }

    bool bReplaceExisting = false;
    bool bAutoMapChains = true;
    bool bAddDefaultOps = true;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
    Params->TryGetBoolField(TEXT("auto_map_chains"), bAutoMapChains);
    Params->TryGetBoolField(TEXT("add_default_ops"), bAddDefaultOps);

    auto LoadAssetExpanded = [](FString AssetPath) -> UObject*
    {
        AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset && AssetPath.StartsWith(TEXT("/")) && !AssetPath.Contains(TEXT(".")))
        {
            const FString AssetName = FPaths::GetBaseFilename(AssetPath);
            const FString ExpandedPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
            Asset = UEditorAssetLibrary::LoadAsset(ExpandedPath);
        }
        return Asset;
    };

    auto SplitGameAssetPath = [](FString RawPath, FString& OutPackagePath, FString& OutAssetName, FString& OutAssetPath, FString& OutObjectPath, FString& OutError) -> bool
    {
        RawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
        FString AssetPath = RawPath;
        if (RawPath.Contains(TEXT(".")))
        {
            RawPath.Split(TEXT("."), &AssetPath, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        }
        while (AssetPath.EndsWith(TEXT("/")) && AssetPath.Len() > 5)
        {
            AssetPath.LeftChopInline(1);
        }
        if (!AssetPath.StartsWith(TEXT("/Game/")))
        {
            OutError = FString::Printf(TEXT("Asset path must be under /Game: %s"), *RawPath);
            return false;
        }
        OutAssetName = FPaths::GetBaseFilename(AssetPath);
        OutPackagePath = FPaths::GetPath(AssetPath);
        if (OutAssetName.IsEmpty() || OutPackagePath.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Asset path must include a package and asset name: %s"), *RawPath);
            return false;
        }
        OutAssetPath = AssetPath;
        OutObjectPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *OutAssetName);
        return true;
    };

    UIKRigDefinition* SourceIKRig = Cast<UIKRigDefinition>(LoadAssetExpanded(SourceIKRigPath));
    USkeletalMesh* SourceMesh = Cast<USkeletalMesh>(LoadAssetExpanded(SourceMeshPath));
    USkeletalMesh* TargetMesh = Cast<USkeletalMesh>(LoadAssetExpanded(TargetMeshPath));
    if (!SourceIKRig || !SourceMesh || !TargetMesh)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::NOT_FOUND,
            FString::Printf(TEXT("Failed to load source IK rig, source mesh, or target mesh: %s | %s | %s"), *SourceIKRigPath, *SourceMeshPath, *TargetMeshPath));
    }

    FString TargetIKRigPackagePath;
    FString TargetIKRigAssetName;
    FString TargetIKRigAssetPath;
    FString TargetIKRigObjectPath;
    FString RetargeterPackagePath;
    FString RetargeterAssetName;
    FString RetargeterAssetPath;
    FString RetargeterObjectPath;
    FString PathError;
    if (!SplitGameAssetPath(TargetIKRigPath, TargetIKRigPackagePath, TargetIKRigAssetName, TargetIKRigAssetPath, TargetIKRigObjectPath, PathError) ||
        !SplitGameAssetPath(RetargeterPath, RetargeterPackagePath, RetargeterAssetName, RetargeterAssetPath, RetargeterObjectPath, PathError))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, PathError);
    }

    auto EnsureCanCreateAsset = [bReplaceExisting](const FString& AssetPath, FString& OutError) -> bool
    {
        if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
        {
            return true;
        }
        if (!bReplaceExisting)
        {
            OutError = FString::Printf(TEXT("Asset already exists and replace_existing is false: %s"), *AssetPath);
            return false;
        }
        if (!UEditorAssetLibrary::DeleteAsset(AssetPath))
        {
            OutError = FString::Printf(TEXT("Failed to delete existing asset before recreate: %s"), *AssetPath);
            return false;
        }
        return true;
    };

    FString ExistingError;
    if (!EnsureCanCreateAsset(TargetIKRigAssetPath, ExistingError) ||
        !EnsureCanCreateAsset(RetargeterAssetPath, ExistingError))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, ExistingError);
    }

    UEditorAssetLibrary::MakeDirectory(TargetIKRigPackagePath);
    UEditorAssetLibrary::MakeDirectory(RetargeterPackagePath);

    UIKRigDefinition* TargetIKRig = UIKRigDefinitionFactory::CreateNewIKRigAsset(TargetIKRigPackagePath, TargetIKRigAssetName);
    if (!TargetIKRig)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Failed to create target IK Rig asset: %s"), *TargetIKRigAssetPath));
    }

    const FReferenceSkeleton& TargetRefSkeleton = TargetMesh->GetRefSkeleton();
    auto HasTargetBone = [&TargetRefSkeleton](const FString& BoneName) -> bool
    {
        return TargetRefSkeleton.FindBoneIndex(FName(*BoneName)) != INDEX_NONE;
    };

    if (!HasTargetBone(TargetRetargetRoot))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::INVALID_PARAM,
            FString::Printf(TEXT("target_retarget_root bone not found on target mesh: %s"), *TargetRetargetRoot));
    }

    struct FRequestedRetargetChain
    {
        FName Name;
        FName StartBone;
        FName EndBone;
        FName Goal;
    };

    TArray<FRequestedRetargetChain> RequestedChains;
    for (const TSharedPtr<FJsonValue>& ChainValue : *TargetChainValues)
    {
        const TSharedPtr<FJsonObject>* ChainObjPtr = nullptr;
        if (!ChainValue.IsValid() || !ChainValue->TryGetObject(ChainObjPtr) || !ChainObjPtr || !(*ChainObjPtr).IsValid())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("Each target_chains entry must be an object"));
        }

        const TSharedPtr<FJsonObject>& ChainObj = *ChainObjPtr;
        FString ChainName;
        FString StartBone;
        FString EndBone;
        FString GoalName;
        if (!ChainObj->TryGetStringField(TEXT("name"), ChainName) ||
            !ChainObj->TryGetStringField(TEXT("start_bone"), StartBone) ||
            !ChainObj->TryGetStringField(TEXT("end_bone"), EndBone) ||
            ChainName.IsEmpty() || StartBone.IsEmpty() || EndBone.IsEmpty())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::INVALID_PARAM,
                TEXT("Each target_chains entry requires name, start_bone, and end_bone"));
        }

        if (!HasTargetBone(StartBone) || !HasTargetBone(EndBone))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::INVALID_PARAM,
                FString::Printf(TEXT("Chain '%s' references missing target bones: %s -> %s"), *ChainName, *StartBone, *EndBone));
        }

        ChainObj->TryGetStringField(TEXT("goal"), GoalName);
        RequestedChains.Add({
            FName(*ChainName),
            FName(*StartBone),
            FName(*EndBone),
            GoalName.IsEmpty() ? NAME_None : FName(*GoalName)
        });
    }

    UIKRigController* TargetRigController = UIKRigController::GetController(TargetIKRig);
    if (!TargetRigController || !TargetRigController->SetSkeletalMesh(TargetMesh))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::UNKNOWN_ERROR, TEXT("Failed to initialize target IK Rig controller with target mesh"));
    }
    if (!TargetRigController->SetRetargetRoot(FName(*TargetRetargetRoot)))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::INVALID_PARAM,
            FString::Printf(TEXT("Failed to set target retarget root: %s"), *TargetRetargetRoot));
    }

    for (const FRequestedRetargetChain& Chain : RequestedChains)
    {
        const FName AddedName = TargetRigController->AddRetargetChain(Chain.Name, Chain.StartBone, Chain.EndBone, Chain.Goal);
        if (AddedName == NAME_None)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                MCPErrorCodes::INVALID_PARAM,
                FString::Printf(TEXT("Failed to add target retarget chain: %s"), *Chain.Name.ToString()));
        }
    }
    TargetRigController->SortRetargetChains();

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UIKRetargetFactory* RetargetFactory = NewObject<UIKRetargetFactory>();
    UObject* RetargeterObject = AssetToolsModule.Get().CreateAsset(
        RetargeterAssetName,
        RetargeterPackagePath,
        UIKRetargeter::StaticClass(),
        RetargetFactory);
    UIKRetargeter* Retargeter = Cast<UIKRetargeter>(RetargeterObject);
    if (!Retargeter)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Failed to create IK Retargeter asset: %s"), *RetargeterAssetPath));
    }

    UIKRetargeterController* RetargeterController = UIKRetargeterController::GetController(Retargeter);
    if (!RetargeterController)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::UNKNOWN_ERROR, TEXT("Failed to get IK Retargeter controller"));
    }

    RetargeterController->SetIKRig(ERetargetSourceOrTarget::Source, SourceIKRig);
    RetargeterController->SetIKRig(ERetargetSourceOrTarget::Target, TargetIKRig);
    RetargeterController->SetPreviewMesh(ERetargetSourceOrTarget::Source, SourceMesh);
    RetargeterController->SetPreviewMesh(ERetargetSourceOrTarget::Target, TargetMesh);
    if (bAddDefaultOps)
    {
        RetargeterController->AddDefaultOps();
        RetargeterController->AssignIKRigToAllOps(ERetargetSourceOrTarget::Source, SourceIKRig);
        RetargeterController->AssignIKRigToAllOps(ERetargetSourceOrTarget::Target, TargetIKRig);
    }
    if (bAutoMapChains)
    {
        RetargeterController->AutoMapChains(EAutoMapChainType::Exact, true);
    }
    RetargeterController->CleanAsset();

    TargetIKRig->MarkPackageDirty();
    TargetIKRig->PostEditChange();
    Retargeter->MarkPackageDirty();
    Retargeter->PostEditChange();

    const bool bSavedTargetRig = UEditorAssetLibrary::SaveLoadedAsset(TargetIKRig, false);
    const bool bSavedRetargeter = UEditorAssetLibrary::SaveLoadedAsset(Retargeter, false);

    auto ChainNamesToJson = [](const TArray<FBoneChain>& Chains) -> TArray<TSharedPtr<FJsonValue>>
    {
        TArray<TSharedPtr<FJsonValue>> Names;
        for (const FBoneChain& Chain : Chains)
        {
            Names.Add(MakeShared<FJsonValueString>(Chain.ChainName.ToString()));
        }
        return Names;
    };

    TArray<TSharedPtr<FJsonValue>> ChainMappings;
    for (const FBoneChain& TargetChain : TargetIKRig->GetRetargetChains())
    {
        TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();
        MappingObj->SetStringField(TEXT("target_chain"), TargetChain.ChainName.ToString());
        MappingObj->SetStringField(TEXT("source_chain"), RetargeterController->GetSourceChain(TargetChain.ChainName).ToString());
        ChainMappings.Add(MakeShared<FJsonValueObject>(MappingObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_ik_rig_path"), SourceIKRig->GetPathName());
    ResultObj->SetStringField(TEXT("source_mesh_path"), SourceMesh->GetPathName());
    ResultObj->SetStringField(TEXT("target_mesh_path"), TargetMesh->GetPathName());
    ResultObj->SetStringField(TEXT("target_ik_rig_path"), TargetIKRig->GetPathName());
    ResultObj->SetStringField(TEXT("retargeter_path"), Retargeter->GetPathName());
    ResultObj->SetStringField(TEXT("target_retarget_root"), TargetRetargetRoot);
    ResultObj->SetBoolField(TEXT("replace_existing"), bReplaceExisting);
    ResultObj->SetBoolField(TEXT("auto_map_chains"), bAutoMapChains);
    ResultObj->SetBoolField(TEXT("add_default_ops"), bAddDefaultOps);
    ResultObj->SetBoolField(TEXT("saved_target_ik_rig"), bSavedTargetRig);
    ResultObj->SetBoolField(TEXT("saved_retargeter"), bSavedRetargeter);
    ResultObj->SetArrayField(TEXT("source_chain_names"), ChainNamesToJson(SourceIKRig->GetRetargetChains()));
    ResultObj->SetArrayField(TEXT("target_chain_names"), ChainNamesToJson(TargetIKRig->GetRetargetChains()));
    ResultObj->SetArrayField(TEXT("chain_mappings"), ChainMappings);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleListAnimationAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString SearchPath = TEXT("/Game/");
    bool bRecursive = true;
    int32 Limit = 500;

    TArray<FString> ClassNames;
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("search_path"), SearchPath);
        Params->TryGetBoolField(TEXT("recursive"), bRecursive);

        double LimitNumber = 0.0;
        if (Params->TryGetNumberField(TEXT("limit"), LimitNumber))
        {
            Limit = FMath::Max(0, static_cast<int32>(LimitNumber));
        }

        const TArray<TSharedPtr<FJsonValue>>* ClassNameValues = nullptr;
        if (Params->TryGetArrayField(TEXT("class_names"), ClassNameValues) && ClassNameValues)
        {
            for (const TSharedPtr<FJsonValue>& Value : *ClassNameValues)
            {
                if (Value.IsValid())
                {
                    ClassNames.Add(Value->AsString());
                }
            }
        }
    }

    if (!SearchPath.StartsWith(TEXT("/")))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::INVALID_PARAM, TEXT("search_path must be an Unreal content path such as /Game/"));
    }

    const TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(SearchPath, bRecursive, false);
    TArray<TSharedPtr<FJsonValue>> Assets;

    auto IsWantedClass = [&ClassNames](const FString& ClassName) -> bool
    {
        if (ClassNames.Num() > 0)
        {
            for (const FString& RequestedClass : ClassNames)
            {
                if (ClassName.Equals(RequestedClass, ESearchCase::IgnoreCase))
                {
                    return true;
                }
            }
            return false;
        }

        return ClassName.Equals(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase) ||
               ClassName.Equals(TEXT("AnimSequence"), ESearchCase::IgnoreCase) ||
               ClassName.Equals(TEXT("IKRetargeter"), ESearchCase::IgnoreCase) ||
               ClassName.Equals(TEXT("IKRigDefinition"), ESearchCase::IgnoreCase);
    };

    for (const FString& AssetPath : AssetPaths)
    {
        if (Limit > 0 && Assets.Num() >= Limit)
        {
            break;
        }

        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset)
        {
            continue;
        }

        const FString ClassName = Asset->GetClass() ? Asset->GetClass()->GetName() : TEXT("");
        if (!IsWantedClass(ClassName))
        {
            continue;
        }

        TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
        AssetObj->SetStringField(TEXT("name"), Asset->GetName());
        AssetObj->SetStringField(TEXT("path"), Asset->GetPathName());
        AssetObj->SetStringField(TEXT("package_path"), AssetPath);
        AssetObj->SetStringField(TEXT("class"), ClassName);
        Assets.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("assets"), Assets);
    ResultObj->SetNumberField(TEXT("count"), Assets.Num());
    ResultObj->SetStringField(TEXT("search_path"), SearchPath);
    ResultObj->SetBoolField(TEXT("recursive"), bRecursive);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    FEpicUnrealMCPBlueprintCommands BlueprintCommands;
    return BlueprintCommands.HandleCommand(TEXT("spawn_blueprint_actor"), Params);
}

// ============================================================================
// Editor lifecycle commands (existing + Phase 1 new)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRequestEditorExit(const TSharedPtr<FJsonObject>& Params)
{
    bool bForce = false;
    if (Params.IsValid()) { Params->TryGetBoolField(TEXT("force"), bForce); }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("accepted"), true);
    ResultObj->SetBoolField(TEXT("force"), bForce);
    ResultObj->SetStringField(TEXT("message"), TEXT("Editor exit requested"));
    FPlatformMisc::RequestExit(bForce);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRestartEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString AdditionalArgs;
    if (Params.IsValid()) { Params->TryGetStringField(TEXT("additional_args"), AdditionalArgs); }

    const FString ExecutablePath = FPlatformProcess::ExecutablePath();
    const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

    if (ExecutablePath.IsEmpty()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to determine Unreal Editor executable path")); }
    if (ProjectPath.IsEmpty()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to determine current project path")); }

    FString LaunchArgs = FString::Printf(TEXT("\"%s\""), *ProjectPath);
    if (!AdditionalArgs.IsEmpty()) { LaunchArgs += TEXT(" ") + AdditionalArgs; }

    FProcHandle NewEditorProcess = FPlatformProcess::CreateProc(*ExecutablePath, *LaunchArgs, true, false, false, nullptr, 0, nullptr, nullptr);
    if (!NewEditorProcess.IsValid()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to launch replacement Unreal Editor process")); }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("accepted"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Editor restart requested"));
    ResultObj->SetStringField(TEXT("executable_path"), ExecutablePath);
    ResultObj->SetStringField(TEXT("project_path"), ProjectPath);
    ResultObj->SetStringField(TEXT("launch_args"), LaunchArgs);
    FPlatformMisc::RequestExit(false);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetEditorState(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

    FString Mode = TEXT("editing");
    bool bIsPaused = false;

    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        if (GEditor->bIsSimulatingInEditor)
        {
            Mode = TEXT("simulating");
        }
        else
        {
            Mode = TEXT("pie");
        }

        if (GEditor->PlayWorld)
        {
            bIsPaused = GEditor->PlayWorld->IsPaused();
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    FString MapName = World ? World->GetMapName() : TEXT("Unknown");
    FString ProjectName = FApp::GetProjectName();

    ResultObj->SetStringField(TEXT("mode"), Mode);
    ResultObj->SetStringField(TEXT("project_name"), ProjectName);
    ResultObj->SetStringField(TEXT("map_name"), MapName);
    ResultObj->SetBoolField(TEXT("is_paused"), bIsPaused);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetEngineVersion(const TSharedPtr<FJsonObject>& Params)
{
    const FEngineVersion& Version = FEngineVersion::Current();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetNumberField(TEXT("major"), Version.GetMajor());
    ResultObj->SetNumberField(TEXT("minor"), Version.GetMinor());
    ResultObj->SetNumberField(TEXT("patch"), Version.GetPatch());
    ResultObj->SetNumberField(TEXT("changelist"), Version.GetChangelist());
    ResultObj->SetStringField(TEXT("branch"), Version.GetBranch());
    ResultObj->SetStringField(TEXT("full_string"), Version.ToString());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
    bool bResult = FEditorFileUtils::SaveDirtyPackages(
        /*bPromptUserToSave=*/ false,
        /*bSaveMapPackages=*/ true,
        /*bSaveContentPackages=*/ true);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bResult);
    ResultObj->SetStringField(TEXT("message"), bResult ? TEXT("All dirty packages saved") : TEXT("Save completed with warnings"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandlePlayInEditorStart(const TSharedPtr<FJsonObject>& Params)
{
    if (!GUnrealEd)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("GUnrealEd not available"));
    }

    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("A play session is already in progress"));
    }

    FRequestPlaySessionParams PlayParams;
    PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;

    GUnrealEd->RequestPlaySession(PlayParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Play In Editor session requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandlePlayInEditorStop(const TSharedPtr<FJsonObject>& Params)
{
    if (!GUnrealEd)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("GUnrealEd not available"));
    }

    if (!GEditor || !GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("No play session is currently active"));
    }

    GUnrealEd->RequestEndPlayMap();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Play session stop requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSimulateStart(const TSharedPtr<FJsonObject>& Params)
{
    if (!GUnrealEd)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("GUnrealEd not available"));
    }

    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("A play/simulate session is already in progress"));
    }

    FRequestPlaySessionParams SimParams;
    SimParams.WorldType = EPlaySessionWorldType::SimulateInEditor;

    GUnrealEd->RequestPlaySession(SimParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Simulate In Editor session requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSimulateStop(const TSharedPtr<FJsonObject>& Params)
{
    if (!GUnrealEd)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("GUnrealEd not available"));
    }

    if (!GEditor || !GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("No simulation session is currently active"));
    }

    GUnrealEd->RequestEndPlayMap();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Simulation stop requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandlePauseGame(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor || !GEditor->IsPlaySessionInProgress() || !GEditor->PlayWorld)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("No active play session to pause/unpause"));
    }

    // Optional explicit pause state; if not provided, toggle
    bool bNewPaused = false;
    bool bExplicit = false;
    if (Params.IsValid())
    {
        bExplicit = Params->TryGetBoolField(TEXT("pause"), bNewPaused);
    }

    if (!bExplicit)
    {
        bNewPaused = !GEditor->PlayWorld->IsPaused();
    }

    GEditor->PlayWorld->bDebugPauseExecution = bNewPaused;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("paused"), bNewPaused);
    ResultObj->SetStringField(TEXT("message"), bNewPaused ? TEXT("Game paused") : TEXT("Game resumed"));
    return ResultObj;
}

// ============================================================================
// Level lifecycle commands (Phase 1 new)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleListLevels(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("persistent_level"), World->GetMapName());

    TArray<TSharedPtr<FJsonValue>> StreamingLevels;
    const TArray<ULevelStreaming*>& Levels = World->GetStreamingLevels();
    for (const ULevelStreaming* StreamingLevel : Levels)
    {
        if (!StreamingLevel) continue;

        TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
        LevelObj->SetStringField(TEXT("package_name"), StreamingLevel->GetWorldAssetPackageName());
        LevelObj->SetBoolField(TEXT("is_loaded"), StreamingLevel->IsLevelLoaded());
        LevelObj->SetBoolField(TEXT("is_visible"), StreamingLevel->IsLevelVisible());
        LevelObj->SetBoolField(TEXT("should_be_visible"), StreamingLevel->GetShouldBeVisibleFlag());
        StreamingLevels.Add(MakeShared<FJsonValueObject>(LevelObj));
    }

    ResultObj->SetArrayField(TEXT("streaming_levels"), StreamingLevels);
    ResultObj->SetNumberField(TEXT("streaming_level_count"), StreamingLevels.Num());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_path"), LevelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_path' parameter"));
    }

    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::EDITOR_BUSY, TEXT("Cannot open level during PIE/simulation"));
    }

    FString LoadErrors;
    if (!UEditorLoadingAndSavingUtils::LoadMap(LevelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Failed to open level: %s"), *LevelPath));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("map_name"), World ? World->GetMapName() : TEXT("Unknown"));
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Opened level: %s"), *LevelPath));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSaveLevel(const TSharedPtr<FJsonObject>& Params)
{
    bool bResult = FEditorFileUtils::SaveCurrentLevel();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bResult);
    ResultObj->SetStringField(TEXT("message"), bResult ? TEXT("Current level saved") : TEXT("Failed to save current level"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("name"), LevelName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'name' parameter"));
    }

    FString BasePath = TEXT("/Game/Maps/");
    if (Params->HasField(TEXT("path")))
    {
        Params->TryGetStringField(TEXT("path"), BasePath);
        if (!BasePath.EndsWith(TEXT("/"))) { BasePath += TEXT("/"); }
    }

    FString FullPath = BasePath + LevelName;

    // Check if asset already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, FString::Printf(TEXT("Level already exists: %s"), *FullPath));
    }

    // Create a new blank map by loading the default empty map template
    UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Failed to create level world: %s"), *FullPath));
    }

    // Save it to the specified path
    bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(World, FullPath);
    if (!bSaved)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            MCPErrorCodes::UNKNOWN_ERROR,
            FString::Printf(TEXT("Failed to save level: %s"), *FullPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("level_path"), FullPath);
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Created level: %s"), *FullPath));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDuplicateLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath, DestPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("source_path"), SourcePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'source_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'dest_path' parameter"));
    }

    UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
    if (!DuplicatedAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Failed to duplicate '%s' to '%s'"), *SourcePath, *DestPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_path"), SourcePath);
    ResultObj->SetStringField(TEXT("dest_path"), DestPath);
    ResultObj->SetStringField(TEXT("message"), TEXT("Level duplicated successfully"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_path"), LevelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_path' parameter"));
    }

    // Prevent deleting the currently loaded level
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (World)
    {
        FString CurrentMapPath = World->GetOutermost()->GetName();
        if (LevelPath == CurrentMapPath || LevelPath == World->GetMapName())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("Cannot delete the currently loaded level"));
        }
    }

    bool bDeleted = UEditorAssetLibrary::DeleteAsset(LevelPath);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bDeleted);
    ResultObj->SetStringField(TEXT("message"), bDeleted ? TEXT("Level deleted") : TEXT("Failed to delete level"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetCurrentWorld(const TSharedPtr<FJsonObject>& Params)
{
    // set_current_world is semantically equivalent to open_level for the persistent level
    return HandleOpenLevel(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleAddSublevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_path"), LevelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_path' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    ULevelStreaming* NewStreamingLevel = UEditorLevelUtils::AddLevelToWorld(
        World, *LevelPath, ULevelStreamingDynamic::StaticClass());

    if (!NewStreamingLevel)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Failed to add sublevel: %s"), *LevelPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("level_path"), LevelPath);
    ResultObj->SetStringField(TEXT("message"), TEXT("Sublevel added"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRemoveSublevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_name"), LevelName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (!StreamingLevel) continue;

        if (StreamingLevel->GetWorldAssetPackageName().Contains(LevelName))
        {
            ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
            if (LoadedLevel)
            {
                UEditorLevelUtils::RemoveLevelFromWorld(LoadedLevel);
            }

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetBoolField(TEXT("success"), true);
            ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Sublevel removed: %s"), *LevelName));
            return ResultObj;
        }
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Sublevel not found: %s"), *LevelName));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleToggleSublevelVisibility(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("level_name"), LevelName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'level_name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (!StreamingLevel) continue;

        if (StreamingLevel->GetWorldAssetPackageName().Contains(LevelName))
        {
            ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
            if (!LoadedLevel)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, FString::Printf(TEXT("Level '%s' is not loaded"), *LevelName));
            }

            // Check for explicit visible parameter, otherwise toggle
            bool bNewVisible;
            bool bExplicit = Params->TryGetBoolField(TEXT("visible"), bNewVisible);
            if (!bExplicit)
            {
                bNewVisible = !StreamingLevel->IsLevelVisible();
            }

            UEditorLevelUtils::SetLevelVisibility(LoadedLevel, bNewVisible, false);

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetBoolField(TEXT("success"), true);
            ResultObj->SetBoolField(TEXT("visible"), bNewVisible);
            ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Sublevel '%s' visibility set to %s"), *LevelName, bNewVisible ? TEXT("visible") : TEXT("hidden")));
            return ResultObj;
        }
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::NOT_FOUND, FString::Printf(TEXT("Sublevel not found: %s"), *LevelName));
}

// ============================================================================
// Selection and viewport commands (Phase 1 new)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));
    }

    USelection* Selection = GEditor->GetSelectedActors();
    TArray<TSharedPtr<FJsonValue>> SelectedArray;

    if (Selection)
    {
        for (int32 i = 0; i < Selection->Num(); ++i)
        {
            AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
            if (Actor)
            {
                SelectedArray.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), SelectedArray);
    ResultObj->SetNumberField(TEXT("count"), SelectedArray.Num());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSelectActors(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));
    }

    bool bAddToSelection = false;
    if (Params.IsValid()) { Params->TryGetBoolField(TEXT("add_to_selection"), bAddToSelection); }

    if (!bAddToSelection)
    {
        GEditor->SelectNone(true, true, false);
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world")); }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    int32 SelectedCount = 0;

    // Select by names
    const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
    if (Params.IsValid() && Params->TryGetArrayField(TEXT("actor_names"), ActorNames) && ActorNames)
    {
        for (const TSharedPtr<FJsonValue>& NameVal : *ActorNames)
        {
            if (!NameVal.IsValid()) continue;
            FString Name = NameVal->AsString();
            for (AActor* Actor : AllActors)
            {
                if (Actor && Actor->GetName() == Name)
                {
                    GEditor->SelectActor(Actor, true, true);
                    SelectedCount++;
                    break;
                }
            }
        }
    }

    // Select by class
    FString ActorClass;
    if (Params.IsValid() && Params->TryGetStringField(TEXT("actor_class"), ActorClass))
    {
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetClass()->GetName().Contains(ActorClass))
            {
                GEditor->SelectActor(Actor, true, true);
                SelectedCount++;
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("selected_count"), SelectedCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFocusViewportOnSelection(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available"));
    }

    USelection* Selection = GEditor->GetSelectedActors();
    TArray<AActor*> SelectedActors;
    if (Selection)
    {
        for (int32 i = 0; i < Selection->Num(); ++i)
        {
            AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
            if (Actor) { SelectedActors.Add(Actor); }
        }
    }

    if (SelectedActors.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::INVALID_PARAM, TEXT("No actors selected to focus on"));
    }

    GEditor->MoveViewportCamerasToActor(SelectedActors, false);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Viewport focused on %d selected actor(s)"), SelectedActors.Num()));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
    if (!ViewportClient)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active level editing viewport"));
    }

    if (Params->HasField(TEXT("location")))
    {
        FVector Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        ViewportClient->SetViewLocation(Location);
    }

    if (Params->HasField(TEXT("rotation")))
    {
        FRotator Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
        ViewportClient->SetViewRotation(Rotation);
    }

    double FOV = 0.0;
    if (Params->TryGetNumberField(TEXT("fov"), FOV) && FOV > 0.0)
    {
        ViewportClient->ViewFOV = static_cast<float>(FOV);
    }

    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Viewport camera updated"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCaptureViewportScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    FString Filename;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("filename"), Filename))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(MCPErrorCodes::MISSING_PARAM, TEXT("Missing 'filename' parameter"));
    }

    // Ensure path is absolute
    if (FPaths::IsRelative(Filename))
    {
        Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), Filename);
    }

    // Ensure .png extension
    if (!Filename.EndsWith(TEXT(".png")))
    {
        Filename += TEXT(".png");
    }

    FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
    if (!ViewportClient || !ViewportClient->Viewport)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active viewport for screenshot"));
    }

    FViewport* Viewport = ViewportClient->Viewport;
    TArray<FColor> Bitmap;
    if (!Viewport->ReadPixels(Bitmap))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read viewport pixels"));
    }

    int32 Width = Viewport->GetSizeXY().X;
    int32 Height = Viewport->GetSizeXY().Y;

    TArray64<uint8> CompressedData;
    FImageUtils::PNGCompressImageArray(Width, Height, Bitmap, CompressedData);

    if (!FFileHelper::SaveArrayToFile(CompressedData, *Filename))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save screenshot to: %s"), *Filename));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("file_path"), Filename);
    ResultObj->SetNumberField(TEXT("width"), Width);
    ResultObj->SetNumberField(TEXT("height"), Height);
    ResultObj->SetStringField(TEXT("message"), TEXT("Screenshot captured"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetViewportStats(const TSharedPtr<FJsonObject>& Params)
{
    FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
    if (!ViewportClient)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active level editing viewport"));
    }

    FVector Location = ViewportClient->GetViewLocation();
    FRotator Rotation = ViewportClient->GetViewRotation();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

    TArray<TSharedPtr<FJsonValue>> LocationArr;
    LocationArr.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArr.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArr.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ResultObj->SetArrayField(TEXT("location"), LocationArr);

    TArray<TSharedPtr<FJsonValue>> RotationArr;
    RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArr.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ResultObj->SetArrayField(TEXT("rotation"), RotationArr);

    ResultObj->SetNumberField(TEXT("fov"), ViewportClient->ViewFOV);

    if (ViewportClient->Viewport)
    {
        ResultObj->SetNumberField(TEXT("width"), ViewportClient->Viewport->GetSizeXY().X);
        ResultObj->SetNumberField(TEXT("height"), ViewportClient->Viewport->GetSizeXY().Y);
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// ============================================================================
// Generic object reflection commands (merged from standalone)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleBuildNavMesh(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    UClass* NavSysClass = FindObject<UClass>(nullptr, TEXT("/Script/NavigationSystem.NavigationSystemV1"));
    if (!NavSysClass) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NavigationSystemV1 class not found. Is NavigationSystem module loaded?"));

    UNavigationSystemBase* NavSys = World->GetNavigationSystem();
    if (!NavSys) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Navigation System not found in world"));

    UFunction* BuildFunc = NavSys->FindFunction(TEXT("Build"));
    if (!BuildFunc) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NavigationSystemV1::Build() function not found"));

    NavSys->ProcessEvent(BuildFunc, nullptr);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("NavMesh build triggered"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetObjectProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString ResolveError;
    UObject* TargetObject = ResolveTargetObject(Params, ResolveError);
    if (!TargetObject)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError);
    }

    TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
    const TArray<TSharedPtr<FJsonValue>>* RequestedPropertyNames = nullptr;
    const bool bHasFilter = Params.IsValid() &&
                            Params->TryGetArrayField(TEXT("property_names"), RequestedPropertyNames) &&
                            RequestedPropertyNames;

    if (bHasFilter)
    {
        for (const TSharedPtr<FJsonValue>& NameValue : *RequestedPropertyNames)
        {
            if (!NameValue.IsValid() || NameValue->Type != EJson::String) continue;
            const FString PropertyName = NameValue->AsString();
            FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
            if (!Property) { PropertiesObj->SetField(PropertyName, MakeShared<FJsonValueNull>()); continue; }
            void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
            PropertiesObj->SetField(PropertyName, PropertyValueToJson(Property, PropertyValuePtr));
        }
    }
    else
    {
        for (TFieldIterator<FProperty> It(TargetObject->GetClass()); It; ++It)
        {
            FProperty* Property = *It;
            if (!Property) continue;
            void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
            PropertiesObj->SetField(Property->GetName(), PropertyValueToJson(Property, PropertyValuePtr));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("object_name"), TargetObject->GetName());
    ResultObj->SetStringField(TEXT("object_class"), TargetObject->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("object_path"), TargetObject->GetPathName());
    ResultObj->SetObjectField(TEXT("properties"), PropertiesObj);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetObjectProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString ResolveError;
    UObject* TargetObject = ResolveTargetObject(Params, ResolveError);
    if (!TargetObject) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError); }

    const TSharedPtr<FJsonObject>* PropertiesToSetPtr = nullptr;
    if (!Params.IsValid() || !Params->TryGetObjectField(TEXT("properties"), PropertiesToSetPtr) || !PropertiesToSetPtr || !PropertiesToSetPtr->IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' object"));
    }

    const TSharedPtr<FJsonObject>& PropertiesToSet = *PropertiesToSetPtr;
    TArray<TSharedPtr<FJsonValue>> UpdatedProperties;
    TSharedPtr<FJsonObject> FailedProperties = MakeShared<FJsonObject>();

    TargetObject->Modify();

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : PropertiesToSet->Values)
    {
        const FString& PropertyName = Pair.Key;
        FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
        if (!Property) { FailedProperties->SetStringField(PropertyName, FString::Printf(TEXT("Property not found: %s"), *PropertyName)); continue; }
        void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
        FString SetError;
        if (!SetPropertyFromJsonValue(Property, PropertyValuePtr, Pair.Value, SetError)) { FailedProperties->SetStringField(PropertyName, SetError); continue; }
        UpdatedProperties.Add(MakeShared<FJsonValueString>(PropertyName));
    }

    TargetObject->PostEditChange();
    TargetObject->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("object_name"), TargetObject->GetName());
    ResultObj->SetStringField(TEXT("object_class"), TargetObject->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("object_path"), TargetObject->GetPathName());
    ResultObj->SetArrayField(TEXT("updated_properties"), UpdatedProperties);
    ResultObj->SetNumberField(TEXT("updated_count"), UpdatedProperties.Num());

    if (FailedProperties->Values.Num() > 0)
    {
        ResultObj->SetObjectField(TEXT("failed_properties"), FailedProperties);
        ResultObj->SetNumberField(TEXT("failed_count"), FailedProperties->Values.Num());
    }
    else
    {
        ResultObj->SetNumberField(TEXT("failed_count"), 0);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCallUObjectFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString ResolveError;
    UObject* TargetObject = ResolveTargetObject(Params, ResolveError);
    if (!TargetObject) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ResolveError); }

    FString FunctionName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UFunction* Function = TargetObject->FindFunction(FName(*FunctionName));
    if (!Function)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Function '%s' not found on object '%s'"), *FunctionName, *TargetObject->GetName()));
    }

    TSharedPtr<FJsonObject> ArgumentsObj = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject>* ArgumentsObjPtr = nullptr;
    if (Params->TryGetObjectField(TEXT("arguments"), ArgumentsObjPtr) && ArgumentsObjPtr && ArgumentsObjPtr->IsValid())
    {
        ArgumentsObj = *ArgumentsObjPtr;
    }

    uint8* ParamsBuffer = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
    FMemory::Memzero(ParamsBuffer, Function->ParmsSize);

    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
        const FString ParamName = Property->GetName();
        if (!ArgumentsObj->HasField(ParamName)) continue;
        TSharedPtr<FJsonValue> ParamValue = ArgumentsObj->TryGetField(ParamName);
        if (!ParamValue.IsValid()) continue;
        void* ParamValuePtr = Property->ContainerPtrToValuePtr<void>(ParamsBuffer);
        FString SetError;
        if (!SetPropertyFromJsonValue(Property, ParamValuePtr, ParamValue, SetError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to set parameter '%s': %s"), *ParamName, *SetError));
        }
    }

    TargetObject->ProcessEvent(Function, ParamsBuffer);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("object_name"), TargetObject->GetName());
    ResultObj->SetStringField(TEXT("object_class"), TargetObject->GetClass()->GetName());
    ResultObj->SetStringField(TEXT("object_path"), TargetObject->GetPathName());
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);

    TSharedPtr<FJsonObject> OutParamsObj = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm)) continue;
        void* ParamValuePtr = Property->ContainerPtrToValuePtr<void>(ParamsBuffer);
        TSharedPtr<FJsonValue> JsonValue = PropertyValueToJson(Property, ParamValuePtr);

        if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            ResultObj->SetField(TEXT("return_value"), JsonValue);
        }
        else if (Property->HasAnyPropertyFlags(CPF_OutParm))
        {
            OutParamsObj->SetField(Property->GetName(), JsonValue);
        }
    }

    if (OutParamsObj->Values.Num() > 0)
    {
        ResultObj->SetObjectField(TEXT("out_params"), OutParamsObj);
    }

    return ResultObj;
}
