#include "AntiAim.hpp"

#include "../Structs.hpp"
#include "../Options.hpp"

#include "AimRage.hpp"
#include "Resolver.hpp"
#include "RebuildGameMovement.hpp"
#include "Miscellaneous.hpp"
#include "PredictionSystem.hpp"
#include "Animation.hpp"

#include "../helpers/Utils.hpp"
#include "../helpers/Math.hpp"

#include "../lua/lua.hpp"

#include <time.h>

void AntiAim::Work(CUserCmd *usercmd)
{
	if (g_Options.misc_smacbypass)
		return;
	g_Options.hvh_resolver_experimental = true;
	junkcode::call();
	this->usercmd = usercmd;

	if (usercmd->buttons & IN_USE)
		return;

	auto weapon = g_LocalPlayer->m_hActiveWeapon().Get();

	if (!weapon)
		return;

	if (weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
	{
		if (usercmd->buttons & IN_ATTACK2)
			return;

		if (weapon->CanFirePostPone() && (usercmd->buttons & IN_ATTACK))
			return;
	}
	else if (weapon->IsGrenade())
	{
		if (weapon->IsInThrow())
			return;
	}
	else
	{
		if (((weapon->m_iItemDefinitionIndex() >= WEAPON_KNIFE_BAYONET && weapon->m_iItemDefinitionIndex() < GLOVE_STUDDED_BLOODHOUND) //dirty but it works
			|| weapon->m_iItemDefinitionIndex() == WEAPON_KNIFE_T || weapon->m_iItemDefinitionIndex() == WEAPON_KNIFE_CT)
			&& ((usercmd->buttons & IN_ATTACK) || (usercmd->buttons & IN_ATTACK2)))
			return;
		else if ((usercmd->buttons & IN_ATTACK) && (weapon->m_iItemDefinitionIndex() != WEAPON_C4 || (g_Options.hvh_antiaim_x != AA_PITCH_OFF && weapon->CanFire())))
			return;
	}

	if ((g_LocalPlayer->GetMoveType() == MOVETYPE_NOCLIP || g_LocalPlayer->GetMoveType() == MOVETYPE_LADDER) && (usercmd->forwardmove || usercmd->sidemove))
		return;
	junkcode::call();
	static bool calls = true;
	static int lastcnt = 0;

	if (!Global::bFakelag)
		Global::bSendPacket = !Global::last_packet;
		//Global::bSendPacket = usercmd->command_number % 2;

	/*
	if (!Global::bSendPacket && g_Options.hvh_antiaim_lby_breaker && (g_LocalPlayer->m_vecVelocity().Length2D() < 10 && g_LocalPlayer->m_fFlags() & FL_ONGROUND))
	{
		if (m_bBreakLowerBody && g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind))
		{
			//Global::bSendPacket = true;
			//usercmd->viewangles.yaw += 114.0f;
			//usercmd->viewangles.yaw = GetFakeYaw() + Utils::RandomFloat(-50, 50);
			//usercmd->viewangles.yaw = usercmd->tick_count % 2 ? GetYaw() + Utils::RandomFloat(100, 140) : GetYaw() - Utils::RandomFloat(100, 140);
			//usercmd->viewangles.yaw += Utils::RandomFloat(50, 150);
			//if(sqrt(pow(Global::realyaw-usercmd->viewangles.yaw,2)) < 35)
			//	usercmd->viewangles.yaw += Utils::RandomFloat(50, 150);
			//usercmd->viewangles.yaw = GetYaw() + Utils::RandomFloat(-150, 150);
			//if (pow(GetYaw() - usercmd->viewangles.yaw, 2) < 1225)
			//	usercmd->viewangles.yaw += 180;
			//float lbydelta = g_Options.hvh_antiaim_lby_delta ? g_Options.hvh_antiaim_lby_delta : Utils::RandomFloat(50, 160);

			//QAngle viewang;
			//g_EngineClient->GetViewAngles(viewang);
			//usercmd->viewangles.yaw = viewang.yaw + lbydelta;
		}
	}
	*/
	junkcode::call();
	if (g_Options.hvh_antiaim_new)
	{
		usercmd->viewangles.pitch = GetPitchNew();
		usercmd->viewangles.yaw = GetYawNew();
	}
	else
	{
		usercmd->viewangles.pitch = GetPitch();

		float tempyaw_fake = GetFakeYaw();
		static float tempyaw_real = 0;
		if (!Global::bSendPacket)
		{
			tempyaw_real = GetYaw();
			usercmd->viewangles.yaw = tempyaw_real;
		}
		else
			usercmd->viewangles.yaw = tempyaw_fake;
	}

	FakeMove(usercmd);
	static float air_toadd = 0;
	static float lby_delta = 0;

	if (!(g_LocalPlayer->m_fFlags() & FL_ONGROUND)) {
		switch (g_Options.hvh_antiaim_y_onair) {
		case AA_ONAIR_OFF:
			break;
		case AA_ONAIR_SPIN:
			air_toadd += Utils::RandomFloat(0,10);	//lol
			usercmd->viewangles.yaw += air_toadd;
			break;
		case AA_ONAIR_180z:
			usercmd->viewangles.yaw += 90 + fmod(g_GlobalVars->curtime / 2.0f * 130.0f, 90.0f);
			break;
		case AA_ONAIR_FLIP:
			usercmd->viewangles.yaw += 180;
			break;
		case AA_ONAIR_EVADE:
			if (!Global::bSendPacket)
			{
				lby_delta = std::fabsf(Math::ClampYaw(Math::ClampYaw(usercmd->viewangles.yaw) - g_LocalPlayer->m_flLowerBodyYawTarget()));
				if (lby_delta < 135)
				{
					usercmd->viewangles.yaw += 180;
				}
			}
			break;
		}
	}
	else
		air_toadd = 0;
}

void AntiAim::FakeMove(CUserCmd* cmd)
{
	if (g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind) && g_Options.misc_fakewalk) // we dont want to be fakewalking and doing this too, very bad things will happen.
		return;

	if (abs(cmd->forwardmove) > 1 || abs(cmd->sidemove) > 1)
		return;

	Vector velocity = Global::vecUnpredictedVel; // get our current velocity.

	if (velocity.Length2D() < 0.1f) // if we're not moving why would we try to do this?
		return;

	int32_t ticks_to_stop; // lets predict how many ticks until we stop for later.
	for (ticks_to_stop = 0; ticks_to_stop < 15; ticks_to_stop++)
	{
		if (velocity.Length2D() < 0.1f)
			break;

		if (g_LocalPlayer->m_fFlags() & FL_ONGROUND)
		{
			velocity[2] = 0.0f;
			Friction(velocity);
			WalkMove(g_LocalPlayer, velocity);
		}
	}

	const int32_t max_ticks = g_Options.misc_fakelag_value;
	const int32_t chocked = Miscellaneous::Get().GetChocked();
	int32_t ticks_left = max_ticks - chocked;

	if (ticks_to_stop < ticks_left && !Global::bSendPacket) // this will be our final fakelag "cycle" meaning this is the last time we'll be able to update
	{														// update our real before stopping, without fucking up our fakelag value ofc.

															// there is something i removed right here that is not necessary for the
															// functionality of this, but it is nice to have and you'll probably guess
															// what it is after stopping close to the edge of some corners a few
															// times with this enabled. think freestanding, proper freestanding.
		antiresolve = true;
	}
}

void AntiAim::FrameStageNotify(ClientFrameStage_t stage)
{
	if (stage != ClientFrameStage_t::FRAME_NET_UPDATE_POSTDATAUPDATE_START)
		return;
	junkcode::call();
	if (!g_LocalPlayer->IsAlive())
		return;

	static float lastlby = 0;

	auto flServerTime = TICKS_TO_TIME(AimRage::Get().GetTickbase());

	auto bIsOnGround = (g_LocalPlayer->m_fFlags() & FL_ONGROUND);
	auto bIsMoving = (g_LocalPlayer->m_vecVelocity().Length2D() > 0.1f);

	m_angEyeAngles = g_LocalPlayer->m_angEyeAngles();

	m_flLowerBody = g_LocalPlayer->m_flLowerBodyYawTarget();

	auto flLowerBodySnap = std::abs(Math::ClampYaw(m_angEyeAngles[1] - m_flLowerBody));

	if (!Global::bSendPacket)	// lby doesnt update while choking packets
	{
		m_bBreakLowerBody = false;
	}
	else if (bIsOnGround && bIsMoving)	// we're moving
	{
		m_bBreakLowerBody = true;

		m_flLowerBodyNextUpdate = 0.22f;
		m_flLowerBodyLastUpdate = flServerTime;
	}
	else if (bIsOnGround && ( flLowerBodySnap > 35.f ) && (flServerTime > (m_flLowerBodyLastUpdate + m_flLowerBodyNextUpdate)))		// standing still
	{
		m_bBreakLowerBody = true;

		m_flLowerBodyNextUpdate = 1.1f;
		m_flLowerBodyLastUpdate = flServerTime;
	}
	else
	{
		m_bBreakLowerBody = false;
		if (!Math::compfloat(lastlby, g_LocalPlayer->m_flLowerBodyYawTarget())) {	// breaker failed but lby still updated, take note of current time for next break
			m_flLowerBodyLastUpdate = flServerTime;
		}
	}
	junkcode::call();
	lastlby = g_LocalPlayer->m_flLowerBodyYawTarget();
}

/*void AntiAim::UpdateLBYBreaker(CUserCmd *usercmd)
{
	bool
		allocate = (m_serverAnimState == nullptr),
		change = (!allocate) && (&g_LocalPlayer->GetRefEHandle() != m_ulEntHandle),
		reset = (!allocate && !change) && (g_LocalPlayer->m_flSpawnTime() != m_flSpawnTime);

	// player changed, free old animation state.
	if (change)
		g_pMemAlloc->Free(m_serverAnimState);

	// need to reset? (on respawn)
	if (reset)
	{
		// reset animation state.
		C_BasePlayer::ResetAnimationState(m_serverAnimState);

		// note new spawn time.
		m_flSpawnTime = g_LocalPlayer->m_flSpawnTime();
	}

	// need to allocate or create new due to player change.
	if (allocate || change)
	{
		// only works with games heap alloc.
		C_CSGOPlayerAnimState *state = (C_CSGOPlayerAnimState*)g_pMemAlloc->Alloc(sizeof(C_CSGOPlayerAnimState));

		if (state != nullptr)
			g_LocalPlayer->CreateAnimationState(state);

		// used to detect if we need to recreate / reset.
		m_ulEntHandle = const_cast<CBaseHandle*>(&g_LocalPlayer->GetRefEHandle());
		m_flSpawnTime = g_LocalPlayer->m_flSpawnTime();

		// note anim state for future use.
		m_serverAnimState = state;
	}

	float_t curtime = TICKS_TO_TIME(AimRage::Get().GetTickbase());
	if (!g_ClientState->chokedcommands && m_serverAnimState)
	{
		C_BasePlayer::UpdateAnimationState(m_serverAnimState, usercmd->viewangles);

		// calculate delta.
		float_t delta = std::abs(Math::ClampYaw(usercmd->viewangles.yaw - g_LocalPlayer->m_flLowerBodyYawTarget()));

		// walking, delay next update by .22s.
		if (m_serverAnimState->m_flVelocity() > 0.1f && (g_LocalPlayer->m_fFlags() & FL_ONGROUND))
			m_flNextBodyUpdate = curtime + 0.22f;

		else if (curtime >= m_flNextBodyUpdate)
		{
			if (delta > 35.f)
				; // server will update lby.

			m_flNextBodyUpdate = curtime + 1.1f;
		}
	}

	// if was jumping and then onground and bsendpacket true, we're gonna update.
	m_bBreakLowerBody = (g_LocalPlayer->m_fFlags() & FL_ONGROUND) && ((m_flNextBodyUpdate - curtime) <= g_GlobalVars->interval_per_tick);
}*/

