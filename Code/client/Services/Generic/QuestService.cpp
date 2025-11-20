#include <TiltedOnlinePCH.h>

#include <Events/ConnectedEvent.h>

#include <Services/QuestService.h>

#include <Components.h>
#include <PlayerCharacter.h>
#include <Actor.h>
#include <Forms/TESQuest.h>
#include <Games/TES.h>
#include <Games/Overrides.h>

#include <Events/ConnectedEvent.h>
#include <Events/EventDispatcher.h>

#include <Messages/RequestQuestUpdate.h>
#include <Messages/NotifyQuestUpdate.h>

using Quest = TESQuest; // Alias for PAPYRUS_FUNCTION namespace string "Quest"

static TESQuest* FindQuestByNameId(const String& name)
{
    auto& questRegistry = ModManager::Get()->quests;
    auto it = std::find_if(questRegistry.begin(), questRegistry.end(), [name](auto* it) { return std::strcmp(it->idName.AsAscii(), name.c_str()) == 0; });

    return it != questRegistry.end() ? *it : nullptr;
}

// Helper: map PlayerId -> Actor*
static Actor* GetActorByPlayerId(uint32_t aPlayerId, entt::registry& aWorld)
{
    auto view = aWorld.view<FormIdComponent, PlayerComponent>();
    auto it = std::find_if(view.begin(), view.end(), [view, aPlayerId](auto e) { return view.get<PlayerComponent>(e).Id == aPlayerId; });
    if (it == view.end())
        return nullptr;
    const auto& formIdComponent = view.get<FormIdComponent>(*it);
    return Cast<Actor>(TESForm::GetById(formIdComponent.Id));
}

QuestService::QuestService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_joinedConnection = aDispatcher.sink<ConnectedEvent>().connect<&QuestService::OnConnected>(this);
    m_questUpdateConnection = aDispatcher.sink<NotifyQuestUpdate>().connect<&QuestService::OnQuestUpdate>(this);

    // Game quest events
    auto* pEventList = EventDispatcherManager::Get();
    pEventList->questStartStopEvent.RegisterSink(this);
    pEventList->questStageEvent.RegisterSink(this);
}

void QuestService::OnConnected(const ConnectedEvent&) noexcept
{
    // nothing for now
}

