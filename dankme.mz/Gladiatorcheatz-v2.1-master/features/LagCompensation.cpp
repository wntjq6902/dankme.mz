#include "LagCompensation.hpp"
#include "Animation.hpp"

#include "../Options.hpp"
#include "AimRage.hpp"

#include "../helpers/Utils.hpp"
#include "../helpers/Math.hpp"
#include "PredictionSystem.hpp"
#include "RebuildGameMovement.hpp"

void LagRecord::SaveRecord(C_BasePlayer *player)
{
	//CMBacktracking::Get().AnimationFix(player);

	//Animation::Get().HandleAnimFix(player, false);

	m_vecOrigin = player->m_vecOrigin();
	m_vecAbsOrigin = player->GetAbsOrigin();
	m_angAngles = player->m_angEyeAngles();
	m_flSimulationTime = player->m_flSimulationTime();
	m_vecMins = player->GetCollideable()->OBBMins();
	m_vecMax = player->GetCollideable()->OBBMaxs();
	m_nFlags = player->m_fFlags();
	m_vecVelocity = player->m_vecVelocity();

	int layerCount = player->GetNumAnimOverlays();
	for (int i = 0; i < layerCount; i++)
	{
		AnimationLayer *currentLayer = player->GetAnimOverlay(i);
		m_LayerRecords[i].m_nOrder = currentLayer->m_nOrder;
		m_LayerRecords[i].m_nSequence = currentLayer->m_nSequence;
		m_LayerRecords[i].m_flWeight = currentLayer->m_flWeight;
		m_LayerRecords[i].m_flCycle = currentLayer->m_flCycle;
	}
	m_arrflPoseParameters = player->m_flPoseParameter();

	m_iTickCount = g_GlobalVars->tickcount;
	m_vecHeadSpot = player->GetBonePos(8);

	m_strResolveMode = Global::resolverModes[player->EntIndex()];

	//Animation::Get().HandleAnimFix(player, true);
}

void CMBacktracking::FrameUpdatePostEntityThink()
{
	static auto sv_unlag = g_CVar->FindVar("sv_maxunlag");
	if (g_GlobalVars->maxClients <= 1 /*|| !sv_unlag->GetBool()*/)
	{
		CMBacktracking::Get().ClearHistory();
		return;
	}

	for (int i = 1; i <= g_GlobalVars->maxClients; i++)
	{
		C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);

		auto &lag_records = this->m_LagRecord[i];

		if (!IsPlayerValid(player))
		{
			if (lag_records.size() > 0)
				lag_records.clear();

			continue;
		}

		int32_t ent_index = player->EntIndex();
		float_t sim_time = player->m_flSimulationTime();

		LagRecord cur_lagrecord;

		RemoveBadRecords(ent_index, lag_records);

		if (lag_records.size() > 0)
		{
			auto &tail = lag_records.back();

			if (tail.m_flSimulationTime == sim_time)
				continue;
		}

		cur_lagrecord.SaveRecord(player); // first let's create the record

		if (!lag_records.empty()) // apply specific stuff that is needed
		{
			auto &temp_lagrecord = lag_records.back();
			int32_t priority_level = GetPriorityLevel(player, &temp_lagrecord);

			cur_lagrecord.m_iPriority = priority_level;
			cur_lagrecord.m_flPrevLowerBodyYaw = temp_lagrecord.m_flPrevLowerBodyYaw;
			cur_lagrecord.m_arrflPrevPoseParameters = temp_lagrecord.m_arrflPrevPoseParameters;

			if (priority_level == 3)
				cur_lagrecord.m_angAngles.yaw = temp_lagrecord.m_angAngles.yaw;
		}

		lag_records.emplace_back(cur_lagrecord);
	}
}

void CMBacktracking::ProcessCMD(int iTargetIdx, CUserCmd *usercmd)
{
	LagRecord recentLR = m_RestoreLagRecord[iTargetIdx].first;
	if (!IsTickValid(TIME_TO_TICKS(recentLR.m_flSimulationTime)) && (g_GlobalVars->curtime - recentLR.m_flSimulationTime) > g_Options.rage_fakelatency)
		usercmd->tick_count = TIME_TO_TICKS(C_BasePlayer::GetPlayerByIndex(iTargetIdx)->m_flSimulationTime() + GetLerpTime());
	else
		usercmd->tick_count = TIME_TO_TICKS(recentLR.m_flSimulationTime + GetLerpTime());
}

void CMBacktracking::RemoveBadRecords(int Idx, std::deque<LagRecord>& records)
{
	auto& m_LagRecords = records; // Should use rbegin but can't erase
	for (auto lag_record = m_LagRecords.begin(); lag_record != m_LagRecords.end(); lag_record++)
	{
		if (!IsTickValid(TIME_TO_TICKS(lag_record->m_flSimulationTime)) && (g_GlobalVars->curtime - lag_record->m_flSimulationTime) > g_Options.rage_fakelatency)
		{
			m_LagRecords.erase(lag_record);
			if (!m_LagRecords.empty())
				lag_record = m_LagRecords.begin();
			else break;
		}
	}
}

