#pragma once

#include "../Singleton.hpp"
#include "../Structs.hpp"
#include <chrono>

#define M_PI 3.14159265358979323846

class CUserCmd;
class CRecvProxyData;
class Vector;

inline float get_move_angle(float speed)
{
	auto move_angle = RAD2DEG(asin(15.f / speed));

	if (!isfinite(move_angle) || move_angle > 90.f)
		move_angle = 90.f;
	else if (move_angle < 0.f)
		move_angle = 0.f;

	return move_angle;
}

struct AntiaimData_t
{
	AntiaimData_t(const float& dist, const bool& inair, int player)
	{
		this->flDist = dist;
		this->bInAir = inair;
		this->iPlayer = player;
	}

	float flDist;
	bool bInAir;
	int	iPlayer;
};

enum AA_PITCH
{
	AA_PITCH_OFF,
	AA_PITCH_DYNAMIC,
	AA_PITCH_EMOTION,
	AA_PITCH_STRAIGHT,
	AA_PITCH_UP,
	AA_PITCH_FAKEJITTER,
	AA_PITCH_ATTARGET
};

enum AA_BASEYAW
{
	AA_BASEYAW_VIEWANG,
	AA_BASEYAW_STATIC,
	AA_BASEYAW_ATTARGET,
	AA_BASEYAW_ADEPTIVE
};

enum AA_YAW
{
	AA_YAW_OFF,
	AA_YAW_BACKWARDS,
	AA_YAW_BACKWARDS_JITTER,
	AA_YAW_MANUALLBY,
	AA_YAW_MANUALLBYJITTER,
	AA_YAW_CUSTOM_DELTA,
	AA_YAW_ANTILBY
};

enum AA_FAKEYAW
{
	AA_FAKEYAW_OFF,
	AA_FAKEYAW_BACKWARDS,
	AA_FAKEYAW_BACKWARDS_JITTER,
	AA_FAKEYAW_MANUALLBY,
	AA_FAKEYAW_MANUALLBYJITTER,
	AA_FASTSPIN,
	AA_LBYRAND,
	AA_FAKEYAW_CUSTOM_DELTA
};

enum NEW_AA_PITCH
{
	AA_PITCH_NEW_VIEWANG,
	AA_PITCH_NEW_CUSTOM_STATIC,
	AA_PITCH_NEW_CUSTOM_JITTER,
	AA_PITCH_NEW_LUA
};

enum NEW_AA_YAW
{
	AA_YAW_NEW_VIEWANG,
	AA_YAW_NEW_SPIN,
	AA_YAW_NEW_CUSTOM_STATIC,
	AA_YAW_NEW_CUSTOM_JITTER,
	AA_YAW_NEW_MENUAL,
	AA_YAW_NEW_MENUAL_JITTER,
	AA_YAW_NEW_LUA
};

enum AA_ONAIR
{
	AA_ONAIR_OFF,
	AA_ONAIR_SPIN,
	AA_ONAIR_180z,
	AA_ONAIR_FLIP,
	AA_ONAIR_EVADE
};

struct AA_Freestand_Records
{
	Vector position;
	float angle;
	float time;
};

class AntiAim : public Singleton<AntiAim>
{

public:

	void Work(CUserCmd *usercmd);
	void FrameStageNotify(ClientFrameStage_t stage);
	//void UpdateLBYBreaker(CUserCmd *usercmd);
	void UpdateLowerBodyBreaker(const QAngle& angles);
	void Fakewalk(CUserCmd *usercmd);

	float GetLbyUpdateTime(bool last);
	void PastedLBYBreaker(CUserCmd* cmd);

	bool IsFreestanding();
	float GetFreestandingYaw();

private:

	void FakeMove(CUserCmd* cmd);
	float GetPitch();
	float GetYaw(bool no_freestand = false);
	float GetFakeYaw();
	
	int LBYUpdate();
	float GetPitchNew();
	float GetYawNew();

	void Accelerate(C_BasePlayer *player, Vector &wishdir, float wishspeed, float accel, Vector &outVel);
	void WalkMove(C_BasePlayer *player, Vector &outVel);
	void Friction(Vector &outVel);
	bool WallDetection(float &yaw);
	float Freestanding();
	float Freestanding_V2();
	float Freestanding_V3();
	float PastedFreestanding();
	void SelectTarget();

	CUserCmd *usercmd = nullptr;

	bool m_bBreakLowerBody = false;
	float_t m_flSpawnTime = 0.f;
	float_t m_flNextBodyUpdate = 0.f;
	CBaseHandle *m_ulEntHandle = nullptr;

	C_CSGOPlayerAnimState *m_serverAnimState = nullptr;

	QAngle m_angEyeAngles = QAngle(0.f, 0.f, 0.f);
	QAngle m_angRealAngle = QAngle(0.f, 0.f, 0.f);
	float_t m_flLowerBody = 0.f;
	bool m_bLowerBodyUpdate = false;
	bool antiresolve = false;
	float m_flLowerBodyNextUpdate = 0.f;
	float m_flLowerBodyLastUpdate = 0.f;
	float m_flLowerBodyTarget = 0.f;

	float m_flFreestandingYaw = -99999.f;

	std::vector<AntiaimData_t> Entities;
	std::deque<float> m_flFreestandingYaws;
};