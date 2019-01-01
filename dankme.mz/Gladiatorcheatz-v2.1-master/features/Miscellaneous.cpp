#include "Miscellaneous.hpp"

#include "../Structs.hpp"
#include "../Options.hpp"
#include "../helpers/Math.hpp"

#include "AntiAim.hpp"
#include "AimRage.hpp"
#include "PredictionSystem.hpp"

#include <chrono>
#include <fstream>

template<class T, class U>
T Clamp(T in, U low, U high)
{
	if (in <= low)
		return low;

	if (in >= high)
		return high;

	return in;
}

std::vector<std::string> msgs =
{
	"Learn Good, learncpp.com",
	"learncpp.com Best FREE CSGO Cheat",
	"learncpp.com owns me and all",
	"unknowncheats.me Best FREE CSGO Cheat",
	"unknowncheats.me Owns me and all",
	"mpgh.net Best FREE CSGO Cheat",
	"mpgh.net Owns me and all"
};

void Miscellaneous::Bhop(CUserCmd *userCMD)
{
	if (g_LocalPlayer->GetMoveType() == MOVETYPE_NOCLIP || g_LocalPlayer->GetMoveType() == MOVETYPE_LADDER) return;
	/*
	if (userCMD->buttons & IN_JUMP && !(g_LocalPlayer->m_fFlags() & FL_ONGROUND)) {
		userCMD->buttons &= ~IN_JUMP;
		//Global::bSendPacket = true;
	}
	*/

	if (g_Options.misc_smacbypass && g_LocalPlayer->m_vecVelocity().z > 10)
	{
		userCMD->buttons |= IN_JUMP;
	}
	else
	{
		if (!bJumped && bFake)
		{
			bFake = false;
			userCMD->buttons |= IN_JUMP;
		}
		else if (userCMD->buttons & IN_JUMP)
		{
			if (g_LocalPlayer->m_fFlags() & FL_ONGROUND)
			{
				bJumped = true;
				bFake = true;
			}
			else
			{
				userCMD->buttons &= ~IN_JUMP;
				bJumped = false;
			}
		}
		else
		{
			bJumped = false;
			bFake = false;
		}
	}
}

bool will_hit_obstacle_in_future(float predict_time, float step)
{
	auto local_player = g_LocalPlayer;
	if (!local_player) return false;

	static auto sv_gravity = g_CVar->FindVar("sv_gravity");
	static auto sv_jump_impulse = g_CVar->FindVar("sv_jump_impulse");

	bool ground = local_player->m_fFlags() & FL_ONGROUND;
	auto gravity_per_tick = sv_gravity->GetFloat() * g_GlobalVars->interval_per_tick;

	auto start = local_player->GetAbsOrigin(), end = start;
	auto velocity = local_player->m_vecVelocity();

	auto min = local_player->GetVecMins(), maxs = local_player->GetVecMaxs();

	trace_t trace;
	CTraceFilterWorldOnly filter;
	Ray_t ray;

	auto predicted_ticks_needed = TIME_TO_TICKS(predict_time);
	auto velocity_rotation_angle = RAD2DEG(atan2(velocity.y, velocity.x));
	auto ticks_done = 0;

	if (predicted_ticks_needed <= 0)
		return false;

	while (true)
	{
		auto rotation_angle = velocity_rotation_angle + step;

		velocity.x = cos(DEG2RAD(rotation_angle)) * velocity.Length2D();
		velocity.y = sin(DEG2RAD(rotation_angle)) * velocity.Length2D();
		velocity.z = ground ? sv_jump_impulse->GetFloat() : velocity.z - gravity_per_tick;

		end += velocity * g_GlobalVars->interval_per_tick;

		ray.Init(start, end, min, maxs);
		g_EngineTrace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);

		if (trace.fraction != 1.f && trace.plane.normal.z <= 0.9f || trace.startsolid || trace.allsolid)
			break;

		end = trace.endpos;
		end.z -= 2.f;

		ray.Init(trace.endpos, end, min, maxs);
		g_EngineTrace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);

		ground = (trace.fraction < 1.f || trace.allsolid || trace.startsolid) && trace.plane.normal.z >= 0.7f;

		if (++ticks_done >= predicted_ticks_needed)
			return false;

		velocity_rotation_angle = rotation_angle;
	}
	return true;
}

