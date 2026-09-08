#include "PCH.h"
#include "RuntimeLight.h"
#include <chrono>
#include <mutex>
#include <thread>

namespace ScreenshotLightingAssistant::RuntimeLight
{
    namespace
    {
        struct Runtime final : RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
            std::mutex mutex;
            LightSession session;
            Localization::Message status = "停止中 : 撮影3灯＋フェイスライトを試せます。";
            bool statusImportant{};
            std::atomic_bool queued = false;
            std::atomic_bool dirty = false;
            std::atomic_bool ownsLight = false; // true if ANY owned point exists
            struct TargetCommand {
                std::uint64_t epoch;
                TargetAction action;
                std::uint64_t revision;
                std::size_t index;
            };
            std::optional<TargetCommand> targetCommand;
            View targetView;
            std::uint64_t targetWorld = 1;
            std::uint64_t appliedTargetWorld = 0;
            // Game-task-only handles. Never resolve actors from an ImGui callback.
            RE::ActorHandle targetHandle;
            std::vector<RE::ActorHandle> candidateHandles;
            std::uint64_t candidateRevision = 0;
            RE::FormID playerCellID = 0;
            bool initialized = false;
            bool loadingMenu = false;
            bool mainMenu = false;

            // Accessed only from one-shot SKSE game tasks. Never serialized.
            RE::NiPointer<RE::ShadowSceneNode> scene;
            struct OwnedLight
            {
                RE::NiPointer<RE::NiPointLight> point;
                RE::NiPointer<RE::BSLight> registration;
                std::optional<LightRegistrationSettings> registeredSettings;
                std::optional<int> appliedType;
            };
            std::array<OwnedLight, kRuntimeLightCount> lights;
            std::optional<CameraBasis> basis;
            std::uint64_t appliedEpoch = 0;
            std::uint64_t appliedAlignment = 0;
            RE::FormID cellID = 0;
            bool headFallback = false;
            bool positionFallback = false;

            static Runtime& Get()
            {
                // Process lifetime: no NiPointer destruction or thread join in DLL detach
                // after Skyrim's allocator/renderer has already shut down.
                static auto* runtime = new Runtime;
                return *runtime;
            }

            View Read()
            {
                std::scoped_lock lock(mutex);
                auto result = targetView;
                result.session = session.Get();
                result.status = status;
                result.statusImportant = statusImportant;
                return result;
            }
            LightSessionSnapshot Request()
            {
                std::scoped_lock lock(mutex);
                return session.Get();
            }
            bool Current(std::uint64_t epoch)
            {
                std::scoped_lock lock(mutex);
                return session.Get().epoch == epoch;
            }
            void Report(std::uint64_t epoch, const Localization::Message& message, bool important)
            {
                std::scoped_lock lock(mutex);
                if (session.Get().epoch == epoch) { status = message; statusImportant = important; }
            }
            void Halt(std::uint64_t epoch, const char* message)
            {
                {
                    std::scoped_lock lock(mutex);
                    if (session.Stop(epoch)) { status = message; statusImportant = true; targetView.targetBusy = false; }
                }
                Clear();
                basis.reset();
                cellID = 0;
                logger::info("Lighting session suspended: {}", message);
            }
            void ClearSlot(std::size_t slot)
            {
                auto& owned = lights[slot];
                if (owned.point) {
                    // Extinguish first: renderer removal may be queued.
                    owned.point->GetLightRuntimeData().fade = 0.0F;
                    owned.point->SetAppCulled(true);
                }
                if (scene && owned.registration) {
                    scene->RemoveLight(owned.registration);
                    logger::info("Removed owned Light {}", slot + 1);
                }
                owned.registration.reset();
                owned.registeredSettings.reset();
                owned.appliedType.reset();
                owned.point.reset();
                ownsLight.store(std::any_of(lights.begin(), lights.end(),
                    [](const auto& light) { return static_cast<bool>(light.point); }));
            }
            void Clear()
            {
                for (std::size_t slot = 0; slot < lights.size(); ++slot) { ClearSlot(slot); }
                scene.reset();
            }

            static LightVector Vec(const RE::NiPoint3& p) { return { p.x, p.y, p.z }; }
            LightVector Anchor(RE::Actor* actor, RE::NiAVObject* root)
            {
                positionFallback = false;
                headFallback = false;
                if (auto* chest = root->GetObjectByName(RE::BSFixedString("NPC Spine2 [Spn2]"))) {
                    return Vec(chest->world.translate);
                }
                if (auto* head = root->GetObjectByName(RE::BSFixedString("NPC Head [Head]"))) {
                    headFallback = true;
                    return Vec(head->world.translate);
                }
                positionFallback = true;
                auto anchor = Vec(actor->GetPosition());
                anchor.z += 1.2F * kUnitsPerMetre;
                return anchor;
            }


