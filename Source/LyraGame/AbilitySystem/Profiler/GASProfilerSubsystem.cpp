// Source/LyraGame/AbilitySystem/Profiler/GASProfilerSubsystem.cpp
#include "AbilitySystem/Profiler/GASProfilerSubsystem.h"
#include "AbilitySystem/Profiler/GASProfilerActor.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Player/LyraPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Animation/AC_CharacterAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGASProfiler, Log, All);

TStatId UGASProfilerSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UGASProfilerSubsystem, STATGROUP_Tickables);
}

void UGASProfilerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UWorld* World = GetWorld();
    if (!ensureMsgf(World, TEXT("GASProfilerSubsystem: null World"))) return;

    FActorSpawnParameters Params;
    Params.Name         = TEXT("GASProfiler");
    Params.ObjectFlags |= RF_Transient;
    ProfilerActor = World->SpawnActor<AGASProfilerActor>(
        AGASProfilerActor::StaticClass(), FTransform::Identity, Params);

#if WITH_EDITOR
    if (ProfilerActor)
    {
        ProfilerActor->SetActorLabel(TEXT("GASProfiler"));
    }
#endif

    UE_LOG(LogGASProfiler, Log, TEXT("GASProfilerSubsystem initialized. Actor path: %s"),
        ProfilerActor ? *ProfilerActor->GetPathName() : TEXT("SPAWN FAILED"));
}

void UGASProfilerSubsystem::Deinitialize()
{
    if (IsValid(BoundASC)) UnbindASCDelegates(BoundASC);

    if (IsValid(ProfilerActor))
    {
        ProfilerActor->Destroy();
        ProfilerActor = nullptr;
    }

    Super::Deinitialize();
}

void UGASProfilerSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!IsValid(BoundASC))
    {
        if (ULyraAbilitySystemComponent* ASC = GetLocalASC())
        {
            BindASCDelegates(ASC);
        }
    }

    if (IsValid(BoundASC))
    {
        RefreshAnimationSnapshot();
    }

    TickAccumulator += DeltaTime;
    if (TickAccumulator >= TickInterval)
    {
        TickAccumulator = 0.f;
        if (IsValid(BoundASC)) RefreshSnapshot();
    }
}

void UGASProfilerSubsystem::RefreshAnimationSnapshot()
{
    CachedSnapshot.Animation.Empty();

    const AActor* Avatar = BoundASC->GetAvatarActor();
    const USkeletalMeshComponent* Mesh =
        Avatar ? Avatar->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
    UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;

    if (!AnimInstance)
    {
        LastAnimationName.Reset();
        return;
    }

    FString CurrentName;

    // Motion matching runs the anim graph, so it is what plays unless a montage overrides it.
    if (const UAC_CharacterAnimInstance* MotionMatchingInstance =
            Cast<UAC_CharacterAnimInstance>(AnimInstance))
    {
        const FAC_MotionMatchingDebugState State = MotionMatchingInstance->GetMotionMatchingDebugState();
        if (State.bIsValid)
        {
            FGASAnimationInfo Info;
            Info.Source = TEXT("MotionMatching");
            Info.AssetName = State.AnimationName;
            Info.DatabaseName = State.DatabaseName;
            Info.Time = State.Time;
            Info.SearchCost = State.SearchCost;
            Info.bIsContinuingPose = State.bIsContinuingPose;
            CachedSnapshot.Animation.Add(MoveTemp(Info));

            CurrentName = State.AnimationName;
        }
    }

    for (const FAnimMontageInstance* MontageInstance : AnimInstance->MontageInstances)
    {
        if (!MontageInstance || !MontageInstance->Montage) continue;

        FGASAnimationInfo Info;
        Info.Source = TEXT("Montage");
        Info.AssetName = MontageInstance->Montage->GetName();
        Info.Time = MontageInstance->GetPosition();
        Info.Weight = MontageInstance->GetWeight();
        CachedSnapshot.Animation.Add(MoveTemp(Info));
    }

    // Only the change is worth a history entry; the current value is already in the snapshot.
    if (CurrentName != LastAnimationName)
    {
        if (!CurrentName.IsEmpty())
        {
            AddEventEntry(TEXT("ANIM_CHANGED"), CurrentName,
                LastAnimationName.IsEmpty() ? TEXT("") : FString::Printf(TEXT("from %s"), *LastAnimationName));
        }
        LastAnimationName = CurrentName;
    }
}