bool get_closest_plane(Vector* plane)
{
	auto local_player = g_LocalPlayer;
	if (!local_player) return false;

	trace_t trace; CTraceFilterWorldOnly filter; Ray_t ray;

	auto start = local_player->GetAbsOrigin(), mins = local_player->GetVecMins(), maxs = local_player->GetVecMaxs();

	Vector planes;
	auto count = 0;

	for (auto step = 0.f; step <= M_PI * 2.f; step += M_PI / 10.f)
	{
		auto end = start;

		end.x += cos(step) * 64.f;
		end.y += sin(step) * 64.f;

		ray.Init(start, end, mins, maxs);
		g_EngineTrace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);

		if (trace.fraction < 1.f)
		{
			planes += trace.plane.normal;
			count++;
		}
	}

	planes /= count;

	if (planes.z < 0.1f) { *plane = planes; return true; }
	return false;
}

inline float NormalizeRad(float a)
{
	a = std::fmod(a, M_PI * 2);
	if (a >= M_PI)
		a -= 2 * M_PI;
	else if (a < -M_PI)
		a += 2 * M_PI;
	return a;
}
float MaxAccelTheta(Vector vel, float wishspeed)	//pasted from hlstrafe lmao
{
	float accel = g_CVar->FindVar("sv_airaccelerate")->GetFloat();
	float accelspeed = accel * wishspeed * g_GlobalVars->interval_per_tick;

	if (accelspeed <= 0.0)
		return M_PI;

	if (vel.x == 0 && vel.y == 0)
		return 0.0;

	float wishspeed_capped = 30;
	float tmp = wishspeed_capped - accelspeed;
	float speed = vel.Length2D();
	if (abs(tmp) < 1.f || abs(tmp) < speed)
		return std::acos(tmp / max(speed, 1));
	/*
	if (tmp <= 0.0)
		return M_PI / 2;

	if (tmp < speed)
		return std::acos(tmp / speed);
		*/

	return 0.0;
}
float MaxAccelIntoYawTheta(Vector vel, float wishspeed, float vel_yaw, float yaw)
{
	if (!(vel.x == 0 && vel.y == 0))
		vel_yaw = std::atan2(vel[1], vel[0]);

	float theta = MaxAccelTheta(vel, wishspeed);
	if (theta == 0.0 || theta == M_PI)
		return NormalizeRad(yaw - vel_yaw + theta);
	return std::copysign(theta, NormalizeRad(yaw - vel_yaw));
}
float MaxAngleTheta(Vector vel, float wishspeed)
{
	float speed = vel.Length2D();
	float accel = g_CVar->FindVar("sv_airaccelerate")->GetFloat();
	float accelspeed = accel * wishspeed * g_GlobalVars->interval_per_tick;

	if (accelspeed <= 0.0)
	{
		float wishspeed_capped = 30;
		accelspeed *= -1;
		if (accelspeed >= speed)
		{
			if (wishspeed_capped >= speed)
				return 0.0;
			else
			{
				return std::acos(wishspeed_capped / speed); // The actual angle needs to be _less_ than this.
			}
		}
		else
		{
			if (wishspeed_capped >= speed)
				return std::acos(accelspeed / speed);
			else
			{
				return std::acos(min(accelspeed, wishspeed_capped) / speed); // The actual angle needs to be _less_ than this if wishspeed_capped <= accelspeed.
			}
		}
	}
	else
	{
		if (accelspeed >= speed)
			return M_PI;
		else
			return std::acos(-1 * accelspeed / speed);
	}
}

void Miscellaneous::circle_strafe(CUserCmd* cmd, float* circle_yaw)
{
	auto local_player = g_LocalPlayer;
	if (!local_player) return;

	auto velocity_2d = local_player->m_vecVelocity().Length2D();
	auto velocity = local_player->m_vecVelocity();
	velocity.z = 0.f;

	const auto min_step = 1.0f;
	float max_step = RAD2DEG(MaxAccelIntoYawTheta(g_LocalPlayer->m_vecVelocity(), g_CVar->FindVar("sv_maxspeed")->GetFloat(), 0, *circle_yaw)) / 4;
	auto ideal_strafe = max_step / 4;

	auto predict_time = clamp(295.f / velocity_2d, 0.25f, 1.15f);
	//auto predict_time = clamp(circlevel / velocity_2d, circlemin, circlemax);

	auto step = ideal_strafe;

	//extern QAngle org_angle; //getto
	QAngle temp;
	
	while (true)
	{
		if (!will_hit_obstacle_in_future(predict_time, step) || step > max_step)
			break;

		step += 0.2f;
	}

	if (step > max_step)
	{
		step = ideal_strafe;

		while (true)
		{
			if (!will_hit_obstacle_in_future(predict_time, step) || step < -min_step)
				break;

			step -= 0.2f;
		}

		if (step < -min_step)
		{
			Vector plane;
			if (get_closest_plane(&plane))
			{
				temp = QAngle (0, (*circle_yaw - RAD2DEG(atan2(plane.y, plane.x))) * 0.03f, 0);
				Math::NormalizeAngles(temp);
				step = -temp.yaw;
			}
		}
		else
			step -= 0.2f;
	}
	else
		step += 0.2f;

	temp = QAngle(0, *circle_yaw + step, 0);
	Math::NormalizeAngles(temp);
	//org_angle.yaw = *circle_yaw = temp.yaw;
	cmd->viewangles.yaw = *circle_yaw = temp.yaw;
	cmd->sidemove = copysign(450.f, -step);
}