void AntiAim::UpdateLowerBodyBreaker(const QAngle& angles)
{
	if (!g_LocalPlayer)
		return;

	auto allocate = (m_serverAnimState == nullptr);
	auto change = (!allocate) && (g_LocalPlayer->GetRefEHandle() != *m_ulEntHandle);
	auto reset = (!allocate && !change) && (g_LocalPlayer->m_flSpawnTime() != m_flSpawnTime);

	// player changed, free old animation state.
	if (change)
		g_pMemAlloc->Free(m_serverAnimState);
	junkcode::call();
	if (reset)
	{
		// reset animation state.
		C_BasePlayer::ResetAnimationState(m_serverAnimState);

		// note new spawn time.
		m_flSpawnTime = g_LocalPlayer->m_flSpawnTime();
	}

	if (allocate || change)
	{
		// only works with games heap alloc.
		C_CSGOPlayerAnimState *state = (C_CSGOPlayerAnimState*)g_pMemAlloc->Alloc(sizeof(C_CSGOPlayerAnimState));

		if (state != nullptr)
			g_LocalPlayer->CreateAnimationState(state);

		// used to detect if we need to recreate / reset.
		m_ulEntHandle = const_cast<CBaseHandle*>(&g_LocalPlayer->GetRefEHandle());
		m_flSpawnTime = g_LocalPlayer->m_flSpawnTime();

		// note anim state for future use.
		m_serverAnimState = state;
	}
	else if (!g_ClientState->chokedcommands && m_serverAnimState)
	{

		m_angRealAngle = angles;

		// lag_compensation.BackupPlayer( m_pLocalPlayer );

		if (!usercmd || usercmd == nullptr)
			return;
		junkcode::call();
		C_BasePlayer::UpdateAnimationState(m_serverAnimState, usercmd->viewangles);
		m_flLowerBody = g_LocalPlayer->m_flLowerBodyYawTarget();

		// lag_compensation.RestorePlayer( m_pLocalPlayer );

		float server_time = server_time = TICKS_TO_TIME(AimRage::Get().GetTickbase());
		auto angle_distance = std::abs(Math::ClampYaw(m_angRealAngle[1] - g_LocalPlayer->m_flLowerBodyYawTarget()));

		if (m_serverAnimState->m_flVelocity() > 0.1f)
			m_flNextBodyUpdate = (server_time + 0.22f);
		else if ((angle_distance > 35.f) && (server_time > m_flNextBodyUpdate))
			m_flNextBodyUpdate = (server_time + 1.1f);
	}
}

float AntiAim::GetPitch()
{
	if (g_Options.hvh_antiaim_legit)
		return usercmd->viewangles.pitch;

	junkcode::call();
	switch (g_Options.hvh_antiaim_x)
	{
	case AA_PITCH_OFF:
		return usercmd->viewangles.pitch;
		break;

	case AA_PITCH_DYNAMIC:
		return g_LocalPlayer->m_hActiveWeapon().Get()->IsSniper() ? (g_LocalPlayer->m_hActiveWeapon().Get()->m_zoomLevel() != 0 ? 87.f : 85.f) : 88.99f;
		break;

	case AA_PITCH_EMOTION:
		return 88.99f;
		break;

	case AA_PITCH_STRAIGHT:
		return 0.f;
		break;

	case AA_PITCH_UP:
		return -88.99f;
		break;
	case AA_PITCH_FAKEJITTER:
		return (Global::bSendPacket ? Utils::RandomFloat(-88.99f, 88.99f) : (Utils::RandomFloat(0.0f, 1.0f) > 0.5f ? 45.f : 88.99f));
		break;
	case AA_PITCH_ATTARGET:
		C_BasePlayer *target; float cloestdest = 4096, dest; bool has_target = false;
		for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
		{
			if (!AimRage::Get().CheckTarget(i))
				continue;
			junkcode::call();
			dest = g_LocalPlayer->GetAbsOrigin().DistTo(C_BasePlayer::GetPlayerByIndex(i)->GetAbsOrigin());
			if (dest < cloestdest)
			{
				cloestdest = dest;
				target = C_BasePlayer::GetPlayerByIndex(i);
				has_target = true;
			}
		}
		if (has_target)
			return Math::CalcAngle(g_LocalPlayer->GetAbsOrigin(), target->GetAbsOrigin()).pitch;
		else
			return usercmd->viewangles.pitch;
		break;
	}
	junkcode::call();
	return usercmd->viewangles.pitch;
}

bool AntiAim::WallDetection(float &yaw)
{
	trace_t trace;
	Ray_t ray;
	CTraceFilterWorldOnly filter;

	float trace_distance = 25.f;
	auto head_position = g_LocalPlayer->GetEyePos();

	float last_fraction = 1.f;
	float lowest_fraction = 1.f;
	bool found_wall = false;

	for (int i = 0; i < 360; i += 2)
	{
		Vector direction;
		Math::AngleVectors(QAngle(0, i, 0), direction);
		junkcode::call();
		ray.Init(head_position, head_position + (direction * trace_distance));
		g_EngineTrace->TraceRay(ray, MASK_ALL, &filter, &trace);

		if (trace.fraction > last_fraction)
		{
			found_wall = true;
			break;
		}
		last_fraction = trace.fraction;
		if (trace.fraction < lowest_fraction)
		{
			lowest_fraction = trace.fraction;
			yaw = i;
		}
	}

	return found_wall;
}

Vector rotate_extend_vec(Vector pos, float rot, float ext)
{
	return Vector(ext * cos(DEG2RAD(rot)) + pos.x, ext * sin(DEG2RAD(rot)) + pos.y, pos.z);	//thanks aimtux
}

float calcdamage(float yaw, int &targets, bool should_multitarget, C_BasePlayer *target, bool lite = false)
{
	Ray_t ray, ray2;
	trace_t trace;
	CTraceFilter filter;
	filter.pSkip = g_LocalPlayer;

	Vector origin = g_LocalPlayer->GetAbsOrigin(), offset = Vector(0, 0, (Global::userCMD->buttons & IN_DUCK ? 49.f : 64.f)), headpos = origin + offset; //i know, hardcoding is bad and all.																																 //but using view offset breaks when in tp
	junkcode::call();

	float damage = 0;

	float radius = 2;

	if (lite)
	{
		Vector location(64 * cos(DEG2RAD(yaw)) + headpos.x, 64 * sin(DEG2RAD(yaw)) + headpos.y, headpos.z);	//thanks aimtux
		ray.Init(headpos, location);
		g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace); location = trace.endpos;

		if (should_multitarget)
		{
			for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
			{
				if (!AimRage::Get().CheckTarget(i))
					continue;

				auto ent = C_BasePlayer::GetPlayerByIndex(i);
				//damage += AimRage::Get().GetDamageVec(location, ent, HITBOX_HEAD, true, false, ent);
				Vector enemy_headpos = ent->GetEyePos();
				float to_target_yaw = Math::CalcAngle(headpos, enemy_headpos).yaw;

				Vector pos1, pos2, pos3, pos4;

				ray.Init(location, rotate_extend_vec(enemy_headpos, Math::ClampYaw(to_target_yaw - 90), 128));
				ray2.Init(location, rotate_extend_vec(enemy_headpos, Math::ClampYaw(to_target_yaw + 90), 128));
				g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace); pos1 = (enemy_headpos + trace.endpos) / 2; pos3 = trace.endpos;
				g_EngineTrace->TraceRay(ray2, MASK_SHOT_BRUSHONLY, &filter, &trace); pos2 = (enemy_headpos + trace.endpos) / 2; pos4 = trace.endpos;

				damage += AimRage::Get().GetDamageVec2(pos1, location, ent, g_LocalPlayer, HITBOX_NECK).damage;
				damage += AimRage::Get().GetDamageVec2(pos2, location, ent, g_LocalPlayer, HITBOX_NECK).damage;
				damage += AimRage::Get().GetDamageVec2(pos3, location, ent, g_LocalPlayer, HITBOX_NECK).damage;
				damage += AimRage::Get().GetDamageVec2(pos4, location, ent, g_LocalPlayer, HITBOX_NECK).damage;
				damage /= 4;
				targets++;
			}
		}
		else
		{
			//damage = AimRage::Get().GetDamageVec(location, target, HITBOX_HEAD, true, false, target);
			Vector enemy_headpos = target->GetEyePos();

			float to_target_yaw = Math::CalcAngle(headpos, enemy_headpos).yaw;

			Vector pos1, pos2, pos3, pos4;

			ray.Init(location, rotate_extend_vec(enemy_headpos, Math::ClampYaw(to_target_yaw - 90), 128));
			ray2.Init(location, rotate_extend_vec(enemy_headpos, Math::ClampYaw(to_target_yaw + 90), 128));
			g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace); pos1 = (enemy_headpos + trace.endpos) / 2; pos3 = trace.endpos;
			g_EngineTrace->TraceRay(ray2, MASK_SHOT_BRUSHONLY, &filter, &trace); pos2 = (enemy_headpos + trace.endpos) / 2;  pos4 = trace.endpos;

			damage += AimRage::Get().GetDamageVec2(pos1, location, target, g_LocalPlayer, HITBOX_NECK).damage;
			damage += AimRage::Get().GetDamageVec2(pos2, location, target, g_LocalPlayer, HITBOX_NECK).damage;
			damage += AimRage::Get().GetDamageVec2(pos3, location, target, g_LocalPlayer, HITBOX_NECK).damage;
			damage += AimRage::Get().GetDamageVec2(pos4, location, target, g_LocalPlayer, HITBOX_NECK).damage;
			damage /= 4;
		}
		return damage;
	}
	for (int i = 0; i < 3; i++, radius += (g_Options.hvh_antiaim_freestand_aggresive + 1) * 64)	//gay
	{
		Vector location(radius * cos(DEG2RAD(yaw)) + headpos.x, radius * sin(DEG2RAD(yaw)) + headpos.y, headpos.z);	//thanks aimtux
		ray.Init(headpos, location);
		g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace); location = trace.endpos;

		if (should_multitarget)
		{
			for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
			{
				if (!AimRage::Get().CheckTarget(i))
					continue;

				auto ent = C_BasePlayer::GetPlayerByIndex(i);
				//damage += AimRage::Get().GetDamageVec(location, ent, HITBOX_HEAD, true, false, ent);
				Vector enemy_headpos = ent->GetEyePos();
				float to_target_yaw = Math::CalcAngle(headpos, enemy_headpos).yaw;

				Vector pos1, pos2, pos3, pos4;

				ray.Init(location, rotate_extend_vec(enemy_headpos, Math::ClampYaw(to_target_yaw - 90), 128));
				ray2.Init(location, rotate_extend_vec(enemy_headpos, Math::ClampYaw(to_target_yaw + 90), 128));
				g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace); pos1 = (enemy_headpos + trace.endpos) / 2; pos3 = trace.endpos;
				g_EngineTrace->TraceRay(ray2, MASK_SHOT_BRUSHONLY, &filter, &trace); pos2 = (enemy_headpos + trace.endpos) / 2; pos4 = trace.endpos;

				damage += AimRage::Get().GetDamageVec2(pos1, location, ent, g_LocalPlayer, HITBOX_NECK).damage;
				damage += AimRage::Get().GetDamageVec2(pos2, location, ent, g_LocalPlayer, HITBOX_NECK).damage;
				if (!lite && radius > 10)
				{
					damage += AimRage::Get().GetDamageVec2(pos3, location, ent, g_LocalPlayer, HITBOX_NECK).damage;
					damage += AimRage::Get().GetDamageVec2(pos4, location, ent, g_LocalPlayer, HITBOX_NECK).damage;
				}
				if (radius > 10) damage /= 2;
				else targets++;
			}
		}
		else
		{
			//damage = AimRage::Get().GetDamageVec(location, target, HITBOX_HEAD, true, false, target);
			Vector enemy_headpos = target->GetEyePos();

			float to_target_yaw = Math::CalcAngle(headpos, enemy_headpos).yaw;

			Vector pos1, pos2, pos3, pos4;

			ray.Init(location, rotate_extend_vec(enemy_headpos, Math::ClampYaw(to_target_yaw - 90), 128));
			ray2.Init(location, rotate_extend_vec(enemy_headpos, Math::ClampYaw(to_target_yaw + 90), 128));
			g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace); pos1 = (enemy_headpos + trace.endpos) / 2; pos3 = trace.endpos;
			g_EngineTrace->TraceRay(ray2, MASK_SHOT_BRUSHONLY, &filter, &trace); pos2 = (enemy_headpos + trace.endpos) / 2;  pos4 = trace.endpos;

			damage += AimRage::Get().GetDamageVec2(pos1, location, target, g_LocalPlayer, HITBOX_NECK).damage;
			damage += AimRage::Get().GetDamageVec2(pos2, location, target, g_LocalPlayer, HITBOX_NECK).damage;
			if (!lite && radius > 10)
			{
				damage += AimRage::Get().GetDamageVec2(pos3, location, target, g_LocalPlayer, HITBOX_NECK).damage;
				damage += AimRage::Get().GetDamageVec2(pos4, location, target, g_LocalPlayer, HITBOX_NECK).damage;
			}
			if (radius > 10) damage /= 2;
		}
		if (damage > 20)
		{
			return damage * (g_Options.hvh_antiaim_freestand_aggresive + 1);
		}
	}

	if (damage > 1)
	{
		return damage * (g_Options.hvh_antiaim_freestand_aggresive + 1);
	}

	return 0;
}