ULyraAbilitySystemComponent* UGASProfilerSubsystem::GetLocalASC() const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) return nullptr;

    ALyraPlayerState* PS = PC->GetPlayerState<ALyraPlayerState>();
    if (!PS) return nullptr;

    return PS->GetLyraAbilitySystemComponent();
}

void UGASProfilerSubsystem::BindASCDelegates(ULyraAbilitySystemComponent* ASC)
{
    BoundASC = ASC;

    ASC->AbilityActivatedCallbacks.AddUObject(this, &UGASProfilerSubsystem::OnAbilityActivated);
    ASC->AbilityEndedCallbacks.AddUObject(this, &UGASProfilerSubsystem::OnAbilityEnded);
    ASC->AbilityFailedCallbacks.AddUObject(this, &UGASProfilerSubsystem::OnAbilityFailed);
    ASC->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute())
        .AddUObject(this, &UGASProfilerSubsystem::OnHealthAttributeChanged);

    UE_LOG(LogGASProfiler, Log, TEXT("GASProfilerSubsystem: bound to ASC on %s"),
        *ASC->GetOwner()->GetName());
}

void UGASProfilerSubsystem::UnbindASCDelegates(ULyraAbilitySystemComponent* ASC)
{
    ASC->AbilityActivatedCallbacks.RemoveAll(this);
    ASC->AbilityEndedCallbacks.RemoveAll(this);
    ASC->AbilityFailedCallbacks.RemoveAll(this);
    ASC->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute())
        .RemoveAll(this);
}

void UGASProfilerSubsystem::RefreshSnapshot()
{
    CachedSnapshot.ActiveAbilities.Empty();
    CachedSnapshot.ActiveEffects.Empty();
    CachedSnapshot.Attributes.Empty();

    // Abilities
    for (const FGameplayAbilitySpec& Spec : BoundASC->GetActivatableAbilities())
    {
        if (!Spec.Ability) continue;

        FGASAbilityInfo Info;
        Info.ClassName = Spec.Ability->GetClass()->GetName();
        Info.bIsActive = Spec.IsActive();

        if (const ULyraGameplayAbility* LA = Cast<ULyraGameplayAbility>(Spec.Ability))
        {
            switch (LA->GetActivationGroup())
            {
                case ELyraAbilityActivationGroup::Independent:
                    Info.ActivationGroup = TEXT("Independent"); break;
                case ELyraAbilityActivationGroup::Exclusive_Replaceable:
                    Info.ActivationGroup = TEXT("Exclusive_Replaceable"); break;
                case ELyraAbilityActivationGroup::Exclusive_Blocking:
                    Info.ActivationGroup = TEXT("Exclusive_Blocking"); break;
                default:
                    Info.ActivationGroup = TEXT("Unknown"); break;
            }
        }

        FGameplayTagContainer Tags;
        Info.OwnedTags = Spec.Ability->GetAssetTags().ToStringSimple();

        CachedSnapshot.ActiveAbilities.Add(MoveTemp(Info));
    }

    // Effects
    {
        FGameplayEffectQuery Query;
        for (const FActiveGameplayEffectHandle& Handle : BoundASC->GetActiveEffects(Query))
        {
            const FActiveGameplayEffect* Active = BoundASC->GetActiveGameplayEffect(Handle);
            if (!Active || !Active->Spec.Def) continue;

            FGASEffectInfo Info;
            Info.ClassName     = Active->Spec.Def->GetClass()->GetName();
            Info.Duration      = BoundASC->GetGameplayEffectDuration(Handle);
            Info.TimeRemaining = Active->GetTimeRemaining(GetWorld()->GetTimeSeconds());
            Info.StackCount = Active->Spec.GetStackCount();
            CachedSnapshot.ActiveEffects.Add(MoveTemp(Info));
        }
    }

    // Attributes — iterate all attribute sets via reflection
    for (const UAttributeSet* Set : BoundASC->GetSpawnedAttributes())
    {
        if (!Set) continue;
        for (TFieldIterator<FProperty> PropIt(Set->GetClass()); PropIt; ++PropIt)
        {
            const FStructProperty* SP = CastField<FStructProperty>(*PropIt);
            if (!SP || SP->Struct != FGameplayAttributeData::StaticStruct()) continue;

            const FGameplayAttributeData* Data =
                SP->ContainerPtrToValuePtr<FGameplayAttributeData>(Set);

            FGASAttributeInfo Info;
            Info.Name         = FString::Printf(TEXT("%s.%s"),
                                    *Set->GetClass()->GetName(), *PropIt->GetName());
            Info.CurrentValue = Data->GetCurrentValue();
            Info.BaseValue    = Data->GetBaseValue();
            CachedSnapshot.Attributes.Add(MoveTemp(Info));
        }
    }
}

