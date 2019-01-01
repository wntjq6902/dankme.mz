#pragma once

#include <string>

#include "../Singleton.hpp"
#include "../helpers/Utils.hpp"
#include "../math/QAngle.hpp"

#define PI 3.14159265358979323846f
#define RAD2DEG( x ) ( ( float )( x ) * ( float )( 180.0f / ( float )( PI ) ) )

class CUserCmd;
class QAngle;
class CViewSetup;

struct acinfo
{
	acinfo()
	{
		smac_aimbot = smac_autotrigger = smac_client = smac_commands = smac_core = smac_cvars = smac_eyetest = smac_speedhack = smac_spinhack = false;
	}

	bool smac_core;
	bool smac_aimbot;
	bool smac_autotrigger;
	bool smac_client;
	bool smac_commands;
	bool smac_cvars;
	bool smac_eyetest;
	bool smac_speedhack;
	//bool smac_wallhack;	2016-03-09	GoD-Tony	Remove Anti-Wallhack support for CS:GO. Already exists with sv_occlude_players. default tip
	bool smac_spinhack;
};

class NET_SetConVar	//for the namespam meme
{
public:
	NET_SetConVar(const char* name, const char* value)
	{
		typedef void(__thiscall* oConstructor)(PVOID);
		static oConstructor Constructor = (oConstructor)(Utils::PatternScan(GetModuleHandle("engine.dll"), "FF CC CC CC CC CC CC CC CC CC CC CC 56 8B F1 C7 06 ? ? ? ? 8D") + 12);

		Constructor(this);

		typedef void(__thiscall* oInit)(PVOID, const char*, const char*);
		static oInit Init = (oInit)(Utils::PatternScan(GetModuleHandle("engine.dll"), "55 8B EC 56 8B F1 57 83 4E 14 01")); // Search for Custom user info value

		Init(this, name, value);
	}

	~NET_SetConVar()
	{
		typedef void(__thiscall* oDestructor)(PVOID);
		static oDestructor Destructor = (oDestructor)(Utils::PatternScan(GetModuleHandle("engine.dll"), "00 CC CC CC CC CC CC CC CC CC CC CC 56 8B F1 57 8D") + 12); // Search for Custom user info value

		Destructor(this);
	}

private:
	DWORD pad0[13];
};

class Miscellaneous : public Singleton<Miscellaneous>
{
public:

	void Bhop(CUserCmd *userCMD);
	void circle_strafe(CUserCmd* cmd, float* circle_yaw);
	void AutoStrafe(CUserCmd *userCMD);
	void EdgeLag(CUserCmd *userCMD);
	void Fakelag(CUserCmd *userCMD);
	inline int32_t GetChocked() { return choked; }
	void ChangeName(const char *name);
	void NameChanger();
	void NameStealer();
	void AntiAyyware();
	void NameHider();
	void NameSpam();
	void ChatSpamer();
	void ClanTag();
	void ThirdPerson();
	void DetectAC(acinfo &output);

	void FixMovement(CUserCmd *usercmd, QAngle &wish_angle);
	void AntiAim(CUserCmd *usercmd);
	void AutoPistol(CUserCmd *usercmd);

	void PunchAngleFix_RunCommand(void* base_player);
	void PunchAngleFix_FSN();

	int changes = -1;
	bool bJumped = false;
	bool bFake = false;

	bool bDoNameExploit = true;

	template<class T, class U>
	T clamp(T in, U low, U high);

	float GetIdealRotation(float speed)
	{
		return clamp(RAD2DEG(std::atan2(15.f, speed)), 0.f, 45.f);
	} /// per tick

private:

	const char *setStrRight(std::string txt, unsigned int value);
	std::string gladTag = "dankme.mz ";

	QAngle m_aimPunchAngle[128];

	int32_t choked = 0;
	bool edgelag = false;
};