float calcdamage(Vector pos, bool should_multitarget, C_BasePlayer *target)
{
	float damage = 0;
	if (should_multitarget)
	{
		for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
		{
			if (!AimRage::Get().CheckTarget(i))
				continue;

			auto ent = C_BasePlayer::GetPlayerByIndex(i);
			//damage += AimRage::Get().GetDamageVec(pos, ent, HITBOX_HEAD, true, false, ent);
			damage += AimRage::Get().GetDamageVec2(ent->GetEyePos(), pos, ent, g_LocalPlayer, HITBOX_NECK).damage;
		}
	}
	else
	{
		//damage = AimRage::Get().GetDamageVec(pos, target, HITBOX_HEAD, true, false, target);
		damage = AimRage::Get().GetDamageVec2(target->GetEyePos(), pos, target, g_LocalPlayer, HITBOX_NECK).damage;
	}
	return damage;
}

float getbestang(float startang, float scanrange, float step, bool alt = false, bool should_multitarget = true, C_BasePlayer *target = nullptr)
{
	float currang = startang;
	float altang = startang;
	static bool altchk = false;
	float best_damage = 9999, best_delta = 0, best_yaw = -9999;
	int i = 0;
	while ((std::fabsf(currang - startang) < scanrange) || (alt && std::fabsf(altang - startang) < scanrange))
	{
		float damage = calcdamage((alt && altchk) ? altang : currang, i, should_multitarget, target);
		if (best_damage < 10)
		{
			float delta = calcdamage(((alt && altchk) ? altang : currang) + 180, i, should_multitarget, target) - damage;
			if (delta > best_delta)
			{
				best_delta = delta;
				best_yaw = (alt && altchk) ? altang : currang;
			}
		}
		else if (damage < best_damage)
		{
			best_damage = damage;
			best_yaw = (alt && altchk) ? altang : currang;
		}
		if(!alt || altchk)
			currang += step;
		else
			altang += (step * -1);
		altchk = !altchk;
	}
	return Math::ClampYaw(best_yaw);
}

float AntiAim::Freestanding_V2()
{
	static float timestemp = 0, timestemp2 = 0;
	static int last_target = 0;
	float curtime = g_GlobalVars->curtime;

	m_flFreestandingYaw = -9999;
	junkcode::call();
	if (!g_Options.hvh_antiaim_freestand && g_Options.hvh_antiaim_y != AA_YAW_ANTILBY && !g_Options.hvh_antiaim_new)
		return -9999;

	if (g_Options.hvh_antiaim_new) g_Options.hvh_antiaim_freestand_aggresive = true;

	if (abs(curtime - timestemp) > 0.2 * pow(last_target, 2))
	{
		timestemp = curtime;

		QAngle viewangle; g_EngineClient->GetViewAngles(viewangle);

		bool aready_pushed = false;
		int cloest_target = -1; int targets = 0; float cloest_fov = 360;
		for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
		{
			if (!AimRage::Get().CheckTarget(i))
				continue;
			targets++;
			float fov = fabsf(Math::ClampYaw(Math::CalcAngle(g_LocalPlayer->GetAbsOrigin(), C_BasePlayer::GetPlayerByIndex(i)->GetAbsOrigin()).yaw - viewangle.yaw));
			if (fov < cloest_fov)
			{
				cloest_fov = fov;
				cloest_target = i;
			}
		}

		last_target = targets;

		if (cloest_target < 0)
		{
			m_flFreestandingYaws.push_front(-9999);
			aready_pushed = true;
		}
		junkcode::call();
		float multiTargetdamage = 0;
		if (!aready_pushed)
		{
			auto cloest_enemy = C_BasePlayer::GetPlayerByIndex(cloest_target);
			bool should_multitarget;
			if (!cloest_enemy)
			{
				return -9999;
			}
			else
			{
				//bool should_multitarget = (AimRage::Get().GetDamageVec(g_LocalPlayer->GetAbsOrigin(), cloest_enemy, HITBOX_HEAD, true, false, cloest_enemy) - multiTargetdamage < -25);
				//should_multitarget = (multiTargetdamage - AimRage::Get().GetDamageVec2(cloest_enemy->GetAbsOrigin() + cloest_enemy->m_vecViewOffset(), g_LocalPlayer->GetHitboxPos(0), cloest_enemy, g_LocalPlayer, HITBOX_NECK).damage > 25);
				should_multitarget = true;
			}

			float temp = 0;
			Vector origin = g_LocalPlayer->GetAbsOrigin(), offset = Vector(0, 0, (Global::userCMD->buttons & IN_DUCK ? 49.f : 64.f)), headpos = origin + offset;

			//if (WallDetection(temp))
			//{
			//	m_flFreestandingYaws.push_front(getbestang(temp, 120.f, 30.f, true));
			//}
			//else
			{
				int targets_front, targets_back, targets_left, targets_right = 0;
				targets_front = targets_back = targets_left = targets_right;
				float leftang = Math::ClampYaw(viewangle.yaw + 90.f), rightang = Math::ClampYaw(viewangle.yaw - 90.f), backang = Math::ClampYaw(viewangle.yaw + 180.f);
				float leftdmg = calcdamage(leftang, targets_left, should_multitarget, cloest_enemy), rightdmg = calcdamage(rightang, targets_right, should_multitarget, cloest_enemy), frontdmg, backdmg;
				if ((leftdmg > 100 && rightdmg > 100) || std::fabsf(leftdmg - rightdmg) < (g_Options.hvh_antiaim_freestand_aggresive ? 5 : 10))
				{
					frontdmg = calcdamage(viewangle.yaw, targets_front, should_multitarget, cloest_enemy); backdmg = calcdamage(backang, targets_back, should_multitarget, cloest_enemy);
					if ((frontdmg > 100 && backdmg > 100) || std::fabsf(frontdmg - backdmg) < (g_Options.hvh_antiaim_freestand_aggresive ? 5 : 10))
					{
						float temp_yaw = PastedFreestanding(), extend = 64;
						if (temp_yaw < -1000)
						{
							m_flFreestandingYaws.push_front(-9999);
							aready_pushed = true;
						}
						else
						{
							for (; extend < 256; extend *= 1.5)
							{
								Vector loc = rotate_extend_vec(headpos, temp_yaw, extend);
								Vector loc2 = rotate_extend_vec(headpos, Math::ClampYaw(temp_yaw + 180), extend);
								if (calcdamage(loc, should_multitarget, cloest_enemy) > calcdamage(loc2, should_multitarget, cloest_enemy))
								{
									if (g_Options.hvh_antiaim_freestand_aggresive)
									{
										m_flFreestandingYaws.push_front(getbestang(Math::ClampYaw(temp_yaw + 180), 90, 22.5f, true, should_multitarget, cloest_enemy));
										break;
									}
									else
									{
										m_flFreestandingYaws.push_front(-9999);
										aready_pushed = true;
										break;
									}
								}
							}
							if (!aready_pushed)
							{
								m_flFreestandingYaws.push_front(getbestang(temp_yaw, 30, 22.5f, true, should_multitarget, cloest_enemy));
							}
						}
					}
					else if (frontdmg < backdmg)
					{
						m_flFreestandingYaws.push_front(m_flFreestandingYaw = getbestang(viewangle.yaw, 90, 22.5f * (leftdmg < rightdmg ? -1 : 1), false, should_multitarget, cloest_enemy));
					}
					else
					{
						m_flFreestandingYaws.push_front(getbestang(backang, 90, 22.5f * (leftdmg < rightdmg > 90 ? 1 : -1), false, should_multitarget, cloest_enemy));
					}
				}
				else if (leftdmg < rightdmg)
				{
					m_flFreestandingYaws.push_front(getbestang(leftang, 90, 22.5f, true, should_multitarget, cloest_enemy));
				}
				else if (rightdmg < leftdmg)
				{
					m_flFreestandingYaws.push_front(getbestang(rightang, 90, -22.5f, true, should_multitarget, cloest_enemy));
				}
			}
		}

		if (m_flFreestandingYaws.front() < -181 && abs(curtime - timestemp2) < 0.25)
			m_flFreestandingYaws.pop_front();
		else if (m_flFreestandingYaws.front() >= -180)
			timestemp2 = curtime;

	}

	while (m_flFreestandingYaws.size() > 2)
	{
		m_flFreestandingYaws.pop_back();
	}

	if (m_flFreestandingYaws.size() <= 0)
	{
		return m_flFreestandingYaw;
	}

	m_flFreestandingYaw = 0;

	int divval = m_flFreestandingYaws.size();
	for (auto preval = m_flFreestandingYaws.begin() + 1; preval != m_flFreestandingYaws.end(); preval++)
	{
		if (*preval < -181)
		{
			divval--;
			continue;
		}
		m_flFreestandingYaw += *preval;
	}

	if (divval <= 1)
	{
		m_flFreestandingYaw = -9999;
		return m_flFreestandingYaw;
	}
	m_flFreestandingYaw /= divval;
	return m_flFreestandingYaw;
}