void Miscellaneous::AutoStrafe(CUserCmd *userCMD)
{
	if (g_LocalPlayer->GetMoveType() == MOVETYPE_NOCLIP || g_LocalPlayer->GetMoveType() == MOVETYPE_LADDER || !g_LocalPlayer->IsAlive()) return;

	static auto circle_yaw = 0.f;
	static auto old_yaw = 0.f;

	/*
	const auto min_step = 2.25f;
	float max_step = clamp(g_CVar->FindVar("sv_airaccelerate")->GetFloat() / 4, 2.55f, 25.f);

	auto ideal_strafe = clamp(get_move_angle(g_LocalPlayer->m_vecVelocity().Length2D()) * 2.f, min_step, max_step);
	*/

	auto ideal_strafe = RAD2DEG(MaxAccelIntoYawTheta(g_LocalPlayer->m_vecVelocity(), g_CVar->FindVar("sv_maxspeed")->GetFloat(), 0, userCMD->viewangles.yaw));


	// If we're not jumping or want to manually move out of the way/jump over an obstacle don't strafe.
	if (!g_InputSystem->IsButtonDown(ButtonCode_t::KEY_SPACE) ||
		g_InputSystem->IsButtonDown(ButtonCode_t::KEY_A) ||
		g_InputSystem->IsButtonDown(ButtonCode_t::KEY_D) ||
		g_InputSystem->IsButtonDown(ButtonCode_t::KEY_S) ||
		g_InputSystem->IsButtonDown(ButtonCode_t::KEY_W))
		return;

	if (!(g_LocalPlayer->m_fFlags() & FL_ONGROUND))
	{
		if (userCMD->mousedx > 1 || userCMD->mousedx < -1)
		{
			userCMD->sidemove = clamp(userCMD->mousedx < 0.f ? -400.f : 400.f, -400, 400);
		}
		else
		{
			QAngle temp; g_EngineClient->GetViewAngles(temp); temp.yaw -= old_yaw; Math::NormalizeAngles(temp);
			auto yaw_delta = temp.yaw;
			auto absolute_yaw_delta = abs(yaw_delta);

			QAngle temp2; g_EngineClient->GetViewAngles(temp2);
			if (g_InputSystem->IsButtonDown(g_Options.misc_circlestrafe_bind))
			{
				circle_strafe(userCMD, &circle_yaw);
				old_yaw = circle_yaw;
				return;
			}

			//if (absolute_yaw_delta <= ideal_strafe)
			{
				if (g_LocalPlayer->m_vecVelocity().Length2D() == 0 || g_LocalPlayer->m_vecVelocity().Length2D() == NAN || g_LocalPlayer->m_vecVelocity().Length2D() == INFINITE)
				{
					userCMD->forwardmove = 400;
					return;
				}

				userCMD->forwardmove = clamp(5850.f / g_LocalPlayer->m_vecVelocity().Length2D(), -400, 400);
				if (userCMD->forwardmove < -400 || userCMD->forwardmove > 400)
					userCMD->forwardmove = 0;
				userCMD->sidemove = clamp((userCMD->command_number % 2) == 0 ? -400.f : 400.f, -400, 400);
				if (userCMD->sidemove < -400 || userCMD->sidemove > 400)
					userCMD->sidemove = 0;

				userCMD->viewangles.yaw += ideal_strafe * (userCMD->sidemove < 0) ? (-1) : (1);
				old_yaw = userCMD->viewangles.yaw;
				circle_yaw = old_yaw;
			}
			//else
			//{
			//	old_yaw += ideal_strafe * (userCMD->sidemove < 0) ? (-1) : (1);
			//	circle_yaw = old_yaw;
			//}
		}
	}
}