            void ResetTargetView()
            {
                // Caller holds mutex. Invalidate queued choices without touching handles.
                ++targetWorld;
                targetCommand.reset();
                targetView = {};
                targetView.targetRevision = targetWorld;
            }
            static bool Eligible(RE::Actor* actor, RE::PlayerCharacter* player)
            {
                if (!actor || !player || actor->IsDeleted() || actor->IsDisabled()) { return false; }
                auto* cell = actor->GetParentCell();
                auto* playerCell = player->GetParentCell();
                if (!cell || !playerCell || !cell->IsAttached()) { return false; }
                if (cell != playerCell && (!cell->IsExteriorCell() || !playerCell->IsExteriorCell() ||
                    actor->GetWorldspace() != player->GetWorldspace())) { return false; }
                auto* root = actor->Get3D(false);
                return root && (actor == player || root->GetObjectByName(RE::BSFixedString("NPC Head [Head]")) ||
                    root->GetObjectByName(RE::BSFixedString("NPC Spine2 [Spn2]")));
            }
            void ProcessTargetCommand()
            {
                std::optional<TargetCommand> command;
                std::uint64_t world;
                {
                    std::scoped_lock lock(mutex);
                    world = targetWorld;
                    command = std::exchange(targetCommand, std::nullopt);
                }
                if (world != appliedTargetWorld) {
                    targetHandle.reset();
                    candidateHandles.clear();
                    ++candidateRevision;
                    playerCellID = 0;
                    appliedTargetWorld = world;
                }
                if (!command) { return; }
                const auto request = Request();
                const auto finish = [&](const char* message) {
                    std::scoped_lock lock(mutex);
                    if (targetWorld == world) {
                        targetView.targetBusy = false;
                        targetView.targetNotice = message;
                    }
                };
                if (request.epoch != command->epoch || !request.ready || request.blocked) {
                    finish("状態が変わりました。対象を選び直してください。"); return;
                }
                auto* player = RE::PlayerCharacter::GetSingleton();
                auto* ui = RE::UI::GetSingleton();
                if (!player || !ui || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
                    finish("ロード完了後に対象を選んでください。"); return;
                }
                if (command->action == TargetAction::Refresh) {
                    std::vector<TargetCandidate> found;
                    std::vector<std::pair<RE::FormID, RE::ActorHandle>> handles;
                    // The engine supplies loaded references only; never force-load NPCs.
                    if (auto* tes = RE::TES::GetSingleton()) {
                        tes->ForEachReferenceInRange(player, kTargetSearchMetres * kUnitsPerMetre, [&](RE::TESObjectREFR* ref) {
                            auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
                            if (actor != player && Eligible(actor, player)) {
                                const auto delta = actor->GetPosition() - player->GetPosition();
                                const float distance = delta.Length() / kUnitsPerMetre;
                                const char* name = actor->GetDisplayFullName();
                                found.push_back({actor->GetFormID(), name && *name ? name : "(NPC)", distance});
                                handles.emplace_back(actor->GetFormID(), actor->GetHandle());
                            }
                            return RE::BSContainer::ForEachResult::kContinue;
                        });
                    }
                    SortTargetCandidates(found);
                    std::vector<RE::ActorHandle> ordered;
                    for (const auto& entry : found) {
                        const auto it = std::find_if(handles.begin(), handles.end(), [&](const auto& e) { return e.first == entry.formID; });
                        ordered.push_back(it->second);
                    }
                    {
                        std::scoped_lock lock(mutex);
                        if (targetWorld != world || session.Get().epoch != command->epoch) {
                            if (targetWorld == world) { targetView.targetBusy = false; }
                            return;
                        }
                        candidateHandles = std::move(ordered);
                        targetView.candidates = std::move(found);
                        targetView.targetRevision = ++candidateRevision;
                        targetView.targetBusy = false;
                        targetView.targetNotice = "候補を更新しました。距離はプレイヤーからの距離です。";
                    }
                    return;
                }
                RE::NiPointer<RE::Actor> chosen;
                if (command->action == TargetAction::Player) { chosen.reset(player); }
                else if (command->action == TargetAction::Nearby) {
                    if (!ValidTargetChoice(command->revision, candidateRevision, command->index, candidateHandles.size())) {
                        finish("候補が変わりました。一覧を更新してください。"); return;
                    }
                    chosen = candidateHandles[command->index].get();
                } else {
                    RE::NiPointer<RE::TESObjectREFR> ref;
                    if (command->action == TargetAction::Console) { ref = RE::Console::GetSelectedRef(); }
                    else if (auto* crosshair = RE::CrosshairPickData::GetSingleton()) { ref = crosshair->GetActiveTarget().get(); }
                    if (ref) { chosen.reset(ref->As<RE::Actor>()); }
                }
                if (!Eligible(chosen.get(), player)) {
                    finish("対象を取得できません。近くの人型NPCを選ぶか、一覧を更新してください。"); return;
                }
                const bool npc = chosen.get() != player;
                const auto handle = chosen->GetHandle();
                const auto id = chosen->GetFormID();
                const char* displayName = chosen->GetDisplayFullName();
                const std::string name = displayName && *displayName ? displayName : "(NPC)";
                {
                    std::scoped_lock lock(mutex);
                    if (targetWorld != world || !session.Stop(command->epoch)) {
                        if (targetWorld == world) { targetView.targetBusy = false; }
                        return;
                    }
                    targetHandle = npc ? handle : RE::ActorHandle{};
                    targetView.npcTarget = npc;
                    targetView.targetName = npc ? name : "";
                    targetView.targetFormID = npc ? id : 0;
                    targetView.targetBusy = false;
                    targetView.targetNotice = "対象を変更しました。設定を保持して消灯しました。撮影ライトを開始してください。";
                    status = "対象を変更しました。設定を保持して消灯しました。撮影ライトを開始してください。";
                    statusImportant = true;
                }
                Clear();
                basis.reset();
                cellID = 0;
                playerCellID = 0;
            }

