#include "AimRage.hpp"

#include "../Options.hpp"
#include "../helpers/Math.hpp"
#include "../helpers/Utils.hpp"
#include "LagCompensation.hpp"
#include "RebuildGameMovement.hpp"
#include <limits>
#include <stack>	//for self written aimbot targetting meme
					//but still better then randomly not shooting meme

#define M_PI 3.14159265358979323846

#define NETVAROFFS(type, name, table, netvar)                           \
	junkcode::call();			\
    int name##() const {                                          \
        static int _##name = NetMngr::Get().getOffs(table, netvar);     \
        return _##name;                 \
	}

void AimRage::Work(CUserCmd *usercmd)
{
	if (!g_Options.rage_enabled)
		return;

	this->local_weapon = g_LocalPlayer->m_hActiveWeapon().Get();
	this->usercmd = usercmd;
	this->cur_time = this->GetTickbase() * g_GlobalVars->interval_per_tick;

	Global::bAimbotting = false;
	Global::bVisualAimbotting = false;

	if (g_Options.rage_autocockrevolver)
	{
		if (!this->CockRevolver())
			return;
	}

	if (g_Options.misc_smacbypass && (!Global::AimTargets || (usercmd->buttons & IN_USE)))
	{
		QAngle target_angle = usercmd->viewangles;
		QAngle delta = last_angle - target_angle; Math::ClampAngles(delta);

		if (delta.Length() >= 10)	//angle delta is greater then 10 deagree, maximum value is 45 but just to be safe.
		{
			if (abs(delta.pitch) >= 10)
			{
				last_angle.pitch += (delta.pitch > 0) ? -10 : 10;	//clamp maximum pitch change to 10
			}
			else
			{
				last_angle.pitch = target_angle.pitch;		//we can safly lock to target on pitch axes
			}

			if (abs(delta.yaw) >= 10 - abs(delta.pitch))	//yaw delta is higher then 10
			{
				last_angle.yaw += (delta.yaw > 0) ? -10 + abs(delta.pitch) : 10 - abs(delta.pitch);	//clamp maximum yaw change to 10
			}
			else
			{
				last_angle.yaw = target_angle.yaw;		//we can safly lock to target on yaw axes
			}

			usercmd->buttons &= ~IN_USE;
		}
		else
		{
			last_angle = target_angle;
		}
	}

	last_angle.roll = 0;

	if (g_Options.misc_smacbypass && g_Options.rage_silent && !(g_Options.hvh_antiaim_y || g_Options.hvh_antiaim_x || g_Options.hvh_antiaim_legit))
	{
		usercmd->viewangles = last_angle;
	}

	if (!local_weapon)
		return;

	if (g_LocalPlayer->m_flNextAttack() > this->cur_time && !g_Options.misc_smacbypass)
		return;

	if (g_Options.rage_usekey && !g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.rage_aimkey)))
		return;

	// Also add checks for grenade throw time if we dont have that yet.
	if (!g_LocalPlayer->m_hActiveWeapon().Get()->IsKnife() && (g_LocalPlayer->m_hActiveWeapon().Get()->IsWeaponNonAim() || g_LocalPlayer->m_hActiveWeapon().Get()->m_iClip1() < 1))
		return;

	//TargetEntities();
	NewTargetEntities();

	//if (g_Options.rage_autocockrevolver && !Global::bAimbotting && local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
	//	usercmd->buttons &= ~IN_ATTACK;
}

bool AimRage::TargetSpecificEnt(C_BasePlayer* pEnt)
{
	int i = pEnt->EntIndex();
	auto firedShots = g_LocalPlayer->m_iShotsFired();

	int iHitbox = realHitboxSpot[g_Options.rage_hitbox];

	Vector vecTarget;
	junkcode::call();
	// Disgusting ass codes, can't think of a cleaner way now though. FIX ME.
	bool LagComp_Hitchanced = false;
	if (g_Options.rage_lagcompensation)
	{
		CMBacktracking::Get().RageBacktrack(pEnt, usercmd, vecTarget, LagComp_Hitchanced);
	}
	else
	{
		matrix3x4_t matrix[128];
		if (!pEnt->SetupBonesExperimental(matrix, 128, 256, g_EngineClient->GetLastTimeStamp()))
			return false;

		if ((g_Options.rage_autobaim && (firedShots > g_Options.rage_baim_after_x_shots || GetDamageVec(pEnt->GetHitboxPos(HITBOX_PELVIS), pEnt, HITBOX_PELVIS) > pEnt->m_iHealth())) || (strstr(Global::resolverModes[pEnt->EntIndex()].c_str(), "Fakewalking") && firedShots > 2)) {
			vecTarget = CalculateBestPoint(pEnt, HITBOX_PELVIS, g_Options.rage_mindmg, false, matrix, true);
		}
		else
			vecTarget = CalculateBestPoint(pEnt, iHitbox, g_Options.rage_mindmg, g_Options.rage_prioritize, matrix);
	}

	// Invalid target/no hitable points at all.
	if (!vecTarget.IsValid())
		return false;

	if (GetDamageVec(vecTarget, pEnt, aimhitbox) < g_Options.rage_mindmg)
		return false;

	AutoStop();
	AutoCrouch();

	QAngle new_aim_angles = Math::CalcAngle(g_LocalPlayer->GetEyePos(), vecTarget) - (g_Options.rage_norecoil ? g_LocalPlayer->m_aimPunchAngle() * 2.f : QAngle(0, 0, 0));
	bool psilent_choking = true;

	this->usercmd->viewangles = Global::vecVisualAimbotAngs = new_aim_angles;
	Global::vecVisualAimbotAngs += (g_Options.removals_novisualrecoil ? g_LocalPlayer->m_aimPunchAngle() * 2.f : QAngle(0, 0, 0));
	Global::bVisualAimbotting = true;

	if (this->can_fire_weapon || !(local_weapon->IsPistol() && !(local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)))
	{
		// Save more fps by remembering to try the same entity again next time.
		prev_aimtarget = pEnt->EntIndex();

		if (g_Options.rage_autoscope && g_LocalPlayer->m_hActiveWeapon().Get()->IsSniper() && g_LocalPlayer->m_hActiveWeapon().Get()->m_zoomLevel() == 0)
		{
			usercmd->buttons |= IN_ATTACK2;
		}
		else if ((g_Options.rage_lagcompensation && LagComp_Hitchanced) || (!LagComp_Hitchanced && HitChance(usercmd->viewangles, pEnt, g_Options.rage_hitchance_amount)))
		{
			Global::bAimbotting = true;

			if (g_Options.rage_autoshoot)
			{
				usercmd->buttons |= IN_ATTACK;
			}
		}
	}

	return true;
}

void AimRage::TargetEntities()
{
	static C_BaseCombatWeapon *oldWeapon; // what is this for?
	if (local_weapon != oldWeapon)
	{
		oldWeapon = local_weapon;
		usercmd->buttons &= ~IN_ATTACK;
		return;
	}
	if (local_weapon->IsPistol() && !(local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER) && usercmd->tick_count % 2)
	{
		static int lastshot;
		if (usercmd->buttons & IN_ATTACK)
			lastshot++;

		if (!usercmd->buttons & IN_ATTACK || lastshot > 1)
		{
			usercmd->buttons &= ~IN_ATTACK;
			lastshot = 0;
		}
		//return;
	}

	/*
		We should also add those health/fov based memes and only check newest record. Good enough IMO
	*/

	this->can_fire_weapon = local_weapon->CanFire();
	if (prev_aimtarget && CheckTarget(prev_aimtarget))
	{
		if (TargetSpecificEnt(C_BasePlayer::GetPlayerByIndex(prev_aimtarget)))
			return;
	}
	junkcode::call();
	for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
	{
		// Failed to shoot at him again, reset him and try others.
		if (prev_aimtarget && prev_aimtarget == i)
		{
			prev_aimtarget = NULL;
			continue;
		}

		if (!CheckTarget(i))
			continue;

		C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);

		if (TargetSpecificEnt(player))
			return;
	}
}