float LiteFreestanding()
{
	int a = 0;

	QAngle viewangle; g_EngineClient->GetViewAngles(viewangle);

	float angle_right = Math::ClampYaw(viewangle.yaw - 90.f), angle_left = Math::ClampYaw(viewangle.yaw + 90.f), angle_back = Math::ClampYaw(viewangle.yaw + 180.f), angle_front = viewangle.yaw;
	float dmg_rignt = calcdamage(angle_right, a, true, nullptr, true), dmg_left = calcdamage(angle_left, a, true, nullptr, true), dmg_back = calcdamage(angle_back, a, true, nullptr, true), dmg_front = calcdamage(angle_front, a, true, nullptr, true);
	if (!a) return -9999;

	if (abs(dmg_rignt - dmg_left) > 20)
	{
		if (abs(dmg_back - dmg_front) > 20) return -9999;

		if (dmg_back < dmg_front) return angle_back;
		else					  return angle_front;
	}
	else
	{
		if (dmg_rignt < dmg_left) return angle_right;
		else					  return angle_left;
	}
}

float AntiAim::Freestanding_V3()
{
	static std::deque<AA_Freestand_Records> records;

	static Vector last_enemy_pos_avg = Vector(0, 0, 0);

	float lite_angle = LiteFreestanding();

	if (lite_angle < -1000)
	{
		m_flFreestandingYaw = PastedFreestanding();
		return m_flFreestandingYaw;
	}
	else
	{
		Vector avg_enemy_pos = g_LocalPlayer->m_vecOrigin();
		float closest_dist = 99999999.f;
		float closest_time = -1.f;
		float returnval = lite_angle;
		bool usedRecord = false;

		for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
		{
			if (!AimRage::Get().CheckTarget(i)) continue;
			avg_enemy_pos += C_BasePlayer::GetPlayerByIndex(i)->m_vecOrigin();
			avg_enemy_pos /= 2;
		}

		for (auto i = records.begin(); i != records.end();)
		{
			if (i->position.DistTo(g_LocalPlayer->m_vecOrigin()) < 64.f)
			{
				float timeDelta = abs(i->time - g_GlobalVars->curtime);
				if ((abs(i->angle - lite_angle) > 90.f && timeDelta > 5.0f) || timeDelta > 120.f)
				{
					i = records.erase(i);
					continue;
				}
				else
				{
					if (i->position.DistTo(g_LocalPlayer->m_vecOrigin()) < closest_dist)
					{
						closest_dist = i->position.DistTo(g_LocalPlayer->m_vecOrigin());
						closest_time = i->time;
					}
					if (timeDelta > 1.5f || avg_enemy_pos.DistTo(last_enemy_pos_avg) > 16.f)
					{
						i->angle += getbestang(i->angle, 140.f, 70.f, true);
						i->angle /= 2;
						i->time = g_GlobalVars->curtime;
						last_enemy_pos_avg = avg_enemy_pos;
					}
					returnval += i->angle;
					returnval /= 2;
					usedRecord = true;
				}
			}
			i++;
		}

		if (!usedRecord)
		{
			if (avg_enemy_pos.DistTo(last_enemy_pos_avg) > 16.f)
			{
				float angle = getbestang(lite_angle, 120.f, 22.5f, true);
				records.push_back({ g_LocalPlayer->m_vecOrigin(), angle, g_GlobalVars->curtime });
				last_enemy_pos_avg = avg_enemy_pos;
				m_flFreestandingYaw = angle;
				return angle;
			}
			else
			{
				m_flFreestandingYaw = lite_angle;
				return lite_angle;
			}
		}
		else
		{
			if (closest_dist > 16.f || g_GlobalVars->curtime - closest_time > 5.f) records.push_back({ g_LocalPlayer->m_vecOrigin(), returnval, g_GlobalVars->curtime });
			m_flFreestandingYaw = returnval;
			return returnval;
		}
	}
}

float GetBaseYaw(float viewangle)
{
	C_BasePlayer * target; float cloestdest = 4096, dest; bool has_target = false;
	float yaw = 0; int count = 0;
	switch (g_Options.hvh_antiaim_base)
	{
	case AA_BASEYAW_VIEWANG:
		return viewangle;
		break;
	case AA_BASEYAW_STATIC:
		return 0;
		break;
	case AA_BASEYAW_ATTARGET:
		for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
		{
			if (!AimRage::Get().CheckTarget(i))
				continue;

			dest = g_LocalPlayer->GetAbsOrigin().DistTo(C_BasePlayer::GetPlayerByIndex(i)->GetAbsOrigin());
			if (dest < cloestdest)
			{
				cloestdest = dest;
				target = C_BasePlayer::GetPlayerByIndex(i);
				has_target = true;
			}
		}
		if (has_target)
			return Math::CalcAngle(g_LocalPlayer->GetAbsOrigin(), target->GetAbsOrigin()).pitch;
		else
			return viewangle;
		break;
	case AA_BASEYAW_ADEPTIVE:
		for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
		{
			if (!AimRage::Get().CheckTarget(i))
				continue;

			yaw += Math::CalcAngle(g_LocalPlayer->GetAbsOrigin(), C_BasePlayer::GetPlayerByIndex(i)->GetAbsOrigin()).yaw;
			count++;
		}

		if (count)
			return (yaw / count);
		else
			return viewangle;
		break;
	}
}

float AntiAim::GetYaw(bool no_freestand)
{
	static bool left = false;
	static bool right = false;
	static bool backwards = true;

	static bool flip = false;
	junkcode::call();
	flip = !flip;

	if (AimRage::Get().local_weapon->IsKnife())
	{
		float mindest = 9999999;
		C_BasePlayer *best = nullptr;
		for (int i = 1; i < g_GlobalVars->maxClients; i++)
		{
			if (!AimRage::Get().CheckTarget(i)) continue;
			auto player = C_BasePlayer::GetPlayerByIndex(i);
			float dest = g_LocalPlayer->m_vecOrigin().DistTo(player->m_vecOrigin());
			if (dest < mindest)
			{
				mindest = dest;
				best = player;
			}
		}

		if (!checks::is_bad_ptr(best))
		{
			return (Math::CalcAngle(g_LocalPlayer->m_vecOrigin(), best->m_vecOrigin()).yaw);
		}
	}

	//float freestand = Freestanding();
	//float freestand = PastedFreestanding();
	float freestand = Freestanding_V2();

	if (g_Options.hvh_antiaim_legit)
	{
		if (freestand > -180)
			return freestand + Utils::RandomFloat(-30, 30);

		return (usercmd->viewangles.yaw + ((Math::ClampYaw(usercmd->viewangles.yaw - g_LocalPlayer->m_flLowerBodyYawTarget()) > 0) ? 90 : -90));
	}

	if (!no_freestand && (g_LocalPlayer->m_fFlags() & FL_ONGROUND) && g_Options.hvh_antiaim_y != AA_YAW_ANTILBY)
	{
		if (freestand > -180)
			return freestand + Utils::RandomFloat(-30, 30);
	}

	float_t pos = GetBaseYaw(usercmd->viewangles.yaw);

	if ((g_LocalPlayer->m_vecVelocity().Length2D() > 1) && !(g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind) && g_Options.misc_fakewalk) && g_LocalPlayer->m_fFlags() & FL_ONGROUND) //anti-"exploits retarded moving aa" resolver
	{																																																				  //do it in lby breaker code when it's enabled cause it's p-er
		switch (g_Options.hvh_antiaim_y_moving)
		{
		case AA_YAW_BACKWARDS:

			return pos + 180.0f;
			break;

		case AA_YAW_BACKWARDS_JITTER:

			return pos + 180.0f + Utils::RandomFloat(-25.5f, 25.5f);
			break;

		case AA_YAW_MANUALLBY:

			if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey))) {
				if (GetAsyncKeyState(0x41) & 0x8000) { left = true; right = false;  backwards = false; }
				else if (GetAsyncKeyState(0x44) & 0x8000) { left = false; right = true; backwards = false; }
				else if (GetAsyncKeyState(0x53) & 0x8000) { left = false; right = false; backwards = true; }
			}

			if (left) // left real
				return pos + 90.f;

			else if (right) // right real
				return pos - 90.f;

			else if (backwards) // backwards
				return pos + 180.f;
			break;

		case AA_YAW_MANUALLBYJITTER:

			if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey))) {
				if (GetAsyncKeyState(0x41) & 0x8000) { left = true; right = false;  backwards = false; }
				else if (GetAsyncKeyState(0x44) & 0x8000) { left = false; right = true; backwards = false; }
				else if (GetAsyncKeyState(0x53) & 0x8000) { left = false; right = false; backwards = true; }
			}

			if (left)
				return pos + (90.f + Utils::RandomFloat(-25.5f, 25.5f));

			else if (right)
				return pos - (90.f + Utils::RandomFloat(-25.5f, 25.5f));

			else if (backwards)
				return pos + 180.0f + Utils::RandomFloat(-25.5f, 25.5f);
			break;
		case AA_YAW_CUSTOM_DELTA:
			return pos + g_Options.hvh_antiaim_real_delta + Utils::RandomFloat(-25, 25);
			break;
		case AA_YAW_ANTILBY:
			return g_LocalPlayer->m_flLowerBodyYawTarget() + Utils::RandomFloat(-120, 120);
			break;
		}
	}
	else {
		switch (g_Options.hvh_antiaim_y)
		{
		case AA_YAW_BACKWARDS:

			return pos + 180.0f;
			break;

		case AA_YAW_BACKWARDS_JITTER:

			return pos + 180.0f + Utils::RandomFloat(-25.5f, 25.5f);
			break;

		case AA_YAW_MANUALLBY:

			if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey))) {
				if (GetAsyncKeyState(0x41) & 0x8000) { left = true; right = false;  backwards = false; }
				else if (GetAsyncKeyState(0x44) & 0x8000) { left = false; right = true; backwards = false; }
				else if (GetAsyncKeyState(0x53) & 0x8000) { left = false; right = false; backwards = true; }
			}

			if (left) // left real
				return pos + 90.f;

			else if (right) // right real
				return pos - 90.f;

			else if (backwards) // backwards
				return pos + 180.f;
			break;

		case AA_YAW_MANUALLBYJITTER:

			if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey))) {
				if (GetAsyncKeyState(0x41) & 0x8000) { left = true; right = false;  backwards = false; }
				else if (GetAsyncKeyState(0x44) & 0x8000) { left = false; right = true; backwards = false; }
				else if (GetAsyncKeyState(0x53) & 0x8000) { left = false; right = false; backwards = true; }
			}

			if (left)
				return pos + (90.f + Utils::RandomFloat(-25.5f, 25.5f));

			else if (right)
				return pos - (90.f + Utils::RandomFloat(-25.5f, 25.5f));

			else if (backwards)
				return pos + 180.0f + Utils::RandomFloat(-25.5f, 25.5f);
			break;
		case AA_YAW_CUSTOM_DELTA:
			return pos + g_Options.hvh_antiaim_real_delta + Utils::RandomFloat(-25, 25);
			break;

		case AA_YAW_ANTILBY:
			return g_LocalPlayer->m_flLowerBodyYawTarget() + Utils::RandomFloat(-140, 140);
			break;
		}
	}
	junkcode::call();
	return pos;
}