BSTEventResult QuestService::OnEvent(const TESQuestStartStopEvent* apEvent, const EventDispatcher<TESQuestStartStopEvent>*)
{
    if (ScopedQuestOverride::IsOverriden() || !m_world.Get().GetPartyService().IsInParty())
        return BSTEventResult::kOk;

    spdlog::info("Quest start/stop event: {:X}", apEvent->formId);

    if (TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(apEvent->formId)))
    {
        if (!ShouldProcessQuestEvents(pQuest))
            return BSTEventResult::kOk;

        if (IsNonSyncableQuest(pQuest))
            return BSTEventResult::kOk;

        if (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous)
        {
            GameId Id;
            auto& modSys = m_world.GetModSystem();
            if (modSys.GetServerModId(pQuest->formID, Id))
            {
                spdlog::info(__FUNCTION__ ": queuing type none/misc quest gameId {:X} questStage {} questStatus {} questType {} formId {:X} name {}",
                             Id.LogFormat(),  pQuest->currentStage, pQuest->IsStopped() ? RequestQuestUpdate::Stopped : RequestQuestUpdate::Started,
                             static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type),
                             pQuest->formID, pQuest->fullName.value.AsAscii());
            }
        }

        m_world.GetRunner().Queue(
            [&, formId = pQuest->formID, stageId = pQuest->currentStage, stopped = pQuest->IsStopped(), type = pQuest->type]()
            {
                GameId Id;
                auto& modSys = m_world.GetModSystem();
                if (modSys.GetServerModId(formId, Id))
                {
                    RequestQuestUpdate update;
                    update.Id = Id;
                    update.Stage = stageId;
                    update.Status = stopped ? RequestQuestUpdate::Stopped : RequestQuestUpdate::Started;
                    update.ClientQuestType = static_cast<std::underlying_type_t<TESQuest::Type>>(type);

                    m_world.GetTransport().Send(update);
                }
            });
    }

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESQuestStageEvent* apEvent, const EventDispatcher<TESQuestStageEvent>*)
{
    if (ScopedQuestOverride::IsOverriden() || !m_world.Get().GetPartyService().IsInParty())
        return BSTEventResult::kOk;

    spdlog::info("Quest stage event: {:X}, stage: {}", apEvent->formId, apEvent->stageId);

    if (TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(apEvent->formId)))
    {
        if (!ShouldProcessQuestEvents(pQuest))
            return BSTEventResult::kOk;

        if (IsNonSyncableQuest(pQuest))
            return BSTEventResult::kOk;

        if (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous)
        {
            GameId Id;
            auto& modSys = m_world.GetModSystem();
            if (modSys.GetServerModId(pQuest->formID, Id))
            {
                spdlog::info(__FUNCTION__ ": queuing type none/misc quest gameId {:X} questStage {} questStatus {} questType {} formId {:X} name {}",
                             Id.LogFormat(), pQuest->currentStage,
                             RequestQuestUpdate::StageUpdate,
                             static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type),
                             pQuest->formID, pQuest->fullName.value.AsAscii());
            }
        }

        m_world.GetRunner().Queue(
            [&, formId = apEvent->formId, stageId = apEvent->stageId, type = pQuest->type]()
            {
                GameId Id;
                auto& modSys = m_world.GetModSystem();
                if (modSys.GetServerModId(formId, Id))
                {
                    RequestQuestUpdate update;
                    update.Id = Id;
                    update.Stage = stageId;
                    update.Status = RequestQuestUpdate::StageUpdate;
                    update.ClientQuestType = static_cast<std::underlying_type_t<TESQuest::Type>>(type);

                    m_world.GetTransport().Send(update);
                }
            });
    }

    return BSTEventResult::kOk;
}

void QuestService::OnQuestUpdate(const NotifyQuestUpdate& aUpdate) noexcept
{
    m_world.GetRunner().Queue([this, update = aUpdate]()
    {
        if (!TryApplyQuestUpdate(update))
        {
            spdlog::debug("Quest update pending for {:X}, stage {}", update.Id.LogFormat(), update.Stage);
            m_pendingUpdates.push_back(update);
        }
    });
}

void QuestService::OnUpdate(const UpdateEvent&) noexcept
{
    FlushPendingUpdates();
}

void QuestService::FlushPendingUpdates() noexcept
{
    if (m_pendingUpdates.empty())
        return;

    TiltedPhoques::Vector<NotifyQuestUpdate> remaining;
    remaining.reserve(m_pendingUpdates.size());

    for (const auto& update : m_pendingUpdates)
    {
        if (!TryApplyQuestUpdate(update))
            remaining.push_back(update);
    }

    m_pendingUpdates = std::move(remaining);
}

