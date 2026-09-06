#include "UCServerPanel.h"
#include "UnrealConnect.h"
#include "UCLogDevice.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "UnrealConnectServer.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "SUCServerPanel"

void SUCServerPanel::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SVerticalBox)

        // ── Header ──────────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f, 8.f, 8.f, 4.f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("Title", "UnrealConnect HTTP Server"))
            .Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SSeparator)
        ]

        // ── Status & Control ────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f, 6.f)
        [
            SNew(SHorizontalBox)

            // Status indicator
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(0.f, 0.f, 12.f, 0.f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("StatusLabel", "Status"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(this, &SUCServerPanel::GetStatusText)
                    .ColorAndOpacity(this, &SUCServerPanel::GetStatusColor)
                    .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                ]
            ]

            // Port info
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(0.f, 0.f, 24.f, 0.f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PortLabel", "Port"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("8765")))
                    .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                ]
            ]

            // Spacer
            + SHorizontalBox::Slot().FillWidth(1.f)

            // Start button
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(4.f, 0.f)
            [
                SNew(SButton)
                .Text(LOCTEXT("StartBtn", "Start Server"))
                .IsEnabled(this, &SUCServerPanel::IsStartEnabled)
                .OnClicked(this, &SUCServerPanel::OnStartClicked)
                .ButtonColorAndOpacity(FLinearColor(0.1f, 0.5f, 0.2f, 1.f))
            ]

            // Stop button
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(4.f, 0.f)
            [
                SNew(SButton)
                .Text(LOCTEXT("StopBtn", "Stop Server"))
                .IsEnabled(this, &SUCServerPanel::IsStopEnabled)
                .OnClicked(this, &SUCServerPanel::OnStopClicked)
                .ButtonColorAndOpacity(FLinearColor(0.5f, 0.1f, 0.1f, 1.f))
            ]
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SSeparator)
        ]

        // ── GAS Profiler Dashboard ──────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f, 8.f, 8.f, 4.f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("GASTitle", "GAS Profiler Dashboard"))
            .Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SSeparator)
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f, 6.f)
        [
            SNew(SHorizontalBox)

            // GAS Status
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(0.f, 0.f, 12.f, 0.f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("GASStatusLabel", "Status"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(this, &SUCServerPanel::GetGASStatusText)
                    .ColorAndOpacity(this, &SUCServerPanel::GetGASStatusColor)
                    .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                ]
            ]

            // GAS Port
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(0.f, 0.f, 24.f, 0.f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("GASPortLabel", "Port"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("8080")))
                    .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                ]
            ]

            // Spacer
            + SHorizontalBox::Slot().FillWidth(1.f)

            // Open button
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(4.f, 0.f)
            [
                SNew(SButton)
                .Text(LOCTEXT("OpenGASBtn", "Open GAS Profiler"))
                .IsEnabled(this, &SUCServerPanel::IsOpenGASProfilerEnabled)
                .OnClicked(this, &SUCServerPanel::OnOpenGASProfilerClicked)
                .ButtonColorAndOpacity(FLinearColor(0.1f, 0.4f, 0.8f, 1.f))
            ]

            // Stop button
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(4.f, 0.f)
            [
                SNew(SButton)
                .Text(LOCTEXT("StopGASBtn", "Stop"))
                .IsEnabled(this, &SUCServerPanel::IsStopGASProfilerEnabled)
                .OnClicked(this, &SUCServerPanel::OnStopGASProfilerClicked)
                .ButtonColorAndOpacity(FLinearColor(0.5f, 0.1f, 0.1f, 1.f))
            ]
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SSeparator)
        ]

        // ── Plan Refinery ────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f, 8.f, 8.f, 4.f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("PlanTitle", "Plan Refinery"))
            .Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SSeparator)
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f, 6.f)
        [
            SNew(SHorizontalBox)

            // Status
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(0.f, 0.f, 12.f, 0.f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FPStatusLabel", "Status"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(this, &SUCServerPanel::GetPlanStatusText)
                    .ColorAndOpacity(this, &SUCServerPanel::GetPlanStatusColor)
                    .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                ]
            ]

            // Port
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(0.f, 0.f, 24.f, 0.f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FPPortLabel", "Port"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("8081")))
                    .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                ]
            ]

            // Spacer
            + SHorizontalBox::Slot().FillWidth(1.f)

            // Open button
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(4.f, 0.f)
            [
                SNew(SButton)
                .Text(LOCTEXT("OpenPlanBtn", "Open Plan Refinery"))
                .IsEnabled(this, &SUCServerPanel::IsOpenPlanEnabled)
                .OnClicked(this, &SUCServerPanel::OnOpenPlanClicked)
                .ButtonColorAndOpacity(FLinearColor(0.1f, 0.4f, 0.8f, 1.f))
            ]

            // Stop button
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(4.f, 0.f)
            [
                SNew(SButton)
                .Text(LOCTEXT("StopPlanBtn", "Stop"))
                .IsEnabled(this, &SUCServerPanel::IsStopPlanEnabled)
                .OnClicked(this, &SUCServerPanel::OnStopPlanClicked)
                .ButtonColorAndOpacity(FLinearColor(0.5f, 0.1f, 0.1f, 1.f))
            ]
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SSeparator)
        ]

        // ── Log header ──────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f, 4.f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .FillWidth(1.f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("LogTitle", "Server Log"))
                .Font(FAppStyle::GetFontStyle("SmallFontBold"))
            ]

            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("ClearBtn", "Clear"))
                .OnClicked(this, &SUCServerPanel::OnClearLogClicked)
            ]
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SSeparator)
        ]

        // ── Log list ────────────────────────────────────────────
        + SVerticalBox::Slot()
        .FillHeight(1.f)
        .Padding(4.f)
        [
            SNew(SBox)
            .MinDesiredHeight(200.f)
            [
                SAssignNew(LogListView, SListView<TSharedPtr<FString>>)
                .ListItemsSource(&DisplayedLogs)
                .OnGenerateRow(this, &SUCServerPanel::MakeLogRow)
                .SelectionMode(ESelectionMode::None)
            ]
        ]
    ];

    // Refresh log every 0.5 seconds
    RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SUCServerPanel::OnRefreshTimer));
}