bool CMBacktracking::StartLagCompensation(C_BasePlayer *player)
{
	if (!player || player == nullptr)
		return false;

	backtrack_records.clear();

	enum
	{
		// Only try to awall the "best" records, otherwise fail.
		TYPE_BEST_RECORDS,
		// Only try to awall the newest and the absolute best record.
		TYPE_BEST_AND_NEWEST,
		// Awall everything (fps killer)
		TYPE_ALL_RECORDS,
	};

	auto& m_LagRecords = this->m_LagRecord[player->EntIndex()];
	m_RestoreLagRecord[player->EntIndex()].second.SaveRecord(player);

	switch (g_Options.rage_lagcompensation_type)
	{
	case TYPE_BEST_RECORDS:
	{
		for (auto it : m_LagRecords)
		{
			if (it.m_iPriority == 2)
				backtrack_records.emplace_front(it);	//try to hit lby backtrack first
			else if (it.m_iPriority >= 1 || (it.m_vecVelocity.Length2D() > 35.f)) // let's account for those moving fags aswell -> it's experimental and not supposed what this lagcomp mode should do
				backtrack_records.emplace_back(it);
		}
		break;
	}
	case TYPE_BEST_AND_NEWEST:
	{
		LagRecord newest_record = LagRecord();
		for (auto it : m_LagRecords)
		{
			if (it.m_flSimulationTime > newest_record.m_flSimulationTime)
				newest_record = it;

			if (it.m_iPriority == 2)
				backtrack_records.emplace_front(it);	//try to hit lby backtrack first
			else if (it.m_iPriority >= 1 /*&& !(it.m_nFlags & FL_ONGROUND) && it.m_vecVelocity.Length2D() > 150*/)
				backtrack_records.emplace_back(it);
		}
		backtrack_records.emplace_back(newest_record);
		break;
	}
	case TYPE_ALL_RECORDS:
		// Ouch, the fps drop will be H U G E.
		backtrack_records = m_LagRecords;
		break;
	}

	std::sort(backtrack_records.begin(), backtrack_records.end(), [](LagRecord const &a, LagRecord const &b) { return a.m_iPriority > b.m_iPriority; });
	return backtrack_records.size() > 0;
}

bool CMBacktracking::FindViableRecord(C_BasePlayer *player, LagRecord* record)
{
	auto &m_LagRecords = this->m_LagRecord[player->EntIndex()];

	// Ran out of records to check. Go back.
	if (backtrack_records.empty())
	{
		return false;
	}

	LagRecord
		recentLR = *backtrack_records.begin(),
		prevLR;

	// Should still use m_LagRecords because we're checking for LC break.
	auto iter = std::find(m_LagRecords.begin(), m_LagRecords.end(), recentLR);
	auto idx = std::distance(m_LagRecords.begin(), iter);
	if (0 != idx) prevLR = *std::prev(iter);

	// Saving first record for processcmd.
	m_RestoreLagRecord[player->EntIndex()].first = recentLR;

	if (!IsTickValid(TIME_TO_TICKS(recentLR.m_flSimulationTime)) && (g_GlobalVars->curtime - recentLR.m_flSimulationTime) > g_Options.rage_fakelatency)
	{
		backtrack_records.pop_front();
		return backtrack_records.size() > 0; // RET_NO_RECORDS true false
	}

	// Remove a record...
	backtrack_records.pop_front();

	if ((0 != idx) && (recentLR.m_vecOrigin - prevLR.m_vecOrigin).Length2D() > 64.f)
	{
		float simulationTimeDelta = recentLR.m_flSimulationTime - prevLR.m_flSimulationTime;

		int simulationTickDelta = clamp(TIME_TO_TICKS(simulationTimeDelta), 1, 15);

		Vector origin = player->m_vecOrigin();
		Vector velocity = player->m_vecVelocity();
		int flags = player->m_fFlags();
		bool was_in_air = false;
		for (; simulationTickDelta > 0; simulationTickDelta--)
		{
			/*
			int last_flag = flags;
			RebuildGameMovement::Get().VelocityExtrapolate(player, origin, velocity, flags, was_in_air);
			was_in_air = (!(last_flag & FL_ONGROUND) && (flags & FL_ONGROUND));
			*/
			RebuildGameMovement::Get().FullWalkMove(player);
		}

		// Bandage fix so we "restore" to the lagfixed player.
		m_RestoreLagRecord[player->EntIndex()].second.SaveRecord(player);
		*record = m_RestoreLagRecord[player->EntIndex()].second;

		// Clear so we don't try to bt shit we can't
		backtrack_records.clear();

		return true; // Return true so we still try to aimbot.
	}
	else
	{
		player->InvalidateBoneCache();

		player->GetCollideable()->OBBMins() = recentLR.m_vecMins;
		player->GetCollideable()->OBBMaxs() = recentLR.m_vecMax;

		player->SetAbsAngles(QAngle(0, recentLR.m_angAngles.yaw, 0));
		player->SetAbsOrigin(recentLR.m_vecOrigin);

		player->m_fFlags() = recentLR.m_nFlags;

		int layerCount = player->GetNumAnimOverlays();
		for (int i = 0; i < layerCount; ++i)
		{
			AnimationLayer *currentLayer = player->GetAnimOverlay(i);
			currentLayer->m_nOrder = recentLR.m_LayerRecords[i].m_nOrder;
			currentLayer->m_nSequence = recentLR.m_LayerRecords[i].m_nSequence;
			currentLayer->m_flWeight = recentLR.m_LayerRecords[i].m_flWeight;
			currentLayer->m_flCycle = recentLR.m_LayerRecords[i].m_flCycle;
		}

		player->m_flPoseParameter() = recentLR.m_arrflPoseParameters;

		*record = recentLR;
		return true;
	}
}