float AntiAim::GetFakeYaw()
{
	static bool left = false;
	static bool right = false;
	static bool backwards = true;

	static bool flip = false;
	flip = !flip;

	junkcode::call();
	//float freestand = Freestanding();
	//float freestand = PastedFreestanding();
	if (g_Options.hvh_antiaim_legit)
	{
		return usercmd->viewangles.yaw;
	}
	float freestand = Freestanding_V2();
	if (freestand > -180 && (g_LocalPlayer->m_fFlags() & FL_ONGROUND))
		return freestand + 180 + Utils::RandomFloat(-30, 30);
		

	float_t pos = GetBaseYaw(usercmd->viewangles.yaw);
	static float lastpos, lastposmov = 0;
	static float ang = 0;
	static float lby = 0;

	if ((g_LocalPlayer->m_vecVelocity().Length2D() > 1) && !(g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind) && g_Options.misc_fakewalk) && g_LocalPlayer->m_fFlags() & FL_ONGROUND)
	{
		switch (g_Options.hvh_antiaim_y_movingfake)	//lol
		{
		case AA_FAKEYAW_BACKWARDS:

			return pos + 180.0f;
			break;

		case AA_YAW_BACKWARDS_JITTER:

			return pos + 180.0f + Utils::RandomFloat(-25.5f, 25.5f);
			break;

		case AA_FAKEYAW_MANUALLBY:

			if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey))) {
				if (GetAsyncKeyState(0x41) & 0x8000) { left = true; right = false;  backwards = false; }
				else if (GetAsyncKeyState(0x44) & 0x8000) { left = false; right = true; backwards = false; }
				else if (GetAsyncKeyState(0x53) & 0x8000) { left = false; right = false; backwards = true; }
			}

			if (right) // right fake
				return pos + 90.f;

			else if (left) // left fake
				return pos - 90.f;

			else if (backwards) // backwards (spin) fake
				return fmod(g_GlobalVars->curtime / 1.f * 360.f, 360.f) + 180;
			break;

		case AA_FAKEYAW_MANUALLBYJITTER:

			if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey))) {
				if (GetAsyncKeyState(0x41) & 0x8000) { left = true; right = false;  backwards = false; }
				else if (GetAsyncKeyState(0x44) & 0x8000) { left = false; right = true; backwards = false; }
				else if (GetAsyncKeyState(0x53) & 0x8000) { left = false; right = false; backwards = true; }
			}

			if (right)
				return pos + (90.f + Utils::RandomFloat(-25.5f, 25.5f));

			else if (left)
				return pos - (90.f + Utils::RandomFloat(-25.5f, 25.5f));

			else if (backwards)
				return fmod(g_GlobalVars->curtime / 0.5f * 360.f, 360.f) + 180;
			break;
		case AA_FASTSPIN:
			if (lastposmov > 1000)
				lastposmov = 0;
			lastposmov += 69.69f; // p meme
			return lastposmov;
			break;
		case AA_LBYRAND:
			if (abs(lby - Global::LastLBYUpdate) > 0.1)
			{
				ang = Utils::RandomFloat(0, 360);
				lby = Global::LastLBYUpdate;
			}
			return ang;
			break;
		case AA_FAKEYAW_CUSTOM_DELTA:
			return pos + g_Options.hvh_antiaim_fake_delta + Utils::RandomFloat(-25, 25);
			break;
		}
	}
	else {
		switch (g_Options.hvh_antiaim_y_fake)
		{
		case AA_FAKEYAW_BACKWARDS:

			return pos + 180.0f;
			break;

		case AA_YAW_BACKWARDS_JITTER:

			return pos + 180.0f + Utils::RandomFloat(-25.5f, 25.5f);
			break;

		case AA_FAKEYAW_MANUALLBY:

			if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey))) {
				if (GetAsyncKeyState(0x41) & 0x8000) { left = true; right = false;  backwards = false; }
				else if (GetAsyncKeyState(0x44) & 0x8000) { left = false; right = true; backwards = false; }
				else if (GetAsyncKeyState(0x53) & 0x8000) { left = false; right = false; backwards = true; }
			}

			if (right) // right fake
				return pos + 90.f;

			else if (left) // left fake
				return pos - 90.f;

			else if (backwards) // backwards (spin) fake
				return fmod(g_GlobalVars->curtime / 1.f * 360.f, 360.f) + 180;
			break;

		case AA_FAKEYAW_MANUALLBYJITTER:

			if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey))) {
				if (GetAsyncKeyState(0x41) & 0x8000) { left = true; right = false;  backwards = false; }
				else if (GetAsyncKeyState(0x44) & 0x8000) { left = false; right = true; backwards = false; }
				else if (GetAsyncKeyState(0x53) & 0x8000) { left = false; right = false; backwards = true; }
			}

			if (right)
				return pos + (90.f + Utils::RandomFloat(-25.5f, 25.5f));

			else if (left)
				return pos - (90.f + Utils::RandomFloat(-25.5f, 25.5f));

			else if (backwards)
				return fmod(g_GlobalVars->curtime / 0.5f * 360.f, 360.f) + 180;
			break;
		case AA_FASTSPIN:
			if (lastpos > 1000)
				lastpos = 0;
			lastpos += 69.69f; // p meme
			return lastpos;
		case AA_LBYRAND:
			if (abs(lby - Global::LastLBYUpdate) > 0.1)
			{
				ang = Utils::RandomFloat(0, 360);
				lby = Global::LastLBYUpdate;
			}
			return ang;
			break;
		case AA_FAKEYAW_CUSTOM_DELTA:
			return pos + g_Options.hvh_antiaim_fake_delta + Utils::RandomFloat(-25, 25);
			break;
		}
	}
	junkcode::call();
	return pos;
}

int AntiAim::LBYUpdate()
{
	if (!g_Options.hvh_antiaim_new)
		return 0;

	static int choked = 0;
	static bool wasMoving = true;
	static bool firstBreak = false;
	static bool preBreak = false;
	static bool shouldBreak = false;
	static bool brokeThisTick = false;
	static bool preBreakNeeded = true;
	static float lastLBY = 0;

	double curtime = g_GlobalVars->curtime; //AimRage::Get().GetTickbase() * g_GlobalVars->interval_per_tick;
	bool bIsMoving = false;

	//if (checks::is_bad_ptr(Animation::Get().GetPlayerAnimationInfo(g_LocalPlayer->EntIndex()).m_playerAnimState))
	if (checks::is_bad_ptr(g_LocalPlayer->GetPlayerAnimState()))
	{	//anim state is invalid. fuck fuck fuck fuck fuck fuck fuck fuck fuck
		bIsMoving = g_LocalPlayer->m_vecVelocity().Length2D() >= 0.1f;
	}
	else
	{	//use anim state velocity for more accurate result
		//bIsMoving = Animation::Get().GetPlayerAnimationInfo(g_LocalPlayer->EntIndex()).m_playerAnimState->m_flVelocity() >= 0.1f;
		bIsMoving = g_LocalPlayer->GetPlayerAnimState()->m_flVelocity() >= 0.1f;
	}

	auto nci = g_EngineClient->GetNetChannelInfo();

	if (g_LocalPlayer->m_flLowerBodyYawTarget() != lastLBY)
	{
		Global::LastLBYUpdate = curtime;
		lastLBY = g_LocalPlayer->m_flLowerBodyYawTarget();
	}

	if (Global::bSendPacket)
	{
		brokeThisTick = false;
		choked = g_ClientState->chokedcommands - Global::prevChoked;

		if (bIsMoving && (g_LocalPlayer->m_fFlags() & FL_ONGROUND) && !(g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind) && g_Options.misc_fakewalk))
		{
			m_flNextBodyUpdate = curtime + 0.22;
			Global::NextLBYUpdate = curtime + 0.22;
			wasMoving = true;
			firstBreak = true;
		}
		else
		{
			if (wasMoving && curtime - Global::LastLBYUpdate >= 0.22)
			{
				wasMoving = false;
				firstBreak = false;
				shouldBreak = true;
				m_flNextBodyUpdate = curtime + 1.1;
				Global::NextLBYUpdate = curtime + 1.1;
			}
			else if (curtime - Global::LastLBYUpdate >= 1.1 - TICKS_TO_TIME(choked))
			{
				shouldBreak = true;
				firstBreak = false;
				m_flNextBodyUpdate = curtime + 1.1;
				Global::NextLBYUpdate = curtime + 1.1;
			}
			else if (curtime - Global::LastLBYUpdate >= 1.1 - (TICKS_TO_TIME(choked) + g_GlobalVars->interval_per_tick * 2) && !firstBreak)
			{
				preBreak = true;
			}
			/*
			else if (abs(Math::ClampYaw(oldlby - g_LocalPlayer->m_flLowerBodyYawTarget())) > 1 && abs(Math::ClampYaw(m_flLowerBodyTarget - g_LocalPlayer->m_flLowerBodyYawTarget())) > 10)
			{
				m_flNextBodyUpdate = server_time + g_GlobalVars->interval_per_tick + 1.1;
				oldCurtime = server_time + nci->GetLatency(FLOW_INCOMING);
				//if (abs(Math::ClampYaw(target_pre - g_LocalPlayer->m_flLowerBodyYawTarget())) < 1) oldCurtime += pre_delay;
				m_flNextBodyUpdate = server_time + 1.1;
				firstBreak = true;
			}
			*/
		}
	}
	else
	{
		static float last_last_update = 0;
		if (preBreak)
		{
			preBreak = false;
			return 2;
		}
		else if (shouldBreak)
		{
			shouldBreak = false;
			Global::LastLBYUpdate = curtime;
			last_last_update = curtime;
			preBreakNeeded = true;
			return 1;
		}
		else
		{
			if (Global::LastLBYUpdate != last_last_update && abs(Global::LastLBYUpdate - last_last_update) < 1.f)
			{
				firstBreak = true;
				last_last_update = Global::LastLBYUpdate;
			}
			else if (abs(Math::ClampYaw(Global::realyaw - g_LocalPlayer->m_flLowerBodyYawTarget())) < 50)
			{
				preBreakNeeded = false;
			}

			return 0;
		}
	}
}