void AimRage::NewTargetEntities() {
	this->can_fire_weapon = local_weapon->CanFire();
	Global::bAimbotting = false;
	static C_BaseCombatWeapon *oldWeapon; // what is this for?
	if (local_weapon != oldWeapon)
	{
		oldWeapon = local_weapon;
		usercmd->buttons &= ~IN_ATTACK;
		return;
	}
	if (local_weapon->IsPistol() && !(local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER) && ((g_Options.misc_smacbypass) ? (!usercmd->tick_count % 3) : (usercmd->tick_count % 2)))
	{
		static int lastshot;
		if (usercmd->buttons & IN_ATTACK)
			lastshot++;

		if (!usercmd->buttons & IN_ATTACK || lastshot > 1)
		{
			usercmd->buttons &= ~IN_ATTACK;
			lastshot = 0;
		}
		//return;
	}

	if (!local_weapon || !(can_fire_weapon || local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER || local_weapon->IsKnife()))
		return;

	if (local_weapon->IsKnife())
	{
		KnifeBot();
		return;
	}

	junkcode::call();
	bool hitchanced = false;
	QAngle aimang;

	if (prev_aimtarget && CanHit(prev_aimtarget, hitchanced, aimang))
	{	//fps saving
		if (aimatTarget(prev_aimtarget, aimang, hitchanced))
			return;

		hitchanced = false;
	}

	prev_aimtarget = NULL;
	Global::AimTargets = 0;

	std::vector<int> targets;
	std::stack<float> targetdest;
	std::stack<int> temptarget;
	std::stack<float> temptargetdest;
	float maxdest = 999999.f;

	targets.clear();

	for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
	{
		if (!CheckTarget(i))
			continue;

		targets.push_back(i);
	}
	junkcode::call();
	QAngle viewangle; g_EngineClient->GetViewAngles(viewangle);

	std::stable_sort(targets.begin(), targets.end(), [&](const int& a, const int& b)	//using stable sort for no reason other then i think it's faster?
		{
			junkcode::call();
			C_BasePlayer *aent = reinterpret_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(a)), *bent = reinterpret_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(b));
			QAngle anga, angb;
			switch (g_Options.rage_targetselection)
			{
			case AR_TARGET_DEST:
				return (g_LocalPlayer->GetAbsOrigin().DistTo(aent->GetAbsOrigin()) < g_LocalPlayer->GetAbsOrigin().DistTo(bent->GetAbsOrigin()));
				break;
			case AR_TARGET_HEALTH:
				return (aent->m_iHealth() < bent->m_iHealth());
				break;
			case AR_TARGET_PING:
				return (aent->GetPing() < bent->GetPing());
				break;
			case AR_TARGET_FOV:
				anga = viewangle - Math::CalcAngle(g_LocalPlayer->GetEyePos(), aent->GetHitboxPos(0));
				angb = viewangle - Math::CalcAngle(g_LocalPlayer->GetEyePos(), bent->GetHitboxPos(0));
				Math::ClampAngles(anga); Math::ClampAngles(angb);
				return (anga.Length() < angb.Length());
				break;
			case AR_TARGET_LASTDMG:
				return (Global::lastdmg[a] < Global::lastdmg[b]);
				break;
			}
		}
	);
	Global::AimTargets = targets.size();
	while (!targets.empty())
	{
		if (CanHit(targets.front(), hitchanced, aimang))
		{
			if(aimatTarget(targets.front(), aimang, hitchanced))
				return;
		}
		targets.erase(targets.begin());
	}
}

bool IsBehind(Vector src, Vector dst, QAngle viewangle)
{
	Vector delta = dst - src;
	delta.z = 0;
	delta.NormalizeInPlace();

	Vector viewvector;	//cuz, you know, it's vector instad of angle? kill my self please
	Math::AngleVectors(viewangle, viewvector);
	viewvector.z = 0;

	// CSS:0.8, CSGO:0.475
	return (delta.Dot(viewvector) > 0.475);
}

bool KnifeTrace(C_BasePlayer *player, Vector &out)
{
	static int hitboxesLoop[] =
	{
		HITBOX_HEAD,
		HITBOX_PELVIS,
		HITBOX_UPPER_CHEST,
		HITBOX_CHEST,
		//HITBOX_LOWER_NECK,	thanks volvo
		HITBOX_LEFT_FOREARM,
		HITBOX_RIGHT_FOREARM,
		HITBOX_RIGHT_HAND,
		HITBOX_LEFT_THIGH,
		HITBOX_RIGHT_THIGH,
		HITBOX_LEFT_CALF,
		HITBOX_RIGHT_CALF,
		HITBOX_LEFT_FOOT,
		HITBOX_RIGHT_FOOT
	};

	int loopSize = ARRAYSIZE(hitboxesLoop);
	int closest_hitbox = -1;
	float closest_dist = 4096;
	for (int i = 0; i < loopSize; ++i)
	{
		float dist = g_LocalPlayer->GetEyePos().DistTo(player->GetHitboxPos(hitboxesLoop[i]));
		if (dist < closest_dist)
		{
			closest_dist = dist;
			closest_hitbox = hitboxesLoop[i];
		}
	}
	Ray_t ray;
	//CTraceFilter filter;
	CTraceFilterWorldAndPropsOnly filter;
	//filter.pSkip = g_LocalPlayer;
	trace_t trace;

	std::vector<Vector> vecArray;

	matrix3x4_t matrix[128]; if (!player->SetupBonesExperimental(matrix, 128, 256, g_EngineClient->GetLastTimeStamp())) return false;
	studiohdr_t *studioHdr = g_MdlInfo->GetStudiomodel(player->GetModel()); if (checks::is_bad_ptr(studioHdr)) return false;
	mstudiohitboxset_t *set = studioHdr->pHitboxSet(player->m_nHitboxSet()); if (checks::is_bad_ptr(set)) return false;
	mstudiobbox_t *hitbox = set->pHitbox(closest_hitbox); if (checks::is_bad_ptr(hitbox)) return false;

	float mod = hitbox->m_flRadius != -1.f ? hitbox->m_flRadius : 0.f;

	junkcode::call();

	Vector max; Vector min;
	Math::VectorTransform(hitbox->bbmax + mod, matrix[hitbox->bone], max);
	Math::VectorTransform(hitbox->bbmin - mod, matrix[hitbox->bone], min);

	auto center = (min + max) * 0.5f;

	QAngle curAngles = Math::CalcAngle(center, g_LocalPlayer->GetEyePos());
	Vector forward; Math::AngleVectors(curAngles, forward);

	Vector right = forward.Cross(Vector(0, 0, 1));
	Vector left = Vector(-right.x, -right.y, right.z);

	const float POINT_SCALE = g_Options.rage_pointscale;
	if (g_Options.rage_multipoint)
	{
		for (auto i = 0; i < 2; ++i)
		{
			vecArray.emplace_back(center);
		}
		vecArray[0] += right * (hitbox->m_flRadius * POINT_SCALE);
		vecArray[1] += left * (hitbox->m_flRadius * POINT_SCALE);
	}
	else
	{
		vecArray.push_back(center);
	}

	for (Vector cur : vecArray)
	{
		ray.Init(g_LocalPlayer->GetEyePos(), cur);
		g_EngineTrace->TraceRay(ray, MASK_SHOT_HULL, &filter, &trace);

		if (trace.fraction >= 1.0f)
		{
			out = cur;
			return true;
		}
	}
	return false;
}