void CMBacktracking::FinishLagCompensation(C_BasePlayer *player)
{
	int idx = player->EntIndex();

	player->InvalidateBoneCache();

	player->GetCollideable()->OBBMins() = m_RestoreLagRecord[idx].second.m_vecMins;
	player->GetCollideable()->OBBMaxs() = m_RestoreLagRecord[idx].second.m_vecMax;

	player->SetAbsAngles(QAngle(0, m_RestoreLagRecord[idx].second.m_angAngles.yaw, 0));
	player->SetAbsOrigin(m_RestoreLagRecord[idx].second.m_vecOrigin);

	player->m_fFlags() = m_RestoreLagRecord[idx].second.m_nFlags;

	int layerCount = player->GetNumAnimOverlays();
	for (int i = 0; i < layerCount; ++i)
	{
		AnimationLayer *currentLayer = player->GetAnimOverlay(i);
		currentLayer->m_nOrder = m_RestoreLagRecord[idx].second.m_LayerRecords[i].m_nOrder;
		currentLayer->m_nSequence = m_RestoreLagRecord[idx].second.m_LayerRecords[i].m_nSequence;
		currentLayer->m_flWeight = m_RestoreLagRecord[idx].second.m_LayerRecords[i].m_flWeight;
		currentLayer->m_flCycle = m_RestoreLagRecord[idx].second.m_LayerRecords[i].m_flCycle;
	}

	player->m_flPoseParameter() = m_RestoreLagRecord[idx].second.m_arrflPoseParameters;
}

int CMBacktracking::GetPriorityLevel(C_BasePlayer *player, LagRecord* lag_record)
{
	int priority = 0;

	if (lag_record->m_flPrevLowerBodyYaw != player->m_flLowerBodyYawTarget())
	{
		lag_record->m_angAngles.yaw = player->m_flLowerBodyYawTarget();
		priority = 3;
	}

	if ((player->m_flPoseParameter()[1] > (0.85f) && lag_record->m_arrflPrevPoseParameters[1] <= (0.85f)) || (player->m_flPoseParameter()[1] <= (0.85f) && lag_record->m_arrflPrevPoseParameters[1] > (0.85f)))
		priority = 1;

	lag_record->m_flPrevLowerBodyYaw = player->m_flLowerBodyYawTarget();
	lag_record->m_arrflPrevPoseParameters = player->m_flPoseParameter();

	return priority;
}

void CMBacktracking::SimulateMovement(Vector &velocity, Vector &origin, C_BasePlayer *player, int &flags, bool was_in_air)
{
	if (!(flags & FL_ONGROUND))
		velocity.z -= (g_GlobalVars->frametime * g_CVar->FindVar("sv_gravity")->GetFloat());
	else if (was_in_air)
		velocity.z = g_CVar->FindVar("sv_jump_impulse")->GetFloat();

	const Vector mins = player->GetCollideable()->OBBMins();
	const Vector max = player->GetCollideable()->OBBMaxs();

	const Vector src = origin;
	Vector end = src + (velocity * g_GlobalVars->frametime);

	Ray_t ray;
	ray.Init(src, end, mins, max);

	trace_t trace;
	CTraceFilter filter;
	filter.pSkip = (void*)(player);

	g_EngineTrace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);

	if (trace.fraction != 1.f)
	{
		for (int i = 0; i < 2; i++)
		{
			velocity -= trace.plane.normal * velocity.Dot(trace.plane.normal);

			const float dot = velocity.Dot(trace.plane.normal);
			if (dot < 0.f)
			{
				velocity.x -= dot * trace.plane.normal.x;
				velocity.y -= dot * trace.plane.normal.y;
				velocity.z -= dot * trace.plane.normal.z;
			}

			end = trace.endpos + (velocity * (g_GlobalVars->interval_per_tick * (1.f - trace.fraction)));

			ray.Init(trace.endpos, end, mins, max);
			g_EngineTrace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);

			if (trace.fraction == 1.f)
				break;
		}
	}

	origin = trace.endpos;
	end = trace.endpos;
	end.z -= 2.f;

	ray.Init(origin, end, mins, max);
	g_EngineTrace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);

	flags &= ~(1 << 0);

	if (trace.DidHit() && trace.plane.normal.z > 0.7f)
		flags |= (1 << 0);
}