void Miscellaneous::EdgeLag(CUserCmd *userCMD)
{
	if (!g_Options.misc_fakelag_enabled)
		return;
	if (Global::flFakewalked == PredictionSystem::Get().GetOldCurTime())
		return;

	static int choke = (1 / g_GlobalVars->interval_per_tick) + choked;	//choke 1 sec

	if (!edgelag)
	{
		if ((abs(userCMD->forwardmove) > 10 || abs(userCMD->sidemove) > 10) && g_LocalPlayer->m_fFlags() & FL_ONGROUND)
		{
			Vector pos = g_LocalPlayer->GetEyePos();
			Vector predicted_pos = pos + (g_LocalPlayer->m_vecVelocity() * (1 / g_GlobalVars->interval_per_tick));

			float posdmg = 0;
			float predposdmg = 0;

			for (int i = 1; i < 65; i++)
			{
				if (!AimRage::Get().CheckTarget(i))
					continue;

				auto player = C_BasePlayer::GetPlayerByIndex(i);

				posdmg += AimRage::Get().GetDamageVec2(pos, player->GetEyePos(), g_LocalPlayer, player, HITBOX_HEAD).damage;
				predposdmg += AimRage::Get().GetDamageVec2(predicted_pos, player->GetEyePos(), g_LocalPlayer, player, HITBOX_HEAD).damage;
			}

			if (predposdmg - posdmg > 100)
			{
				edgelag = true;
				choke = (1 / g_GlobalVars->interval_per_tick) + choked;
			}
		}
	}
	else
	{
		if (choked > choke || !((abs(userCMD->forwardmove) > 10 || abs(userCMD->sidemove) > 10) && g_LocalPlayer->m_fFlags() & FL_ONGROUND))
		{
			edgelag = false;
			AimRage::Get().delay_firing = false;
			choked = 0;
		}
		else
		{
			choked++;
		}
	}
}

void Miscellaneous::Fakelag(CUserCmd *userCMD)
{
	if (bFake && !g_Options.misc_smacbypass)
	{
		Global::bSendPacket = false;
		Global::bFakelag = true;
		choked++;
		return;
	}

	if (!g_Options.misc_fakelag_enabled)
		return;

	static Vector last_pos = Vector(0, 0, 0);
	static float lcbreak_dest = 4096;
	int choke = std::min<int>(g_Options.misc_fakelag_value, 14);
	if (g_Options.misc_fakelag_adaptive)
	{
		switch (g_Options.misc_fakelag_adeptive_type)
		{
		case 0: choke = (g_LocalPlayer->m_fFlags() & FL_ONGROUND) ? (10) : (14); break;
		case 1: choke = 14; break;
		case 2: choke = 64; break;
		}
	}

	const float cock_time = 0.234375f;
	int cock_needed = cock_time / g_GlobalVars->interval_per_tick;

	if (!g_InputSystem->IsButtonDown(g_Options.misc_fakewalk_bind))
	{
		if (Global::bAimbotting)
		{
			choked = 0;
			return;
		}
		if ((g_Options.misc_fakelag_activation_type == 1 && g_LocalPlayer->m_vecVelocity().Length() < 3.0f)
			|| (g_Options.misc_fakelag_activation_type == 2 && (g_LocalPlayer->m_fFlags() & FL_ONGROUND))
			|| (!(g_LocalPlayer->m_fFlags() & FL_ONGROUND) && choked >= 13)
			|| (!(g_LocalPlayer->m_fFlags() & FL_ONGROUND) && g_LocalPlayer->m_vecVelocity().Length2D() < 100.0f && choked >= 6))
		{
			choked = 0;
			return;
		}

		if (g_Options.rage_autocockrevolver && !checks::is_bad_ptr(AimRage::Get().local_weapon) && AimRage::Get().local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER && AimRage::Get().revolver_cocks >= cock_needed - 2)
		{
			choked = 0;
			return;
		}
	}

	if (!(Global::flFakewalked == PredictionSystem::Get().GetOldCurTime()))
	{
		Global::bSendPacket = (choked >= choke);

		if (g_Options.misc_fakelag_adaptive && pow((g_LocalPlayer->m_vecOrigin() - last_pos).Length2D(), 2) > Utils::RandomInt(lcbreak_dest, lcbreak_dest * 1.5))
			Global::bSendPacket = true;
	}

	if (Global::bSendPacket)
	{
		last_pos = g_LocalPlayer->m_vecOrigin();
		choked = 0;

	}
	else
		choked++;

	Global::bFakelag = true;
}