// ── Button callbacks ─────────────────────────────────────────────────────────

FReply SUCServerPanel::OnStartClicked()
{
    FUnrealConnectModule& Module = FUnrealConnectModule::Get();
    if (!Module.GetServer() || !Module.GetServer()->IsRunning())
    {
        Module.StartServer();
    }
    return FReply::Handled();
}

FReply SUCServerPanel::OnStopClicked()
{
    FUnrealConnectModule& Module = FUnrealConnectModule::Get();
    if (Module.GetServer() && Module.GetServer()->IsRunning())
    {
        Module.StopServer();
    }
    return FReply::Handled();
}

FReply SUCServerPanel::OnOpenGASProfilerClicked()
{
    const FString ToolsDir = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir() / TEXT("Tools"));

    const FString ProfilerUrl = TEXT("http://localhost:8080/GASProfiler.html");

    // 이미 서버가 떠 있으면 브라우저만 다시 열기
    if (PythonProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PythonProcHandle))
    {
        FPlatformProcess::LaunchURL(*ProfilerUrl, nullptr, nullptr);
        return FReply::Handled();
    }

    const FString PythonExe = TEXT("python");
    const FString Args = TEXT("-m http.server 8080");

    PythonProcHandle = FPlatformProcess::CreateProc(
        *PythonExe,
        *Args,
        /*bLaunchDetached=*/false,
        /*bLaunchHidden=*/true,
        /*bLaunchReallyHidden=*/true,
        /*OutProcessID=*/nullptr,
        /*PriorityModifier=*/0,
        *ToolsDir,
        /*PipeWriteChild=*/nullptr
    );

    if (PythonProcHandle.IsValid())
    {
        FPlatformProcess::LaunchURL(*ProfilerUrl, nullptr, nullptr);
    }

    return FReply::Handled();
}

FReply SUCServerPanel::OnStopGASProfilerClicked()
{
    if (PythonProcHandle.IsValid())
    {
        FPlatformProcess::TerminateProc(PythonProcHandle, true);
        FPlatformProcess::CloseProc(PythonProcHandle);
        PythonProcHandle.Reset();
    }

    return FReply::Handled();
}

FReply SUCServerPanel::OnClearLogClicked()
{
    FUnrealConnectModule& Module = FUnrealConnectModule::Get();
    if (FUCLogDevice* LogDevice = Module.GetLogDevice())
    {
        LogDevice->Clear();
    }
    DisplayedLogs.Empty();
    LastLogCount = 0;
    LogListView->RequestListRefresh();
    return FReply::Handled();
}

// ── State helpers ────────────────────────────────────────────────────────────

FText SUCServerPanel::GetStatusText() const
{
    const FUnrealConnectModule& Module = FUnrealConnectModule::Get();
    if (Module.GetServer() && Module.GetServer()->IsRunning())
    {
        return LOCTEXT("StatusRunning", "Running");
    }
    return LOCTEXT("StatusStopped", "Stopped");
}

FSlateColor SUCServerPanel::GetStatusColor() const
{
    const FUnrealConnectModule& Module = FUnrealConnectModule::Get();
    if (Module.GetServer() && Module.GetServer()->IsRunning())
    {
        return FSlateColor(FLinearColor(0.2f, 0.9f, 0.3f));
    }
    return FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f));
}