void CMBacktracking::AnimationFix(C_BasePlayer *player)
{
	// aw reversed; useless, you miss more with it than without it -> missing for sure other code parts
	// to make this work lel
	// to harCs: this is fakelag fix, lmao
	
	auto &lag_records = this->m_LagRecord[player->EntIndex()];

	auto leet = [](C_BasePlayer *player) -> void
	{
		static ConVar *sv_pvsskipanimation = g_CVar->FindVar("sv_pvsskipanimation");

		int32_t backup_sv_pvsskipanimation = sv_pvsskipanimation->GetInt();
		sv_pvsskipanimation->SetValue(0);

		*(int32_t*)((uintptr_t)player + 0xA30) = 0;
		*(int32_t*)((uintptr_t)player + 0x269C) = 0;

		int32_t backup_effects = *(int32_t*)((uintptr_t)player + 0xEC);
		*(int32_t*)((uintptr_t)player + 0xEC) |= 8;

		player->SetupBonesExperimental(NULL, -1, 0x7FF00, g_GlobalVars->curtime);

		*(int32_t*)((uintptr_t)player + 0xEC) = backup_effects;
		sv_pvsskipanimation->SetValue(backup_sv_pvsskipanimation);
	};

	// backup
	const float curtime = g_GlobalVars->curtime;
	const float frametime = g_GlobalVars->frametime;

	static auto host_timescale = g_CVar->FindVar(("host_timescale"));

	g_GlobalVars->frametime = g_GlobalVars->interval_per_tick * host_timescale->GetFloat();
	g_GlobalVars->curtime = player->m_flOldSimulationTime() + g_GlobalVars->interval_per_tick;

	Vector backup_origin = player->m_vecOrigin();
	Vector backup_absorigin = player->GetAbsOrigin();
	Vector backup_velocity = player->m_vecVelocity();
	int backup_flags = player->m_fFlags();

	if (lag_records.size() > 2)
	{
		bool bChocked = TIME_TO_TICKS(player->m_flSimulationTime() - lag_records.back().m_flSimulationTime) > 1;
		bool bInAir = false;

		if (!(player->m_fFlags() & FL_ONGROUND) || !(lag_records.back().m_nFlags & FL_ONGROUND))
			bInAir = true;

		if (bChocked)
		{
			player->m_vecOrigin() = lag_records.back().m_vecOrigin;
			player->SetAbsOrigin(lag_records.back().m_vecAbsOrigin);
			player->m_vecVelocity() = lag_records.back().m_vecVelocity;
			player->m_fFlags() = lag_records.back().m_nFlags;
		}

		Vector data_origin = player->m_vecOrigin();
		Vector data_velocity = player->m_vecVelocity();
		int data_flags = player->m_fFlags();

		if (bChocked)
		{
			SimulateMovement(data_velocity, data_origin, player, data_flags, bInAir);

			player->m_vecOrigin() = data_origin;
			player->SetAbsOrigin(data_origin);
			player->m_vecVelocity() = data_velocity;

			player->m_fFlags() &= 0xFFFFFFFE;
			auto penultimate_record = *std::prev(lag_records.end(), 2);
			if ((lag_records.back().m_nFlags & FL_ONGROUND) && (penultimate_record.m_nFlags & FL_ONGROUND))
				player->m_fFlags() |= 1;
			if (*(float*)((uintptr_t)player->GetAnimOverlay(0) + 0x138) > 0.f)
				player->m_fFlags() |= 1;
		}
	}

	AnimationLayer backup_layers[15];
	std::memcpy(backup_layers, player->GetAnimOverlays(), (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));

	// invalidates prior animations so the entity gets animated on our client 100% via UpdateClientSideAnimation
	C_CSGOPlayerAnimState *state = player->GetPlayerAnimState();
	if (state)
		state->m_iLastClientSideAnimationUpdateFramecount() = g_GlobalVars->framecount - 1;

	player->m_bClientSideAnimation() = true;

	// updates local animations + poses + calculates new abs angle based on eyeangles and other stuff
	player->UpdateClientSideAnimation();

	player->m_bClientSideAnimation() = false;

	// restore
	std::memcpy(player->GetAnimOverlays(), backup_layers, (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));
	player->m_vecOrigin() = backup_origin;
	player->SetAbsOrigin(backup_absorigin);
	player->m_vecVelocity() = backup_velocity;
	player->m_fFlags() = backup_flags;
	g_GlobalVars->curtime = curtime;
	g_GlobalVars->frametime = frametime;

	leet(player);
}

void CMBacktracking::AnimationFix2(ClientFrameStage_t stage)
{
	if (!g_LocalPlayer->IsAlive())
		return;

	static int userId[64];
	static AnimationLayer
		backupLayersUpdate[64][15],
		backupLayersInterp[64][15];

	for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
	{
		C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);

		if (!player ||
			player == g_LocalPlayer ||
			player->m_iTeamNum() == g_LocalPlayer->m_iTeamNum() ||
			!player->IsPlayer() ||
			player->IsDormant())
			continue;

		player_info_t player_info;
		g_EngineClient->GetPlayerInfo(i, &player_info);

		switch (stage)
		{
		case ClientFrameStage_t::FRAME_NET_UPDATE_START: // Copy new, server layers to use when drawing.
			userId[i] = player_info.userId;
			memcpy(&backupLayersUpdate[i], player->GetAnimOverlays(), (sizeof AnimationLayer) * player->GetNumAnimOverlays());
			break;
		case ClientFrameStage_t::FRAME_RENDER_START: // Render started, don't use inaccurately extrapolated layers but save them to not mess shit up either.
			if (userId[i] != player_info.userId) continue;
			memcpy(&backupLayersInterp[i], player->GetAnimOverlays(), (sizeof AnimationLayer) * player->GetNumAnimOverlays());
			memcpy(player->GetAnimOverlays(), &backupLayersUpdate[i], (sizeof AnimationLayer) * player->GetNumAnimOverlays());
			break;
		case ClientFrameStage_t::FRAME_RENDER_END: // Restore layers to keep being accurate when backtracking.
			if (userId[i] != player_info.userId) continue;
			memcpy(player->GetAnimOverlays(), &backupLayersInterp[i], (sizeof AnimationLayer) * player->GetNumAnimOverlays());
			break;
		default:
			return;
		}
	}
}

void CMBacktracking::SetOverwriteTick(C_BasePlayer *player, QAngle angles, float_t correct_time, uint32_t priority)
{
	int idx = player->EntIndex();
	LagRecord overwrite_record;
	auto& m_LagRecords = this->m_LagRecord[player->EntIndex()];

	if (!IsTickValid(TIME_TO_TICKS(correct_time)) && (g_GlobalVars->curtime - TIME_TO_TICKS(correct_time)) > g_Options.rage_fakelatency)
		g_CVar->ConsoleColorPrintf(Color(255, 0, 0, 255), "Dev Error: failed to overwrite tick, delta too big. Priority: %d\n", priority);

	overwrite_record.SaveRecord(player);
	overwrite_record.m_angAngles = angles;
	overwrite_record.m_iPriority = priority;
	overwrite_record.m_flSimulationTime = correct_time;
	m_LagRecords.emplace_back(overwrite_record);
}

void CMBacktracking::RageBacktrack(C_BasePlayer* target, CUserCmd* usercmd, Vector &aim_point, bool &hitchanced)
{
	auto firedShots = g_LocalPlayer->m_iShotsFired();
	if (StartLagCompensation(target))
	{
		/*auto forwardtrack = forwardtracking_record[target->EntIndex()];
		if (forwardtrack.valid) {
			//if (!forwardtrack.IsTickValid())
			//	forwardtracking_record[target->EntIndex()].valid = false;
			//else {
				//if (!forwardtrack.m_bMatrixBuilt) {
			if (!target->SetupBones(forwardtracking_record[target->EntIndex()].matrix, 128, 256, forwardtrack.m_iSimTime + TICKS_TO_TIME(forwardtrack.m_iTickdeff)))
				forwardtrack.valid = false;
				//		forwardtracking_record[target->EntIndex()].m_bMatrixBuilt = true;
				//}
				//if (forwardtrack.m_bMatrixBuilt) {
				if (forwardtrack.valid) {
					if (g_Options.rage_autobaim && (firedShots > g_Options.rage_baim_after_x_shots || AimRage::Get().GetDamageVec(target->GetHitboxPos(HITBOX_PELVIS), g_LocalPlayer, HITBOX_PELVIS) > target->m_iHealth()))
						aim_point = AimRage::Get().CalculateBestPoint(target, HITBOX_PELVIS, g_Options.rage_mindmg, true, forwardtrack.matrix);
					else
						aim_point = AimRage::Get().CalculateBestPoint(target, realHitboxSpot[g_Options.rage_hitbox], g_Options.rage_mindmg, g_Options.rage_prioritize, forwardtrack.matrix);
					if (aim_point.IsValid()) {
						QAngle aimAngle = Math::CalcAngle(g_LocalPlayer->GetEyePos(), aim_point) - (g_Options.rage_norecoil ? g_LocalPlayer->m_aimPunchAngle() * 2.f : QAngle(0, 0, 0));

						hitchanced = AimRage::Get().HitChance(aimAngle, target, g_Options.rage_hitchance_amount);

						this->current_forwardtracking_record[target->EntIndex()] = forwardtrack;
						FinishLagCompensation(target);
						usercmd->tick_count = TIME_TO_TICKS(forwardtrack.m_iSimTime) + forwardtrack.m_iTickdeff;
						return;
					}
				}
			//}
		}*/
		LagRecord cur_record;
		auto& m_LagRecords = this->m_LagRecord[target->EntIndex()];
		auto backup_anim = Animation::Get().GetPlayerAnimationInfo(target->EntIndex());
		while (FindViableRecord(target, &cur_record))
		{
			auto iter = std::find(m_LagRecords.begin(), m_LagRecords.end(), cur_record);
			if (iter == m_LagRecords.end())
				continue;

			if (iter->m_bNoGoodSpots)
			{
				// Already awalled from same spot, don't try again like a dumbass.
				if (iter->m_vecLocalAimspot == g_LocalPlayer->GetEyePos())
					continue;
				else
					iter->m_bNoGoodSpots = false;
			}

			if (!iter->m_bMatrixBuilt)
			{
				Animation::Get().RestoreAnim(target, iter->m_flSimulationTime);
				if (!target->SetupBonesExperimental(iter->matrix, 128, 256, iter->m_flSimulationTime))
					continue;

				iter->m_bMatrixBuilt = true;
			}

			float mindmg = g_Options.rage_mindmg_wep[0];
			auto weapinfo = AimRage::Get().local_weapon->GetWeapInfo();
			switch (weapinfo->weapon_type)
			{
			case WEAPONTYPE_PISTOL:
				if (AimRage::Get().local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
					mindmg = g_Options.rage_mindmg_wep[2];
				else
					mindmg = g_Options.rage_mindmg_wep[1];
				break;
			case WEAPONTYPE_RIFLE:
				switch (AimRage::Get().local_weapon->m_iItemDefinitionIndex())
				{
				case WEAPON_SCAR20:
				case WEAPON_G3SG1:
					mindmg = g_Options.rage_mindmg_wep[5];
					break;
				case WEAPON_SCOUT:
					mindmg = g_Options.rage_mindmg_wep[6];
					break;
				case WEAPON_AWP:
					mindmg = g_Options.rage_mindmg_wep[7];
					break;
				default:
					mindmg = g_Options.rage_mindmg_wep[4];
					break;
				}
				break;
			case WEAPONTYPE_SUBMACHINEGUN:
				mindmg = g_Options.rage_mindmg_wep[3];
				break;
			}
			/*
			if (AimRage::Get().local_weapon->IsPistol())
			{
				if (AimRage::Get().local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
					mindmg = g_Options.rage_mindmg_wep[2];
				else
					mindmg = g_Options.rage_mindmg_wep[1];
			}
			else if (AimRage::Get().local_weapon->IsRifle())
			{
				mindmg = g_Options.rage_mindmg_wep[4];
			}
			else if (AimRage::Get().local_weapon->IsSniper())
			{
				switch (AimRage::Get().local_weapon->m_iItemDefinitionIndex())
				{
				case WEAPON_SCAR20:
				case WEAPON_G3SG1:
					mindmg = g_Options.rage_mindmg_wep[5];
					break;
				case WEAPON_SCOUT:
					mindmg = g_Options.rage_mindmg_wep[6];
					break;
				case WEAPON_AWP:
					mindmg = g_Options.rage_mindmg_wep[7];
					break;
				}
			}
			//mindmg = g_Options.rage_mindmg_wep[3];	smg is disabled for now
			*/
			Vector baimTarget = AimRage::Get().CalculateBestPoint(target, HITBOX_PELVIS, mindmg, false, iter->matrix, true);
			if ((g_Options.rage_autobaim && (firedShots > g_Options.rage_baim_after_x_shots || AimRage::Get().GetDamageVec(baimTarget, target, HITBOX_PELVIS) > target->m_iHealth()))
				|| (Global::bBaim[target->EntIndex()] && iter->m_strResolveMode.find("Update") == std::string::npos && iter->m_strResolveMode.find("Moving") == std::string::npos && iter->m_strResolveMode.find("No Fake") == std::string::npos && iter->m_strResolveMode.find("Shot") == std::string::npos))
				aim_point = baimTarget;
			else
				aim_point = AimRage::Get().CalculateBestPoint(target, realHitboxSpot[g_Options.rage_hitbox], mindmg, g_Options.rage_prioritize, iter->matrix, false);

			if (!aim_point.IsValid())
			{
				FinishLagCompensation(target);
				iter->m_bNoGoodSpots = true;
				iter->m_vecLocalAimspot = g_LocalPlayer->GetEyePos();
				continue;
			}

			QAngle aimAngle = Math::CalcAngle(g_LocalPlayer->GetEyePos(), aim_point) - (g_Options.rage_norecoil ? g_LocalPlayer->m_aimPunchAngle() * 2.f : QAngle(0,0,0));

			float hitchan = g_Options.rage_hitchance_wep[0];
			switch (weapinfo->weapon_type)
			{
			case WEAPONTYPE_PISTOL:
				if (AimRage::Get().local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
					hitchan = g_Options.rage_hitchance_wep[2];
				else
					hitchan = g_Options.rage_hitchance_wep[1];
				break;
			case WEAPONTYPE_RIFLE:
				switch (AimRage::Get().local_weapon->m_iItemDefinitionIndex())
				{
				case WEAPON_SCAR20:
				case WEAPON_G3SG1:
					hitchan = g_Options.rage_hitchance_wep[5];
					break;
				case WEAPON_SCOUT:
					hitchan = g_Options.rage_hitchance_wep[6];
					break;
				case WEAPON_AWP:
					hitchan = g_Options.rage_hitchance_wep[7];
					break;
				default:
					hitchan = g_Options.rage_hitchance_wep[4];
					break;
				}
				break;
			case WEAPONTYPE_SUBMACHINEGUN:
				hitchan = g_Options.rage_hitchance_wep[3];
				break;
			}
			/*
			if (AimRage::Get().local_weapon->IsPistol())
			{
				if (AimRage::Get().local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
					hitchan = g_Options.rage_hitchance_wep[2];
				else
					hitchan = g_Options.rage_hitchance_wep[1];
			}
			else if (AimRage::Get().local_weapon->IsRifle())
			{
				hitchan = g_Options.rage_hitchance_wep[4];
			}
			else if (AimRage::Get().local_weapon->IsSniper())
			{
				switch (AimRage::Get().local_weapon->m_iItemDefinitionIndex())
				{
				case WEAPON_SCAR20:
				case WEAPON_G3SG1:
					hitchan = g_Options.rage_hitchance_wep[5];
					break;
				case WEAPON_SCOUT:
					hitchan = g_Options.rage_hitchance_wep[6];
					break;
				case WEAPON_AWP:
					hitchan = g_Options.rage_hitchance_wep[7];
					break;
				}
			}
			//hitchan = g_Options.rage_hitchance_wep[3];	smg is disabled for now
			*/
			hitchanced = AimRage::Get().HitChance(aimAngle, target, hitchan);

			this->current_record[target->EntIndex()] = *iter;
			break;
		}
		Animation::Get().ApplyAnim(target, backup_anim);
		FinishLagCompensation(target);
		ProcessCMD(target->EntIndex(), usercmd);
	}
}

bool CMBacktracking::IsTickValid(int tick)
{	
	// better use polak's version than our old one, getting more accurate results

	INetChannelInfo *nci = g_EngineClient->GetNetChannelInfo();

	auto sv_maxunlag = g_CVar->FindVar("sv_maxunlag");

	if (!nci || !sv_maxunlag)
		return false;

	float correct = clamp(nci->GetLatency(FLOW_OUTGOING) + GetLerpTime(), 0.f, sv_maxunlag->GetFloat());

	float deltaTime = correct - (g_GlobalVars->curtime - TICKS_TO_TIME(tick));

	return fabsf(deltaTime) <= 0.2f;
}

bool CMBacktracking::IsPlayerValid(C_BasePlayer *player)
{
	if (!player)
		return false;

	if (!player->IsPlayer())
		return false;

	if (player == g_LocalPlayer)
		return false;

	if (player->IsDormant())
		return false;

	if (!player->IsAlive())
		return false;

	if (player->m_iTeamNum() == g_LocalPlayer->m_iTeamNum())
		return false;

	if (player->m_bGunGameImmunity())
		return false;

	return true;
}

float CMBacktracking::GetLerpTime()
{
	int ud_rate = g_CVar->FindVar("cl_updaterate")->GetInt();
	ConVar *min_ud_rate = g_CVar->FindVar("sv_minupdaterate");
	ConVar *max_ud_rate = g_CVar->FindVar("sv_maxupdaterate");

	if (min_ud_rate && max_ud_rate)
		ud_rate = max_ud_rate->GetInt();

	float ratio = g_CVar->FindVar("cl_interp_ratio")->GetFloat();

	if (ratio == 0)
		ratio = 1.0f;

	float lerp = g_CVar->FindVar("cl_interp")->GetFloat();
	ConVar *c_min_ratio = g_CVar->FindVar("sv_client_min_interp_ratio");
	ConVar *c_max_ratio = g_CVar->FindVar("sv_client_max_interp_ratio");

	if (c_min_ratio && c_max_ratio && c_min_ratio->GetFloat() != 1)
		ratio = clamp(ratio, c_min_ratio->GetFloat(), c_max_ratio->GetFloat());

	return max(lerp, (ratio / ud_rate));
}

template<class T, class U>
T CMBacktracking::clamp(T in, U low, U high)
{
	if (in <= low)
		return low;

	if (in >= high)
		return high;

	return in;
}

bool vischeck(matrix3x4_t matrix[MAXSTUDIOBONES], C_BasePlayer* target) {
	Vector eyePos = g_LocalPlayer->GetEyePos();

	CGameTrace tr;
	Ray_t ray;
	CTraceFilter filter;
	filter.pSkip = g_LocalPlayer;

	auto studio_model = g_MdlInfo->GetStudiomodel(target->GetModel());
	if (!studio_model)
	{
		return false;
	}
	for (int i = 0; i < HITBOX_MAX; i++)
	{
		auto hitbox = studio_model->pHitboxSet(target->m_nHitboxSet())->pHitbox(i);
		if (hitbox)
		{
			auto
				min = Vector{},
				max = Vector{};

			Math::VectorTransform(hitbox->bbmin, matrix[hitbox->bone], min);
			Math::VectorTransform(hitbox->bbmax, matrix[hitbox->bone], max);

			ray.Init(eyePos, (min + max) * 0.5);
			g_EngineTrace->TraceRay(ray, MASK_SHOT | CONTENTS_GRATE, &filter, &tr);

			if (tr.hit_entity == target || tr.fraction > 0.97f)
				return true;
		}
	}
	return false;
}

float GetFOV(const Vector from, const Vector to)
{
	return RAD2DEG(acos(from.Dot(to) / from.LengthSqr()));
}
void CMBacktracking::LegitBackTrack(CUserCmd *usercmd)
{
	QAngle viewangle; g_EngineClient->GetViewAngles(viewangle);
	static ConVar* weapon_recoil_scale = g_CVar->FindVar("weapon_recoil_scale");
	if (checks::is_bad_ptr(weapon_recoil_scale))
	{
		weapon_recoil_scale = g_CVar->FindVar("weapon_recoil_scale");
		viewangle += g_LocalPlayer->m_aimPunchAngle() * 2.f;
	}
	else
	{
		viewangle += g_LocalPlayer->m_aimPunchAngle() * weapon_recoil_scale->GetFloat();
	}

	float cloest_fov = 360;
	int best_target = -1;
	for (int i = 1; i < g_EngineClient->GetMaxClients() && cloest_fov > 0; i++)
	{
		if (!AimRage::Get().CheckTarget(i))
			continue;
		auto temptarget = C_BasePlayer::GetPlayerByIndex(i);
		float fov = fabsf((Math::CalcAngle(g_LocalPlayer->GetEyePos(), temptarget->GetBonePos(0)) - viewangle).Length());
		if (fov < cloest_fov)
		{
			best_target = i;
			cloest_fov = fov;
		}
	}
	if (best_target < 0)
		return;

	auto records = this->m_LagRecord[best_target];
	auto best_entity = C_BasePlayer::GetPlayerByIndex(best_target);

	auto studio_model = g_MdlInfo->GetStudiomodel(best_entity->GetModel());
	if (studio_model)
	{
		Vector cloest_aim = Vector(0, 0, 0);
		if (StartLagCompensation(best_entity))
		{
			cloest_fov = 360;
			LagRecord cur_record;
			LagRecord best_record;
			while (FindViableRecord(best_entity, &cur_record))
			{
				auto iter = std::find(records.begin(), records.end(), cur_record);

				if (iter == records.end())
					continue;

				if (!iter->m_bMatrixBuilt)
				{
					if (!best_entity->SetupBonesExperimental(iter->matrix, 128, 256, iter->m_flSimulationTime))
						continue;

					iter->m_bMatrixBuilt = true;
				}

				if (!vischeck(iter->matrix, best_entity))
					continue;

				float fov = 360;
				auto min = Vector{}, max = Vector{};
				for (int i = 0; i < HITBOX_MAX; i++)
				{
					auto hitbox = studio_model->pHitboxSet(best_entity->m_nHitboxSet())->pHitbox(i);
					if (hitbox)
					{
						Math::VectorTransform(hitbox->bbmin, iter->matrix[hitbox->bone], min);
						Math::VectorTransform(hitbox->bbmax, iter->matrix[hitbox->bone], max);

						float tempfov = fabsf((Math::CalcAngle(g_LocalPlayer->GetEyePos(), (min + max) * 0.5) - viewangle).Length());
						if (tempfov < fov)
						{
							fov = tempfov;
						}
					}
				}

				if (fov > cloest_fov)
					continue;

				cloest_fov = fov;
				cloest_aim = (min + max) * 0.5;

				best_record = *iter;
			}
			m_RestoreLagRecord[best_target].first = best_record;
			this->current_record[best_target] = best_record;
			FinishLagCompensation(best_entity);
			ProcessCMD(best_target, usercmd);
			if (g_Options.legit_enabled && cloest_fov < g_Options.legit_fov / g_Options.legit_smooth_factor && (usercmd->buttons & IN_ATTACK))
			{
				usercmd->viewangles = Math::CalcAngle(g_LocalPlayer->GetEyePos(), cloest_aim) + (g_LocalPlayer->m_aimPunchAngle() * weapon_recoil_scale->GetFloat());
			}
		}
	}
}

void CMBacktracking::UpdateIncomingSequences()
{
	auto nci = Global::netchan;
	if (checks::is_bad_ptr(nci))
		return;

	if (nci->m_nInSequenceNr > last_incoming_sequence)
	{
		last_incoming_sequence = nci->m_nInSequenceNr;

		sequence_records.push_front(Incoming_Sequence_Record(nci->m_nInReliableState, nci->m_nOutReliableState, nci->m_nInSequenceNr, g_GlobalVars->realtime));
	}

	if (sequence_records.size() > 2048)
		sequence_records.pop_back();
}

void CMBacktracking::AddLatency(INetChannel* net_channel, float latency)
{
	for (const auto& sequence : sequence_records)
	{
		if (g_GlobalVars->realtime - sequence.time >= latency)
		{
			net_channel->m_nInReliableState = sequence.in_reliable_state;
			//net_channel->m_nOutReliableState = sequence.out_reliable_state;
			net_channel->m_nInSequenceNr = sequence.in_sequence_num;
			break;
		}
	}
}

void CMBacktracking::AddLatency2(INetChannel* net_channel, float latency)
{
	net_channel->m_nInSequenceNr += 2 * NET_FRAMES_MASK - static_cast<uint32_t>(NET_FRAMES_MASK * latency);
}