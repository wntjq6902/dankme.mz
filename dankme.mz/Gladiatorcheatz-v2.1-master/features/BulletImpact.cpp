#include "BulletImpact.hpp"

#include "../Structs.hpp"
#include "Visuals.hpp"
#include "AimRage.hpp"
#include "Resolver.hpp"

#include "../Options.hpp"
#include "../helpers/Math.hpp"

void BulletImpactEvent::FireGameEvent(IGameEvent *event)
{
	if (!g_LocalPlayer || !event)
		return;

	if (g_Options.visuals_others_bulletimpacts)
	{
		auto player = C_BasePlayer::GetPlayerByUserId(event->GetInt("userid"));
		float x = event->GetFloat("x"), y = event->GetFloat("y"), z = event->GetFloat("z");

		if (checks::is_bad_ptr(player) || player->IsDormant() || !x || !y || !z)
			return;

		if (g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) == g_EngineClient->GetLocalPlayer() && g_LocalPlayer && g_LocalPlayer->IsAlive())
		{
			bulletImpactInfo.push_back({ g_GlobalVars->curtime, Vector(x, y, z), g_EngineClient->GetLocalPlayer() });
		}
		else if (player->m_iTeamNum() == 2)
		{
			bulletImpactInfo_t.push_back({ g_GlobalVars->curtime, Vector(x, y, z), g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) });
		}
		else
		{
			bulletImpactInfo_ct.push_back({ g_GlobalVars->curtime, Vector(x, y, z), g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) });
		}
	}

	if (g_Options.hvh_resolver)
	{
		int32_t userid = g_EngineClient->GetPlayerForUserID(event->GetInt("userid"));

		if (userid == g_EngineClient->GetLocalPlayer())
		{
			if (AimRage::Get().prev_aimtarget != NULL)
			{
				auto player = C_BasePlayer::GetPlayerByIndex(AimRage::Get().prev_aimtarget);
				if (!player)
					return;

				int32_t idx = player->EntIndex();
				auto &player_recs = Resolver::Get().arr_infos[idx];

				if (!player->IsDormant())
				{
					int32_t tickcount = g_GlobalVars->tickcount;

					if (tickcount != tickHitWall)
					{
						tickHitWall = tickcount;
						originalShotsMissed = player_recs.m_nShotsMissed;
						originalCorrectedFakewalkIdx = player_recs.m_nCorrectedFakewalkIdx;

						if (tickcount != tickHitPlayer)
						{
							tickHitWall = tickcount;
							++player_recs.m_nShotsMissed;

							if (++player_recs.m_nCorrectedFakewalkIdx > 3)
								player_recs.m_nCorrectedFakewalkIdx = 0;
						}
					}
				}
			}
		}
	}
}

int BulletImpactEvent::GetEventDebugID(void)
{
	return EVENT_DEBUG_ID_INIT;
}

void BulletImpactEvent::RegisterSelf()
{
	g_GameEvents->AddListener(this, "bullet_impact", false);
}

void BulletImpactEvent::UnregisterSelf()
{
	g_GameEvents->RemoveListener(this);
}