            void Pump()
            {
                dirty.exchange(false);
                ProcessTargetCommand();
                const auto request = Request();
                if (request.epoch != appliedEpoch) {
                    Clear();
                    basis.reset();
                    cellID = 0;
                    playerCellID = 0;
                    appliedEpoch = request.epoch;
                }
                if (!request.ready || request.blocked || !request.running) {
                    Clear();
                    if (request.ready && !request.blocked) {
                        std::scoped_lock lock(mutex);
                        if (session.Get().epoch == request.epoch && status == "停止処理待ち : 設定・履歴は保持されています。") {
                            status = "停止しました。ライト設定・履歴は保持されています。";
                        }
                    }
                    return;
                }
                if (!Current(request.epoch)) { Clear(); return; }
                auto* ui = RE::UI::GetSingleton();
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
                    Halt(request.epoch, "画面遷移で停止しました。撮影場所で開始してください。");
                    return;
                }
                bool npc;
                { std::scoped_lock lock(mutex); npc = targetView.npcTarget; }
                RE::NiPointer<RE::Actor> actor = npc ? targetHandle.get() : RE::NiPointer<RE::Actor>{player};
                if (!Eligible(actor.get(), player)) {
                    Halt(request.epoch, "撮影対象を読み込めないため停止しました。対象を選び直してください。");
                    return;
                }
                auto* cell = actor->GetParentCell();
                auto* playerCell = player->GetParentCell();
                if (playerCellID && playerCellID != playerCell->GetFormID()) {
                    Halt(request.epoch, "場所の移動で停止しました。必要な場所で再開してください。");
                    return;
                }
                playerCellID = playerCell->GetFormID();
                RE::NiPointer<RE::NiAVObject> root{ actor->Get3D(false) };
                auto* camera = RE::PlayerCamera::GetSingleton();
                RE::NiPointer<RE::NiNode> cameraRoot = camera ? camera->cameraRoot : nullptr;
                auto* currentScene = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
                if (!cell || !cell->IsAttached() || !root || !cameraRoot || !currentScene) {
                    Halt(request.epoch, "対象・カメラを取得できません。ロード完了後、三人称かフリーカメラで開始してください。");
                    return;
                }
                if ((cellID && cellID != cell->GetFormID()) || (scene && scene.get() != currentScene)) {
                    Halt(request.epoch, "場所の移動で停止しました。必要な場所で再開してください。");
                    return;
                }
                cellID = cell->GetFormID();
                const auto anchor = Anchor(actor.get(), root.get());
                const auto cameraPosition = Vec(cameraRoot->world.translate);
                if (!basis || appliedAlignment != request.alignment) {
                    basis = MakeCameraBasis(anchor, cameraPosition);
                    appliedAlignment = request.alignment;
                    if (!basis) {
                        Halt(request.epoch, "正面を判定できません。カメラを対象から少し離して開始してください。");
                        return;
                    }
                    logger::info("Captured camera basis; anchor={}, cell={:08X}",
                        positionFallback ? "position+1.2m" : headFallback ? "head" : "chest", cellID);
                }
                // Hold the scene even with all slots OFF; scene replacement must still disarm.
                if (!scene) { scene.reset(currentScene); }
                std::optional<LightVector> facePosition;
                if (LightSession::ShouldIlluminateFace(request)) {
                    if (auto* head = root->GetObjectByName(RE::BSFixedString("NPC Head [Head]"))) {
                        const auto& face = request.lights.face;
                        if (face.basis == FaceLightBasis::Head) {
                            // Read the current animated head transform on the existing game-thread task.
                            // Never retain the bone pointer or write to the actor/skeleton.
                            facePosition = HeadFaceLightPosition(Vec(head->world.translate),
                                Vec(head->world.rotate * RE::NiPoint3{0, 1, 0}),
                                Vec(head->world.rotate * RE::NiPoint3{0, 0, 1}), face.heightOffset);
                        } else {
                            facePosition = FaceLightPosition(Vec(head->world.translate), cameraPosition, face.heightOffset);
                        }
                    }
                }
                // Retire removed/retyped-shadow registrations first, across the whole scene.
                // A 3 -> 2 light preset cannot retain the third light while creating replacements.
                for (std::size_t slot = 0; slot < lights.size(); ++slot) {
                    const auto& owned = lights[slot];
                    const auto settings = RuntimeSettings(request.lights, slot);
                    if (!settings || !LightSession::ShouldIlluminateRuntime(request, slot) ||
                        (slot == kFaceRuntimeSlot && !facePosition) ||
                        (owned.registration && owned.registeredSettings != LightRegistrationSettings::From(*settings))) {
                        ClearSlot(slot);
                    }
                }
                std::size_t active = 0, shadowed = 0;
                for (std::size_t slot = 0; slot < lights.size(); ++slot) {
                    if (!Current(request.epoch)) { Clear(); return; }
                    if (!LightSession::ShouldIlluminateRuntime(request, slot) || (slot == kFaceRuntimeSlot && !facePosition)) {
                        continue;
                    }
                    const auto settings = RuntimeSettings(request.lights, slot);
                    if (!settings) { Halt(request.epoch, "光源設定が無効なため停止しました。"); return; }
                    const auto& light = *settings;
                    const auto tint = RuntimeTint(light);
                    if (!tint) { Halt(request.epoch, "光源の色設定が無効なため全灯を停止しました。"); return; }
                    auto& owned = lights[slot];
                    const auto position = slot == kFaceRuntimeSlot ? facePosition : LightPosition(anchor, *basis, light);
                    if (!position) {
                        Halt(request.epoch, "配置座標が無効なため停止しました。");
                        return;
                    }
                    if (!Current(request.epoch)) { Clear(); return; }
                    const auto desiredSettings = LightRegistrationSettings::From(light);
                    const auto aim = desiredSettings.spot ? AimPlacedSpotlight(*position, anchor, *basis, light) : std::optional<SpotlightBasis>{};
                    if (desiredSettings.spot && !aim) {
                        Halt(request.epoch, "スポットの向きを計算できないため停止しました。");
                        return;
                    }
                    if (!owned.point) {
                        owned.point.reset(RE::NiPointLight::Create());
                        if (!owned.point) { Halt(request.epoch, "光源を作成できませんでした。"); return; }
                        owned.point->name = RE::BSFixedString(slot == kFaceRuntimeSlot ? "ScreenshotLightingAssistant:Face" :
                            std::format("ScreenshotLightingAssistant:Light{}", slot + 1));
                        // Deliberately NOT attached to actor/cell 3D and NOT a TESObjectREFR.
                        // The manager owns this transient light; only the renderer registers it.
                        ownsLight.store(true);
                    }
                    const float radius = light.range * kUnitsPerMetre;
                    auto& data = owned.point->GetLightRuntimeData();
                    data.ambient = { 0.0F, 0.0F, 0.0F };
                    data.diffuse = { tint->red, tint->green, tint->blue };
                    if (owned.appliedType != light.type) {
                        logger::info("Light {} tint type={} diffuse=({:.2f},{:.2f},{:.2f})",
                            slot + 1, light.type, tint->red, tint->green, tint->blue);
                        owned.appliedType = light.type;
                    }
                    data.radius = { radius, radius, radius };
                    data.fade = light.intensity;
                    owned.point->SetLightAttenuation(radius);
                    owned.point->local.translate = { position->x, position->y, position->z };
                    owned.point->local.rotate = RE::NiMatrix3{};
                    if (aim) {
                        // Explicit columns: NiMatrix3's three-vector constructor takes ROWS.
                        auto& m = owned.point->local.rotate.entry;
                        m[0][0] = aim->forward.x; m[1][0] = aim->forward.y; m[2][0] = aim->forward.z;
                        m[0][1] = aim->up.x;      m[1][1] = aim->up.y;      m[2][1] = aim->up.z;
                        m[0][2] = aim->right.x;   m[1][2] = aim->right.y;   m[2][2] = aim->right.z;
                    }
                    RE::NiUpdateData update{};
                    owned.point->Update(update);
                    owned.point->SetAppCulled(false);
                    if (!owned.registration) {
                        RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
                        params.dynamic = true;
                        params.shadowLight = desiredSettings.shadow;
                        params.affectLand = true;
                        params.affectWater = true;
                        params.neverFades = true;
                        params.fov = desiredSettings.fov;
                        params.falloff = desiredSettings.falloff;
                        params.nearDistance = desiredSettings.nearDistance;
                        params.depthBias = desiredSettings.depthBias;
                        owned.registration.reset(scene->AddLight(owned.point.get(), params));
                        if (!owned.registration) { Halt(request.epoch, "描画側への光源登録に失敗しました。"); return; }
                        // Use the engine's type-query interface, never reinterpret an unknown light.
                        bool correctKind = owned.registration->IsShadowLight() == desiredSettings.shadow;
                        if (correctKind && desiredSettings.shadow) {
                            auto* shadow = static_cast<RE::BSShadowLight*>(owned.registration.get());
                            correctKind = desiredSettings.spot ? shadow->GetIsFrustumLight() : shadow->GetIsOmniLight();
                        }
                        if (!correctKind) {
                            logger::error("Renderer returned unexpected light kind (shadow={}, spot={})", desiredSettings.shadow, desiredSettings.spot);
                            Halt(request.epoch, "要求と異なる種類の光源が返されたため停止しました。ログを確認してください。");
                            return;
                        }
                        owned.registeredSettings = desiredSettings;
                        logger::info("Light {} verified kind: {}; fov={:.3f}, falloff={:.2f}; aim target=({:.1f},{:.1f},{:.1f})",
                            slot + 1, !desiredSettings.shadow ? "shadowless" : desiredSettings.spot ? "shadow spotlight" : "shadow omni",
                            params.fov, params.falloff, anchor.x, anchor.y, anchor.z);
                        logger::info("Registered transient Light {} (shadow={}, depthBias={:.2f}, nearDistance={:.1f}), position=({:.1f},{:.1f},{:.1f}), radius={:.1f}, intensity={:.2f}, distance={:.2f}m, height={:.2f}m",
                            slot + 1, desiredSettings.shadow, desiredSettings.depthBias, params.nearDistance,
                            position->x, position->y, position->z, radius, light.intensity,
                            light.distance, light.heightOffset);
                    }
                    if (!Current(request.epoch)) { Clear(); return; }
                    if (slot < kManualLightCount) { ++active; }
                    if (desiredSettings.shadow) { ++shadowed; }
                }
                if (!Current(request.epoch)) { Clear(); return; }
                const bool faceActive = static_cast<bool>(lights[kFaceRuntimeSlot].registration);
                const char* faceStatus = !request.lights.face.enabled ? "OFF" : request.lights.face.intensity == 0.0F ? "強さ0" :
                    faceActive ? "ON" : "待機（頭・向き・カメラを確認）";
                const char* anchorStatus = positionFallback ? "3灯基準は足元＋1.2m（代替）" : headFallback ? "3灯基準は頭（代替）" : "3灯基準は胸";
                constexpr const char* statusFormat = "実ライト動作中 : {}/3灯（影付き{}灯） / フェイス {} / {}";
                const Localization::Message message{
                    Localization::Fmt(statusFormat, active, shadowed, faceStatus, anchorStatus),
                    Localization::Fmt(Localization::Tr(statusFormat, Localization::Language::English), active, shadowed,
                        Localization::Tr(faceStatus, Localization::Language::English), Localization::Tr(anchorStatus, Localization::Language::English))};
                const bool faceWaiting = request.lights.face.enabled && request.lights.face.intensity > 0.0F && !faceActive;
                Report(request.epoch, message, positionFallback || headFallback || faceWaiting);
            }

            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (!event) { return RE::BSEventNotifyControl::kContinue; }
                if (event->menuName == RE::MainMenu::MENU_NAME || event->menuName == RE::LoadingMenu::MENU_NAME) {
                    {
                        std::scoped_lock lock(mutex);
                        if (event->menuName == RE::MainMenu::MENU_NAME) { mainMenu = event->opening; }
                        else { loadingMenu = event->opening; }
                        session.Block(mainMenu || loadingMenu);
                        if (event->opening) { ResetTargetView(); }
                        status = "画面遷移で停止しました。自動再点灯はしません。";
                        statusImportant = true;
                    }
                    dirty.store(true);
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    void Initialize()
    {
        auto& runtime = Runtime::Get();
        if (runtime.initialized) { return; }
        runtime.initialized = true;
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(&runtime);
        }
        SetGameReady(true);
        // Worker touches only owned request state. Engine reads/writes are game tasks.
        // Never self-requeue inside AddTask: SKSE drains until empty in the same frame.
        std::thread([] {
            auto& rt = Runtime::Get();
            for (;;) {
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
                if (!rt.dirty.load() && !rt.ownsLight.load() && !rt.Request().running) { continue; }
                if (rt.queued.exchange(true)) { continue; }
                SKSE::GetTaskInterface()->AddTask([] {
                    auto& state = Runtime::Get();
                    try { state.Pump(); }
                    catch (const std::exception& error) {
                        logger::error("Light task failed: {}", error.what());
                        state.Halt(state.Read().session.epoch, "光源処理エラーで停止しました。ログを確認してください。");
                    }
                    state.queued.store(false);
                });
            }
        }).detach();
        logger::info("Transient lighting runtime initialized; no persistent forms or cosave records");
    }