float AntiAim::GetPitchNew()
{
	static float last_angle[2] = { -9999 };

	float pos = usercmd->viewangles.pitch;

	bool real   = !g_Options.hvh_disable_antiut;
	bool moving = g_LocalPlayer->m_vecVelocity().Length2D() >= 0.1f;
	bool freestanding = Freestanding_V3() > -180.f;

	int   option = g_Options.hvh_antiaim_new_options[real][0][moving][freestanding];
	float offset = g_Options.hvh_antiaim_new_offset[real][0][moving][freestanding];
	float extra  = g_Options.hvh_antiaim_new_extra[real][0][moving][freestanding];

	static LUA state_aaPitch;

	if (!state_aaPitch.state.usuable)
	{
		state_aaPitch.KillState();
		state_aaPitch.CreateState();
	}

	if (!state_aaPitch.state.can_execute || Global::currentLUA != state_aaPitch.state.loaded)
	{
		state_aaPitch.LoadFile(Global::currentLUA.c_str());
	}

	if (real)
	{
		switch (option)
		{
		case NEW_AA_PITCH::AA_PITCH_NEW_VIEWANG:	   return pos; break;
		case NEW_AA_PITCH::AA_PITCH_NEW_CUSTOM_STATIC: return offset; break;
		case NEW_AA_PITCH::AA_PITCH_NEW_CUSTOM_JITTER: return Math::Clamp(offset + Utils::RandomFloat(-extra / 2, extra / 2), -89.f, 89.f); break;
		case NEW_AA_PITCH::AA_PITCH_NEW_LUA:
		{
			float res = state_aaPitch.ExecuteFunction("AntiAimPitch", std::vector<float>({ pos, last_angle[real], offset, extra, static_cast<float>(real) }));
			if (res == failure_code::CANT_EXECUTE || res == failure_code::FAILED_EXECUTE || res == failure_code::UNKNOWN_TYPE)	//script failed
			{
				return 89.f;
			}
			else
			{
				last_angle[real] = Math::ClampPitch(res);
				return res;
			}
		}
		}
	}
	else
	{
		static float last_return = 0;
		static float last_target = -9999;
		float target;

		switch (option)
		{
		case NEW_AA_PITCH::AA_PITCH_NEW_VIEWANG:	   target = pos; break;
		case NEW_AA_PITCH::AA_PITCH_NEW_CUSTOM_STATIC: target = offset; break;
		case NEW_AA_PITCH::AA_PITCH_NEW_CUSTOM_JITTER: target = Math::Clamp(offset + Utils::RandomFloat(-extra / 2, extra / 2), -89.f, 89.f); break;
		case NEW_AA_PITCH::AA_PITCH_NEW_LUA:
		{
			float res = state_aaPitch.ExecuteFunction("AntiAimPitch", std::vector<float>({ pos, last_angle[real], offset, extra, static_cast<float>(real) }));
			if (res == failure_code::CANT_EXECUTE || res == failure_code::FAILED_EXECUTE || res == failure_code::UNKNOWN_TYPE)	//script failed
			{
				target = 89.f;
			}
			else
			{
				last_angle[real] = Math::ClampPitch(res);
				target = res;
			}
		}
		}

		if (target == last_target)
		{
			return last_return;		//need to optimize as much as we can on this because it's fking heavy
		}
		else
		{
			last_target = target;
			last_return = Math::FindSmallestFake(target, Utils::RandomInt(0, 4));
			return last_return;
		}
	}

	return 0;
}

float AntiAim::GetYawNew()
{
	static float last_angle[2] = { -9999 };
	static bool right = false;

	float pos = usercmd->viewangles.yaw;

	bool real		  = !Global::bSendPacket;
	bool moving		  = g_LocalPlayer->m_vecVelocity().Length2D() >= 0.1f;
	bool freestanding = Freestanding_V3() > -180.f;

	int   option = g_Options.hvh_antiaim_new_options[real][1][moving][freestanding];
	float offset = g_Options.hvh_antiaim_new_offset[real][1][moving][freestanding];
	float extra  = g_Options.hvh_antiaim_new_extra[real][1][moving][freestanding];

	int   shouldBreak = LBYUpdate();
	bool  breakLBY = g_Options.hvh_antiaim_new_breakLBY[freestanding];
	float LBYDelta = g_Options.hvh_antiaim_new_LBYDelta[freestanding];
	float addAngle = (shouldBreak == 2 && (LBYDelta < 100 || LBYDelta > 120)) ? (Global::fps >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.9 * max(g_ClientState->chokedcommands, Global::prevChoked) + 100) : 145.f) : 0;

	if (freestanding)
	{
		if (option == NEW_AA_YAW::AA_YAW_NEW_VIEWANG)
		{
			option = g_Options.hvh_antiaim_new_options[real][1][moving][0];
			offset = g_Options.hvh_antiaim_new_offset[real][1][moving][0];
			extra = g_Options.hvh_antiaim_new_extra[real][1][moving][0];
		}
		else
		{
			pos = m_flFreestandingYaw;
		}
	}
	static LUA state_aaYaw;

	if (!state_aaYaw.state.usuable)
	{
		state_aaYaw.KillState();
		state_aaYaw.CreateState();
	}

	if (!state_aaYaw.state.can_execute || Global::currentLUA != state_aaYaw.state.loaded)
	{
		state_aaYaw.LoadFile(Global::currentLUA.c_str());
	}

	if (g_InputSystem->IsButtonDown(static_cast<ButtonCode_t>(g_Options.hvh_antiaim_menkey)))
	{
		if		(GetAsyncKeyState(0x41) & 0x8000) right = false;
		else if (GetAsyncKeyState(0x44) & 0x8000) right = true;
	}

	if (AimRage::Get().local_weapon->IsKnife() && option != NEW_AA_YAW::AA_YAW_NEW_VIEWANG)
	{
		float mindest = 9999999;
		C_BasePlayer *best = nullptr;
		for (int i = 1; i < g_GlobalVars->maxClients; i++)
		{
			if (!AimRage::Get().CheckTarget(i)) continue;
			auto player = C_BasePlayer::GetPlayerByIndex(i);
			if (!player->m_hActiveWeapon().Get() || !player->m_hActiveWeapon().Get()->IsKnife()) continue;
			float dest = g_LocalPlayer->m_vecOrigin().DistTo(player->m_vecOrigin());
			if (dest < mindest)
			{
				mindest = dest;
				best = player;
			}
		}

		if (!checks::is_bad_ptr(best))
		{
			return (Math::CalcAngle(g_LocalPlayer->m_vecOrigin(), best->m_vecOrigin()).yaw);
		}
	}

	if (shouldBreak && breakLBY)
	{
		switch (option)
		{
			case NEW_AA_YAW::AA_YAW_NEW_VIEWANG:	   return pos + LBYDelta + addAngle; break;
			case NEW_AA_YAW::AA_YAW_NEW_SPIN:		   return addAngle; break;
			case NEW_AA_YAW::AA_YAW_NEW_CUSTOM_STATIC: case NEW_AA_YAW::AA_YAW_NEW_CUSTOM_JITTER: return pos + offset + LBYDelta + addAngle; break;
			case NEW_AA_YAW::AA_YAW_NEW_MENUAL:		   return pos + (right) ? (90) : (-90) + LBYDelta + addAngle; break;
			case NEW_AA_YAW::AA_YAW_NEW_MENUAL_JITTER: return pos + (right) ? (90) : (-90) + Utils::RandomFloat(-extra / 2, extra / 2) + LBYDelta + addAngle; break;
			case NEW_AA_YAW::AA_YAW_NEW_LUA:
			{
				float res = state_aaYaw.ExecuteFunction("AntiAimYaw", std::vector<float>({ pos, last_angle[real], offset, extra, static_cast<float>(real) }));
				if (res == failure_code::CANT_EXECUTE || res == failure_code::FAILED_EXECUTE || res == failure_code::UNKNOWN_TYPE)	//script failed
				{
					return 89.f;
				}
				else
				{
					last_angle[real] = Math::ClampYaw(res);
					return res + LBYDelta + addAngle;
				}
			}
		}
	}
	switch (option)
	{
		case NEW_AA_YAW::AA_YAW_NEW_VIEWANG:	   return pos; break;
		case NEW_AA_YAW::AA_YAW_NEW_SPIN:		   if (last_angle[real] < -180) last_angle[real] = offset; last_angle[real] += extra; last_angle[real] = Math::ClampYaw(last_angle[real]); return last_angle[real] + offset; break;
		case NEW_AA_YAW::AA_YAW_NEW_CUSTOM_STATIC: return pos + offset; break;
		case NEW_AA_YAW::AA_YAW_NEW_CUSTOM_JITTER: return pos + offset + Utils::RandomFloat(-extra / 2, extra / 2); break;
		case NEW_AA_YAW::AA_YAW_NEW_MENUAL:		   return pos + (right) ? (90) : (-90); break;
		case NEW_AA_YAW::AA_YAW_NEW_MENUAL_JITTER: return pos + (right) ? (90) : (-90) + Utils::RandomFloat(-extra / 2, extra / 2); break;
		case NEW_AA_YAW::AA_YAW_NEW_LUA:
		{
			float res = state_aaYaw.ExecuteFunction("AntiAimYaw", std::vector<float>({ pos, last_angle[real], offset, extra, static_cast<float>(real) }));
			if (res == failure_code::CANT_EXECUTE || res == failure_code::FAILED_EXECUTE || res == failure_code::UNKNOWN_TYPE)	//script failed
			{
				return 89.f;
			}
			else
			{
				last_angle[real] = Math::ClampYaw(res);
				return res;
			}
		}
	}

	return 0;
}

void AntiAim::Accelerate(C_BasePlayer *player, Vector &wishdir, float wishspeed, float accel, Vector &outVel)
{
	// See if we are changing direction a bit
	float currentspeed = outVel.Dot(wishdir);

	// Reduce wishspeed by the amount of veer.
	float addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of accleration.
	float accelspeed = accel * g_GlobalVars->frametime * wishspeed * player->m_surfaceFriction();
	junkcode::call();
	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust velocity.
	for (int i = 0; i < 3; i++)
		outVel[i] += accelspeed * wishdir[i];
}

void AntiAim::WalkMove(C_BasePlayer *player, Vector &outVel)
{
	Vector forward, right, up, wishvel, wishdir, dest;
	float_t fmove, smove, wishspeed;

	Math::AngleVectors(player->m_angEyeAngles(), forward, right, up);  // Determine movement angles
	// Copy movement amounts
	g_MoveHelper->SetHost(player);
	fmove = g_MoveHelper->m_flForwardMove;
	smove = g_MoveHelper->m_flSideMove;
	g_MoveHelper->SetHost(nullptr);
	junkcode::call();
	if (forward[2] != 0)
	{
		forward[2] = 0;
		Math::NormalizeVector(forward);
	}

	if (right[2] != 0)
	{
		right[2] = 0;
		Math::NormalizeVector(right);
	}

	for (int i = 0; i < 2; i++)	// Determine x and y parts of velocity
		wishvel[i] = forward[i] * fmove + right[i] * smove;

	wishvel[2] = 0;	// Zero out z part of velocity

	wishdir = wishvel; // Determine maginitude of speed of move
	wishspeed = wishdir.Normalize();

	// Clamp to server defined max speed
	g_MoveHelper->SetHost(player);
	if ((wishspeed != 0.0f) && (wishspeed > g_MoveHelper->m_flMaxSpeed))
	{
		VectorMultiply(wishvel, player->m_flMaxspeed() / wishspeed, wishvel);
		wishspeed = player->m_flMaxspeed();
	}
	g_MoveHelper->SetHost(nullptr);
	// Set pmove velocity
	outVel[2] = 0;
	Accelerate(player, wishdir, wishspeed, g_CVar->FindVar("sv_accelerate")->GetFloat(), outVel);
	outVel[2] = 0;
	junkcode::call();
	// Add in any base velocity to the current velocity.
	VectorAdd(outVel, player->m_vecBaseVelocity(), outVel);

	float spd = outVel.Length();

	if (spd < 1.0f)
	{
		outVel.Init();
		// Now pull the base velocity back out. Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(outVel, player->m_vecBaseVelocity(), outVel);
		return;
	}

	g_MoveHelper->SetHost(player);
	g_MoveHelper->m_outWishVel += wishdir * wishspeed;
	g_MoveHelper->SetHost(nullptr);

	// Don't walk up stairs if not on ground.
	if (!(player->m_fFlags() & FL_ONGROUND))
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(outVel, player->m_vecBaseVelocity(), outVel);
		return;
	}
	junkcode::call();
	// Now pull the base velocity back out. Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract(outVel, player->m_vecBaseVelocity(), outVel);
}