bool QuestService::TryApplyQuestUpdate(const NotifyQuestUpdate& aUpdate) noexcept
{
    ModSystem& modSystem = World::Get().GetModSystem();
    const uint32_t formId = modSystem.GetGameId(aUpdate.Id);
    if (formId == 0)
    {
        spdlog::warn("Quest update awaiting mod resolution, base id: {:X}, mod id: {:X}", aUpdate.Id.BaseId, aUpdate.Id.ModId);
        return false;
    }

    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(formId));
    if (!pQuest)
    {
        spdlog::warn("Quest {:X} not loaded yet, deferring update", formId);
        return false;
    }

    if (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous)
    {
        spdlog::info(__FUNCTION__ ": receiving type none/misc quest update gameId {:X} questStage {} questStatus {} questType {} formId {:X} name {}",
                     aUpdate.Id.LogFormat(), aUpdate.Stage, aUpdate.Status,
                     aUpdate.ClientQuestType, formId, pQuest->fullName.value.AsAscii());
    }

    if (!ShouldProcessQuestEvents(pQuest))
    {
        spdlog::debug("Skipping quest update for follower in leader's cell, quest {:X}", pQuest->formID);
        return true;
    }

    bool bResult = false;

    ScopedQuestOverride questOverride;

    switch (aUpdate.Status)
    {
    case NotifyQuestUpdate::Started:
    {
        bool startSuccess = false;
        pQuest->EnsureQuestStarted(startSuccess, true);
        pQuest->SetActive(true);
        pQuest->ScriptSetStage(aUpdate.Stage);
        bResult = true;
        spdlog::info("Remote quest started: {:X}, stage: {}", formId, aUpdate.Stage);
        break;
    }
    case NotifyQuestUpdate::StageUpdate:
        pQuest->SetActive(true);
        pQuest->ScriptSetStage(aUpdate.Stage);
        bResult = true;
        spdlog::info("Remote quest updated: {:X}, stage: {}", formId, aUpdate.Stage);
        break;
    case NotifyQuestUpdate::Stopped:
        bResult = StopQuest(formId);
        spdlog::info("Remote quest stopped: {:X}, stage: {}", formId, aUpdate.Stage);
        break;
    default:
        spdlog::warn("Unhandled quest update status {} for quest {:X}", aUpdate.Status, formId);
        break;
    }

    if (!bResult)
        spdlog::error("Failed to update the client quest state, quest: {:X}, stage: {}, status: {}", formId, aUpdate.Stage, aUpdate.Status);

    return bResult;
}

bool QuestService::StopQuest(uint32_t aformId)
{
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(aformId));
    if (pQuest)
    {
        ScopedQuestOverride _;
        pQuest->SetActive(false);
        pQuest->SetStopped();
        return true;
    }

    return false;
}

static constexpr std::array kNonSyncableQuestIds = std::to_array<uint32_t>({
    0x2BA16,   // Werewolf transformation quest
    0x20071D0, // Vampire transformation quest
    0x3AC44,   // MS13BleakFallsBarrowLeverScene
    0xF2593 // Skill experience quest
});

bool QuestService::IsNonSyncableQuest(TESQuest* apQuest)
{
    return    apQuest->stages.Empty()
           || std::find(kNonSyncableQuestIds.begin(), kNonSyncableQuestIds.end(), apQuest->formID) != kNonSyncableQuestIds.end();
}

void QuestService::DebugDumpQuests()
{
    auto& quests = ModManager::Get()->quests;
    for (TESQuest* pQuest : quests)
        spdlog::info("{:X}|{}|{}|{}", pQuest->formID, (uint8_t)pQuest->type, pQuest->priority, pQuest->idName.AsAscii());
}

bool QuestService::ShouldProcessQuestEvents(TESQuest* apQuest) const noexcept
{
    auto& partyService = m_world.GetPartyService();
    if (!partyService.IsInParty())
        return true;

    if (partyService.IsLeader())
        return true;

    const uint32_t leaderPlayerId = partyService.GetLeaderPlayerId();
    if (leaderPlayerId == 0)
        return true;

    Actor* pLeader = GetActorByPlayerId(leaderPlayerId, m_world);
    Actor* pLocal = PlayerCharacter::Get();
    if (!pLeader || !pLocal)
        return true;

    TESObjectCELL* pLeaderCell = pLeader->GetParentCellEx();
    TESObjectCELL* pLocalCell = pLocal->GetParentCellEx();

    if (pLeaderCell && pLocalCell && pLeaderCell == pLocalCell)
    {
        spdlog::debug("Follower and leader share cell; suppressing quest scripting for quest {:X}", apQuest->formID);
        return false;
    }

    return true;
}