void AimRage::KnifeBot()
{
	const float slash_range = 48.f, stab_range = 32.f;

	struct table_t
	{
		unsigned char swing[2][2][2];
		unsigned char stab[2][2];
	};
	static const table_t table = {
	{ { { 25,90 },{ 21,76 } },{ { 40,90 },{ 34,76 } } },
	{ { 65,180 },{ 55,153 } }
	};

	bool first = local_weapon->m_flNextPrimaryAttack() + 0.4f < GetTickbase() * g_GlobalVars->interval_per_tick;

	std::vector<int> targets;

	for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
	{
		if (!CheckTarget(i))
			continue;

		targets.push_back(i);
	}

	std::stable_sort(targets.begin(), targets.end(), [&](const int& a, const int& b)	//using stable sort for no reason other then i think it's faster?
		{
			junkcode::call();
			C_BasePlayer *aent = reinterpret_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(a)), *bent = reinterpret_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(b));
			return (g_LocalPlayer->GetAbsOrigin().DistTo(aent->GetAbsOrigin()) < g_LocalPlayer->GetAbsOrigin().DistTo(bent->GetAbsOrigin()));
		}
	);
	Global::AimTargets = targets.size();

	for (auto i = targets.begin(); i != targets.end(); i++)
	{
		auto player = C_BasePlayer::GetPlayerByIndex(*i);

		Vector AimTarget; if (!KnifeTrace(player, AimTarget)) continue;

		if (g_LocalPlayer->GetEyePos().DistTo(AimTarget) > slash_range)
			continue;

		Global::vecAimpos = AimTarget;

		bool armor = player->m_ArmorValue() > 0;
		bool back = IsBehind(g_LocalPlayer->m_vecOrigin(), player->m_vecOrigin(), player->GetAbsAngles());

		int stab_dmg = table.stab[armor][back];
		int slash_dmg = table.swing[false][armor][back];
		int swing_dmg = table.swing[first][armor][back];
		int health = player->m_iHealth();

		bool stab;
		if (health <= swing_dmg) stab = false;
		else if (g_LocalPlayer->GetEyePos().DistTo(AimTarget) <= stab_range)
		{
			if (health <= stab_dmg) stab = true;
			else if (health > (swing_dmg + slash_dmg + stab_dmg)) stab = true;
			else stab = false;
		}
		else stab = false;

		this->usercmd->viewangles = Global::vecVisualAimbotAngs = Math::CalcAngle(g_LocalPlayer->GetEyePos(), AimTarget);
		Global::bVisualAimbotting = true;
		Global::bAimbotting = true;
		usercmd->buttons |= (stab) ? (IN_ATTACK2) : (IN_ATTACK);
		Global::aimbot_target = player->EntIndex();
		break;
	}
}

