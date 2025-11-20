#pragma once

#include <World.h>
#include <Events/EventDispatcher.h>
#include <Events/UpdateEvent.h>
#include <Games/Events.h>
#include <TiltedCore/Stl.hpp>
#include <Messages/NotifyQuestUpdate.h>

struct NotifyQuestUpdate;

struct TESQuest;

/**
 * @brief Handles quest sync
 */
class QuestService final : public BSTEventSink<TESQuestStartStopEvent>, BSTEventSink<TESQuestStageEvent>
{
public:
    QuestService(World&, entt::dispatcher&);
    ~QuestService() = default;

    static bool IsNonSyncableQuest(TESQuest* apQuest);
    static void DebugDumpQuests();
    static bool StopQuest(uint32_t aformId);

private:
    friend struct QuestEventHandler;

    void OnConnected(const ConnectedEvent&) noexcept;

    // Game quest events
    BSTEventResult OnEvent(const TESQuestStartStopEvent*, const EventDispatcher<TESQuestStartStopEvent>*) override;
    BSTEventResult OnEvent(const TESQuestStageEvent*, const EventDispatcher<TESQuestStageEvent>*) override;

    // Network quest updates
    void OnQuestUpdate(const NotifyQuestUpdate&) noexcept;
    void OnUpdate(const UpdateEvent&) noexcept;
    bool TryApplyQuestUpdate(const NotifyQuestUpdate& aUpdate) noexcept;
    void FlushPendingUpdates() noexcept;
    bool ShouldProcessQuestEvents(TESQuest* apQuest) const noexcept;

    World& m_world;

    // Existing connections
    entt::scoped_connection m_joinedConnection;
    entt::scoped_connection m_leftConnection;
    entt::scoped_connection m_questUpdateConnection;
    entt::scoped_connection m_updateConnection;

    TiltedPhoques::Vector<NotifyQuestUpdate> m_pendingUpdates;
};
