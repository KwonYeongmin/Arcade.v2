// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LyraEditor : ModuleRules
{
    public LyraEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				"LyraEditor"
			}
		);

        PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAF/Source/UAFUncookedOnly/Internal"),
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAF/Source/UAF/Internal"),
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAFAnimGraph/Source/UAFAnimGraphUncookedOnly/Internal"),
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAFAnimGraph/Source/UAFAnimGraph/Internal"),
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAFLayering/Source/UAFLayering/Internal"),
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAFLayering/Source/UAFLayeringUncookedOnly/Internal"),
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAFStateTree/Source/UAFStateTree/Internal"),
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAFStateTree/Source/UAFStateTree/Private"),
				Path.Combine(EngineDirectory, "Plugins/Experimental/UAF/UAFStateTree/Source/UAFStateTreeUncookedOnly/Internal")
			}
		);

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "EditorFramework",
                "UnrealEd",
				"PhysicsCore",
				"GameplayTagsEditor",
				"GameplayTasksEditor",
				"GameplayAbilities",
				"GameplayAbilitiesEditor",
				"StudioTelemetry",
				"LyraGame",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"InputCore",
				"AnimGraph",
				"AnimGraphRuntime",
				"BlueprintGraph",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"EditorStyle",
				"DataValidation",
				"MessageLog",
				"Projects",
				"DeveloperToolSettings",
				"CollectionManager",
				"SourceControl",
				"Chaos",
				"AssetTools",
				"EnhancedInput",
				"GameplayTags",
				"Mover",
				"RigVM",
				"RigVMDeveloper",
				"UAF",
				"UAFAnimGraph",
				"UAFAnimGraphUncookedOnly",
				"UAFAnimNodeEditor",
				"UAFUncookedOnly",
				"UAFLayering",
				"UAFLayeringUncookedOnly",
				"HierarchyTableRuntime",
				"HierarchyTableAnimationRuntime",
				"StateTreeModule",
				"StateTreeEditorModule",
				"PropertyBindingUtils",
				"UAFStateTree",
				"UAFStateTreeUncookedOnly"
			}
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
			}
		);
		// Basic setup for External RPC Framework.
		// Functionality within framework will be stripped in shipping to remove vulnerabilities.
		PrivateDependencyModuleNames.Add("ExternalRpcRegistry");
		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			PublicDefinitions.Add("WITH_RPC_REGISTRY=0");
			PublicDefinitions.Add("WITH_HTTPSERVER_LISTENERS=0");
		}
		else
		{
			PrivateDependencyModuleNames.Add("HTTPServer");
			PublicDefinitions.Add("WITH_RPC_REGISTRY=1");
			PublicDefinitions.Add("WITH_HTTPSERVER_LISTENERS=1");
		}
		// Generate compile errors if using DrawDebug functions in test/shipping builds.
		PublicDefinitions.Add("SHIPPING_DRAW_DEBUG_ERROR=1");
    }
}