void BulletImpactEvent::Paint(void)
{
	if (!g_Options.visuals_others_bulletimpacts)
		return;

	if (!g_EngineClient->IsInGame() || !g_LocalPlayer)
	{
		bulletImpactInfo.clear();
		bulletImpactInfo_t.clear();
		bulletImpactInfo_ct.clear();
		return;
	}
	Color current_color;

	std::vector<BulletImpactInfo> &impacts = bulletImpactInfo;

	if (!impacts.empty())
	{
		current_color = Color(g_Options.visuals_others_bulletimpacts_color);

		for (size_t i = 0; i < impacts.size(); i++)
		{
			auto current_impact = impacts.at(i);

			if (checks::is_bad_ptr(C_BasePlayer::GetPlayerByIndex(current_impact.player)))
				continue;

			BeamInfo_t beamInfo;

			beamInfo.m_nType = TE_BEAMPOINTS;
			//beamInfo.m_pszModelName = "sprites/physbeam.vmt";
			beamInfo.m_pszModelName = "sprites/laserbeam.vmt";
			beamInfo.m_pszHaloName = "sprites/laserbeam.vmt";
			beamInfo.m_flHaloScale = 6.0f;
			beamInfo.m_nModelIndex = -1;
			beamInfo.m_flLife = 1.5f;
			beamInfo.m_flWidth = 3.0f;
			beamInfo.m_flEndWidth = 3.0f;
			beamInfo.m_flFadeLength = 0.5f;
			beamInfo.m_flAmplitude = 0.0f;
			beamInfo.m_flBrightness = 255.0f;
			beamInfo.m_flSpeed = 0.0f;
			beamInfo.m_nStartFrame = 0;
			beamInfo.m_flFrameRate = 0.f;
			beamInfo.m_flRed = current_color.r();
			beamInfo.m_flGreen = current_color.g();
			beamInfo.m_flBlue = current_color.b();
			beamInfo.m_nSegments = -1;
			beamInfo.m_bRenderable = true;
			beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM | FBEAM_FADEOUT;

			beamInfo.m_vecStart = g_LocalPlayer->GetEyePos();
			beamInfo.m_vecStart.z -= g_Options.misc_thirdperson ? 0 : 1;
			beamInfo.m_vecEnd = current_impact.m_vecHitPos;

			auto beam = g_RenderBeams->CreateBeamPoints(beamInfo);

			beamInfo.m_flWidth = 0.75f;
			beamInfo.m_flEndWidth = 0.75f;
			beamInfo.m_flBrightness = 255.0f;
			beamInfo.m_flHaloScale = 1.5f;
			beamInfo.m_flRed = beamInfo.m_flGreen = beamInfo.m_flBlue = 255;

			auto beam2 = g_RenderBeams->CreateBeamPoints(beamInfo);

			beamInfo.m_flLife = 3.0f;
			beamInfo.m_flWidth = 0.125f;
			beamInfo.m_flEndWidth = 0.125f;
			beamInfo.m_flFadeLength = 0.f;
			beamInfo.m_flRed = current_color.r();
			beamInfo.m_flGreen = current_color.g();
			beamInfo.m_flBlue = current_color.b();
			beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE;

			auto beam3 = g_RenderBeams->CreateBeamPoints(beamInfo);

			if (beam) g_RenderBeams->DrawBeam(beam);
			if (beam2) g_RenderBeams->DrawBeam(beam2);
			if (beam3) g_RenderBeams->DrawBeam(beam3);

			g_DebugOverlay->AddBoxOverlay(current_impact.m_vecHitPos, Vector(-3, -3, -3), Vector(3, 3, 3), QAngle(0, 0, 0), current_color.r(), current_color.g(), current_color.b(), current_color.a(), 3.0f);

			impacts.erase(impacts.begin() + i);
		}
	}
	std::vector<BulletImpactInfo> &impacts2 = bulletImpactInfo_ct;

	if (!impacts2.empty())
	{
		current_color = Color(g_Options.visuals_others_bulletimpacts_color_ct);

		for (size_t i = 0; i < impacts2.size(); i++)
		{
			auto current_impact = impacts2.at(i);

			if (checks::is_bad_ptr(C_BasePlayer::GetPlayerByIndex(current_impact.player)))
				continue;

			BeamInfo_t beamInfo;

			beamInfo.m_nType = TE_BEAMPOINTS;
			//beamInfo.m_pszModelName = "sprites/physbeam.vmt";
			beamInfo.m_pszModelName = "sprites/laserbeam.vmt";
			beamInfo.m_pszHaloName = "sprites/laserbeam.vmt";
			beamInfo.m_flHaloScale = 6.0f;
			beamInfo.m_nModelIndex = -1;
			beamInfo.m_flLife = 1.5f;
			beamInfo.m_flWidth = 3.0f;
			beamInfo.m_flEndWidth = 3.0f;
			beamInfo.m_flFadeLength = 0.5f;
			beamInfo.m_flAmplitude = 0.0f;
			beamInfo.m_flBrightness = 255.0f;
			beamInfo.m_flSpeed = 0.0f;
			beamInfo.m_nStartFrame = 0;
			beamInfo.m_flFrameRate = 0.f;
			beamInfo.m_flRed = current_color.r();
			beamInfo.m_flGreen = current_color.g();
			beamInfo.m_flBlue = current_color.b();
			beamInfo.m_nSegments = -1;
			beamInfo.m_bRenderable = true;
			beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM | FBEAM_FADEOUT;

			beamInfo.m_vecStart = C_BasePlayer::GetPlayerByIndex(current_impact.player)->GetEyePos();
			beamInfo.m_vecEnd = current_impact.m_vecHitPos;

			auto beam = g_RenderBeams->CreateBeamPoints(beamInfo);

			beamInfo.m_flWidth = 0.75f;
			beamInfo.m_flEndWidth = 0.75f;
			beamInfo.m_flBrightness = 255.0f;
			beamInfo.m_flHaloScale = 1.5f;
			beamInfo.m_flRed = beamInfo.m_flGreen = beamInfo.m_flBlue = 255;

			auto beam2 = g_RenderBeams->CreateBeamPoints(beamInfo);

			beamInfo.m_flLife = 3.0f;
			beamInfo.m_flWidth = 0.125f;
			beamInfo.m_flEndWidth = 0.125f;
			beamInfo.m_flFadeLength = 0.f;
			beamInfo.m_flRed = current_color.r();
			beamInfo.m_flGreen = current_color.g();
			beamInfo.m_flBlue = current_color.b();
			beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE;

			auto beam3 = g_RenderBeams->CreateBeamPoints(beamInfo);

			if (beam) g_RenderBeams->DrawBeam(beam);
			if (beam2) g_RenderBeams->DrawBeam(beam2);
			if (beam3) g_RenderBeams->DrawBeam(beam3);

			g_DebugOverlay->AddBoxOverlay(current_impact.m_vecHitPos, Vector(-3, -3, -3), Vector(3, 3, 3), QAngle(0, 0, 0), current_color.r(), current_color.g(), current_color.b(), current_color.a(), 3.0f);

			impacts2.erase(impacts2.begin() + i);
		}
	}
	std::vector<BulletImpactInfo> &impacts3 = bulletImpactInfo_t;

	if (!impacts3.empty())
	{
		current_color = Color(g_Options.visuals_others_bulletimpacts_color_t);

		for (size_t i = 0; i < impacts3.size(); i++)
		{
			auto current_impact = impacts3.at(i);

			if (checks::is_bad_ptr(C_BasePlayer::GetPlayerByIndex(current_impact.player)))
				continue;

			BeamInfo_t beamInfo;

			beamInfo.m_nType = TE_BEAMPOINTS;
			//beamInfo.m_pszModelName = "sprites/physbeam.vmt";
			beamInfo.m_pszModelName = "sprites/laserbeam.vmt";
			beamInfo.m_pszHaloName = "sprites/laserbeam.vmt";
			beamInfo.m_flHaloScale = 6.0f;
			beamInfo.m_nModelIndex = -1;
			beamInfo.m_flLife = 1.5f;
			beamInfo.m_flWidth = 3.0f;
			beamInfo.m_flEndWidth = 3.0f;
			beamInfo.m_flFadeLength = 0.5f;
			beamInfo.m_flAmplitude = 0.0f;
			beamInfo.m_flBrightness = 255.0f;
			beamInfo.m_flSpeed = 0.0f;
			beamInfo.m_nStartFrame = 0;
			beamInfo.m_flFrameRate = 0.f;
			beamInfo.m_flRed = current_color.r();
			beamInfo.m_flGreen = current_color.g();
			beamInfo.m_flBlue = current_color.b();
			beamInfo.m_nSegments = -1;
			beamInfo.m_bRenderable = true;
			beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM | FBEAM_FADEOUT;

			beamInfo.m_vecStart = C_BasePlayer::GetPlayerByIndex(current_impact.player)->GetEyePos();
			beamInfo.m_vecEnd = current_impact.m_vecHitPos;

			auto beam = g_RenderBeams->CreateBeamPoints(beamInfo);

			beamInfo.m_flWidth = 0.75f;
			beamInfo.m_flEndWidth = 0.75f;
			beamInfo.m_flBrightness = 255.0f;
			beamInfo.m_flHaloScale = 1.5f;
			beamInfo.m_flRed = beamInfo.m_flGreen = beamInfo.m_flBlue = 255;

			auto beam2 = g_RenderBeams->CreateBeamPoints(beamInfo);

			beamInfo.m_flLife = 3.0f;
			beamInfo.m_flWidth = 0.125f;
			beamInfo.m_flEndWidth = 0.125f;
			beamInfo.m_flFadeLength = 0.f;
			beamInfo.m_flRed = current_color.r();
			beamInfo.m_flGreen = current_color.g();
			beamInfo.m_flBlue = current_color.b();
			beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE;

			auto beam3 = g_RenderBeams->CreateBeamPoints(beamInfo);

			if (beam) g_RenderBeams->DrawBeam(beam);
			if (beam2) g_RenderBeams->DrawBeam(beam2);
			if (beam3) g_RenderBeams->DrawBeam(beam3);

			g_DebugOverlay->AddBoxOverlay(current_impact.m_vecHitPos, Vector(-3, -3, -3), Vector(3, 3, 3), QAngle(0, 0, 0), current_color.r(), current_color.g(), current_color.b(), current_color.a(), 3.0f);

			impacts3.erase(impacts3.begin() + i);
		}
	}
}