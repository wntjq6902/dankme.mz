#pragma once

#include "../Singleton.hpp"

#include "../Structs.hpp"

struct AnimationInfo
{
	AnimationInfo()
	{
		m_flSpawnTime = 0.f;
		m_ulEntHandle = nullptr;
		m_playerAnimState = nullptr;
	}

	std::array<float_t, 24> m_flPoseParameters;
	AnimationLayer m_AnimationLayer[15];

	float_t m_flSpawnTime;
	CBaseHandle *m_ulEntHandle;

	C_CSGOPlayerAnimState *m_playerAnimState;
};

class Animation : public Singleton<Animation>
{

public:

	void UpdatePlayerAnimations(int32_t idx);

	void UpdateAnimationAngles(AnimationInfo &anim, QAngle angles);

	AnimationInfo &GetPlayerAnimationInfo(int32_t idx);

	AnimationInfo &GetPlayerAnimationInfo(int32_t idx, float time);

	std::deque<std::pair<float, AnimationInfo>> GetPlayerAnimationRecord(int32_t idx);

	void RestoreAnim(C_BasePlayer *player, float time);

	void RestoreAnim(C_BasePlayer *player);

	void ApplyAnim(C_BasePlayer *player, AnimationInfo anim);

	void HandleAnimFix(C_BasePlayer *player, bool restore, bool modify_angles = true, bool modify_origin = true);

private:

	AnimationInfo arr_infos[65];

	std::deque<std::pair<float, AnimationInfo>> arr_infos_record[65];

	std::array<Vector, 65> origin_backup;
	std::array<QAngle, 65> angles_backup;

	std::array<AnimationInfo, 65> animation_backup;
};