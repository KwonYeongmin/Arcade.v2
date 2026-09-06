using UnrealBuildTool;

public class UnrealConnect : ModuleRules
{
    public UnrealConnect(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "HTTPServer",
            "HTTP",
            "Json",
            "JsonUtilities",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "LyraGame",
            "UnrealEd",
            "EditorScriptingUtilities",
            "EditorSubsystem",
            "Kismet",
            "KismetCompiler",
            "BlueprintGraph",
            "GraphEditor",
            "LevelEditor",
            "AssetTools",
            "AssetRegistry",
            "ContentBrowser",
            "ContentBrowserData",
            "MaterialEditor",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "WorkspaceMenuStructure",
            "Projects",
            "InputCore",
        });
    }
}