void Miscellaneous::ChangeName(const char *name)
{
	static float timestamp = 0;
	ConVar *cv = g_CVar->FindVar("name");
	*(int*)((DWORD)&cv->m_fnChangeCallbacks + 0xC) = 0;/*
	if (bDoNameExploit)
	{
		cv->SetValue("\n\xAD\xAD\xAD­­­");
		bDoNameExploit = false;
		timestamp = g_GlobalVars->curtime;
	}
	else if (abs(g_GlobalVars->curtime - timestamp) > 0.2)
		cv->SetValue(name);*/
	
	if (bDoNameExploit)
	{
		NET_SetConVar exploit("name", "\n\xAD\xAD\xAD­­­");
		((INetChannel*)g_EngineClient->GetNetChannelInfo())->SendNetMsg(&exploit, false);
		bDoNameExploit = false;
	}
	NET_SetConVar convar("name", name);
	((INetChannel*)g_EngineClient->GetNetChannelInfo())->SendNetMsg(&convar, false);
}

void Miscellaneous::NameChanger()
{
	if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
		return;

	if (changes == -1)
		return;

	long curTime;
	if (Global::use_ud_vmt)
		curTime = g_GlobalVars->realtime * 1000;	//idk why we were using chrono in the first place but i'm sticking with that
	else
		curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	static long timestamp = curTime;

	if ((curTime - timestamp) < 150)
		return;

	timestamp = curTime;
	++changes;

	if (changes >= 5)
	{
		std::string name = "\n\xAD\xAD\xAD";
		char chars[3] = { '\n', '\xAD', '\t' };

		for (int i = 0; i < 127; i++)
			name += chars[rand() % 2];

		name += "dankme.mz repasted";

		ChangeName(name.c_str());

		changes = -1;

		return;
	}
	ChangeName(setStrRight("dankme.mz repasted", strlen("dankme.mz repasted") + changes));
}

void Miscellaneous::NameStealer()
{
	if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected() || !g_Options.misc_stealname)
		return;

	/*
	if (bDoNameExploit)
	{
		ConVar *cv = g_CVar->FindVar("name");
		*(int*)((DWORD)&cv->m_fnChangeCallbacks + 0xC) = 0;
		cv->SetValue("\n\xAD\xAD\xAD­­­");
		bDoNameExploit = false;
	}
	else*/
	{

		long curTime;
		if (Global::use_ud_vmt)
			curTime = g_GlobalVars->realtime * 1000;	//idk why we were using chrono in the first place but i'm sticking with that
		else
			curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		static long timestamp = curTime;

		static std::deque<std::string> names;


		if (names.empty() || curTime - timestamp > 1000)
		{
			names.clear();
			for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
			{
				auto target = C_BasePlayer::GetPlayerByIndex(i);

				if (checks::is_bad_ptr(target) || target == g_LocalPlayer || !target->IsPlayer())
					continue;

				std::string text = target->GetName();
				text += "  \n";
				names.push_front(text);
			}
			timestamp = curTime;
		}

		if (names.empty())
			return;

		ChangeName(names.at(Utils::RandomInt(0, names.size() - 1)).c_str());
	}
}

void Miscellaneous::AntiAyyware()
{
	if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
		return;

	/*
	if (bDoNameExploit)
	{
		ConVar *cv = g_CVar->FindVar("name");
		*(int*)((DWORD)&cv->m_fnChangeCallbacks + 0xC) = 0;
		cv->SetValue("\n\xAD\xAD\xAD­­­");
		bDoNameExploit = false;
	}
	else*/
	{

		long curTime;
		if (Global::use_ud_vmt)
			curTime = g_GlobalVars->realtime * 1000;	//idk why we were using chrono in the first place but i'm sticking with that
		else
			curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		static long timestamp = curTime;

		if ((curTime - timestamp) < 100)
			return;

		static bool flip = true;

		flip = !flip;

		std::string crasher_nick = (flip) ? ("rekting ayyware scam...") : ("...rekting ayyware scam");

		while (crasher_nick.length() < 100 * sizeof(char))
		{
			crasher_nick += (char)Utils::RandomInt(0x20, 0x7F);
		}
		crasher_nick += (char)0;

		ChangeName(crasher_nick.c_str());

		timestamp = curTime;
	}
}

void Miscellaneous::NameHider()
{
	if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected() || g_Options.misc_animated_clantag)
		return;

	static size_t lastTime = 0;

	if (GetTickCount() > lastTime)
	{
		Utils::SetClantag("\xAD\xAD\xAD\n");
		lastTime = GetTickCount() + 100;
	}
}