FGASProfilerSnapshot UGASProfilerSubsystem::GetSnapshot()
{
    // Copy current event history into the snapshot and return
    CachedSnapshot.EventHistory = EventHistory;
    return CachedSnapshot;
}

void UGASProfilerSubsystem::ForceActivateAbility(const FString& AbilityTag)
{
    if (!IsValid(BoundASC)) return;

    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*AbilityTag), /*bErrorIfNotFound=*/false);
    if (!Tag.IsValid())
    {
        UE_LOG(LogGASProfiler, Warning, TEXT("ForceActivateAbility: tag '%s' not found"), *AbilityTag);
        return;
    }

    FGameplayTagContainer Container;
    Container.AddTag(Tag);
    BoundASC->TryActivateAbilitiesByTag(Container);
}

void UGASProfilerSubsystem::SetAttributeValue(const FString& AttributeName, float Value)
{
    if (!IsValid(BoundASC)) return;

    FGameplayAttribute Attr;
    if (AttributeName.Equals(TEXT("LyraHealthSet.Health"), ESearchCase::IgnoreCase))
        Attr = ULyraHealthSet::GetHealthAttribute();
    else if (AttributeName.Equals(TEXT("LyraHealthSet.MaxHealth"), ESearchCase::IgnoreCase))
        Attr = ULyraHealthSet::GetMaxHealthAttribute();
    else
    {
        UE_LOG(LogGASProfiler, Warning,
            TEXT("SetAttributeValue: unknown attribute '%s'. Supported: LyraHealthSet.Health, LyraHealthSet.MaxHealth"),
            *AttributeName);
        return;
    }

    BoundASC->SetNumericAttributeBase(Attr, Value);
}

void UGASProfilerSubsystem::ClearEventHistory()
{
    EventHistory.Empty();
}

void UGASProfilerSubsystem::AddEventEntry(const FString& EventType, const FString& AbilityOrTag, const FString& Detail)
{
    FGASEventEntry Entry;
    Entry.Timestamp   = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    Entry.EventType   = EventType;
    Entry.AbilityOrTag = AbilityOrTag;
    Entry.Detail      = Detail;

    EventHistory.Add(MoveTemp(Entry));

    // Ring buffer: keep most recent MaxHistory entries
    if (EventHistory.Num() > MaxHistory)
    {
        EventHistory.RemoveAt(0, EventHistory.Num() - MaxHistory);
    }
}

void UGASProfilerSubsystem::OnAbilityActivated(UGameplayAbility* Ability)
{
    if (!Ability) return;
    AddEventEntry(TEXT("ACTIVATED"), Ability->GetClass()->GetName());
}

void UGASProfilerSubsystem::OnAbilityEnded(UGameplayAbility* Ability)
{
    if (!Ability) return;
    AddEventEntry(TEXT("ENDED"), Ability->GetClass()->GetName());
}

void UGASProfilerSubsystem::OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags)
{
    if (!Ability) return;
    AddEventEntry(TEXT("FAILED"), Ability->GetClass()->GetName(), FailureTags.ToStringSimple());
}

void UGASProfilerSubsystem::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
    const FString Detail = FString::Printf(TEXT("%.1f -> %.1f (%.1f)"),
        Data.OldValue, Data.NewValue, Data.NewValue - Data.OldValue);
    AddEventEntry(TEXT("HEALTH_CHANGED"), TEXT("Health"), Detail);
}
