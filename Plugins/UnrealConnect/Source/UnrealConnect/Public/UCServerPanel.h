#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

/**
 * Slate panel for controlling the UnrealConnect HTTP server.
 * Displays server status, Start/Stop button, and a live log view.
 */
class SUCServerPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SUCServerPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    // --- Button callbacks ---
    FReply OnStartClicked();
    FReply OnStopClicked();
    FReply OnClearLogClicked();

    // --- Widget state helpers ---
    FText       GetStatusText() const;
    FSlateColor GetStatusColor() const;
    bool        IsStartEnabled() const;
    bool        IsStopEnabled() const;

    // --- Log list ---
    TSharedRef<ITableRow> MakeLogRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
    void RefreshLog();

    // Active timer: refresh log every 0.5s
    EActiveTimerReturnType OnRefreshTimer(double InCurrentTime, float InDeltaTime);

    TSharedPtr<SListView<TSharedPtr<FString>>> LogListView;
    TArray<TSharedPtr<FString>> DisplayedLogs;
    int32 LastLogCount = 0;

    // --- GAS Profiler ---
    FReply OnOpenGASProfilerClicked();
    FReply OnStopGASProfilerClicked();

    FText       GetGASStatusText() const;
    FSlateColor GetGASStatusColor() const;
    bool        IsOpenGASProfilerEnabled() const;
    bool        IsStopGASProfilerEnabled() const;

    mutable FProcHandle PythonProcHandle;

    // --- Flight Plan Refinery ---
    FReply OnOpenPlanClicked();
    FReply OnStopPlanClicked();

    FText       GetPlanStatusText() const;
    FSlateColor GetPlanStatusColor() const;
    bool        IsOpenPlanEnabled() const;
    bool        IsStopPlanEnabled() const;

    mutable FProcHandle PlanProcHandle;
};