void Miscellaneous::NameSpam()
{
	if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
		return;

	static int count = 0;
	/*
	ConVar *cv = g_CVar->FindVar("name");
	*(int*)((DWORD)&cv->m_fnChangeCallbacks + 0xC) = 0;
	if (bDoNameExploit)
	{
		cv->SetValue("\n\xAD\xAD\xAD­­­");
		bDoNameExploit = false;
	}
	else*/
	{
		std::string temp, temp2;
		switch (count)
		{
		case 0: temp2 = "<"; break;
		case 1: temp2 = ">"; break;
		case 2: temp2 = "@"; break;
		case 3: temp2 = "$"; break;
		case 4: temp2 = "&"; break;
		}
		temp = temp2;
		temp += (count % 2) ? ("...dankme.mz premium pasta") : ("dankme.mz premium pasta...");
		temp += temp2;
		count++;
		count %= 5;

		//cv->SetValue(temp.c_str());
		ChangeName(temp.c_str());
	}
}

const char *Miscellaneous::setStrRight(std::string txt, unsigned int value)
{
	txt.insert(txt.length(), value - txt.length(), ' ');

	return txt.c_str();
}

void Miscellaneous::ChatSpamer()
{
	if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
		return;

	long curTime;
	if(Global::use_ud_vmt)
		curTime = g_GlobalVars->realtime * 1000;	//idk why we were using chrono in the first place but i'm sticking with that
	else
		curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	static long timestamp = curTime;

	if ((curTime - timestamp) < 850)
		return;

	if (g_Options.misc_chatspamer)
	{
		if (msgs.empty())
			return;

		std::srand(time(NULL));

		std::string msg = msgs[rand() % msgs.size()];

		std::string str;
		str.append("say ");
		str.append(msg);

		g_EngineClient->ExecuteClientCmd(str.c_str());
	}
	timestamp = curTime;
}

void Miscellaneous::ClanTag()
{
	if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
		return;

	if (!g_Options.misc_animated_clantag)
		return;

	static int local_tickbase = g_LocalPlayer->m_nTickBase();
	static float last_realtime = g_GlobalVars->realtime;

	int serverTime = (float)g_LocalPlayer->m_nTickBase() * g_GlobalVars->interval_per_tick * 2.5;
	static int last_serverTime = serverTime;
	if (local_tickbase == g_LocalPlayer->m_nTickBase())
	{
		serverTime += (g_GlobalVars->realtime - last_realtime);
	}
	else
	{
		local_tickbase = g_LocalPlayer->m_nTickBase();
		last_realtime = g_GlobalVars->realtime;
	}

	if (serverTime - last_serverTime < 15 * g_GlobalVars->interval_per_tick && abs(serverTime - last_serverTime) < 10)
		return;

	last_serverTime = serverTime;

	std::string tempstring = gladTag;
	std::rotate(tempstring.begin(), tempstring.begin() + (serverTime % tempstring.length()), tempstring.end());

	if (g_Options.misc_hidename)
		tempstring.insert(14, "\n");
	Utils::SetClantag(tempstring.c_str());
}

void Miscellaneous::ThirdPerson()
{
	auto GetCorrectDistance = [](float ideal_distance) -> float
	{
		QAngle inverse_angles;
		g_EngineClient->GetViewAngles(inverse_angles);

		inverse_angles.pitch *= -1.f, inverse_angles.yaw += 180.f;

		Vector direction;
		Math::AngleVectors(inverse_angles, direction);

		CTraceFilterWorldOnly filter;
		trace_t trace;
		Ray_t ray;

		ray.Init(g_LocalPlayer->GetEyePos(), g_LocalPlayer->GetEyePos() + (direction * ideal_distance));
		g_EngineTrace->TraceRay(ray, MASK_ALL, &filter, &trace);

		return (ideal_distance * trace.fraction) - 10.f;
	};

	static size_t lastTime = 0;

	if (g_InputSystem->IsButtonDown(g_Options.misc_thirdperson_bind))
	{
		if (GetTickCount() > lastTime) {
			g_Options.misc_thirdperson = !g_Options.misc_thirdperson;

			lastTime = GetTickCount() + 650;
		}
	}

	g_Input->m_fCameraInThirdPerson = g_Options.misc_thirdperson && g_LocalPlayer && g_LocalPlayer->IsAlive();
	g_Input->m_vecCameraOffset.z = GetCorrectDistance(100.f);
}