    void SetGameReady(bool ready)
    {
        auto& runtime = Runtime::Get();
        {
            std::scoped_lock lock(runtime.mutex);
            runtime.session.SetReady(ready);
            runtime.ResetTargetView();
            runtime.status = ready ? "停止中 : 撮影場所で撮影ライトを開始してください。" : "ロード待機中 : 実ライトを停止しています。";
            runtime.statusImportant = !ready;
        }
        runtime.dirty.store(true);
        logger::info("Game lifecycle reset (ready={}); previous request tokens invalidated", ready);
    }
    void RequestTarget(std::uint64_t epoch, TargetAction action, std::uint64_t revision, std::size_t index)
    {
        auto& runtime = Runtime::Get();
        {
            std::scoped_lock lock(runtime.mutex);
            const auto& state = runtime.session.Get();
            if (state.epoch != epoch || !state.ready || state.blocked || runtime.targetView.targetBusy) { return; }
            runtime.targetCommand = Runtime::TargetCommand{epoch, action, revision, index};
            runtime.targetView.targetBusy = true;
            runtime.targetView.targetNotice.clear();
        }
        runtime.dirty.store(true);
    }
    View GetView() { return Runtime::Get().Read(); }
    void Start(std::uint64_t epoch, const Scene& lights)
    {
        auto& runtime = Runtime::Get();
        {
            std::scoped_lock lock(runtime.mutex);
            if (!runtime.session.Start(epoch, lights)) { return; }
            runtime.status = "開始待ち : ゲーム側の処理を待っています。";
            runtime.statusImportant = false;
        }
        runtime.dirty.store(true);
    }
    void Stop(std::uint64_t epoch)
    {
        auto& runtime = Runtime::Get();
        {
            std::scoped_lock lock(runtime.mutex);
            if (!runtime.session.Stop(epoch)) { return; }
            runtime.status = "停止処理待ち : 設定・履歴は保持されています。";
            runtime.statusImportant = false;
        }
        runtime.dirty.store(true);
    }
    void Align(std::uint64_t epoch)
    {
        auto& runtime = Runtime::Get();
        std::scoped_lock lock(runtime.mutex);
        runtime.session.Align(epoch);
    }
    void Submit(std::uint64_t epoch, const Scene& lights)
    {
        auto& runtime = Runtime::Get();
        std::scoped_lock lock(runtime.mutex);
        runtime.session.Submit(epoch, lights);
    }
}