void AntiAim::Fakewalk(CUserCmd *usercmd)
{
	if (!g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind))
		return;

	Global::flFakewalked = PredictionSystem::Get().GetOldCurTime();

	Global::bSendPacket = true;
	junkcode::call();
	Vector velocity = Global::vecUnpredictedVel;

	int32_t ticks_to_update = TIME_TO_TICKS(Global::NextLBYUpdate - TICKS_TO_TIME(AimRage::Get().GetTickbase())) - 1;
	int32_t ticks_to_stop;
	for (ticks_to_stop = 0; ticks_to_stop < 15; ticks_to_stop++)
	{
		if (velocity.Length2D() < 0.1f)
			break;

		if (g_LocalPlayer->m_fFlags() & FL_ONGROUND)
		{
			velocity[2] = 0.0f;
			Friction(velocity);
			WalkMove(g_LocalPlayer, velocity);
		}
	}

	const int32_t max_ticks = std::min<int32_t>(14, ticks_to_update);
	const int32_t chocked = Miscellaneous::Get().GetChocked();
	int32_t ticks_left = max_ticks - chocked;

	if (chocked < max_ticks || ticks_to_stop)
		Global::bSendPacket = false;
	else
	{
		Global::bSendPacket = true;
		Global::bFakelag = true;
	}
	junkcode::call();
	if (ticks_to_stop > ticks_left || !chocked || Global::bSendPacket)
	{
		float_t speed = velocity.Length();

		if (speed > 13.f)
		{
			QAngle direction;
			Math::VectorAngles(velocity, direction);

			direction.yaw = usercmd->viewangles.yaw - direction.yaw;

			Vector forward;
			Math::AngleVectors(direction, forward);
			Vector negated_direction = forward * -speed;

			usercmd->forwardmove = negated_direction.x;
			usercmd->sidemove = negated_direction.y;
		}
		else
		{
			usercmd->forwardmove = 0.f;
			usercmd->sidemove = 0.f;
		}
	}
}

void AntiAim::Friction(Vector &outVel)
{
	float speed, newspeed, control;
	float friction;
	float drop;
	junkcode::call();
	speed = outVel.Length();

	if (speed <= 0.1f)
		return;

	drop = 0;

	// apply ground friction
	if (g_LocalPlayer->m_fFlags() & FL_ONGROUND)
	{
		friction = g_CVar->FindVar("sv_friction")->GetFloat() * g_LocalPlayer->m_surfaceFriction();

		// Bleed off some speed, but if we have less than the bleed
		// threshold, bleed the threshold amount.
		control = (speed < g_CVar->FindVar("sv_stopspeed")->GetFloat()) ? g_CVar->FindVar("sv_stopspeed")->GetFloat() : speed;

		// Add the amount to the drop amount.
		drop += control * friction * g_GlobalVars->frametime;
	}
	junkcode::call();
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	if (newspeed != speed)
	{
		// Determine proportion of old speed we are using.
		newspeed /= speed;
		// Adjust velocity according to proportion.
		VectorMultiply(outVel, newspeed, outVel);
	}
}

float AntiAim::GetLbyUpdateTime(bool last)
{
	if (last)
		return m_flLowerBodyLastUpdate;
	else
		return m_flLowerBodyLastUpdate + m_flLowerBodyNextUpdate;
}

void AntiAim::SelectTarget()
{
	for (int index = 1; index <= g_GlobalVars->maxClients; index++)
	{
		//auto pPlayer = reinterpret_cast< C_BasePlayer* >(g_EntityList->GetClientEntity(index));
		C_BasePlayer* pPlayer = C_BasePlayer::GetPlayerByIndex(index);

		if (!AimRage::Get().CheckTarget(index))
			continue;
		junkcode::call();
		float flDist = pPlayer->GetAbsOrigin().DistTo(g_LocalPlayer->GetAbsOrigin());
		bool bInAir = !(pPlayer->m_fFlags() & FL_ONGROUND);
		AntiaimData_t data(index, flDist, bInAir);
		Entities.push_back(data);
	}
}

float AntiAim::PastedFreestanding()
{
	Entities.clear();
	SelectTarget();
	if (Entities.empty())
	{
		return -9999;
	}

	auto eyepos = g_LocalPlayer->GetEyePos();
	auto origin = g_LocalPlayer->GetAbsOrigin();
	auto headpos = g_LocalPlayer->GetHitboxPos(0);

	auto checkWallThickness = [&](C_BasePlayer* pPlayer, Vector newhead) -> float
	{
		Ray_t ray;
		trace_t trace1, trace2;
		Vector endpos1, endpos2;
		Vector eyepos = pPlayer->GetAbsOrigin() + pPlayer->m_vecViewOffset();
		CTraceFilterSkipTwoEntities filter(pPlayer, g_LocalPlayer);

		ray.Init(newhead, eyepos);
		g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace1);

		if (trace1.DidHit())
			endpos1 = trace1.endpos;
		else
			return 0.f;

		ray.Init(eyepos, newhead);
		g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace2);

		if (trace2.DidHit())
			endpos2 = trace2.endpos;

		float add = newhead.DistTo(eyepos) - g_LocalPlayer->GetEyePos().DistTo(eyepos) + 3.f;
		return endpos1.DistTo(endpos2) + add / 3;
	};

	Vector besthead;
	float bestrotation = 0.f;
	float highestthickness = 0.f;
	float step = M_PI * 2.0 / 15;
	float radius = Vector(headpos - origin).Length2D();

	for (float rotation = 0; rotation < (M_PI * 2.0); rotation += step)
	{
		float totalthickness = 0.f;
		Vector newhead(radius * cos(rotation) + eyepos.x, radius * sin(rotation) + eyepos.y, eyepos.z);

		for (auto current : Entities) {
			if(AimRage::Get().CheckTarget(current.iPlayer))
				totalthickness += checkWallThickness(reinterpret_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(current.iPlayer)), newhead);
		}

		if (totalthickness > highestthickness)
		{
			highestthickness = totalthickness;
			bestrotation = rotation;
			besthead = newhead;
		}
	}
	if (highestthickness != 0)
	{
		int temp = 0;
		Vector fliphead(radius * cos(std::max<float>(bestrotation - DEG2RAD(180), 0)) + eyepos.x, radius * sin((std::max<float>(bestrotation - DEG2RAD(180), 0))) + eyepos.y, eyepos.z);
		for (auto current : Entities) {
			if (AimRage::Get().CheckTarget(current.iPlayer))
				temp += checkWallThickness(reinterpret_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(current.iPlayer)), fliphead);
		}
		if (highestthickness - temp > 50)
			return RAD2DEG(bestrotation);
		else
			return -9999;
	}
	else
	{
		QAngle viewangle; g_EngineClient->GetViewAngles(viewangle);

		for (float rotation = 0; rotation < (M_PI * 2.0); rotation += step)
		{
			float totalthickness = 0.f;
			Vector newhead(radius * cos(rotation) + eyepos.x, radius * sin(rotation) + eyepos.y, eyepos.z);

			for (auto current : Entities) {
				if (AimRage::Get().CheckTarget(current.iPlayer))
				{
					float fov = fabsf(Math::ClampYaw(Math::CalcAngle(g_LocalPlayer->GetAbsOrigin(), C_BasePlayer::GetPlayerByIndex(current.iPlayer)->GetAbsOrigin()).yaw - viewangle.yaw));
					if (fov < 50)
						totalthickness += checkWallThickness(reinterpret_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(current.iPlayer)), newhead);
				}
			}

			if (totalthickness > highestthickness)
			{
				highestthickness = totalthickness;
				bestrotation = rotation;
				besthead = newhead;
			}
		}
		if (highestthickness != 0)
		{
			int temp = 0;
			Vector fliphead(radius * cos(std::max<float>(bestrotation - DEG2RAD(180), 0)) + eyepos.x, radius * sin((std::max<float>(bestrotation - DEG2RAD(180), 0))) + eyepos.y, eyepos.z);
			for (auto current : Entities) {
				if (AimRage::Get().CheckTarget(current.iPlayer))
					temp += checkWallThickness(reinterpret_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(current.iPlayer)), fliphead);
			}
			if (highestthickness - temp > 50)
				return RAD2DEG(bestrotation);
			else
				return -9999;
		}
		else
			return -9999;
	}
}