bool SUCServerPanel::IsStartEnabled() const
{
    const FUnrealConnectModule& Module = FUnrealConnectModule::Get();
    return !Module.GetServer() || !Module.GetServer()->IsRunning();
}

bool SUCServerPanel::IsStopEnabled() const
{
    const FUnrealConnectModule& Module = FUnrealConnectModule::Get();
    return Module.GetServer() && Module.GetServer()->IsRunning();
}

FText SUCServerPanel::GetGASStatusText() const
{
    if (PythonProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PythonProcHandle))
    {
        return LOCTEXT("GASStatusRunning", "Running");
    }

    return LOCTEXT("GASStatusStopped", "Stopped");
}

FSlateColor SUCServerPanel::GetGASStatusColor() const
{
    if (PythonProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PythonProcHandle))
    {
        return FSlateColor(FLinearColor(0.2f, 0.9f, 0.3f));
    }

    return FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f));
}

bool SUCServerPanel::IsOpenGASProfilerEnabled() const
{
    return !(PythonProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PythonProcHandle));
}

bool SUCServerPanel::IsStopGASProfilerEnabled() const
{
    return PythonProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PythonProcHandle);
}

// ── Plan Refinery ─────────────────────────────────────────────────────

FReply SUCServerPanel::OnOpenPlanClicked()
{
    const FString ToolsDir = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir() / TEXT("Tools"));
    const FString Url = TEXT("http://localhost:8081/PlanRefinery.html");

    if (PlanProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PlanProcHandle))
    {
        FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
        return FReply::Handled();
    }

    PlanProcHandle = FPlatformProcess::CreateProc(
        TEXT("python"),
        TEXT("-m http.server 8081"),
        /*bLaunchDetached=*/false,
        /*bLaunchHidden=*/true,
        /*bLaunchReallyHidden=*/true,
        /*OutProcessID=*/nullptr,
        /*PriorityModifier=*/0,
        *ToolsDir,
        /*PipeWriteChild=*/nullptr
    );

    if (PlanProcHandle.IsValid())
    {
        FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
    }

    return FReply::Handled();
}

FReply SUCServerPanel::OnStopPlanClicked()
{
    if (PlanProcHandle.IsValid())
    {
        FPlatformProcess::TerminateProc(PlanProcHandle, true);
        FPlatformProcess::CloseProc(PlanProcHandle);
        PlanProcHandle.Reset();
    }
    return FReply::Handled();
}

FText SUCServerPanel::GetPlanStatusText() const
{
    if (PlanProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PlanProcHandle))
        return LOCTEXT("FPStatusRunning", "Running");
    return LOCTEXT("FPStatusStopped", "Stopped");
}

FSlateColor SUCServerPanel::GetPlanStatusColor() const
{
    if (PlanProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PlanProcHandle))
        return FSlateColor(FLinearColor(0.2f, 0.9f, 0.3f));
    return FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f));
}

bool SUCServerPanel::IsOpenPlanEnabled() const
{
    return !(PlanProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PlanProcHandle));
}

bool SUCServerPanel::IsStopPlanEnabled() const
{
    return PlanProcHandle.IsValid() && FPlatformProcess::IsProcRunning(PlanProcHandle);
}

// ── Log list ─────────────────────────────────────────────────────────────────

TSharedRef<ITableRow> SUCServerPanel::MakeLogRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    FLinearColor TextColor = FLinearColor::White;
    if (Item->Contains(TEXT("[ERROR]")))  TextColor = FLinearColor(1.f, 0.3f, 0.3f);
    else if (Item->Contains(TEXT("[WARN]")))  TextColor = FLinearColor(1.f, 0.8f, 0.2f);

    return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
    [
        SNew(STextBlock)
        .Text(FText::FromString(*Item))
        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
        .ColorAndOpacity(FSlateColor(TextColor))
    ];
}

void SUCServerPanel::RefreshLog()
{
    FUnrealConnectModule& Module = FUnrealConnectModule::Get();
    FUCLogDevice* LogDevice = Module.GetLogDevice();
    if (!LogDevice) return;

    int32 CurrentCount = LogDevice->GetLogCount();
    if (CurrentCount == LastLogCount) return;

    DisplayedLogs = LogDevice->GetLogs();
    LastLogCount = CurrentCount;
    LogListView->RequestListRefresh();

    // Auto-scroll to bottom
    if (DisplayedLogs.Num() > 0)
    {
        LogListView->RequestScrollIntoView(DisplayedLogs.Last());
    }
}

EActiveTimerReturnType SUCServerPanel::OnRefreshTimer(double InCurrentTime, float InDeltaTime)
{
    RefreshLog();
    return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE