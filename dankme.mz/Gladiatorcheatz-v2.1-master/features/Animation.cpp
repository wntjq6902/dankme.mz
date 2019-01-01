#include "Animation.hpp"
#include "../helpers/Math.hpp"

void Animation::UpdatePlayerAnimations(int32_t idx)
{
	return;		//recent update broke animatuon fix.
	
	C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(idx);
	if (checks::is_bad_ptr(player) || !player->IsAlive())
		return;

	static float timestamp = 0;

	auto &info = arr_infos[idx];

	bool
		allocate = (checks::is_bad_ptr(info.m_playerAnimState)),
		change = (!allocate) && (&player->GetRefEHandle() != info.m_ulEntHandle),
		reset = (!allocate && !change) && (player->m_flSpawnTime() != info.m_flSpawnTime);

	// player changed, free old animation state.
	if (change)
	{
		if (!checks::is_bad_ptr(info.m_playerAnimState) && abs(g_GlobalVars->curtime - timestamp) > 0.1)	//basic nullptr check and expirmantal crash fix
		{
			arr_infos_record[idx].clear();
			g_pMemAlloc->Free(info.m_playerAnimState);	//calling this too fast crashes?
			timestamp = g_GlobalVars->curtime;
		}
		else
		{
			return;
		}
	}

	// need to reset? (on respawn)
	if (reset)
	{
		if (!checks::is_bad_ptr(info.m_playerAnimState))
		{
			// reset animation state.
			C_BasePlayer::ResetAnimationState(info.m_playerAnimState);

			// note new spawn time.
			info.m_flSpawnTime = player->m_flSpawnTime();
		}
	}

	// need to allocate or create new due to player change.
	if (allocate || change)
	{
		C_CSGOPlayerAnimState *state = (C_CSGOPlayerAnimState*)g_pMemAlloc->Alloc(sizeof(C_CSGOPlayerAnimState));

		if (checks::is_bad_ptr(state))
			return;
		
		player->CreateAnimationState(state);

		// used to detect if we need to recreate / reset.
		info.m_ulEntHandle = const_cast<CBaseHandle*>(&player->GetRefEHandle());
		info.m_flSpawnTime = player->m_flSpawnTime();

		// note anim state for future use.
		info.m_playerAnimState = state;
	}

	if (checks::is_bad_ptr(info.m_playerAnimState))
		return;

	std::array<float_t, 24> backup_poses = player->m_flPoseParameter();

	AnimationLayer backup_layers[15];
	std::memcpy(backup_layers, player->GetAnimOverlays(), (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));

	// fixing legs and few other things missing here
	C_BasePlayer::UpdateAnimationState(info.m_playerAnimState, (player == g_LocalPlayer) ? (Global::visualAngles) : (player->m_angEyeAngles()));

	info.m_flPoseParameters = player->m_flPoseParameter();
	std::memcpy(info.m_AnimationLayer, player->GetAnimOverlays(), (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));

	player->m_flPoseParameter() = backup_poses;
	std::memcpy(player->GetAnimOverlays(), backup_layers, (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));

	arr_infos_record[idx].push_front({ g_GlobalVars->curtime, arr_infos[idx] });
}

void Animation::UpdateAnimationAngles(AnimationInfo &anim, QAngle angles)
{
	return;
	C_BasePlayer::UpdateAnimationState(anim.m_playerAnimState, angles);
}

AnimationInfo &Animation::GetPlayerAnimationInfo(int32_t idx)
{
	return arr_infos[idx];
}

AnimationInfo &Animation::GetPlayerAnimationInfo(int32_t idx, float time)
{
	for (auto i = arr_infos_record[idx].begin(); i != arr_infos_record[idx].end(); i++)
	{
		if (time == i->first)
			return i->second;
	}
	return AnimationInfo();
}

std::deque<std::pair<float, AnimationInfo>> Animation::GetPlayerAnimationRecord(int32_t idx)
{
	return arr_infos_record[idx];
}

void Animation::RestoreAnim(C_BasePlayer *player, float time)
{
	return;
	auto anim = GetPlayerAnimationInfo(player->EntIndex(), time);
	if(anim.m_flSpawnTime > 0.1f)
		ApplyAnim(player, anim);
}

void Animation::RestoreAnim(C_BasePlayer *player)
{
	return;
	if (arr_infos_record[player->EntIndex()].empty())
		return;

	ApplyAnim(player, arr_infos_record[player->EntIndex()].front().second);
}

void Animation::ApplyAnim(C_BasePlayer *player, AnimationInfo anim)
{
	return;
	player->m_flPoseParameter() = anim.m_flPoseParameters;
	std::memcpy(player->GetAnimOverlays(), anim.m_AnimationLayer, (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));
}

void Animation::HandleAnimFix(C_BasePlayer *player, bool restore, bool modify_angles, bool modify_origin)
{
	return;
	if (checks::is_bad_ptr(player))
		return;

	int i = player->EntIndex();
	if (restore)
	{
		if (modify_origin) player->SetAbsOrigin(origin_backup.at(i));
		if (modify_angles) player->SetAbsAngles(angles_backup.at(i));
		player->m_flPoseParameter() = animation_backup.at(i).m_flPoseParameters;
		std::memcpy(player->GetAnimOverlays(), animation_backup.at(i).m_AnimationLayer, (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));
	}
	else
	{
		origin_backup.at(i) = player->GetAbsOrigin();
		angles_backup.at(i) = player->GetAbsAngles();

		animation_backup.at(i).m_flPoseParameters = player->m_flPoseParameter();
		std::memcpy(animation_backup.at(i).m_AnimationLayer, player->GetAnimOverlays(), (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));

		Vector origin = player->m_vecOrigin();
		QAngle angles = (player == g_LocalPlayer) ? (Global::visualAngles) : (player->m_angEyeAngles()); angles.pitch = 0; angles.roll = 0;

		auto &anim_data = GetPlayerAnimationInfo(i);

		if (checks::is_bad_ptr(anim_data.m_playerAnimState))
			return;

		if (modify_origin) player->SetAbsOrigin(origin);
		if (modify_angles) player->SetAbsAngles(angles);
		player->m_flPoseParameter() = anim_data.m_flPoseParameters;
		std::memcpy(player->GetAnimOverlays(), anim_data.m_AnimationLayer, (sizeof(AnimationLayer) * player->GetNumAnimOverlays()));
	}
}