void Miscellaneous::DetectAC(acinfo &output)
{
	g_EngineClient->ExecuteClientCmd("clear");
	ConVar *con_filter_enable = g_CVar->FindVar("con_filter_enable");
	if (checks::is_bad_ptr(con_filter_enable))
		return;
	bool backup = con_filter_enable->GetBool();
	if (con_filter_enable->GetBool())
		con_filter_enable->SetValue(0);

	std::ofstream file;
	file.open("csgo\cfg\findac.cfg", std::ios::app);
	file << "sm plugins list" << std::endl;
	for (int i = 1; i < 10; i++)
	{
		file << "sm plugins " << std::to_string(i) << std::to_string(i) << std::endl;
	}
	file << "condump" << std::endl;
	file.close();
	g_EngineClient->ExecuteClientCmd("exec findac");

	std::string dump;
	std::ifstream dumpfile("csgo\condump000.txt");
	if (!dumpfile.is_open())
		return;
	dumpfile.seekg(dumpfile.end);
	int lenght = dumpfile.tellg();
	dumpfile.seekg(dumpfile.beg);
	char* dumpcstr = new char [lenght];

	dumpfile.read(dumpcstr, lenght);
	dumpfile.close();
	dump = dumpcstr;

	if (dump.find("SMAC") != std::string::npos)	// gay af stack of if but there's no other great way to do this
		output.smac_core = true;
	if (dump.find("SMAC Aimbot") != std::string::npos)
		output.smac_aimbot = true;
	if (dump.find("SMAC AutoTrigger") != std::string::npos)
		output.smac_autotrigger = true;
	if (dump.find("SMAC Client") != std::string::npos)
		output.smac_client = true;
	if (dump.find("SMAC Command") != std::string::npos)
		output.smac_commands = true;
	if (dump.find("SMAC ConVar") != std::string::npos)
		output.smac_cvars = true;
	if (dump.find("SMAC Eye Angle") != std::string::npos)
		output.smac_eyetest = true;
	if (dump.find("SMAC Anti-Speedhack") != std::string::npos)
		output.smac_speedhack = true;
	if (dump.find("SMAC Spinhack") != std::string::npos)
		output.smac_spinhack = true;

	std::remove("csgo\condump000.txt");

	if (backup)
		con_filter_enable->SetValue(1);
}

void Miscellaneous::PunchAngleFix_RunCommand(void* base_player)
{
	if (g_LocalPlayer &&  g_LocalPlayer->IsAlive() && g_LocalPlayer == (C_BasePlayer*)base_player)
		m_aimPunchAngle[AimRage::Get().GetTickbase() % 128] = g_LocalPlayer->m_aimPunchAngle();
}

void Miscellaneous::PunchAngleFix_FSN()
{
	if (g_LocalPlayer && g_LocalPlayer->IsAlive())
	{
		QAngle new_punch_angle = m_aimPunchAngle[AimRage::Get().GetTickbase() % 128];

		if (!new_punch_angle.IsValid())
			return;

		g_LocalPlayer->m_aimPunchAngle() = new_punch_angle;
	}
}

template<class T, class U>
T Miscellaneous::clamp(T in, U low, U high)
{
	if (in <= low)
		return low;

	if (in >= high)
		return high;

	return in;
}