bool AimRage::CanHit(int entindex, bool &hitchanced, QAngle &new_aim_angles) {

	if (!CheckTarget(entindex))	//just to double check cause i auctually crashed here before
		return false;

	auto ent = C_BasePlayer::GetPlayerByIndex(entindex);

	auto firedShots = g_LocalPlayer->m_iShotsFired();

	int iHitbox = realHitboxSpot[g_Options.rage_hitbox];

	Vector vecTarget = Vector(std::numeric_limits<float>::infinity(), 0, 0);

	// Disgusting ass codes, can't think of a cleaner way now though. FIX ME.

	matrix3x4_t matrix[128];
	if (!ent->SetupBonesExperimental(matrix, 128, 256, g_EngineClient->GetLastTimeStamp()))
		return false;

	float mindmg = g_Options.rage_mindmg_wep[0];
	auto weapinfo = local_weapon->GetWeapInfo();
	switch (weapinfo->weapon_type)
	{
	case WEAPONTYPE_PISTOL:
		if (local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
			mindmg = g_Options.rage_mindmg_wep[2];
		else
			mindmg = g_Options.rage_mindmg_wep[1];
		break;
	case WEAPONTYPE_RIFLE:
		switch (local_weapon->m_iItemDefinitionIndex())
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
	if (local_weapon->IsPistol())
	{
		if (local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
			mindmg = g_Options.rage_mindmg_wep[2];
		else
			mindmg = g_Options.rage_mindmg_wep[1];
	}
	else if (local_weapon->IsRifle())
	{
		mindmg = g_Options.rage_mindmg_wep[4];
	}
	else if (local_weapon->IsSniper())
	{
		switch (local_weapon->m_iItemDefinitionIndex())
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

	if (g_Options.rage_lagcompensation)
	{
		CMBacktracking::Get().RageBacktrack(ent, usercmd, vecTarget, hitchanced);
	}

	if (!vecTarget.IsValid())
	{
		if (Global::FakelagFixed[entindex]) usercmd->tick_count += Global::PlayersChockedPackets[entindex];

		Vector baimTarget = CalculateBestPoint(ent, HITBOX_PELVIS, mindmg, false, matrix, true);

		if ((g_Options.rage_autobaim && (firedShots > g_Options.rage_baim_after_x_shots || GetDamageVec(baimTarget, ent, HITBOX_PELVIS) > ent->m_iHealth())) || Global::bBaim[ent->EntIndex()]) {
			vecTarget = baimTarget;
		}
		else
			vecTarget = CalculateBestPoint(ent, iHitbox, mindmg, g_Options.rage_prioritize, matrix, false);
	}

	// Invalid target/no hitable points at all.
	if (!vecTarget.IsValid())
	{
		return false;
	}
	junkcode::call();

	Global::vecAimpos = vecTarget;

	if (g_Options.misc_smacbypass && !(g_Options.hvh_antiaim_y || g_Options.hvh_antiaim_x || g_Options.hvh_antiaim_legit))	//we're using p100 smac bypass and not using aa
	{
		new_aim_angles = this->usercmd->viewangles;
		QAngle target_angle = Math::CalcAngle(g_LocalPlayer->GetEyePos(), vecTarget) - (g_Options.rage_norecoil ? g_LocalPlayer->m_aimPunchAngle() * g_CVar->FindVar("weapon_recoil_scale")->GetFloat() : QAngle(0, 0, 0));
		QAngle delta = new_aim_angles - target_angle; Math::ClampAngles(delta);

		if (delta.Length() >= 10)	//angle delta is greater then 10 deagree, maximum value is 45 but just to be safe.
		{
			delay_firing = true;

			if (abs(delta.pitch) >= 10)
			{
				new_aim_angles.pitch += (delta.pitch > 0) ? -10 : 10;	//clamp maximum pitch change to 10
			}
			else
			{
				new_aim_angles.pitch = target_angle.pitch;		//we can safly lock to target on pitch axes
			}

			if (abs(delta.yaw) >= 10 - abs(delta.pitch))	//yaw delta is higher then 10
			{
				new_aim_angles.yaw += (delta.yaw > 0) ? -10 + abs(delta.pitch) : 10 - abs(delta.pitch);	//clamp maximum yaw change to 10
			}
			else
			{
				new_aim_angles.yaw = target_angle.yaw;		//we can safly lock to target on yaw axes
			}
		}
		else
		{
			delay_firing = false;
			new_aim_angles = target_angle;
		}
	}
	else
	{
		new_aim_angles = Math::CalcAngle(g_LocalPlayer->GetEyePos(), vecTarget) - (g_Options.rage_norecoil ? g_LocalPlayer->m_aimPunchAngle() * g_CVar->FindVar("weapon_recoil_scale")->GetFloat() : QAngle(0, 0, 0));
	}

	last_angle = new_aim_angles;

	return true;
}

bool AimRage::aimatTarget(int entindex, QAngle target, bool lagcomp_hitchan) {

	static long timestamp = 0;
	static int last_target = 0;
	this->usercmd->viewangles = Global::vecVisualAimbotAngs = target;
	Global::vecVisualAimbotAngs += (g_Options.removals_novisualrecoil ? g_LocalPlayer->m_aimPunchAngle() * 2.f : QAngle(0, 0, 0));
	Global::bVisualAimbotting = true;

	float hitchan = g_Options.rage_hitchance_wep[0];
	auto weapinfo = local_weapon->GetWeapInfo();
	switch (weapinfo->weapon_type)
	{
	case WEAPONTYPE_PISTOL:
		if (local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
			hitchan = g_Options.rage_hitchance_wep[2];
		else
			hitchan = g_Options.rage_hitchance_wep[1];
		break;
	case WEAPONTYPE_RIFLE:
		switch (local_weapon->m_iItemDefinitionIndex())
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
	if (local_weapon->IsPistol())
	{
		if (local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
			hitchan = g_Options.rage_hitchance_wep[2];
		else
			hitchan = g_Options.rage_hitchance_wep[1];
	}
	else if (local_weapon->IsRifle())
	{
		hitchan = g_Options.rage_hitchance_wep[4];
	}
	else if (local_weapon->IsSniper())
	{
		switch (local_weapon->m_iItemDefinitionIndex())
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
	if (this->can_fire_weapon || local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
	{
		// Save more fps by remembering to try the same entity again next time.
		last_target = Global::aimbot_target;
		prev_aimtarget = entindex;
		Global::aimbot_target = entindex;
		if (g_Options.rage_autoscope && g_LocalPlayer->m_hActiveWeapon().Get()->IsSniper() && g_LocalPlayer->m_hActiveWeapon().Get()->m_zoomLevel() == 0)
		{
			usercmd->buttons |= IN_ATTACK2;
			return false;
		}
		else if ((g_Options.rage_lagcompensation && lagcomp_hitchan) || (!lagcomp_hitchan && HitChance(usercmd->viewangles, C_BasePlayer::GetPlayerByIndex(entindex), hitchan)))
		{
			if (g_CVar->FindVar("sv_usercmd_custom_random_seed")->GetBool() && g_Options.rage_nospread)
			{
				Utils::RandomSeed(usercmd->random_seed);
				local_weapon->UpdateAccuracyPenalty();
				float fRand1 = Utils::RandomFloat(0.0f, 1.0f);
				float fRandPi1 = Utils::RandomFloat(0.0f, 2.0f * (float)M_PI);
				float fRand2 = Utils::RandomFloat(0.0f, 1.f);
				float fRandPi2 = Utils::RandomFloat(0.0f, 2.0f * (float)M_PI);
				if (local_weapon->GetClientClass()->m_ClassID == WEAPON_REVOLVER && usercmd->buttons & IN_ATTACK2)
				{
					fRand1 = 1.f - fRand1 * fRand1;
					fRand2 = 1.f - fRand2 * fRand2;
				}

				float fRandInaccuracy = fRand1 * local_weapon->GetInaccuracy();
				float fRandSpread = fRand2 * local_weapon->GetSpread();

				float fSpreadX = cos(fRandPi1) * fRandInaccuracy + cos(fRandPi2) * fRandSpread;
				float fSpreadY = sin(fRandPi1) * fRandInaccuracy + sin(fRandPi2) * fRandSpread;

				usercmd->viewangles.pitch += RAD2DEG(atan(sqrt(fSpreadX * fSpreadX + fSpreadY * fSpreadY)));
				usercmd->viewangles.roll = RAD2DEG(atan2(fSpreadX, fSpreadY));
			}

			if (g_Options.rage_autoshoot)
			{
				if (!delay_firing)
					usercmd->buttons |= IN_ATTACK;
				else
					return false;
			}
			Global::bAimbotting = true;

			return true;
		}
		else
		{
			experimentalAutoStop();
		}
		AutoCrouch();
	}
	return false;
}

float AimRage::BestHitPoint(C_BasePlayer *player, int prioritized, float minDmg, mstudiohitboxset_t *hitset, matrix3x4_t matrix[], Vector &vecOut)
{
	mstudiobbox_t *hitbox = hitset->pHitbox(prioritized);
	if (!hitbox)
		return 0.f;

	std::vector<Vector> vecArray;
	float flHigherDamage = 0.f;

	float mod = hitbox->m_flRadius != -1.f ? hitbox->m_flRadius : 0.f;
	junkcode::call();
	Vector max;
	Vector min;

	Math::VectorTransform(hitbox->bbmax + mod, matrix[hitbox->bone], max);
	Math::VectorTransform(hitbox->bbmin - mod, matrix[hitbox->bone], min);

	auto center = (min + max) * 0.5f;

	QAngle curAngles = Math::CalcAngle(center, g_LocalPlayer->GetEyePos());

	Vector forward;
	Math::AngleVectors(curAngles, forward);

	Vector right = forward.Cross(Vector(0, 0, 1));
	Vector left = Vector(-right.x, -right.y, right.z);

	Vector top = Vector(0, 0, 1);
	Vector bot = Vector(0, 0, -1);

	float POINT_SCALE = g_Options.rage_pointscale;
	if (g_Options.rage_multipoint)
	{
		switch (prioritized)
		{
		case HITBOX_HEAD:
			for (auto i = 0; i < 4; ++i)
			{
				vecArray.emplace_back(center);
			}
			vecArray[1] += top * (hitbox->m_flRadius * POINT_SCALE);
			vecArray[2] += right * (hitbox->m_flRadius * POINT_SCALE);
			vecArray[3] += left * (hitbox->m_flRadius * POINT_SCALE);
			break;

		default:

			for (auto i = 0; i < 2; ++i)
			{
				vecArray.emplace_back(center);
			}
			vecArray[0] += right * (hitbox->m_flRadius * POINT_SCALE);
			vecArray[1] += left * (hitbox->m_flRadius * POINT_SCALE);
			break;

		}
	}
	else
		vecArray.emplace_back(center);

	for (Vector cur : vecArray)
	{
		float flCurDamage = GetDamageVec(cur, player, prioritized);

		if (!flCurDamage)
			continue;

		if ((flCurDamage > flHigherDamage) && (flCurDamage > minDmg))
		{
			flHigherDamage = flCurDamage;
			vecOut = cur;
		}
	}
	return flHigherDamage;
}

Vector AimRage::CalculateBestPoint(C_BasePlayer *player, int prioritized, float minDmg, bool onlyPrioritized, matrix3x4_t matrix[], bool baim)
{

	studiohdr_t *studioHdr = g_MdlInfo->GetStudiomodel(player->GetModel());
	mstudiohitboxset_t *set = studioHdr->pHitboxSet(player->m_nHitboxSet());
	Vector vecOutput = Vector(std::numeric_limits<float>::infinity(), 0, 0);

	if (BestHitPoint(player, prioritized, minDmg, set, matrix, vecOutput) > minDmg && onlyPrioritized)
	{
		return vecOutput;
	}
	else
	{
		float flHigherDamage = 0.f;

		Vector vecCurVec;

		// why not use all the hitboxes then
		//static Hitboxes hitboxesLoop;
		static int hitboxesLoop[] =
		{
			HITBOX_HEAD,
			HITBOX_PELVIS,
			HITBOX_UPPER_CHEST,
			HITBOX_CHEST,
			//HITBOX_LOWER_NECK,	thanks volvo
			HITBOX_LEFT_FOREARM,
			HITBOX_RIGHT_FOREARM,
			HITBOX_RIGHT_HAND,
			HITBOX_LEFT_THIGH,
			HITBOX_RIGHT_THIGH,
			HITBOX_LEFT_CALF,
			HITBOX_RIGHT_CALF,
			HITBOX_LEFT_FOOT,
			HITBOX_RIGHT_FOOT
		};

		int loopSize = ARRAYSIZE(hitboxesLoop);

		for (int i = 0; i < loopSize; ++i)
		{
			if (!g_Options.rage_multiHitboxes[i])
				continue;

			if (baim && i == 0)
				continue;

			if (Global::AimTargets > 3 && (i == 6 || i == 5 || i == 18 || i == 16 || i == 8 || i == 7 || i == 10 || i == 9))
				continue;

			float flCurDamage = BestHitPoint(player, hitboxesLoop[i], minDmg, set, matrix, vecCurVec);

			if (!flCurDamage)
				continue;

			if (flCurDamage < g_Options.rage_mindmg)
				continue;

			if (flCurDamage > flHigherDamage)
			{
				flHigherDamage = flCurDamage;
				vecOutput = vecCurVec;
				aimhitbox = hitboxesLoop[i];
				Global::aim_hitbox = aimhitbox;
				if (static_cast<int32_t>(flHigherDamage) >= player->m_iHealth())
					break;
			}
		}
		return vecOutput;
	}
}

bool AimRage::CheckTarget(int i)
{
	junkcode::call();
	C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);

	if (!player || player == nullptr)
		return false;

	if (checks::is_bad_ptr(player))
		return false;

	if (player == g_LocalPlayer)
		return false;

	if (!g_Options.rage_friendlyfire || !g_CVar->FindVar("mp_friendlyfire")->GetBool())
		if (player->m_iTeamNum() == g_LocalPlayer->m_iTeamNum())
			return false;

	if (player->IsDormant())
		return false;

	if (player->m_bGunGameImmunity())
		return false;

	if (!player->IsAlive())
		return false;

	return true;
}

bool AimRage::HitChance(QAngle angles, C_BasePlayer *ent, float chance)
{
	if (g_CVar->FindVar("weapon_accuracy_nospread")->GetBool())
		return true;
	if (g_CVar->FindVar("sv_usercmd_custom_random_seed")->GetBool() && g_Options.rage_nospread)
		return true;

	auto weapon = g_LocalPlayer->m_hActiveWeapon().Get();

	if (!weapon)
		return false;

	Vector forward, right, up;
	Vector src = g_LocalPlayer->GetEyePos();
	Math::AngleVectors(angles, forward, right, up);

	int cHits = 0;
	int cNeededHits = static_cast<int>(30.f * (chance / 100.f));

	weapon->UpdateAccuracyPenalty();
	float weap_spread = weapon->GetSpread();
	float weap_inaccuracy = weapon->GetInaccuracy();

	for (int i = 0; i < 30; i++)
	{
		float a = Utils::RandomFloat(0.f, 1.f);
		float b = Utils::RandomFloat(0.f, 2.f * PI_F);
		float c = Utils::RandomFloat(0.f, 1.f);
		float d = Utils::RandomFloat(0.f, 2.f * PI_F);

		float inaccuracy = a * weap_inaccuracy;
		float spread = c * weap_spread;

		if (weapon->m_iItemDefinitionIndex() == 64)
		{
			a = 1.f - a * a;
			a = 1.f - c * c;
		}

		Vector spreadView((cos(b) * inaccuracy) + (cos(d) * spread), (sin(b) * inaccuracy) + (sin(d) * spread), 0), direction;

		direction.x = forward.x + (spreadView.x * right.x) + (spreadView.y * up.x);
		direction.y = forward.y + (spreadView.x * right.y) + (spreadView.y * up.y);
		direction.z = forward.z + (spreadView.x * right.z) + (spreadView.y * up.z);
		direction.Normalized();

		QAngle viewAnglesSpread;
		Math::VectorAngles(direction, up, viewAnglesSpread);
		Math::NormalizeAngles(viewAnglesSpread);

		Vector viewForward;
		Math::AngleVectors(viewAnglesSpread, viewForward);
		viewForward.NormalizeInPlace();

		viewForward = src + (viewForward * weapon->GetWeapInfo()->m_fRange);

		trace_t tr;
		Ray_t ray;

		ray.Init(src, viewForward);
		g_EngineTrace->ClipRayToEntity(ray, MASK_SHOT | CONTENTS_GRATE, ent, &tr);

		if (tr.hit_entity == ent)
			++cHits;

		Global::lasthc = ((static_cast<float>(cHits) / 30.f) * 100.f);

		if (static_cast<int>((static_cast<float>(cHits) / 30.f) * 100.f) >= chance)
			return true;

		//if ((150 - i + cHits) < cNeededHits)
		//	return false;
	}
	return false;
}

void AimRage::AutoStop()
{
	if (!g_Options.rage_autostop)
		return;

	if (g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind))
		return;

	if (!can_fire_weapon)
		return;

	Vector velocity = g_LocalPlayer->m_vecVelocity();
	Vector direction = velocity.Angle();
	float speed = velocity.Length();

	direction.y = usercmd->viewangles.yaw - direction.y;

	Vector negated_direction = direction * -speed;

	if (velocity.x < 25)
		negated_direction.x = 0;
	if (velocity.y < 25)
		negated_direction.y = 0;

	usercmd->forwardmove = negated_direction.x;
	usercmd->sidemove = negated_direction.y;
}

void AimRage::experimentalAutoStop()
{
	if (!g_Options.rage_autostop)
		return;
	if (g_LocalPlayer->GetMoveType() != MOVETYPE_WALK)
		return; // Not implemented otherwise :(
	if (g_LocalPlayer->m_hActiveWeapon()->m_iItemDefinitionIndex() == WEAPON_TASER)
		return;

	Vector hvel = g_LocalPlayer->m_vecVelocity();
	float speed = hvel.Length2D();

	if (speed < 1.f) // Will be clipped to zero anyways
	{
		usercmd->forwardmove = 0.f;
		usercmd->sidemove = 0.f;
		return;
	}

	// Homework: Get these dynamically //edit: done.
	float accel = g_CVar->FindVar("sv_accelerate")->GetFloat();
	float maxSpeed = g_CVar->FindVar("sv_maxspeed")->GetFloat();
	float playerSurfaceFriction = g_CVar->FindVar("sv_friction")->GetFloat() * g_LocalPlayer->m_surfaceFriction();
	float max_accelspeed = accel * g_GlobalVars->interval_per_tick * maxSpeed * playerSurfaceFriction;

	float wishspeed{};

	// Only do custom deceleration if it won't end at zero when applying max_accel
	// Gamemovement truncates speed < 1 to 0
	if (speed - max_accelspeed <= -1.f)
	{
		// We try to solve for speed being zero after acceleration:
		// speed - accelspeed = 0
		// speed - accel*frametime*wishspeed = 0
		// accel*frametime*wishspeed = speed
		// wishspeed = speed / (accel*frametime)
		// ^ Theoretically, that's the right equation, but it doesn't work as nice as 
		//   doing the reciprocal of that times max_accelspeed, so I'm doing that :shrug:
		wishspeed = max_accelspeed / (speed / (accel * g_GlobalVars->interval_per_tick));
	}
	else // Full deceleration, since it won't overshoot
	{
		// Or use max_accelspeed, doesn't matter
		wishspeed = max_accelspeed;
	}

	Vector direction = hvel.Angle();

	direction.y = usercmd->viewangles.yaw - direction.y;

	Vector negated_direction = direction * -wishspeed;

	if (negated_direction.Length2D() < 5.f)
	{
		usercmd->forwardmove = 0;
		usercmd->sidemove = 0;
	}
	else
	{
		usercmd->forwardmove = negated_direction.x;
		usercmd->sidemove = negated_direction.y;
	}
}

bool AimRage::CockRevolver()
{
	/*
	// 0.234375f to cock and shoot, 15 ticks in 64 servers, 30(31?) in 128

	// THIS DOESNT WORK, WILL WORK ON LATER AGAIN WHEN I FEEL LIKE KILLING MYSELF

	// DONT USE TIME_TO_TICKS as these values aren't good for it. it's supposed to be 0.2f but that's also wrong
	constexpr float REVOLVER_COCK_TIME = 0.2421875f;
	const int count_needed = floor(REVOLVER_COCK_TIME / g_GlobalVars->interval_per_tick);
	static int cocks_done = 0;

	if (!local_weapon ||
		local_weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER ||
		g_LocalPlayer->m_flNextAttack() > g_GlobalVars->curtime ||
		local_weapon->IsReloading())
	{
		if (local_weapon && local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
			usercmd->buttons &= ~IN_ATTACK;
		cocks_done = 0;
		return true;
	}

	if (cocks_done < count_needed)
	{
		usercmd->buttons |= IN_ATTACK;
		++cocks_done;
		return false;
	}
	else
	{
		usercmd->buttons &= ~IN_ATTACK;
		cocks_done = 0;
		return true;
	}

	// 0.0078125 - 128ticks - 31 - 0.2421875
	// 0.015625  - 64 ticks - 16 - 0.234375f

	usercmd->buttons |= IN_ATTACK;
	*/
	/*
		3 steps:

		1. Come, not time for update, cock and return false;

		2. Come, completely outdated, cock and set time, return false;

		3. Come, time is up, cock and return true;

		Notes:
			Will I not have to account for high ping when I shouldn't send another update?
			Lower framerate than ticks = riperino? gotta check if lower then account by sending earlier | frametime memes
	*/
	/*
	float curtime = TICKS_TO_TIME(g_LocalPlayer->m_nTickBase());
	static float next_shoot_time = 0.f;

	bool ret = false;

	if (next_shoot_time - curtime < -0.5)
		next_shoot_time = curtime + 0.2f - g_GlobalVars->interval_per_tick; // -1 because we already cocked THIS tick ???

	if (next_shoot_time - curtime - g_GlobalVars->interval_per_tick <= 0.f) {
		next_shoot_time = curtime + 0.2f;
		ret = true;

		// should still go for one more tick but if we do, we're gonna shoot sooo idk how2do rn, its late
		// the aimbot should decide whether to shoot or not yeh
	}

	return ret;
	*/
	if (!local_weapon ||
		local_weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER ||
		g_LocalPlayer->m_flNextAttack() > g_GlobalVars->curtime ||
		local_weapon->IsReloading() || Global::bAimbotting)
	{
		if (local_weapon && local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
			usercmd->buttons &= ~IN_ATTACK;
		revolver_cocks = -1;
		return true;
	}

	const float cock_time = 0.234375f;
	int cock_needed = cock_time / g_GlobalVars->interval_per_tick;
	
	revolver_cocks++;
	if (revolver_cocks < cock_needed && local_weapon->HasBullets())
	{
		usercmd->buttons |= IN_ATTACK;
		return false;
	}
	else
	{
		revolver_cocks = -1;
		usercmd->buttons &= ~IN_ATTACK;
		return true;
	}	//horrable hardcoded auto r8 meme but still better then notting
}

void AimRage::AutoCrouch()
{
	if (!g_Options.rage_autocrouch)
		return;

	if (g_LocalPlayer->m_hActiveWeapon()->m_iItemDefinitionIndex() == WEAPON_TASER)
		return;

	if(local_weapon->CanFire())
		usercmd->buttons |= IN_DUCK;
}

bool vischeck(int hitbox_to_check, C_BasePlayer* target) {
	matrix3x4_t boneMatrix[MAXSTUDIOBONES];
	Vector eyePos = g_LocalPlayer->GetEyePos();

	CGameTrace tr;
	Ray_t ray;
	CTraceFilter filter;
	filter.pSkip = g_LocalPlayer;

	if (!target->SetupBonesExperimental(boneMatrix, MAXSTUDIOBONES, BONE_USED_BY_HITBOX, 0.0f))
	{
		return false;
	}

	auto studio_model = g_MdlInfo->GetStudiomodel(target->GetModel());
	if (!studio_model)
	{
		return false;
	}
	auto hitbox = studio_model->pHitboxSet(target->m_nHitboxSet())->pHitbox(hitbox_to_check);
	if (hitbox)
	{
		auto
			min = Vector{},
			max = Vector{};

		Math::VectorTransform(hitbox->bbmin, boneMatrix[hitbox->bone], min);
		Math::VectorTransform(hitbox->bbmax, boneMatrix[hitbox->bone], max);

		ray.Init(eyePos, (min + max) * 0.5);
		g_EngineTrace->TraceRay(ray, MASK_SHOT | CONTENTS_GRATE, &filter, &tr);

		if (tr.hit_entity == target || tr.fraction > 0.97f)
			return true;
	}
	return false;
}

Autowall_Return_Info AimRage::GetDamageVec2(Vector start, Vector end, C_BasePlayer* from_entity, C_BasePlayer* to_entity, int specific_hitgroup)
{
	// default values for return info, in case we need to return abruptly
	Autowall_Return_Info return_info;
	return_info.damage = -1;
	return_info.hitgroup = -1;
	return_info.hit_entity = nullptr;
	return_info.penetration_count = 4;
	return_info.thickness = 0.f;
	return_info.did_penetrate_wall = false;

	Autowall_Info autowall_info;
	autowall_info.penetration_count = 4;
	autowall_info.start = start;
	autowall_info.end = end;
	autowall_info.current_position = start;
	autowall_info.thickness = 0.f;

	// direction 
	Math::AngleVectors(Math::CalcAngle(start, end), autowall_info.direction);

	// attacking entity
	if (!from_entity)
		from_entity = g_LocalPlayer;
	if (checks::is_bad_ptr(from_entity))
		return return_info;

	auto filter_player = CTraceFilterOneEntity();
	filter_player.pEntity = to_entity;
	auto filter_local = CTraceFilter();
	filter_local.pSkip = from_entity;

	// setup filters
	if (to_entity)
		autowall_info.filter = &filter_player;
	else
		autowall_info.filter = &filter_player;

	// weapon
	auto weapon = (C_BaseCombatWeapon*)(from_entity->m_hActiveWeapon());
	if (!weapon)
		return return_info;

	// weapon data
	auto weapon_info = weapon->GetWeapInfo();
	if (!weapon_info)
		return return_info;

	// client class
	auto weapon_client_class = reinterpret_cast<C_BaseEntity*>(weapon)->GetClientClass();
	if (!weapon_client_class)
		return return_info;

	// weapon range
	float range = min(weapon_info->m_fRange, (start - end).Length());
	end = start + (autowall_info.direction * range);
	autowall_info.current_damage = weapon_info->m_iDamage;

	while (autowall_info.current_damage > 0 && autowall_info.penetration_count > 0)
	{
		return_info.penetration_count = autowall_info.penetration_count;

		traceIt(autowall_info.current_position, end, MASK_SHOT | CONTENTS_GRATE, g_LocalPlayer, &autowall_info.enter_trace);
		ClipTraceToPlayers(autowall_info.current_position, autowall_info.current_position + autowall_info.direction * 40.f, MASK_SHOT | CONTENTS_GRATE, autowall_info.filter, &autowall_info.enter_trace);

		const float distance_traced = (autowall_info.enter_trace.endpos - start).Length();
		autowall_info.current_damage *= pow(weapon_info->m_fRangeModifier, (distance_traced / 500.f));

		/// if reached the end
		if (autowall_info.enter_trace.fraction == 1.f)
		{
			if (to_entity && specific_hitgroup != -1)
			{
				ScaleDamage(specific_hitgroup, to_entity, weapon_info->m_fArmorRatio, autowall_info.current_damage);

				return_info.damage = autowall_info.current_damage;
				return_info.hitgroup = specific_hitgroup;
				return_info.end = autowall_info.enter_trace.endpos;
				return_info.hit_entity = to_entity;
			}
			else
			{
				return_info.damage = autowall_info.current_damage;
				return_info.hitgroup = -1;
				return_info.end = autowall_info.enter_trace.endpos;
				return_info.hit_entity = nullptr;
			}

			break;
		}
		// if hit an entity
		if (autowall_info.enter_trace.hitgroup > 0 && autowall_info.enter_trace.hitgroup <= 7 && autowall_info.enter_trace.hit_entity)
		{
			C_BasePlayer* hit_player = reinterpret_cast<C_BasePlayer*>(autowall_info.enter_trace.hit_entity);
			// checkles gg
			if ((to_entity && autowall_info.enter_trace.hit_entity != to_entity) ||
				(hit_player->m_iTeamNum() == from_entity->m_iTeamNum()))
			{
				return_info.damage = -1;
				return return_info;
			}

			if (specific_hitgroup != -1)
				ScaleDamage(specific_hitgroup, hit_player, weapon_info->m_fArmorRatio, autowall_info.current_damage);
			else
				ScaleDamage(autowall_info.enter_trace.hitgroup, hit_player, weapon_info->m_fArmorRatio, autowall_info.current_damage);

			// fill the return info
			return_info.damage = autowall_info.current_damage;
			return_info.hitgroup = autowall_info.enter_trace.hitgroup;
			return_info.end = autowall_info.enter_trace.endpos;
			return_info.hit_entity = reinterpret_cast<C_BaseEntity*>(autowall_info.enter_trace.hit_entity);

			break;
		}

		// break out of the loop retard
		//if (!CanPenetrate(from_entity, autowall_info, weapon_info))
		FireBulletData data;
		data.current_damage = autowall_info.current_damage, data.direction = autowall_info.direction, data.enter_trace = autowall_info.enter_trace, data.filter = *autowall_info.filter,
			data.penetrate_count = autowall_info.penetration_count, data.src = autowall_info.start, data.trace_length = autowall_info.thickness;
		if(!HandleBulletPenetration(weapon_info, data))
			break;

		autowall_info.current_damage = data.current_damage, autowall_info.direction = data.direction, autowall_info.enter_trace = data.enter_trace, *autowall_info.filter = data.filter,
			autowall_info.penetration_count = data.penetrate_count, autowall_info.start= data.src, autowall_info.thickness = data.trace_length;

		return_info.did_penetrate_wall = true;
	}

	return_info.penetration_count = autowall_info.penetration_count;

	return return_info;
}

float AimRage::GetDamageVec(const Vector &vecPoint, C_BasePlayer *player, int hitbox)
{
	auto GetHitgroup = [](int hitbox) -> int
	{
		switch (hitbox)
		{
		case HITBOX_HEAD:
		case HITBOX_NECK:
			//case HITBOX_LOWER_NECK:
			return HITGROUP_HEAD;
		case HITBOX_LOWER_CHEST:
		case HITBOX_CHEST:
		case HITBOX_UPPER_CHEST:
			return HITGROUP_CHEST;
		case HITBOX_STOMACH:
		case HITBOX_PELVIS:
			return HITGROUP_STOMACH;
		case HITBOX_LEFT_HAND:
		case HITBOX_LEFT_UPPER_ARM:
		case HITBOX_LEFT_FOREARM:
			return HITGROUP_LEFTARM;
		case HITBOX_RIGHT_HAND:
		case HITBOX_RIGHT_UPPER_ARM:
		case HITBOX_RIGHT_FOREARM:
			return HITGROUP_RIGHTARM;
		case HITBOX_LEFT_THIGH:
		case HITBOX_LEFT_CALF:
		case HITBOX_LEFT_FOOT:
			return HITGROUP_LEFTLEG;
		case HITBOX_RIGHT_THIGH:
		case HITBOX_RIGHT_CALF:
		case HITBOX_RIGHT_FOOT:
			return HITGROUP_RIGHTLEG;
		default:
			return -1;
		}
	};

	float damage = 0.f;

	Vector rem = vecPoint;

	FireBulletData data;

	data.src = g_LocalPlayer->GetEyePos();
	data.filter.pSkip = g_LocalPlayer;

	QAngle angle = Math::CalcAngle(data.src, rem);
	Math::AngleVectors(angle, data.direction);

	data.direction.Normalized();

	auto weap = g_LocalPlayer->m_hActiveWeapon().Get();

	if (weap->GetWeapInfo() == nullptr)
		return 0;

	if (SimulateFireBullet(weap, data, player, hitbox))
		damage = data.current_damage;

	Global::lastawdmg = damage;
	Global::lastdmg[player->EntIndex()] = damage;

	return damage;
}

bool AimRage::SimulateFireBullet(C_BaseCombatWeapon *weap, FireBulletData &data, C_BasePlayer *player, int hitbox)
{
	if (!weap)
		return false;

	auto GetHitgroup = [](int hitbox) -> int
	{
		switch (hitbox)
		{
		case HITBOX_HEAD:
		case HITBOX_NECK:
		//case HITBOX_LOWER_NECK:
			return HITGROUP_HEAD;
		case HITBOX_LOWER_CHEST:
		case HITBOX_CHEST:
		case HITBOX_UPPER_CHEST:
			return HITGROUP_CHEST;
		case HITBOX_STOMACH:
		case HITBOX_PELVIS:
			return HITGROUP_STOMACH;
		case HITBOX_LEFT_HAND:
		case HITBOX_LEFT_UPPER_ARM:
		case HITBOX_LEFT_FOREARM:
			return HITGROUP_LEFTARM;
		case HITBOX_RIGHT_HAND:
		case HITBOX_RIGHT_UPPER_ARM:
		case HITBOX_RIGHT_FOREARM:
			return HITGROUP_RIGHTARM;
		case HITBOX_LEFT_THIGH:
		case HITBOX_LEFT_CALF:
		case HITBOX_LEFT_FOOT:
			return HITGROUP_LEFTLEG;
		case HITBOX_RIGHT_THIGH:
		case HITBOX_RIGHT_CALF:
		case HITBOX_RIGHT_FOOT:
			return HITGROUP_RIGHTLEG;
		default:
			return -1;
		}
	};

	data.penetrate_count = 4;
	data.trace_length = 0.0f;
	WeapInfo_t *weaponData = weap->GetWeapInfo();

	if (weaponData == NULL)
		return false;

	data.current_damage = (float)weaponData->m_iDamage;

	while ((data.penetrate_count > 0) && (data.current_damage >= 1.0f))
	{
		data.trace_length_remaining = weaponData->m_fRange - data.trace_length;

		Vector end = data.src + data.direction * data.trace_length_remaining;

		traceIt(data.src, end, MASK_SHOT | CONTENTS_GRATE, g_LocalPlayer, &data.enter_trace);
		ClipTraceToPlayers(data.src, end + data.direction * 40.f, MASK_SHOT | CONTENTS_GRATE, &data.filter, &data.enter_trace);

		if (data.enter_trace.fraction == 1.0f)
		{
			if (player && (!(player->m_fFlags() & FL_ONGROUND)))
			{
				data.enter_trace.hitgroup = GetHitgroup(hitbox);
				data.enter_trace.hit_entity = player;
			}
			else
				break;
		}

		surfacedata_t *enter_surface_data = g_PhysSurface->GetSurfaceData(data.enter_trace.surface.surfaceProps);
		unsigned short enter_material = enter_surface_data->game.material;
		float enter_surf_penetration_mod = enter_surface_data->game.flPenetrationModifier;

		data.trace_length += data.enter_trace.fraction * data.trace_length_remaining;
		data.current_damage *= pow(weaponData->m_fRangeModifier, data.trace_length * 0.002);

		if (data.trace_length > weaponData->m_fRange && weaponData->m_fPenetration > 0.f || enter_surf_penetration_mod < 0.1f)
			break;

		if ((data.enter_trace.hitgroup <= 7) && (data.enter_trace.hitgroup > 0))
		{
			C_BasePlayer *pPlayer = reinterpret_cast<C_BasePlayer*>(data.enter_trace.hit_entity);
			if (pPlayer->IsPlayer() && (pPlayer->m_iTeamNum() == g_LocalPlayer->m_iTeamNum()))
				return false;

			ScaleDamage(data.enter_trace.hitgroup, pPlayer, weaponData->m_fArmorRatio, data.current_damage);

			return true;
		}

		if (!HandleBulletPenetration(weaponData, data))
			break;
	}

	return false;
}

bool AimRage::HandleBulletPenetration(WeapInfo_t *wpn_data, FireBulletData &data)
{
	bool bSolidSurf = ((data.enter_trace.contents >> 3) & CONTENTS_SOLID);
	bool bLightSurf = (data.enter_trace.surface.flags >> 7) & SURF_LIGHT;

	surfacedata_t *enter_surface_data = g_PhysSurface->GetSurfaceData(data.enter_trace.surface.surfaceProps);
	unsigned short enter_material = enter_surface_data->game.material;
	float enter_surf_penetration_mod = enter_surface_data->game.flPenetrationModifier;

	if (!data.penetrate_count && !bLightSurf && !bSolidSurf && enter_material != 89)
	{
		if (enter_material != 71)
			return false;
	}

	if (data.penetrate_count <= 0 || wpn_data->m_fPenetration <= 0.f)
		return false;

	Vector dummy;
	trace_t trace_exit;

	if (!TraceToExit(dummy, &data.enter_trace, data.enter_trace.endpos, data.direction, &trace_exit))
	{
		if (!(g_EngineTrace->GetPointContents(dummy, MASK_SHOT_HULL) & MASK_SHOT_HULL))
			return false;
	}

	surfacedata_t *exit_surface_data = g_PhysSurface->GetSurfaceData(trace_exit.surface.surfaceProps);
	unsigned short exit_material = exit_surface_data->game.material;

	float exit_surf_penetration_mod = exit_surface_data->game.flPenetrationModifier;
	float exit_surf_damage_mod = exit_surface_data->game.flDamageModifier;

	float final_damage_modifier = 0.16f;
	float combined_penetration_modifier = 0.0f;

	if (enter_material == 89 || enter_material == 71)
	{
		combined_penetration_modifier = 3.0f;
		final_damage_modifier = 0.05f;
	}
	else if (bSolidSurf || bLightSurf)
	{
		combined_penetration_modifier = 1.0f;
		final_damage_modifier = 0.16f;
	}
	else
	{
		combined_penetration_modifier = (enter_surf_penetration_mod + exit_surf_penetration_mod) * 0.5f;
	}

	if (enter_material == exit_material)
	{
		if (exit_material == 87 || exit_material == 85)
			combined_penetration_modifier = 3.0f;
		else if (exit_material == 76)
			combined_penetration_modifier = 2.0f;
	}

	float modifier = fmaxf(0.0f, 1.0f / combined_penetration_modifier);
	float thickness = (trace_exit.endpos - data.enter_trace.endpos).LengthSqr();
	float taken_damage = ((modifier * 3.0f) * fmaxf(0.0f, (3.0f / wpn_data->m_fPenetration) * 1.25f) + (data.current_damage * final_damage_modifier)) + ((thickness * modifier) / 24.0f);

	float lost_damage = fmaxf(0.0f, taken_damage);

	if (lost_damage > data.current_damage)
		return false;

	if (lost_damage > 0.0f)
		data.current_damage -= lost_damage;

	if (data.current_damage < 1.0f)
		return false;

	data.src = trace_exit.endpos;
	data.penetrate_count--;

	return true;
}

bool AimRage::TraceToExit(Vector &end, CGameTrace *enter_trace, Vector start, Vector dir, CGameTrace *exit_trace)
{
	auto distance = 0.0f;
	int first_contents = 0;

	while (distance <= 90.0f)
	{
		distance += 4.0f;
		end = start + (dir * distance);

		if(!first_contents)
			first_contents = g_EngineTrace->GetPointContents(end, MASK_SHOT_HULL | CONTENTS_HITBOX);

		auto point_contents = g_EngineTrace->GetPointContents(end, MASK_SHOT_HULL | CONTENTS_HITBOX);

		if (point_contents & MASK_SHOT_HULL && (!(point_contents & CONTENTS_HITBOX) || first_contents == point_contents))
			continue;

		auto new_end = end - (dir * 4.0f);

		traceIt(end, new_end, MASK_SHOT | CONTENTS_GRATE, nullptr, exit_trace);

		if (exit_trace->startsolid && (exit_trace->surface.flags & SURF_HITBOX) < 0)
		{
			traceIt(end, start, MASK_SHOT_HULL, reinterpret_cast<C_BasePlayer*>(exit_trace->hit_entity), exit_trace);

			if (exit_trace->DidHit() && !exit_trace->startsolid)
			{
				end = exit_trace->endpos;
				return true;
			}
			continue;
		}

		if (!exit_trace->DidHit() || exit_trace->startsolid)
		{
			if (enter_trace->hit_entity)
			{
				if (enter_trace->DidHitNonWorldEntity() && IsBreakableEntity(reinterpret_cast<C_BasePlayer*>(enter_trace->hit_entity)))
				{
					*exit_trace = *enter_trace;
					exit_trace->endpos = start + dir;
					return true;
				}
			}
			continue;
		}

		if ((exit_trace->surface.flags >> 7) & SURF_LIGHT)
		{
			if (IsBreakableEntity(reinterpret_cast<C_BasePlayer*>(exit_trace->hit_entity)) && IsBreakableEntity(reinterpret_cast<C_BasePlayer*>(enter_trace->hit_entity)))
			{
				end = exit_trace->endpos;
				return true;
			}

			if (!((enter_trace->surface.flags >> 7) & SURF_LIGHT))
				continue;
		}

		if (exit_trace->plane.normal.Dot(dir) <= 1.0f)
		{
			float fraction = exit_trace->fraction * 4.0f;
			end = end - (dir * fraction);

			return true;
		}
	}
	return false;
}

bool AimRage::IsBreakableEntity(C_BasePlayer *ent)
{
	typedef bool(__thiscall *isBreakbaleEntityFn)(C_BasePlayer*);
	static isBreakbaleEntityFn IsBreakableEntityFn = (isBreakbaleEntityFn)Utils::PatternScan(GetModuleHandle((Global::bIsPanorama ? "client_panorama.dll" : "client.dll")), "55 8B EC 51 56 8B F1 85 F6 74 68");

	if (IsBreakableEntityFn)
	{
		// 0x27C = m_takedamage

		auto backupval = *reinterpret_cast<int*>((uint32_t)ent + 0x27C);
		auto className = ent->GetClientClass()->m_pNetworkName;

		if (ent != g_EntityList->GetClientEntity(0))
		{
			// CFuncBrush:
			// (className[1] != 'F' || className[4] != 'c' || className[5] != 'B' || className[9] != 'h')
			if ((className[1] == 'B' && className[9] == 'e' && className[10] == 'S' && className[16] == 'e') // CBreakableSurface
				|| (className[1] != 'B' || className[5] != 'D')) // CBaseDoor because fuck doors
			{
				*reinterpret_cast<int*>((uint32_t)ent + 0x27C) = 2;
			}
		}

		bool retn = IsBreakableEntityFn(ent);

		*reinterpret_cast<int*>((uint32_t)ent + 0x27C) = backupval;

		return retn;
	}
	else
		return false;
}

void AimRage::ClipTraceToPlayers(const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, ITraceFilter *filter, CGameTrace *tr)
{
	trace_t playerTrace;
	Ray_t ray;
	float smallestFraction = tr->fraction;
	const float maxRange = 60.0f;

	ray.Init(vecAbsStart, vecAbsEnd);

	{
		for (int i = 1; i <= g_GlobalVars->maxClients; i++)
		{
			C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);

			if (!player || !player->IsAlive() || player->IsDormant())
				continue;

			if (filter && filter->ShouldHitEntity(player, mask) == false)
				continue;

			float range = Math::DistanceToRay(player->WorldSpaceCenter(), vecAbsStart, vecAbsEnd);
			if (range < 0.0f || range > maxRange)
				continue;

			g_EngineTrace->ClipRayToEntity(ray, mask | CONTENTS_HITBOX, player, &playerTrace);
			if (playerTrace.fraction < smallestFraction)
			{
				*tr = playerTrace;
				smallestFraction = playerTrace.fraction;
			}
		}
	}
}

void AimRage::ScaleDamage(int hitgroup, C_BasePlayer *player, float weapon_armor_ratio, float &current_damage)
{
	bool heavArmor = player->m_bHasHeavyArmor();
	int armor = player->m_ArmorValue();

	switch (hitgroup)
	{
	case HITGROUP_HEAD:

		if (heavArmor)
			current_damage *= (current_damage * 4.f) * 0.5f;
		else
			current_damage *= 4.f;

		break;

	case HITGROUP_CHEST:
	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:

		current_damage *= 1.f;
		break;

	case HITGROUP_STOMACH:

		current_damage *= 1.25f;
		break;

	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:

		current_damage *= 0.75f;
		break;
	}

	if (IsArmored(player, armor, hitgroup))
	{
		float v47 = 1.f, armor_bonus_ratio = 0.5f, armor_ratio = weapon_armor_ratio * 0.5f;

		if (heavArmor)
		{
			armor_bonus_ratio = 0.33f;
			armor_ratio = (weapon_armor_ratio * 0.5f) * 0.5f;
			v47 = 0.33f;
		}

		float new_damage = current_damage * armor_ratio;

		if (heavArmor)
			new_damage *= 0.85f;

		if (((current_damage - (current_damage * armor_ratio)) * (v47 * armor_bonus_ratio)) > armor)
			new_damage = current_damage - (armor / armor_bonus_ratio);

		current_damage = new_damage;
	}
}

bool AimRage::IsArmored(C_BasePlayer *player, int armorVal, int hitgroup)
{
	bool res = false;

	if (armorVal > 0)
	{
		switch (hitgroup)
		{
		case HITGROUP_GENERIC:
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:

			res = true;
			break;

		case HITGROUP_HEAD:

			res = player->m_bHasHelmet();
			break;

		}
	}

	return res;
}

void AimRage::traceIt(Vector &vecAbsStart, Vector &vecAbsEnd, unsigned int mask, C_BasePlayer *ign, CGameTrace *tr)
{
	Ray_t ray;

	CTraceFilter filter;
	filter.pSkip = ign;

	ray.Init(vecAbsStart, vecAbsEnd);

	g_EngineTrace->TraceRay(ray, mask, &filter, tr);
}

int AimRage::GetTickbase(CUserCmd* ucmd) {

	static int g_tick = 0;
	static CUserCmd* g_pLastCmd = nullptr;

	if (!ucmd)
		return g_tick;

	if (!g_pLastCmd || g_pLastCmd->hasbeenpredicted) {
		g_tick = g_LocalPlayer->m_nTickBase();
	}
	else
	{
		// Required because prediction only runs on frames, not ticks
		// So if your framerate goes below tickrate, m_nTickBase won't update every tick
		++g_tick;
	}

	g_pLastCmd = ucmd;
}