void AntiAim::PastedLBYBreaker(CUserCmd* cmd)
{
	if ((!g_Options.hvh_antiaim_lby_breaker && g_Options.hvh_antiaim_y != AA_YAW_ANTILBY) || !(g_LocalPlayer->m_fFlags() & FL_ONGROUND) || g_Options.hvh_antiaim_new)
		return;

	static int choked = 0;
	static bool wasMoving = true;
	static bool firstBreak = false;
	static bool preBreak = false;
	static bool shouldBreak = false;
	static bool brokeThisTick = false;
	static bool fakebreak = false;
	static double oldCurtime = TICKS_TO_TIME(AimRage::Get().GetTickbase());
	static float oldlby = 0;
	static float target_lby = 0;
	static float target_pre = 0;
	static float pre_delay = 0;
	bool bIsMoving = false;
	
	float lbydelta = g_Options.hvh_antiaim_lby_delta ? g_Options.hvh_antiaim_lby_delta : Utils::RandomFloat(50, 160);

	double server_time = TICKS_TO_TIME(AimRage::Get().GetTickbase());

	QAngle viewangle; g_EngineClient->GetViewAngles(viewangle);

	if (checks::is_bad_ptr(Animation::Get().GetPlayerAnimationInfo(g_LocalPlayer->EntIndex()).m_playerAnimState))
	{	//anim state is invalid. fuck fuck fuck fuck fuck fuck fuck fuck fuck
		bIsMoving = g_LocalPlayer->m_vecVelocity().Length2D() >= 0.1f;
	}
	else
	{	//use anim state velocity for more accurate result
		bIsMoving = Animation::Get().GetPlayerAnimationInfo(g_LocalPlayer->EntIndex()).m_playerAnimState->m_flVelocity() >= 0.1f;
	}

	auto checkWallThickness = [&](C_BasePlayer* pPlayer, Vector newhead) -> float
	{
		Ray_t ray;
		trace_t trace1, trace2;
		Vector endpos1, endpos2;
		Vector eyepos = pPlayer->GetAbsOrigin() + pPlayer->m_vecViewOffset();
		CTraceFilterSkipTwoEntities filter(pPlayer, g_LocalPlayer);

		ray.Init(newhead, eyepos);
		g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace1);

		if (trace1.DidHit())
			endpos1 = trace1.endpos;
		else
			return 0.f;

		ray.Init(eyepos, newhead);
		g_EngineTrace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace2);

		if (trace2.DidHit())
			endpos2 = trace2.endpos;

		float add = newhead.DistTo(eyepos) - g_LocalPlayer->GetEyePos().DistTo(eyepos) + 3.f;
		return endpos1.DistTo(endpos2) + add / 3;
	};

	auto calcWallThickness = [&](Vector pos) -> float
	{
		float thickness = 0;
		for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
		{
			if (AimRage::Get().CheckTarget(i))
			{
				thickness += checkWallThickness(C_BasePlayer::GetPlayerByIndex(i), pos);
			}
		}
		return thickness;
	};

	auto nci = g_EngineClient->GetNetChannelInfo();

	bool shouldFlipLby = (calcWallThickness(rotate_extend_vec(g_LocalPlayer->GetEyePos(), Math::ClampYaw(usercmd->viewangles.yaw + lbydelta), 64)) <= calcWallThickness(rotate_extend_vec(g_LocalPlayer->GetEyePos(), Math::ClampYaw(usercmd->viewangles.yaw + lbydelta + 120), 64)));

	if (Global::bSendPacket)
	{
		brokeThisTick = false;
		choked = g_ClientState->chokedcommands - Global::prevChoked;

		if (bIsMoving && (g_LocalPlayer->m_fFlags() & FL_ONGROUND) && !(g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind) && g_Options.misc_fakewalk))
		{
			oldCurtime = server_time; + nci->GetLatency(FLOW_OUTGOING);
			m_flNextBodyUpdate = server_time + 0.22;
			Global::LastLBYUpdate = server_time;
			Global::NextLBYUpdate = server_time + 0.22;
			wasMoving = true;
			firstBreak = true;
		}
		else
		{
			if (wasMoving && server_time - oldCurtime > 0.22)
			{
				wasMoving = false;
				firstBreak = false;
				shouldBreak = true;
				Global::LastLBYUpdate = server_time;
				Global::NextLBYUpdate = server_time + 1.1;
				m_flNextBodyUpdate = server_time + 1.1;
			}
			else if (server_time - oldCurtime > 1.1)
			{
				shouldBreak = true;
				firstBreak = false;
				Global::LastLBYUpdate = server_time;
				Global::NextLBYUpdate = server_time + 1.1;
				m_flNextBodyUpdate = server_time + 1.1;
			}
			else if (server_time - oldCurtime > 1.1 - (TICKS_TO_TIME(choked) - TICKS_TO_TIME((Global::bFakelag) ? (2) : (0))) && !firstBreak && (lbydelta < 100 || lbydelta > 120))
			{
				preBreak = true;
				pre_delay = TICKS_TO_TIME(choked) - TICKS_TO_TIME((Global::bFakelag) ? (2) : (0));
			}
			else if (abs(Math::ClampYaw(oldlby - g_LocalPlayer->m_flLowerBodyYawTarget())) > 1 && abs(Math::ClampYaw(target_lby - g_LocalPlayer->m_flLowerBodyYawTarget())) > 10)
			{
				m_flNextBodyUpdate = server_time + g_GlobalVars->interval_per_tick + 1.1;
				oldCurtime = server_time; + nci->GetLatency(FLOW_INCOMING);
				//if (abs(Math::ClampYaw(target_pre - g_LocalPlayer->m_flLowerBodyYawTarget())) < 1) oldCurtime += pre_delay;
				Global::LastLBYUpdate = server_time;
				Global::NextLBYUpdate = server_time + 1.1;
				m_flNextBodyUpdate = server_time + 1.1;
				firstBreak = true;
			}
		}
		oldlby = g_LocalPlayer->m_flLowerBodyYawTarget();
	}
	else if (g_Options.hvh_antiaim_y == AA_YAW_ANTILBY)
	{
		if (m_flFreestandingYaw < -180) m_flFreestandingYaw = PastedFreestanding();

		if (m_flFreestandingYaw < -180)
		{
			if (antiresolve)
			{
				cmd->viewangles.yaw += lbydelta + 180;
				antiresolve = false;
			}
			else if (shouldBreak)
			{
				shouldBreak = false;
				brokeThisTick = true;
				oldCurtime = server_time;

				cmd->viewangles.yaw += ((lbydelta + (g_Options.hvh_antiaim_lby_dynamic ? Utils::RandomFloat(-30, 30) : 0)) * (shouldFlipLby ? -1 : 1));

				target_lby = Math::ClampYaw(cmd->viewangles.yaw);

				if (abs(cmd->viewangles.pitch) < 50)
					cmd->viewangles.pitch = (Utils::RandomInt(0, 1) ? 88.99f : -88.99f);
				else
					cmd->viewangles.pitch *= (Utils::RandomFloat(-0.75, 0.25));
			}
			/*
			else if (preBreak)
			{
				preBreak = false;
				brokeThisTick = true;
				float addAngle = Global::fps >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.9 * max(g_ClientState->chokedcommands, Global::prevChoked) + 100) : 145.f;
				if (abs(Math::ClampYaw(cmd->viewangles.yaw = jitterless_real + ((lbydelta + (g_Options.hvh_antiaim_lby_dynamic ? Utils::RandomFloat(-30, 30) : 0)) * (shouldFlipLby ? -1 : 1)) - target_lby)) < 10)
					cmd->viewangles.yaw = target_lby + addAngle;
				else
					cmd->viewangles.yaw += ((lbydelta + addAngle + (g_Options.hvh_antiaim_lby_dynamic ? Utils::RandomFloat(-30, 30) : 0)) * (shouldFlipLby ? -1 : 1));

				target_pre = Math::ClampYaw(cmd->viewangles.yaw);

				if (abs(cmd->viewangles.pitch) < 50)
					cmd->viewangles.pitch = (Utils::RandomInt(0, 1) ? 88.99f : -88.99f);
				else
					cmd->viewangles.pitch *= (Utils::RandomFloat(-0.75, 0.25));
			}
			*/
		}
		else
		{
			if (antiresolve)
			{
				cmd->viewangles.yaw = m_flFreestandingYaw + 180;
				antiresolve = false;
			}
			else if (shouldBreak)
			{
				shouldBreak = false;
				brokeThisTick = true;
				oldCurtime = server_time;

				if (abs(Math::ClampYaw(target_lby - m_flFreestandingYaw)) < 30)
					cmd->viewangles.yaw = target_lby;
				else
					cmd->viewangles.yaw = m_flFreestandingYaw;

				target_lby = Math::ClampYaw(cmd->viewangles.yaw);
				cmd->viewangles.pitch = 88.99f;
			}
			/*
			else if (preBreak)
			{
				preBreak = false;
				brokeThisTick = true;
				float addAngle = Global::fps >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.9 * max(g_ClientState->chokedcommands, Global::prevChoked) + 100) : 145.f;
				if (abs(Math::ClampYaw(target_lby - m_flFreestandingYaw)) < 30)
					cmd->viewangles.yaw = target_lby + addAngle;
				else
					cmd->viewangles.yaw = m_flFreestandingYaw + addAngle;

				target_pre = Math::ClampYaw(cmd->viewangles.yaw);
				cmd->viewangles.pitch = 88.99f;
			}
			*/
		}

		if (brokeThisTick)
		{
			if (!checks::is_bad_ptr(AimRage::Get().local_weapon) && AimRage::Get().local_weapon->IsMiscellaneousWeapon())
			{
				cmd->buttons &= ~IN_ATTACK;
				cmd->buttons &= ~IN_ATTACK2;
			}
		}
	}
	else if (m_flFreestandingYaw < -180)
	{
		if (antiresolve)
		{
			cmd->viewangles.yaw += lbydelta + 180;
			antiresolve = false;
		}
		else if (shouldBreak)
		{
			shouldBreak = false;
			brokeThisTick = true;
			oldCurtime = server_time;

			cmd->viewangles.yaw += (lbydelta + (g_Options.hvh_antiaim_lby_dynamic ? Utils::RandomFloat(-30, 30) : 0)) * (shouldFlipLby ? -1 : 1);

			target_lby = Math::ClampYaw(cmd->viewangles.yaw);

			if (abs(cmd->viewangles.pitch) < 50)
				cmd->viewangles.pitch = (Utils::RandomInt(0, 1) ? 88.99f : -88.99f);
			else
				cmd->viewangles.pitch *= (Utils::RandomFloat(-0.75, 0.25));
		}
		else if (preBreak)
		{
			preBreak = false;
			brokeThisTick = true;
			float addAngle = Global::fps >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.9 * max(g_ClientState->chokedcommands, Global::prevChoked) + 100) : 145.f;
			cmd->viewangles.yaw += (lbydelta + addAngle +(g_Options.hvh_antiaim_lby_dynamic ? Utils::RandomFloat(-30, 30) : 0)) * (shouldFlipLby ? -1 : 1);

			target_pre = Math::ClampYaw(cmd->viewangles.yaw);

			if (abs(cmd->viewangles.pitch) < 50)
				cmd->viewangles.pitch = (Utils::RandomInt(0, 1) ? 88.99f : -88.99f);
			else
				cmd->viewangles.pitch *= (Utils::RandomFloat(-0.75, 0.25));
		}

		if (brokeThisTick)
		{
			if (!checks::is_bad_ptr(AimRage::Get().local_weapon) && !AimRage::Get().local_weapon->IsMiscellaneousWeapon())
			{
				cmd->buttons &= ~IN_ATTACK;
				cmd->buttons &= ~IN_ATTACK2;
			}
		}
	}
	else
	{
		if (antiresolve)
		{
			cmd->viewangles.yaw = m_flFreestandingYaw + lbydelta + 180;
			antiresolve = false;
		}
		else if (shouldBreak)
		{
			shouldBreak = false;
			brokeThisTick = true;
			oldCurtime = server_time;

			cmd->viewangles.yaw = m_flFreestandingYaw + lbydelta + (g_Options.hvh_antiaim_lby_dynamic ? Utils::RandomFloat(-30, 30) : 0);

			target_lby = Math::ClampYaw(cmd->viewangles.yaw);
			cmd->viewangles.pitch = 88.99f;
		}
		else if (preBreak)
		{
			preBreak = false;
			brokeThisTick = true;
			float addAngle = Global::fps >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.9 * max(g_ClientState->chokedcommands, Global::prevChoked) + 100) : 145.f;
			cmd->viewangles.yaw = m_flFreestandingYaw + lbydelta + addAngle + (g_Options.hvh_antiaim_lby_dynamic ? Utils::RandomFloat(-30, 30) : 0);

			target_pre = Math::ClampYaw(cmd->viewangles.yaw);
			cmd->viewangles.pitch = 88.99f;
		}

		if (brokeThisTick)
		{
			if (!checks::is_bad_ptr(AimRage::Get().local_weapon) && AimRage::Get().local_weapon->IsMiscellaneousWeapon())
			{
				cmd->buttons &= ~IN_ATTACK;
				cmd->buttons &= ~IN_ATTACK2;
			}
		}
	}
}

bool AntiAim::IsFreestanding()
{
	return m_flFreestandingYaw > -181;
}
float AntiAim::GetFreestandingYaw()
{
	return m_flFreestandingYaw;
}