void Miscellaneous::FixMovement(CUserCmd *usercmd, QAngle &wish_angle)
{
	Vector view_fwd, view_right, view_up, cmd_fwd, cmd_right, cmd_up;
	auto viewangles = usercmd->viewangles;
	viewangles.Normalize();

	Math::AngleVectors(wish_angle, view_fwd, view_right, view_up);
	Math::AngleVectors(viewangles, cmd_fwd, cmd_right, cmd_up);

	const float v8 = sqrtf((view_fwd.x * view_fwd.x) + (view_fwd.y * view_fwd.y));
	const float v10 = sqrtf((view_right.x * view_right.x) + (view_right.y * view_right.y));
	const float v12 = sqrtf(view_up.z * view_up.z);

	const Vector norm_view_fwd((1.f / v8) * view_fwd.x, (1.f / v8) * view_fwd.y, 0.f);
	const Vector norm_view_right((1.f / v10) * view_right.x, (1.f / v10) * view_right.y, 0.f);
	const Vector norm_view_up(0.f, 0.f, (1.f / v12) * view_up.z);

	const float v14 = sqrtf((cmd_fwd.x * cmd_fwd.x) + (cmd_fwd.y * cmd_fwd.y));
	const float v16 = sqrtf((cmd_right.x * cmd_right.x) + (cmd_right.y * cmd_right.y));
	const float v18 = sqrtf(cmd_up.z * cmd_up.z);

	const Vector norm_cmd_fwd((1.f / v14) * cmd_fwd.x, (1.f / v14) * cmd_fwd.y, 0.f);
	const Vector norm_cmd_right((1.f / v16) * cmd_right.x, (1.f / v16) * cmd_right.y, 0.f);
	const Vector norm_cmd_up(0.f, 0.f, (1.f / v18) * cmd_up.z);

	const float v22 = norm_view_fwd.x * usercmd->forwardmove;
	const float v26 = norm_view_fwd.y * usercmd->forwardmove;
	const float v28 = norm_view_fwd.z * usercmd->forwardmove;
	const float v24 = norm_view_right.x * usercmd->sidemove;
	const float v23 = norm_view_right.y * usercmd->sidemove;
	const float v25 = norm_view_right.z * usercmd->sidemove;
	const float v30 = norm_view_up.x * usercmd->upmove;
	const float v27 = norm_view_up.z * usercmd->upmove;
	const float v29 = norm_view_up.y * usercmd->upmove;

	usercmd->forwardmove = ((((norm_cmd_fwd.x * v24) + (norm_cmd_fwd.y * v23)) + (norm_cmd_fwd.z * v25))
		+ (((norm_cmd_fwd.x * v22) + (norm_cmd_fwd.y * v26)) + (norm_cmd_fwd.z * v28)))
		+ (((norm_cmd_fwd.y * v30) + (norm_cmd_fwd.x * v29)) + (norm_cmd_fwd.z * v27));
	usercmd->sidemove = ((((norm_cmd_right.x * v24) + (norm_cmd_right.y * v23)) + (norm_cmd_right.z * v25))
		+ (((norm_cmd_right.x * v22) + (norm_cmd_right.y * v26)) + (norm_cmd_right.z * v28)))
		+ (((norm_cmd_right.x * v29) + (norm_cmd_right.y * v30)) + (norm_cmd_right.z * v27));
	usercmd->upmove = ((((norm_cmd_up.x * v23) + (norm_cmd_up.y * v24)) + (norm_cmd_up.z * v25))
		+ (((norm_cmd_up.x * v26) + (norm_cmd_up.y * v22)) + (norm_cmd_up.z * v28)))
		+ (((norm_cmd_up.x * v30) + (norm_cmd_up.y * v29)) + (norm_cmd_up.z * v27));

	usercmd->forwardmove = clamp(usercmd->forwardmove, -450.f, 450.f);
	usercmd->sidemove = clamp(usercmd->sidemove, -450.f, 450.f);
	usercmd->upmove = clamp(usercmd->upmove, -320.f, 320.f);
}

void Miscellaneous::AutoPistol(CUserCmd *usercmd)
{
	if (!g_Options.misc_auto_pistol)
		return;

	if (!g_LocalPlayer)
		return;

	C_BaseCombatWeapon* local_weapon = g_LocalPlayer->m_hActiveWeapon().Get();
	if (!local_weapon)
		return;

	if (!local_weapon->IsPistol())
		return;

	float cur_time = AimRage::Get().GetTickbase() * g_GlobalVars->interval_per_tick;
	if (cur_time >= local_weapon->m_flNextPrimaryAttack() && cur_time >= g_LocalPlayer->m_flNextAttack())
		return;

	usercmd->buttons &= (local_weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER ? ~IN_ATTACK2 : ~IN_ATTACK);
}

void Miscellaneous::AntiAim(CUserCmd *usercmd)
{
	if (!g_LocalPlayer || !g_LocalPlayer->m_hActiveWeapon().Get())
		return;

	bool can_shoot = g_LocalPlayer->m_hActiveWeapon().Get()->CanFire();

	if ((g_LocalPlayer->m_hActiveWeapon().Get()->m_iItemDefinitionIndex() != WEAPON_REVOLVER && !(usercmd->buttons & IN_ATTACK) || !(can_shoot)) && !(usercmd->buttons & IN_USE) || g_LocalPlayer->m_hActiveWeapon().Get()->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
	{
		if (g_Options.hvh_antiaim_y || g_Options.hvh_antiaim_x || g_Options.hvh_antiaim_legit)
		{
			Global::bAimbotting = false;
			AntiAim::Get().Work(usercmd);
			if (g_Options.hvh_antiaim_legit)
			{
				usercmd->buttons &= ~IN_MOVERIGHT;
				usercmd->buttons &= ~IN_MOVELEFT;
				usercmd->buttons &= ~IN_FORWARD;
				usercmd->buttons &= ~IN_BACK;

				if (usercmd->forwardmove > 0.f)
					usercmd->buttons |= IN_FORWARD;
				else if (usercmd->forwardmove < 0.f)
					usercmd->buttons |= IN_BACK;
				if (usercmd->sidemove > 0.f)
				{
					usercmd->buttons |= IN_MOVERIGHT;
				}
				else if (usercmd->sidemove < 0.f)
				{
					usercmd->buttons |= IN_MOVELEFT;
				}
			}
		}
	}
}