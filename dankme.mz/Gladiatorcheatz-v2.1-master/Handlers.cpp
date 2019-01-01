#include "Gamehooking.hpp"
#include "helpers/Utils.hpp"

#include "Menu.hpp"
#include "Options.hpp"

#include "helpers/Math.hpp"

#include "features/Visuals.hpp"
#include "features/Glow.hpp"
#include "features/Miscellaneous.hpp"
#include "features/PredictionSystem.hpp"
#include "features/AimRage.hpp"
#include "features/AimLegit.h"
#include "features/LagCompensation.hpp"
#include "features/Resolver.hpp"
#include "features/AntiAim.hpp"
#include "features/PlayerHurt.hpp"
#include "features/BulletImpact.hpp"
#include "features/GrenadePrediction.h"
#include "features/ServerSounds.hpp"
#include "features/Skinchanger.hpp"
#include "features/RebuildGameMovement.hpp"

#include "helpers/AntiLeak.hpp"
#include "helpers/bfReader.hpp"
#include "Install.hpp"
#include "trusted_list.h"

#include <intrin.h>
#include <thread>
#include <experimental/filesystem> // hack

extern LRESULT ImGui_ImplDX9_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int32_t originalCorrectedFakewalkIdx = 0;
int32_t tickHitPlayer = 0;
int32_t tickHitWall = 0;
int32_t originalShotsMissed = 0;

int32_t nTickBaseShift = 0;
int32_t nSinceUse = 0;
bool bInSendMove = false, bFirstSendMovePack = false;

namespace Global
{
	char my_documents_folder[MAX_PATH];

	int iHwidIndex = 0;
	bool bIsPanorama = false;

	bool bMenuOpen = true;

	float smt = 0.f;
	QAngle visualAngles = QAngle(0.f, 0.f, 0.f);
	bool bSendPacket = false;
	bool bAimbotting = false;
	bool bVisualAimbotting = false;
	QAngle vecVisualAimbotAngs = QAngle(0.f, 0.f, 0.f);
	CUserCmd *userCMD = nullptr;

	float lastdmg[65] = { 0 };

	float realyaw, fakeyaw, realpitch, fakepitch, lasthc, lastawdmg = 0;

	int aimbot_target;
	int aim_hitbox;

	bool hit_while_brute[65] = { false };

	double LastLBYUpdate = 0, NextLBYUpdate = 0;
	int prevChoked = 0;

	char *szLastFunction = "<No function was called>";

	HMODULE hmDll = nullptr;
	HANDLE handleDll = nullptr;

	bool bFakelag = false;
	float flFakewalked = 0.f;
	Vector vecUnpredictedVel = Vector(0, 0, 0);

	float flFakeLatencyAmount = 0.f;
	float flEstFakeLatencyOnServer = 0.f;

	matrix3x4_t traceHitboxbones[128];

	bool bBaim[65] = { false };

	int AimTargets = 0;
	bool last_packet = true;

	std::array<std::string, 64> resolverModes;

	QAngle view_punch_old = QAngle(0, 0, 0);

	INetChannel* netchan = nullptr;

	bool use_ud_vmt = true;

	int fps = 0;
	Vector vecAimpos = Vector(0, 0, 0);

	int PlayersChockedPackets[65] = { 0 };
	Vector FakelagUnfixedPos[65] = { Vector(0, 0, 0) };
	bool FakelagFixed[65] = { false };

	bool bShouldUnload = false;

	std::string currentLUA = "";
}

void debug_visuals()
{
	int x_offset = 0;
	//RECT bbox = Visuals::ESP_ctx.bbox;
	//float_t simtime = Visuals::ESP_ctx.player->m_flOldSimulationTime() + g_GlobalVars->interval_per_tick;
	//Visuals::DrawString(Visuals::ui_font, bbox.left - 5, bbox.top + 0 , Color(255, 255, 255), FONT_LEFT, "simtime %f", simtime);
	if (g_Options.debug_showposes)
	{
		RECT bbox = Visuals::ESP_ctx.bbox;
		auto poses = Visuals::ESP_ctx.player->m_flPoseParameter();
		int i = 0;
		for (; i < 24; ++i)
		{
			//Visuals::DrawString(Visuals::ui_font, bbox.right + 5, bbox.top + i * 12, Color(10 * i, 255, 255, 255), FONT_LEFT, "Pose %d %f", i, poses[i]);
			Visuals::DrawString(Visuals::ui_font, bbox.right + 5, bbox.top + i * 12, Color(10 * i, 255, 255, 255), FONT_LEFT, "Pose %d %f", i, poses[i]);
		}
		/*
		i++; Visuals::DrawString(Visuals::ui_font, bbox.right + 5, bbox.top + i * 12, Color(255, 0, 55, 255), FONT_LEFT, "LBY %f", (Visuals::ESP_ctx.player->m_flLowerBodyYawTarget() + 180.f) / 360.f);
		i++; Visuals::DrawString(Visuals::ui_font, bbox.right + 5, bbox.top + i * 12, Color(255, 0, 55, 255), FONT_LEFT, "P2 %f", Math::ClampYaw((poses[2] - 180) * 360));
		i++; Visuals::DrawString(Visuals::ui_font, bbox.right + 5, bbox.top + i * 12, Color(255, 0, 55, 255), FONT_LEFT, "P11 %f", Math::ClampYaw((poses[11] - 90) * 180));
		i++; Visuals::DrawString(Visuals::ui_font, bbox.right + 5, bbox.top + i * 12, Color(255, 0, 55, 255), FONT_LEFT, "P11 + P2 %f", Math::ClampYaw(((poses[11] - 90) * 180) + ((poses[2] - 180) * 360)));
		i++; Visuals::DrawString(Visuals::ui_font, bbox.right + 5, bbox.top + i * 12, Color(255, 0, 55, 255), FONT_LEFT, "Real %f", Math::ClampYaw(Global::realyaw));
		*/
		x_offset += 50;
	}

	if (g_Options.debug_showactivities)
	{
		float h = fabs(Visuals::ESP_ctx.feet_pos.y - Visuals::ESP_ctx.head_pos.y);
		float w = h / 2.0f;

		int offsety = 0;
		for (int i = 0; i < Visuals::ESP_ctx.player->GetNumAnimOverlays(); i++)
		{
			auto layer = Visuals::ESP_ctx.player->GetAnimOverlays()[i];
			int number = layer.m_nSequence,
				activity = Visuals::ESP_ctx.player->GetSequenceActivity(number);
			Visuals::DrawString(Visuals::ui_font, Visuals::ESP_ctx.head_pos.x + w + x_offset + -14, Visuals::ESP_ctx.head_pos.y + offsety, Color(255, 255, 0, 255), FONT_LEFT, std::to_string(i).c_str());
			Visuals::DrawString(Visuals::ui_font, Visuals::ESP_ctx.head_pos.x + w + x_offset + 4, Visuals::ESP_ctx.head_pos.y + offsety, Color(255, 255, 0, 255), FONT_LEFT, std::to_string(number).c_str());
			Visuals::DrawString(Visuals::ui_font, Visuals::ESP_ctx.head_pos.x + w + x_offset + 40, Visuals::ESP_ctx.head_pos.y + offsety, Color(255, 255, 0, 255), FONT_LEFT, std::to_string(activity).c_str());
			Visuals::DrawString(Visuals::ui_font, Visuals::ESP_ctx.head_pos.x + w + x_offset + 60, Visuals::ESP_ctx.head_pos.y + offsety, Color(255, 255, 0, 255), FONT_LEFT, std::to_string(layer.m_flCycle).c_str());
			Visuals::DrawString(Visuals::ui_font, Visuals::ESP_ctx.head_pos.x + w + x_offset + 104, Visuals::ESP_ctx.head_pos.y + offsety, Color(255, 255, 0, 255), FONT_LEFT, std::to_string(layer.m_flWeight).c_str());
			Visuals::DrawString(Visuals::ui_font, Visuals::ESP_ctx.head_pos.x + w + x_offset + 144, Visuals::ESP_ctx.head_pos.y + offsety, Color(255, 255, 0, 255), FONT_LEFT, std::to_string(layer.m_flPlaybackRate).c_str());

			/*if (activity == 979)
			{
				Visuals::DrawString(Visuals::ui_font, Visuals::ESP_ctx.head_pos.x + w + x_offset + 65, Visuals::ESP_ctx.head_pos.y + offsety, Color(255, 255, 0, 255), FONT_LEFT, std::to_string(layer.m_flWeight).c_str());
				Visuals::DrawString(Visuals::ui_font, Visuals::ESP_ctx.head_pos.x + w + x_offset + 65, Visuals::ESP_ctx.head_pos.y + offsety + 12, Color(255, 255, 0, 255), FONT_LEFT, std::to_string(layer.m_flCycle).c_str());
			}*/

			offsety += 12;
		}
		x_offset += 100;
	}

	if (g_Options.debug_headbox)
	{
		Vector headpos = Visuals::ESP_ctx.player->GetBonePos(8);
		Visuals::Draw3DCube(7.f, Visuals::ESP_ctx.player->m_angEyeAngles(), headpos, Color(40, 40, 40, 160));
	}
}

void __fastcall Handlers::PaintTraverse_h(void *thisptr, void*, unsigned int vguiPanel, bool forceRepaint, bool allowForce)
{
	if (Global::bShouldUnload)
	{
		Installer::uninstallGladiator();
	}
	
	if (hwids.at(Global::iHwidIndex).second.second >= USER_TYPE_MOD && FindDebugger())
	{
		Utils::ConsolePrint(true, "debugger detected!\n");
		Utils::ConsolePrint(true, "please shutdown any debugger running on background.\n");
		Utils::ConsolePrint(true, "Exiting in 5 seconds...\n");
		Installer::uninstallGladiator();
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	}

	static uint32_t HudZoomPanel;
	if (!HudZoomPanel)
	{
		if (!strcmp("HudZoom", g_VGuiPanel->GetName(vguiPanel)))
			HudZoomPanel = vguiPanel;
	}

	if (HudZoomPanel == vguiPanel && g_Options.removals_scope && g_LocalPlayer && g_LocalPlayer->m_hActiveWeapon().Get())
	{
		if (g_LocalPlayer->m_hActiveWeapon().Get()->IsSniper() && g_LocalPlayer->m_bIsScoped())
			return;
	}

	o_PaintTraverse(thisptr, vguiPanel, forceRepaint, allowForce);

	static uint32_t FocusOverlayPanel;
	if (!FocusOverlayPanel)
	{
		const char* szName = g_VGuiPanel->GetName(vguiPanel);

		if (lstrcmpA(szName, "MatSystemTopPanel") == 0)
		{
			FocusOverlayPanel = vguiPanel;

			Visuals::InitFont();

			g_EngineClient->ExecuteClientCmd("clear");
			g_CVar->ConsoleColorPrintf(Color(0, 153, 204, 255), "DankMe.Mz Repasted\n Build Date: %s \n", __DATE__);

			long res = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, Global::my_documents_folder);
			if (res == S_OK)
			{
				std::string config_folder = std::string(Global::my_documents_folder) + "\\DANKMEMZ_Repasted\\";
				Config::Get().CreateConfigFolder(config_folder);

				std::string default_file_path = config_folder + "dankme.mz";

				if (Config::Get().FileExists(default_file_path))
					Config::Get().LoadConfig(default_file_path);
			}

			Skinchanger::Get().LoadSkins();

			g_EngineClient->ExecuteClientCmd("version");
			g_EngineClient->ExecuteClientCmd("toggleconsole");
		}
	}

	if (FocusOverlayPanel == vguiPanel)
	{
		Visuals::DrawWatermark();
		junkcode::call();
		if (g_EngineClient->IsInGame() && g_EngineClient->IsConnected() && g_LocalPlayer)
		{
			ServerSound::Get().Start();

			if (g_Options.visuals_lbylc_indicater) Visuals::RenderLBYLC();
			if (g_Options.visuals_devinfo) Visuals::RenderDevInfo();
			if (g_Options.removals_novisualrecoil) Visuals::DrawRecoilCrosshair(g_LocalPlayer->m_aimPunchAngle());
			if (g_Options.esp_player_spreadch && g_LocalPlayer->IsAlive()) Visuals::DrawSpreadCrosshair();
			if (g_Options.esp_player_anglelines && g_LocalPlayer->IsAlive()) Visuals::DrawAngleLines();
			if (g_Options.esp_planted_c4) Visuals::RenderC4();
			if (g_Options.visuals_weaprange && g_Options.misc_thirdperson) Visuals::DrawWeaponRange();

			for (int i = 1; i <= g_EntityList->GetHighestEntityIndex(); i++)
			{
				C_BasePlayer *entity = C_BasePlayer::GetPlayerByIndex(i);

				if (!entity)
					continue;

				if (i < 65 && Visuals::ValidPlayer(entity))
				{
					if (Visuals::Begin(entity))
					{
						Visuals::RenderFill();
						Visuals::RenderBox();

						if (g_Options.esp_player_snaplines) Visuals::RenderSnapline();
						if (g_Options.esp_player_weapons) Visuals::RenderWeapon();
						if (g_Options.esp_player_name) Visuals::RenderName();
						if (g_Options.esp_player_health) Visuals::RenderHealth();
						if (g_Options.esp_player_skelet) Visuals::RenderSkelet();
						if (g_Options.esp_backtracked_player_skelet) Visuals::RenderBacktrackedSkelet();
						if (g_Options.hvh_resolver && g_Options.hvh_resolver_display) Visuals::DrawResolverModes();
						if (g_Options.misc_radar) Visuals::Radar();

						debug_visuals();
					}
				}
				else if (g_Options.esp_dropped_weapons && entity->IsWeapon())
					Visuals::RenderWeapon((C_BaseCombatWeapon*)entity);

				/*
				else if (entity->IsPlantedC4())
					if (g_Options.esp_planted_c4)
						Visuals::RenderPlantedC4(entity);
						*/

				Visuals::RenderNadeEsp((C_BaseCombatWeapon*)entity);
				
				if (g_Options.esp_player_offscreen) Visuals::RenderOffscreenESP();
			}
			ServerSound::Get().Finish();

			if (g_Options.removals_scope && (g_LocalPlayer && g_LocalPlayer->m_hActiveWeapon().Get() && g_LocalPlayer->m_hActiveWeapon().Get()->IsSniper() && g_LocalPlayer->m_bIsScoped()))
			{
				int screenX, screenY;
				g_EngineClient->GetScreenSize(screenX, screenY);
				g_VGuiSurface->DrawSetColor(Color::Black);
				g_VGuiSurface->DrawLine(screenX / 2, 0, screenX / 2, screenY);
				g_VGuiSurface->DrawLine(0, screenY / 2, screenX, screenY / 2);
			}
			junkcode::call();
			if (g_Options.misc_spectatorlist)
				Visuals::RenderSpectatorList();

			if (g_Options.visuals_others_grenade_pred)
				CCSGrenadeHint::Get().Paint();

			Global::hit_while_brute[Global::aimbot_target] = false;

			if (g_Options.visuals_others_hitmarker || g_Options.misc_logevents)
				PlayerHurtEvent::Get().Paint();

		}
	}
}

bool __stdcall Handlers::CreateMove_h(float smt, CUserCmd *userCMD)
{
	static float old_sens = g_CVar->FindVar("sensitivity")->GetFloat();
	Miscellaneous::Get().ClanTag();
	if (g_Options.rage_lagcompensation && g_Options.hvh_resolver) Resolver::Get().FakelagFix();
	if (!userCMD->command_number || !g_EngineClient->IsInGame() || !g_LocalPlayer || !(g_LocalPlayer->IsAlive() || g_LocalPlayer->m_bIsPlayerGhost()))
	{
		if (!(g_pSendDatagramHook || g_pSendDatagramHook_ud) && g_EngineClient->IsInGame())
		{
			if (checks::is_bad_ptr(g_ClientState->m_NetChannel))
			{
				g_pSendDatagramHook = nullptr;
				g_pSendDatagramHook_ud = nullptr;
				Global::netchan = nullptr;
			}
			else
			{
				Global::netchan = g_ClientState->m_NetChannel;

				if (Global::use_ud_vmt)
				{
					g_pSendDatagramHook_ud = std::make_unique<UDVMT>();
					g_pSendDatagramHook_ud->setup(g_ClientState->m_NetChannel);
					g_pSendDatagramHook_ud->hook_index(46, Handlers::SendDatagram_h);
					o_SendDatagram = g_pSendDatagramHook_ud->get_original<SendDatagram_t>(46);
				}
				else
				{
					g_pSendDatagramHook = std::make_unique<ShadowVTManager>();
					g_pSendDatagramHook->Setup(g_ClientState->m_NetChannel);
					g_pSendDatagramHook->Hook(46, Handlers::SendDatagram_h);
					o_SendDatagram = g_pSendDatagramHook->GetOriginal<SendDatagram_t>(46);
				}
			}
		}
		g_CVar->FindVar("sensitivity")->SetValue(old_sens);
		return o_CreateMove(g_ClientMode, smt, userCMD);
	}
	//CMBacktracking::Get().UpdateIncomingSequences();
	/*
	static bool hooked = false;

	auto clientState = *reinterpret_cast<uintptr_t*>(g_ClientState); //Do a patternscan and make a global var

	if (!hooked && clientState) //ReInject after map change??? Detect mapchange and rehook
	{
		uintptr_t temp = *reinterpret_cast<uintptr_t*>(clientState + 0x9C);
		INetChannel* netchan = reinterpret_cast<INetChannel*>(temp);//Do a global var
		Global::netchan = netchan;
		if (!checks::is_bad_ptr(netchan))
		{
			hooked = true;
			if (Global::use_ud_vmt)
			{
				g_pSendDatagramHook_ud = std::make_unique<UDVMT>();
				g_pSendDatagramHook_ud->setup(netchan);
				g_pSendDatagramHook_ud->hook_index(46, Handlers::SendDatagram_h);
				o_SendDatagram = g_pSendDatagramHook_ud->get_original<SendDatagram_t>(46);
			}
			else
			{
				g_pSendDatagramHook = std::make_unique<ShadowVTManager>();
				g_pSendDatagramHook->Setup(netchan);
				g_pSendDatagramHook->Hook(46, Handlers::SendDatagram_h);
				o_SendDatagram = g_pSendDatagramHook->GetOriginal<SendDatagram_t>(46);
			}
		}
	}
	*/
	if (g_Options.misc_smacbypass && g_CVar->FindVar("sensitivity")->GetFloat() < 7)
		g_CVar->FindVar("sensitivity")->SetValue(7.0f);

	if (g_Options.misc_instant_defuse_plant && (userCMD->buttons & IN_USE))
	{
		if (nSinceUse++ < 3)
			nTickBaseShift = TIME_TO_TICKS(11.f);
	}
	else
		nSinceUse = 0;

	// Update tickbase correction.
	AimRage::Get().GetTickbase(userCMD);

	// Update lby
	//AntiAim::Get().UpdateLBYBreaker(userCMD);

	QAngle org_angle = userCMD->viewangles;

	uintptr_t *framePtr;
	__asm mov framePtr, ebp;
	junkcode::call();
	Global::smt = smt;
	Global::bFakelag = false;
	Global::bSendPacket = true;
	Global::userCMD = userCMD;
	Global::vecUnpredictedVel = g_LocalPlayer->m_vecVelocity();

	if (g_Options.misc_bhop)
		Miscellaneous::Get().Bhop(userCMD);

	if (g_Options.misc_autostrafe)
		Miscellaneous::Get().AutoStrafe(userCMD);

	QAngle wish_angle = userCMD->viewangles;
	userCMD->viewangles = org_angle;

	// -----------------------------------------------
	// Do engine prediction
	PredictionSystem::Get().Start(userCMD, g_LocalPlayer);
	{
		if (g_Options.misc_fakewalk)
			AntiAim::Get().Fakewalk(userCMD);

		//if (g_Options.misc_fakelag_enabled)
		//	Miscellaneous::Get().EdgeLag(userCMD);

		if (g_Options.misc_fakelag_value || g_Options.misc_fakelag_adaptive)
			Miscellaneous::Get().Fakelag(userCMD);

		Miscellaneous::Get().AutoPistol(userCMD);

		AimLegit::Get().Work(userCMD);

		AimRage::Get().Work(userCMD);

		Miscellaneous::Get().AntiAim(userCMD);

		AntiAim::Get().PastedLBYBreaker(userCMD);

		Miscellaneous::Get().FixMovement(userCMD, wish_angle);
	}
	PredictionSystem::Get().End(g_LocalPlayer);

	CCSGrenadeHint::Get().Tick(userCMD->buttons);

	if (g_Options.rage_enabled && Global::bAimbotting && userCMD->buttons & IN_ATTACK)
	{
		Resolver::Get().EventHandler.records.push_back(Global::aimbot_target);

		if (g_Options.hvh_disable_antiut)
		{
			userCMD->viewangles.pitch = Math::FindSmallestFake2(userCMD->viewangles.pitch, Utils::RandomInt(0, 3));
		}

		*(bool*)(*framePtr - 0x1C) = false;
	}

	*(bool*)(*framePtr - 0x1C) = Global::bSendPacket;

	userCMD->buttons |= IN_BULLRUSH;	//b1g exploit pls no copy pasta

	if (!g_Options.hvh_disable_antiut)
	{
		userCMD->forwardmove = Miscellaneous::Get().clamp(userCMD->forwardmove, -450.f, 450.f);
		userCMD->sidemove = Miscellaneous::Get().clamp(userCMD->sidemove, -450.f, 450.f);
		userCMD->upmove = Miscellaneous::Get().clamp(userCMD->upmove, -320.f, 320.f);
		userCMD->viewangles.Clamp();
	}

	/*
	static QAngle last_choke_angle = QAngle(0, 0, 0);

	if (!Global::bSendPacket && g_ClientState->chokedcommands - Global::prevChoked == 0)
	{ last_choke_angle = userCMD->viewangles; }
	else if (Global::bSendPacket && g_ClientState->chokedcommands - Global::prevChoked > 0)
	{ Global::fakepitch = userCMD->viewangles.pitch; Global::fakeyaw = userCMD->viewangles.yaw;
	  Global::realpitch = last_choke_angle.pitch; Global::realyaw = last_choke_angle.yaw; }
	else if (Global::bSendPacket && g_ClientState->chokedcommands - Global::prevChoked == 0)
	{ Global::fakepitch = userCMD->viewangles.pitch; Global::fakeyaw = userCMD->viewangles.yaw;
	  Global::realpitch = userCMD->viewangles.pitch; Global::realyaw = userCMD->viewangles.yaw; }
	*/

	static float real_yaw = 0, real_pitch = 0, next_yaw = 0, next_pitch = 0;

	if (Global::bSendPacket && Global::last_packet)
	{
		Global::fakeyaw = userCMD->viewangles.yaw;
		Global::realyaw = userCMD->viewangles.yaw;

		if (g_Options.hvh_disable_antiut)
		{
			Global::fakepitch = Math::ClampPitch(userCMD->viewangles.pitch);
			Global::realpitch = Math::ComputeBodyPitch(userCMD->viewangles.pitch);
		}
		else
		{
			Global::fakepitch = userCMD->viewangles.pitch;
			Global::realpitch = userCMD->viewangles.pitch;
		}
	}
	else if (Global::bSendPacket)
	{
		Global::fakeyaw = userCMD->viewangles.yaw;

		if (g_Options.hvh_disable_antiut)
		{
			Global::fakepitch = Math::ClampPitch(userCMD->viewangles.pitch);
		}
		else
		{
			Global::fakepitch = userCMD->viewangles.pitch;
		}
	}
	else if (Global::last_packet)
	{
		real_yaw = next_yaw;
		real_pitch = next_pitch;
		next_yaw = userCMD->viewangles.yaw;

		if (g_Options.hvh_disable_antiut)
		{
			next_pitch = (Math::ComputeBodyPitch(userCMD->viewangles.pitch)) * ((Global::fakepitch < 0) ? (-1) : (1));
		}
		else
		{
			next_pitch = userCMD->viewangles.pitch;
		}
	}

	Global::realyaw = real_yaw;
	Global::realpitch = real_pitch;
	if (!g_Options.hvh_disable_antiut) Global::realpitch = Global::fakepitch;

	if (g_Options.hvh_show_real_angles) Global::visualAngles = QAngle(Global::realpitch, Global::realyaw, 0);
	else Global::visualAngles = QAngle(Global::fakepitch, Global::fakeyaw, 0);

	if (!g_Options.rage_silent && Global::bVisualAimbotting)
		g_EngineClient->SetViewAngles(Global::vecVisualAimbotAngs);

	if (!o_TempEntities)
	{
		if (Global::use_ud_vmt)
		{
			g_pClientStateHook_ud->setup((uintptr_t*)((uintptr_t)g_ClientState + 0x8));
			g_pClientStateHook_ud->hook_index(36, Handlers::TempEntities_h);
			o_TempEntities = g_pClientStateHook_ud->get_original<TempEntities_t>(36);
		}
		else
		{
			g_pClientStateHook->Setup((uintptr_t*)((uintptr_t)g_ClientState + 0x8));
			g_pClientStateHook->Hook(36, Handlers::TempEntities_h);
			o_TempEntities = g_pClientStateHook->GetOriginal<TempEntities_t>(36);
		}
	}
	if (Global::bSendPacket)
		Global::prevChoked = g_ClientState->chokedcommands;
	Global::last_packet = Global::bSendPacket;

	return false;
}

void __stdcall Handlers::PlaySound_h(const char *folderIme)
{
	o_PlaySound(g_VGuiSurface, folderIme);

	if (!g_Options.misc_autoaccept) return;
	junkcode::call();
	if (!strcmp(folderIme, "!UI/competitive_accept_beep.wav"))
	{
		Utils::IsReady();

		FLASHWINFO flash;
		flash.cbSize = sizeof(FLASHWINFO);
		flash.hwnd = window;
		flash.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
		flash.uCount = 0;
		flash.dwTimeout = 0;
		FlashWindowEx(&flash);
	}
}

IDirect3DStateBlock9* pixel_state = NULL; IDirect3DVertexDeclaration9* vertDec; IDirect3DVertexShader9* vertShader;
DWORD dwOld_D3DRS_COLORWRITEENABLE;
void SaveState(IDirect3DDevice9 * pDevice)
{
	pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &dwOld_D3DRS_COLORWRITEENABLE);
	//	pDevice->CreateStateBlock(D3DSBT_PIXELSTATE, &pixel_state); // This seam not to be needed anymore because valve fixed their shit
	pDevice->GetVertexDeclaration(&vertDec);
	pDevice->GetVertexShader(&vertShader);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
	pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
	pDevice->SetSamplerState(NULL, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	pDevice->SetSamplerState(NULL, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	pDevice->SetSamplerState(NULL, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
	pDevice->SetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, NULL);
}

void RestoreState(IDirect3DDevice9 * pDevice) // not restoring everything. Because its not needed.
{
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, dwOld_D3DRS_COLORWRITEENABLE);
	pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, true);
	//pixel_state->Apply(); 
	//pixel_state->Release();
	pDevice->SetVertexDeclaration(vertDec);
	pDevice->SetVertexShader(vertShader);
}

HRESULT __stdcall Handlers::EndScene_h(IDirect3DDevice9 *pDevice)
{
	if (!GladiatorMenu::d3dinit)
		GladiatorMenu::GUI_Init(window, pDevice);

	/* This is used because the game calls endscene twice each frame, one for ending the scene and one for finishing textures, this makes it just get called once */
	static auto wanted_ret_address = _ReturnAddress();
	if (_ReturnAddress() == wanted_ret_address)
	{
		/*
		//backup render states
		DWORD colorwrite, srgbwrite;
		pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &colorwrite);
		pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &srgbwrite);
			
		// fix drawing without calling engine functons/cl_showpos
		pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
		// removes the source engine color correction
		pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		*/

		SaveState(pDevice);

    	ImGui::GetIO().MouseDrawCursor = menuOpen;

    	ImGui_ImplDX9_NewFrame();
	
		if (menuOpen)
			GladiatorMenu::mainWindow();

		ImGui::Render();

		RestoreState(pDevice);

		/*
		//restore these
		pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, colorwrite);
		pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, srgbwrite);
		*/
	}
	junkcode::call();
	ImGuiStyle &style = ImGui::GetStyle();
	if (menuOpen)
	{
		if (style.Alpha > 1.f)
			style.Alpha = 1.f;
		else if (style.Alpha != 1.f)
			style.Alpha += 0.01f;
	}
	else
	{
		if (style.Alpha < 0.f)
			style.Alpha = 0.f;
		else if (style.Alpha != 0.f)
			style.Alpha -= 0.01f;
	}

	if ((g_EngineClient->IsInGame() && g_EngineClient->IsConnected()) && g_Options.misc_revealAllRanks && g_InputSystem->IsButtonDown(KEY_TAB)) // need sanity check, cause called inside EndScene
		Utils::RankRevealAll();

	return o_EndScene(pDevice);
}

HRESULT __stdcall Handlers::Reset_h(IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	if (!GladiatorMenu::d3dinit)
		return o_Reset(pDevice, pPresentationParameters);

	ImGui_ImplDX9_InvalidateDeviceObjects();

	auto hr = o_Reset(pDevice, pPresentationParameters);

	if (hr == D3D_OK)
	{
		ImGui_ImplDX9_CreateDeviceObjects();
		Visuals::InitFont();
	}

	return hr;
}

LRESULT __stdcall Handlers::WndProc_h(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_LBUTTONDOWN:

		pressedKey[VK_LBUTTON] = true;
		break;

	case WM_LBUTTONUP:

		pressedKey[VK_LBUTTON] = false;
		break;

	case WM_RBUTTONDOWN:

		pressedKey[VK_RBUTTON] = true;
		break;

	case WM_RBUTTONUP:

		pressedKey[VK_RBUTTON] = false;
		break;

	case WM_KEYDOWN:

		pressedKey[wParam] = true;
		break;

	case WM_KEYUP:

		pressedKey[wParam] = false;
		break;

	default: break;
	}

	GladiatorMenu::openMenu();

	if (GladiatorMenu::d3dinit && menuOpen && ImGui_ImplDX9_WndProcHandler(hWnd, uMsg, wParam, lParam) && !input_shouldListen)
		return true;

	return CallWindowProc(oldWindowProc, hWnd, uMsg, wParam, lParam);
}

ConVar* r_DrawSpecificStaticProp, *fog_enable, *fog_enable_water_fog, *fog_enableskybox;

int GetFPS()
{
	static int fps = 0;
	static int count = 0;
	using namespace std::chrono;
	count++;
	if (Global::use_ud_vmt)
	{
		auto now = g_GlobalVars->realtime;
		static auto last = g_GlobalVars->realtime;

		if (now - last > 1)
		{
			fps = count;
			count = 0;
			last = now;
		}
	}
	else
	{
		auto now = high_resolution_clock::now();
		static auto last = high_resolution_clock::now();

		if (duration_cast<milliseconds>(now - last).count() > 1000)
		{
			fps = count;
			count = 0;
			last = now;
		}
	}

	return fps;
}

void __stdcall Handlers::FrameStageNotify_h(ClientFrameStage_t stage)
{
	g_LocalPlayer = C_BasePlayer::GetLocalPlayer(true);

	static bool OldNightmode;
	static bool OldAsuswall;
	static int OldNightcolor; //not even near p but ''p in theory'' methods doesnt work for some unknown reason.
	int nightcolor = (g_Options.visuals_others_nightmode_color[0] + g_Options.visuals_others_nightmode_color[1] + g_Options.visuals_others_nightmode_color[2]) * 100;//blaim microsoft
	if (!g_LocalPlayer || !g_EngineClient->IsInGame() || !g_EngineClient->IsConnected()) {
		OldNightmode = false;
		return o_FrameStageNotify(stage);
	}

	//CMBacktracking::Get().AnimationFix2(stage);

	QAngle aim_punch_old;
	QAngle view_punch_old;

	QAngle *aim_punch = nullptr;
	QAngle *view_punch = nullptr;

	bool colormod_skybox = false;
	std::string mapname = g_EngineClient->GetLevelName();

	if (mapname == "maps\\de_nuke.bsp" || mapname == "maps\\de_dust2.bsp" || mapname == "maps\\de_inferno.bsp" || mapname == "maps\\de_canals.bsp")
		colormod_skybox = true;
	junkcode::call();
	if ((OldNightmode != g_Options.visuals_others_nightmode || OldNightcolor != nightcolor|| OldAsuswall != g_Options.visuals_others_asuswall) && !g_Options.misc_smacbypass)
	{

		if (!r_DrawSpecificStaticProp)
			r_DrawSpecificStaticProp = g_CVar->FindVar("r_DrawSpecificStaticProp");
		if (!fog_enable)
			fog_enable = g_CVar->FindVar("fog_enable");
		if (!fog_enableskybox)
			fog_enableskybox = g_CVar->FindVar("fog_enableskybox");
		if (!fog_enable_water_fog)
			fog_enable_water_fog = g_CVar->FindVar("fog_enable_water_fog");

		r_DrawSpecificStaticProp->SetValue(0);
		fog_enable->SetValue(0);
		fog_enableskybox->SetValue(0);
		fog_enable_water_fog->SetValue(0);

		for (MaterialHandle_t i = g_MatSystem->FirstMaterial(); i != g_MatSystem->InvalidMaterial(); i = g_MatSystem->NextMaterial(i))
		{
			if (i == 0)		//i = 0 == crash? 687 and 1164 possably too, needs more testing tho
				continue;

			IMaterial* pMaterial = g_MatSystem->GetMaterial(i);

			if (!pMaterial)
				continue;

			if (strstr(pMaterial->GetTextureGroupName(), "World") || strstr(pMaterial->GetTextureGroupName(), "StaticProp") || strstr(pMaterial->GetTextureGroupName(), "Model") || strstr(pMaterial->GetTextureGroupName(), "Decal") || strstr(pMaterial->GetTextureGroupName(), "Unaccounted") || strstr(pMaterial->GetTextureGroupName(), "Morph") || strstr(pMaterial->GetTextureGroupName(), "Lighting") || (strstr(pMaterial->GetTextureGroupName(), "SkyBox") && colormod_skybox))
			//if (!((strstr(pMaterial->GetTextureGroupName(), "SkyBox") && !colormod_skybox) || strstr(pMaterial->GetTextureGroupName(), "Other") || strstr(pMaterial->GetTextureGroupName(), "Pixel Shaders") || strstr(pMaterial->GetTextureGroupName(), "ClientEffect") || strstr(pMaterial->GetTextureGroupName(), "Lighting")) || strstr(pMaterial->GetTextureGroupName(), "de_") || strstr(pMaterial->GetTextureGroupName(), "cs_"))
			{
				if (g_Options.visuals_others_nightmode)
				{
					if (strstr(pMaterial->GetTextureGroupName(), "Lighting"))
						pMaterial->ColorModulate(2.0f - g_Options.visuals_others_nightmode_color[0], 2.0f - g_Options.visuals_others_nightmode_color[1], 2.0f - g_Options.visuals_others_nightmode_color[2]);
					else if ((strstr(pMaterial->GetTextureGroupName(), "StaticProp") || (strstr(pMaterial->GetTextureGroupName(), "Model"))) && !(strstr(pMaterial->GetName(), "player") || strstr(pMaterial->GetName(), "chams") || strstr(pMaterial->GetName(), "debug/debugdrawflat")))
						pMaterial->ColorModulate(min(1.0f, pow(g_Options.visuals_others_nightmode_color[0] - 1, 2) * -0.8 + 1), min(1.0f, pow(g_Options.visuals_others_nightmode_color[1] - 1, 2) * -0.8 + 1), min(1.0f, pow(g_Options.visuals_others_nightmode_color[2] - 1, 2) * -0.8 + 1));
					else if (!(strstr(pMaterial->GetName(), "player") || strstr(pMaterial->GetName(), "chams") || strstr(pMaterial->GetName(), "debug/debugdrawflat")))
						pMaterial->ColorModulate(g_Options.visuals_others_nightmode_color[0], g_Options.visuals_others_nightmode_color[1], g_Options.visuals_others_nightmode_color[2]);
				}
				else
					pMaterial->ColorModulate(1.0f, 1.0f, 1.0f);
				if (g_Options.visuals_others_asuswall)
				{
					if (strstr(pMaterial->GetTextureGroupName(), "StaticProp"))
						pMaterial->AlphaModulate(g_Options.visuals_others_nightmode_color[3]);
				}
				else
					pMaterial->AlphaModulate(1.0f);
			}
		}

		OldNightmode = g_Options.visuals_others_nightmode;
		OldAsuswall = g_Options.visuals_others_asuswall;
		OldNightcolor = nightcolor;
	}

	if (stage == ClientFrameStage_t::FRAME_NET_UPDATE_POSTDATAUPDATE_START)
	{
		if (g_Options.skinchanger_enabled)
			Skinchanger::Get().Work();

		/*
		if (g_LocalPlayer != nullptr && g_LocalPlayer->IsAlive() && g_Options.hvh_antiaim_lby_breaker)
		{												//new (definally not pasted lol) lby breaker!
			AntiAim::Get().UpdateLowerBodyBreaker(g_LocalPlayer->m_angEyeAngles());
			AntiAim::Get().FrameStageNotify(stage);
		}
		*/

		Miscellaneous::Get().PunchAngleFix_FSN();
	}

	if (stage == ClientFrameStage_t::FRAME_NET_UPDATE_POSTDATAUPDATE_END)
	{
		for (int i = 1; i < g_EntityList->GetHighestEntityIndex(); i++)
		{
			C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);

			if (!player)
				continue;

			if (player == g_LocalPlayer)
				continue;

			if (!player->IsAlive())
				continue;

			if (player->m_iTeamNum() == g_LocalPlayer->m_iTeamNum())
				continue;

			VarMapping_t *map = player->VarMapping();
			if (map)
			{
				for (int j = 0; j < map->m_nInterpolatedEntries; j++)
				{
					map->m_Entries[j].m_bNeedsToInterpolate = !g_Options.rage_lagcompensation;
				}
			}
		}

		if (g_Options.hvh_resolver)
			Resolver::Get().Resolve();
	}
	junkcode::call();
	if (stage == ClientFrameStage_t::FRAME_RENDER_START)
	{
		*(bool*)Offsets::bOverridePostProcessingDisable = g_Options.removals_postprocessing;

		if (g_LocalPlayer->IsAlive() || g_LocalPlayer->m_bIsPlayerGhost())
		{
			static ConVar *default_skyname = g_CVar->FindVar("sv_skyname");
			static int iOldSky = 0;

			if (iOldSky != g_Options.visuals_others_sky)
			{
				Utils::LoadNamedSkys(g_Options.visuals_others_sky == 0 ? default_skyname->GetString() : opt_Skynames[g_Options.visuals_others_sky]);
				iOldSky = g_Options.visuals_others_sky;
			}

			if (g_Options.removals_novisualrecoil)
			{
				aim_punch = &g_LocalPlayer->m_aimPunchAngle();
				view_punch = &g_LocalPlayer->m_viewPunchAngle();

				aim_punch_old = *aim_punch;
				view_punch_old = *view_punch;
				Global::view_punch_old = *view_punch;

				*aim_punch = QAngle(0.f, 0.f, 0.f);
				*view_punch = QAngle(0.f, 0.f, 0.f);
			}

			if (*(bool*)((uintptr_t)g_Input + 0xAD))
			{
				g_LocalPlayer->visuals_Angles() = Global::visualAngles;
			}
		}

		if (g_Options.removals_smoke && g_LocalPlayer)
			*(int*)Offsets::smokeCount = 0;

		if (g_Options.removals_flash && g_LocalPlayer)
			if (g_LocalPlayer->m_flFlashDuration() > 0.f)
				g_LocalPlayer->m_flFlashDuration() = 0.f;
	}

	if (!checks::is_bad_ptr(o_FrameStageNotify))	//expiremental fix
	{
		o_FrameStageNotify(stage);

		if (g_EngineClient->IsConnected() && g_EngineClient->IsInGame())
		{
			if (stage == ClientFrameStage_t::FRAME_NET_UPDATE_END)
			{
				if (g_Options.hvh_resolver)
				{
					if (g_EngineClient->IsConnected() && g_EngineClient->IsInGame())
						Resolver::Get().Log();
					else
						Resolver::Get().records->clear();
				}

				if (g_Options.rage_lagcompensation || g_Options.legit_backtrack)
					CMBacktracking::Get().FrameUpdatePostEntityThink();
			}

			if (!checks::is_bad_ptr(g_LocalPlayer))
			{
				if (g_Options.rage_enabled && stage == ClientFrameStage_t::FRAME_NET_UPDATE_START)									//getting new server-sided animation info
				{
					for (int i = 1; i < g_GlobalVars->maxClients; i++)
					{
						static float last_simtime[65] = { 0 };
						static C_BasePlayer *players[65] = { nullptr };
						auto player = C_BasePlayer::GetPlayerByIndex(i);
						if (!checks::is_bad_ptr(player) && player->m_flSimulationTime() != last_simtime[i])	//don't update animation when packet is choked
						{
							if (players[i] != player)
							{
								players[i] = player;
								continue;
							}

							last_simtime[i] = player->m_flSimulationTime();
							try
							{
								Animation::Get().UpdatePlayerAnimations(i);
							}
							catch (...)
							{
								Utils::ConsolePrint(true, "Aniation Update Failure! index %d", i);
							}
						}
					}
				}

				if (g_Options.rage_enabled && stage == ClientFrameStage_t::FRAME_RENDER_START)
				{
					for (int i = 1; i < g_GlobalVars->maxClients; i++)										//starting render, apply animation fix
					{
						auto player = C_BasePlayer::GetPlayerByIndex(i);
						if (checks::is_bad_ptr(player))
							continue;

						Animation::Get().HandleAnimFix(player, false);
					}

					if (g_LocalPlayer && g_LocalPlayer->IsAlive())
					{
						if (g_Options.removals_novisualrecoil && (aim_punch && view_punch))
						{
							*aim_punch = aim_punch_old;
							*view_punch = view_punch_old;
						}
					}
				}

				if (stage == ClientFrameStage_t::FRAME_RENDER_END)
				{
					if (g_Options.rage_enabled)
					{
						for (int i = 1; i < g_GlobalVars->maxClients; i++)										//render finished, restore animation fix
						{
							auto player = C_BasePlayer::GetPlayerByIndex(i);
							if (checks::is_bad_ptr(player))
								continue;

							Animation::Get().HandleAnimFix(player, true);
						}
					}
					Global::fps = GetFPS();
				}
			}
		}
	}
}

bool __fastcall Handlers::FireEventClientSide_h(void *thisptr, void*, IGameEvent *gEvent)
{
	if (!gEvent)
		return o_FireEventClientSide(thisptr, gEvent);

	if (strcmp(gEvent->GetName(), "game_newmap") == 0)
	{
		static ConVar *default_skyname = g_CVar->FindVar("sv_skyname");
		Utils::LoadNamedSkys(g_Options.visuals_others_sky == 0 ? default_skyname->GetString() : opt_Skynames[g_Options.visuals_others_sky]);
		Miscellaneous::Get().bDoNameExploit = true;
		for (int i = 0; i < 65; i++)
		{
			Resolver::Get().missed_shots[i] = 0;
			Animation::Get().GetPlayerAnimationRecord(i).clear();
		}

		g_pSendDatagramHook = nullptr;
		g_pSendDatagramHook_ud = nullptr;
		Global::netchan = nullptr;
	}

	return o_FireEventClientSide(thisptr, gEvent);
}

void __fastcall Handlers::BeginFrame_h(void *thisptr, void*, float ft)
{
	Miscellaneous::Get().NameStealer();
	if (!g_Options.misc_stealname)  Miscellaneous::Get().NameChanger();
	if (g_Options.misc_antiayy) Miscellaneous::Get().AntiAyyware();
	if (g_Options.misc_hidename) Miscellaneous::Get().NameHider();
	if (g_Options.misc_namespam) Miscellaneous::Get().NameSpam();

	Miscellaneous::Get().ChatSpamer();
	BulletImpactEvent::Get().Paint();

	o_BeginFrame(thisptr, ft);
}

void __fastcall Handlers::SetKeyCodeState_h(void* thisptr, void* EDX, ButtonCode_t code, bool bDown)
{
	if (input_shouldListen && bDown)
	{
		input_shouldListen = false;
		if (input_receivedKeyval)
			*input_receivedKeyval = code;
	}

	return o_SetKeyCodeState(thisptr, code, bDown);
}

void __fastcall Handlers::SetMouseCodeState_h(void* thisptr, void* EDX, ButtonCode_t code, MouseCodeState_t state)
{
	if (input_shouldListen && state == BUTTON_PRESSED)
	{
		input_shouldListen = false;
		if (input_receivedKeyval)
			*input_receivedKeyval = code;
	}

	return o_SetMouseCodeState(thisptr, code, state);
}

void __stdcall Handlers::OverrideView_h(CViewSetup* pSetup)
{
	// Do no zoom aswell.
	static float orifov = pSetup->fov;
	g_Options.visuals_others_player_fov_ignore_zoom ? pSetup->fov = orifov + g_Options.visuals_others_player_fov : pSetup->fov += g_Options.visuals_others_player_fov;
	if (g_Options.visuals_others_player_fov_ignore_zoom && g_EngineClient->IsInGame())
		g_CVar->FindVar("zoom_sensitivity_ratio_mouse")->SetValue(0);
	else
		g_CVar->FindVar("zoom_sensitivity_ratio_mouse")->SetValue(1);

	o_OverrideView(pSetup);
	junkcode::call();
	if (g_EngineClient->IsInGame() && g_EngineClient->IsConnected())
	{
		if (g_LocalPlayer)
		{
			CCSGrenadeHint::Get().View();

			Miscellaneous::Get().ThirdPerson();
		}
	}
}

void Proxies::didSmokeEffect(const CRecvProxyData *pData, void *pStruct, void *pOut)
{
	static bool was_removing = false;
	std::vector<const char*> wireframesmoke_mats =
	{
		"particle/vistasmokev1/vistasmokev1_emods",
		"particle/vistasmokev1/vistasmokev1_emods_impactdust",
		"particle/vistasmokev1/vistasmokev1_fire",
		"particle/vistasmokev1/vistasmokev1_smokegrenade",
	};

	if (g_Options.removals_smoke)
	{
		if (g_Options.removals_smoke_type == 0)
			*(bool*)((DWORD)pOut + 0x1) = true;

		if (g_Options.removals_smoke_type == 1)
		{
			for (auto smoke_mat : wireframesmoke_mats)
			{
				IMaterial* mat = g_MatSystem->FindMaterial(smoke_mat, TEXTURE_GROUP_OTHER);
				mat->SetMaterialVarFlag(MATERIAL_VAR_WIREFRAME, true);
			}
		}

		was_removing = true;
	}
	else if (was_removing)
	{
		*(bool*)((DWORD)pOut + 0x1) = false;

		for (auto smoke_mat : wireframesmoke_mats)
		{
			IMaterial* mat = g_MatSystem->FindMaterial(smoke_mat, TEXTURE_GROUP_OTHER);
			mat->SetMaterialVarFlag(MATERIAL_VAR_WIREFRAME, false);
		}

		was_removing = false;
	}

	o_didSmokeEffect(pData, pStruct, pOut);
}

void Proxies::storePlayerPitch(const CRecvProxyData *pData, void *pStruct, void *pOut)
{
	*reinterpret_cast<float*>(pOut) = pData->m_Value.m_Float;

	auto entity = reinterpret_cast<C_BaseEntity*>(pStruct);
	if (checks::is_bad_ptr(entity))
		return;

	Resolver::Get().networkedPlayerPitch[entity->EntIndex()] = pData->m_Value.m_Float;
}
void Proxies::storePlayeryaw(const CRecvProxyData *pData, void *pStruct, void *pOut)
{
	*reinterpret_cast<float*>(pOut) = pData->m_Value.m_Float;

	auto entity = reinterpret_cast<C_BaseEntity*>(pStruct);
	if (checks::is_bad_ptr(entity))
		return;

	Resolver::Get().networkedPlayerYaw[entity->EntIndex()] = pData->m_Value.m_Float;
}
void Proxies::LBYUpdated(const CRecvProxyData *pData, void *pStruct, void *pOut)
{
	*reinterpret_cast<float*>(pOut) = pData->m_Value.m_Float;

	auto ent = reinterpret_cast<C_BasePlayer*>(pStruct);

	if (ent == g_LocalPlayer)
	{
		float curtime = AimRage::Get().GetTickbase() * g_GlobalVars->interval_per_tick;
		//Global::LastLBYUpdate = curtime;
	}
}

bool __stdcall Handlers::InPrediction_h()
{
	if (g_Options.rage_fixup_entities)
	{
		// Breaks more than it fixes.
		//// xref : "%8.4f : %30s : %5.3f : %4.2f  +\n" https://github.com/ValveSoftware/source-sdk-2013/blob/master/mp/src/game/client/c_baseanimating.cpp#L1808
		//static DWORD inprediction_check = (DWORD)Utils::PatternScan(GetModuleHandle("client.dll"), "84 C0 74 17 8B 87");
		//if (inprediction_check == (DWORD)_ReturnAddress()) {
		//	return true; // no sequence transition / decay
		//}
	}
	junkcode::call();
	return o_OriginalInPrediction(g_Prediction);
}

bool __fastcall Handlers::SetupBones_h(void* ECX, void* EDX, matrix3x4_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime)
{
	// Supposed to only setupbones tick by tick, instead of frame by frame.
	if (g_Options.rage_lagcompensation)
	{
		if (ECX && ((IClientRenderable*)ECX)->GetIClientUnknown())
		{
			IClientNetworkable* pNetworkable = ((IClientRenderable*)ECX)->GetIClientUnknown()->GetClientNetworkable();
			if (pNetworkable && pNetworkable->GetClientClass() && pNetworkable->GetClientClass()->m_ClassID == ClassId::ClassId_CCSPlayer)
			{
				static auto host_timescale = g_CVar->FindVar(("host_timescale"));
				auto player = (C_BasePlayer*)ECX;
				float OldCurTime = g_GlobalVars->curtime;
				float OldRealTime = g_GlobalVars->realtime;
				float OldFrameTime = g_GlobalVars->frametime;
				float OldAbsFrameTime = g_GlobalVars->absoluteframetime;
				float OldAbsFrameTimeStart = g_GlobalVars->absoluteframestarttimestddev;
				float OldInterpAmount = g_GlobalVars->interpolation_amount;
				int OldFrameCount = g_GlobalVars->framecount;
				int OldTickCount = g_GlobalVars->tickcount;

				g_GlobalVars->curtime = player->m_flSimulationTime();
				g_GlobalVars->realtime = player->m_flSimulationTime();
				g_GlobalVars->frametime = g_GlobalVars->interval_per_tick * host_timescale->GetFloat();
				g_GlobalVars->absoluteframetime = g_GlobalVars->interval_per_tick * host_timescale->GetFloat();
				g_GlobalVars->absoluteframestarttimestddev = player->m_flSimulationTime() - g_GlobalVars->interval_per_tick * host_timescale->GetFloat();
				g_GlobalVars->interpolation_amount = 0;
				g_GlobalVars->framecount = TIME_TO_TICKS(player->m_flSimulationTime());
				g_GlobalVars->tickcount = TIME_TO_TICKS(player->m_flSimulationTime());

				*(int*)((int)player + 236) |= 8; // IsNoInterpolationFrame
				bool ret_value = o_SetupBones(player, pBoneToWorldOut, nMaxBones, boneMask, g_GlobalVars->curtime);
				*(int*)((int)player + 236) &= ~8; // (1 << 3)

				g_GlobalVars->curtime = OldCurTime;
				g_GlobalVars->realtime = OldRealTime;
				g_GlobalVars->frametime = OldFrameTime;
				g_GlobalVars->absoluteframetime = OldAbsFrameTime;
				g_GlobalVars->absoluteframestarttimestddev = OldAbsFrameTimeStart;
				g_GlobalVars->interpolation_amount = OldInterpAmount;
				g_GlobalVars->framecount = OldFrameCount;
				g_GlobalVars->tickcount = OldTickCount;
				return ret_value;
			}
		}
	}
	return o_SetupBones(ECX, pBoneToWorldOut, nMaxBones, boneMask, currentTime);
}

void __fastcall Handlers::DrawModelExecute_h(void* ecx, void* edx, IMatRenderContext* context, const DrawModelState_t& state, const ModelRenderInfo_t& render_info, matrix3x4_t* matrix)
{
	static void *gameret = nullptr;
	static bool last_tp = false;
	auto chamat = g_MatSystem->FindMaterial("debug/debugdrawflat", TEXTURE_GROUP_MODEL);
	auto ent = C_BasePlayer::GetPlayerByIndex(render_info.entity_index);
	bool is_player = (checks::is_bad_ptr(ent) ? false : ent->IsPlayer());
	if (g_EngineClient->IsConnected() && g_EngineClient->IsInGame())
	{
		std::string name = g_MdlInfo->GetModelName(render_info.pModel);
		if (is_player && gameret == nullptr)
		{
			gameret = _ReturnAddress();		//i can just probably do static auto gameret = _returnaddress() but just a lil sanity check
		}

		//if (is_player && _ReturnAddress() == gameret && g_Options.esp_player_chams && (ent->m_iTeamNum() == g_LocalPlayer->m_iTeamNum() || !g_Options.esp_enemies_only))
		//	return;		//no double drawing meme :)

		if ((render_info.entity_index == g_LocalPlayer->EntIndex() && is_player &&
			!checks::is_bad_ptr(g_LocalPlayer->m_hActiveWeapon().Get()) && g_LocalPlayer->m_hActiveWeapon().Get()->IsSniper() &&
			g_LocalPlayer->m_hActiveWeapon().Get()->m_zoomLevel() != 0))
		{
			if (last_tp && g_RenderView->GetBlend() > 0.1f)
			{
				g_RenderView->SetBlend(0.5f);
			}
			last_tp = g_Options.misc_thirdperson;
		}

		if (!is_player)	//don't run these checks if it's player. fps saving 1/2
		{
			if (!g_Options.misc_thirdperson) //no need if in thirdperson. fps saving 2/2
			{
				if (name.find("v_sleeve") != std::string::npos)
				{
					IMaterial* material = g_MatSystem->FindMaterial(name.c_str(), TEXTURE_GROUP_MODEL);
					if (!material) return;
					material->SetMaterialVarFlag(MATERIAL_VAR_NO_DRAW, true);
					g_MdlRender->ForcedMaterialOverride(material);
				}
				else if (g_Options.esp_hand_chams && name.find("arm") != std::string::npos)
				{
					IMaterial *material = g_MatSystem->FindMaterial(name.c_str(), TEXTURE_GROUP_MODEL);
					switch (g_Options.esp_hand_chams)
					{
					case 1:
					{
						if (!material) return;
						material->SetMaterialVarFlag(MATERIAL_VAR_NO_DRAW, true);
						g_MdlRender->ForcedMaterialOverride(material);
						break;
					}
					case 3:
					{
						g_RenderView->SetColorModulation(g_Options.esp_hand_chams_color_wire);
						chamat->AlphaModulate(g_Options.esp_hand_chams_color_wire[3]);
						chamat->SetMaterialVarFlag(MATERIAL_VAR_WIREFRAME, true);
						chamat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, true);

						g_MdlRender->ForcedMaterialOverride(chamat);

						o_DrawModelExecute(ecx, context, state, render_info, matrix);

						chamat->SetMaterialVarFlag(MATERIAL_VAR_WIREFRAME, false);
						chamat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, false);

						g_MdlRender->ForcedMaterialOverride(nullptr);
					}
					case 2:
					{
						g_RenderView->SetColorModulation(g_Options.esp_hand_chams_color_invis);
						chamat->AlphaModulate(g_Options.esp_hand_chams_color_invis[3]);

						chamat->IncrementReferenceCount();
						chamat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, true);

						g_MdlRender->ForcedMaterialOverride(chamat);

						o_DrawModelExecute(ecx, context, state, render_info, matrix);

						g_MdlRender->ForcedMaterialOverride(nullptr);

						chamat->IncrementReferenceCount();

						g_RenderView->SetColorModulation(g_Options.esp_hand_chams_color_vis);
						chamat->AlphaModulate(g_Options.esp_hand_chams_color_vis[3]);

						chamat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, false);

						g_MdlRender->ForcedMaterialOverride(chamat);
						break;
					}
					}
				}
			}

			if (g_Options.removals_scope)
			{
				IMaterial *xblur_mat = g_MatSystem->FindMaterial("dev/blurfilterx_nohdr", TEXTURE_GROUP_OTHER, true);
				IMaterial *yblur_mat = g_MatSystem->FindMaterial("dev/blurfiltery_nohdr", TEXTURE_GROUP_OTHER, true);
				IMaterial *scope = g_MatSystem->FindMaterial("dev/scope_bluroverlay", TEXTURE_GROUP_OTHER, true);
				xblur_mat->SetMaterialVarFlag(MATERIAL_VAR_NO_DRAW, true);
				yblur_mat->SetMaterialVarFlag(MATERIAL_VAR_NO_DRAW, true);
				scope->SetMaterialVarFlag(MATERIAL_VAR_NO_DRAW, true);
			}
		}
	}
	o_DrawModelExecute(ecx, context, state, render_info, matrix);
	g_MdlRender->ForcedMaterialOverride(nullptr);
}

void processchams(C_BasePlayer *ent, IMaterial *mat, bool xqz)
{
	constexpr float color_gray[4] = { 166, 167, 169, 255 };

	if (xqz)
	{
		if (ent == g_LocalPlayer)
			g_RenderView->SetColorModulation(g_Options.esp_player_chams_color_local);
		else
			g_RenderView->SetColorModulation(ent->m_bGunGameImmunity() ? color_gray : (ent->m_iTeamNum() == 2 ? g_Options.esp_player_chams_color_t : g_Options.esp_player_chams_color_ct));

		mat->IncrementReferenceCount();
		mat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, true);

		g_MdlRender->ForcedMaterialOverride(mat);

		ent->DrawModel(0x1, 255);
		g_MdlRender->ForcedMaterialOverride(nullptr);
	}

	if (ent == g_LocalPlayer)
		g_RenderView->SetColorModulation(g_Options.esp_player_chams_color_local_visiable);
	else
		g_RenderView->SetColorModulation(ent->m_bGunGameImmunity() ? color_gray : (ent->m_iTeamNum() == 2 ? g_Options.esp_player_chams_color_t_visible : g_Options.esp_player_chams_color_ct_visible));

	if (xqz) mat->IncrementReferenceCount();
	mat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, false);

	g_MdlRender->ForcedMaterialOverride(mat);

	ent->DrawModel(0x1, 255);

	g_MdlRender->ForcedMaterialOverride(nullptr);
}

void processchams(C_BasePlayer *ent, IMaterial *mat, bool xqz, float viscolor[4], float inviscolor[4] = { 0 })
{
	if (xqz)
	{
		g_RenderView->SetColorModulation(inviscolor);

		mat->IncrementReferenceCount();
		mat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, true);

		g_MdlRender->ForcedMaterialOverride(mat);

		ent->DrawModel(0x1, 255);
		g_MdlRender->ForcedMaterialOverride(nullptr);
	}
	junkcode::call();
	g_RenderView->SetColorModulation(viscolor);

	if (xqz) mat->IncrementReferenceCount();
	mat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, false);

	g_MdlRender->ForcedMaterialOverride(mat);

	ent->DrawModel(0x1, 255);

	g_MdlRender->ForcedMaterialOverride(nullptr);
}

void __fastcall Handlers::SceneEnd_h(void* thisptr, void* edx)
{
	if (!g_LocalPlayer || !g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
		return o_SceneEnd(thisptr);

	o_SceneEnd(thisptr);

	if (g_Options.esp_player_chams)
	{
		constexpr float color_gray[4] = { 166, 167, 169, 255 };
		IMaterial *mat =
			(g_Options.esp_player_chams_type < 2 ?
				g_MatSystem->FindMaterial("chams", TEXTURE_GROUP_MODEL) :
				g_MatSystem->FindMaterial("debug/debugdrawflat", TEXTURE_GROUP_MODEL));

		if (!mat || mat->IsErrorMaterial())
			return;

		
		for (int i = 1; i < g_GlobalVars->maxClients; ++i)
		{
			auto ent = static_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(i));
			if (ent && ent->IsAlive() && !ent->IsDormant())
			{
				if (g_Options.esp_enemies_only && ent->m_iTeamNum() == g_LocalPlayer->m_iTeamNum())
					continue;

				static Vector oripos = Vector(1, 1, 1);
				static QAngle oriang = QAngle(1, 1, 1);

				if (g_Options.esp_backtracked_player_chams && AimRage::Get().CheckTarget(ent->EntIndex()))
				{
					Vector oripos = ent->m_vecOrigin();
					QAngle oriang = ent->GetAbsAngles();

					auto records = &CMBacktracking::Get().m_LagRecord[ent->EntIndex()];
					auto anim = Animation::Get().GetPlayerAnimationInfo(ent->EntIndex());
					std::deque<LagRecord>::iterator highest = records->begin();
					for (auto record = records->begin(); record != records->end(); record++)
					{
						if (!CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(record->m_flSimulationTime)))
							continue;

						if (highest->m_flSimulationTime > record->m_flSimulationTime)
							highest = record;
					}
					for (auto record = records->begin(); record != records->end(); record++)
					{
						if (!CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(record->m_flSimulationTime)))
							continue;

						if (record->m_vecAbsOrigin == oripos && record->m_angAngles.yaw == oriang.yaw)
							continue;

						if (g_Options.rage_enabled) Animation::Get().RestoreAnim(ent, record->m_flSimulationTime);

						ent->SetAbsOrigin(record->m_vecAbsOrigin);
						ent->SetAbsAngles(QAngle(0, record->m_angAngles.yaw, 0));

						if (CMBacktracking::Get().current_record[ent->EntIndex()].m_vecOrigin == record->m_vecAbsOrigin)
						{
							float temp[4] = { 0, 1, 0, 1 };
							g_RenderView->SetColorModulation(temp);
						}
						else
						{
							g_RenderView->SetColorModulation(g_Options.esp_backtrack_chams_color);
						}

						mat->AlphaModulate(g_Options.esp_backtrack_chams_color[3]);
						mat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, record == highest && (g_Options.esp_player_chams_type == 1 || g_Options.esp_player_chams_type == 3));

						g_MdlRender->ForcedMaterialOverride(mat);

						ent->DrawModel(0x1, 255);
						g_MdlRender->ForcedMaterialOverride(nullptr);
					}
					if (g_Options.rage_enabled) Animation::Get().ApplyAnim(ent, anim);

					ent->SetAbsOrigin(oripos);
					ent->SetAbsAngles(QAngle(0, oriang.yaw, 0));
					mat->IncrementReferenceCount();
					mat->AlphaModulate(1.0f);
				}

				if (ent == g_LocalPlayer && g_Options.esp_player_fakechams)
				{

					oripos = g_LocalPlayer->m_vecOrigin();
					oriang = g_LocalPlayer->GetAbsAngles();
					auto animbak = Animation::Get().GetPlayerAnimationInfo(g_LocalPlayer->EntIndex());
					auto anim = animbak;

					QAngle Fake, LBY = QAngle(0, 0, 0);
					Fake.yaw = Math::ClampYaw(Global::fakeyaw);
					LBY.yaw = Math::ClampYaw(g_LocalPlayer->m_flLowerBodyYawTarget());

					static Vector lastpos = Vector(1, 1, 1);
					if (Global::bSendPacket)
						lastpos = g_LocalPlayer->m_vecOrigin();

					g_RenderView->SetColorModulation(g_Options.esp_player_chams_color_local_visiable);

					g_MdlRender->ForcedMaterialOverride(mat);

					ent->DrawModel(0x1, 255);

					g_RenderView->SetColorModulation((g_LocalPlayer->m_iTeamNum() == 2 ? g_Options.esp_player_chams_color_ct_visible : g_Options.esp_player_chams_color_t_visible));

					g_LocalPlayer->SetAbsAngles(Fake);
					g_LocalPlayer->SetAbsOrigin(lastpos);
					Animation::Get().UpdateAnimationAngles(anim, QAngle(Global::fakepitch, Global::fakeyaw, 0));
					Animation::Get().ApplyAnim(g_LocalPlayer, anim);

					g_MdlRender->ForcedMaterialOverride(mat);

					g_LocalPlayer->DrawModel(0x1, 255);

					g_LocalPlayer->SetAbsOrigin(oripos);

					if (abs(g_LocalPlayer->m_flLowerBodyYawTarget() - Global::realyaw) > 35 && abs(g_LocalPlayer->m_flLowerBodyYawTarget() - Global::fakeyaw) > 35)
					{
						g_RenderView->SetColorModulation((g_LocalPlayer->m_iTeamNum() == 2 ? g_Options.esp_player_chams_color_t_visible : g_Options.esp_player_chams_color_ct_visible));
						g_MdlRender->ForcedMaterialOverride(mat);
						g_LocalPlayer->SetAbsAngles(LBY);
						Animation::Get().UpdateAnimationAngles(anim, QAngle(Global::realpitch, g_LocalPlayer->m_flLowerBodyYawTarget(), 0));
						Animation::Get().ApplyAnim(g_LocalPlayer, anim);

						g_LocalPlayer->DrawModel(0x1, 255);
					}

					g_MdlRender->ForcedMaterialOverride(mat);
					g_LocalPlayer->SetAbsAngles(oriang);
					Animation::Get().ApplyAnim(g_LocalPlayer, animbak);

					g_MdlRender->ForcedMaterialOverride(nullptr);
				}
				else if (g_Options.esp_player_chams_type == 1 || g_Options.esp_player_chams_type == 3)	//fakechams + XQZ = looks garbage
				{	// XQZ Chams
					processchams(ent, mat, true);
				}
				else
				{	// Normal Chams
					processchams(ent, mat, false);
				}
			}
		}
	}

	if (g_Options.glow_enabled)
		Glow::RenderGlow();
}

void __stdcall FireBullets_PostDataUpdate(C_TEFireBullets *thisptr, DataUpdateType_t updateType)
{
	if (!g_LocalPlayer || !g_LocalPlayer->IsAlive())
		return o_FireBullets(thisptr, updateType);
	junkcode::call();
	if (g_Options.hvh_resolver && g_Options.rage_lagcompensation && thisptr)
	{
		int iPlayer = thisptr->m_iPlayer + 1;
		if (iPlayer < 64)
		{
			auto player = C_BasePlayer::GetPlayerByIndex(iPlayer);
			
			if (player && player != g_LocalPlayer && !player->IsDormant() && player->m_iTeamNum() != g_LocalPlayer->m_iTeamNum())
			{
				QAngle calcedAngle = Math::CalcAngle(player->GetEyePos(), g_LocalPlayer->GetEyePos());

				player->m_angEyeAngles().pitch = calcedAngle.pitch;
				player->m_angEyeAngles().yaw = calcedAngle.yaw;
				player->m_angEyeAngles().roll = 0.f;

				float
					event_time = g_GlobalVars->tickcount,
					player_time = player->m_flSimulationTime();

				auto lag_records = CMBacktracking::Get().m_LagRecord[iPlayer];

				float shot_time = TICKS_TO_TIME(event_time);
				for (auto& record : lag_records)
				{
					if (record.m_iTickCount <= event_time)
					{
						shot_time = record.m_flSimulationTime + TICKS_TO_TIME(event_time - record.m_iTickCount); // also get choked from this
#ifdef _DEBUG
						g_CVar->ConsoleColorPrintf(Color(0, 255, 0, 255), "Found <<exact>> shot time: %f, ticks choked to get here: %d\n", shot_time, event_time - record.m_iTickCount);
#endif
						break;
					}
#ifdef _DEBUG
					else
						g_CVar->ConsolePrintf("Bad curtime difference, EVENT: %f, RECORD: %f\n", event_time, record.m_iTickCount);
#endif
				}
#ifdef _DEBUG
				g_CVar->ConsolePrintf("CURTIME_TICKOUNT: %f, SIMTIME: %f, CALCED_TIME: %f\n", event_time, player_time, shot_time);
#endif
				// gay
				int32_t choked = floorf((TICKS_TO_TIME(event_time) - player_time) / g_GlobalVars->interval_per_tick) + 0.5;
				choked = (choked > 14 ? 14 : choked < 1 ? 0 : choked);
				player->m_vecOrigin() = (lag_records.begin()->m_vecOrigin + (g_GlobalVars->interval_per_tick * lag_records.begin()->m_vecVelocity * choked));

				Global::resolverModes[iPlayer] = "Shot";
				CMBacktracking::Get().SetOverwriteTick(player, calcedAngle, shot_time, 2);
			}
		}
	}

	o_FireBullets(thisptr, updateType);
}

__declspec (naked) void __stdcall Handlers::TEFireBulletsPostDataUpdate_h(DataUpdateType_t updateType)
{
	__asm
	{
		push[esp + 4]
		push ecx
		call FireBullets_PostDataUpdate
		retn 4
	}
}

bool __fastcall Handlers::TempEntities_h(void* ECX, void* EDX, void* msg)
{
	if (!g_LocalPlayer || !g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
		return o_TempEntities(ECX, msg);

	bool ret = o_TempEntities(ECX, msg);
	junkcode::call();
	auto CL_ParseEventDelta = [](void *RawData, void *pToData, RecvTable *pRecvTable)
	{
		// "RecvTable_DecodeZeros: table '%s' missing a decoder.", look at the function that calls it.
		static uintptr_t CL_ParseEventDeltaF = (uintptr_t)Utils::PatternScan(GetModuleHandle("engine.dll"), ("55 8B EC 83 E4 F8 53 57"));
		__asm
		{
			mov     ecx, RawData
			mov     edx, pToData
			push	pRecvTable
			call    CL_ParseEventDeltaF
			add     esp, 4
		}
	};

	// Filtering events
	if (!g_Options.rage_lagcompensation || !g_LocalPlayer->IsAlive())
		return ret;

	CEventInfo *ei = g_ClientState->events;
	CEventInfo *next = NULL;

	if (!ei)
		return ret;

	do
	{
		next = *(CEventInfo**)((uintptr_t)ei + 0x38);

		uint16_t classID = ei->classID - 1;

		auto m_pCreateEventFn = ei->pClientClass->m_pCreateEventFn; // ei->pClientClass->m_pCreateEventFn ptr
		if (!m_pCreateEventFn)
			continue;

		IClientNetworkable *pCE = m_pCreateEventFn();
		if (!pCE)
			continue;

		if (classID == ClassId::ClassId_CTEFireBullets)
		{
			// set fire_delay to zero to send out event so its not here later.
			ei->fire_delay = 0.0f;

			auto pRecvTable = ei->pClientClass->m_pRecvTable;
			void *BasePtr = pCE->GetDataTableBasePtr();

			// Decode data into client event object and use the DTBasePtr to get the netvars
			CL_ParseEventDelta(ei->pData, BasePtr, pRecvTable);

			if (!BasePtr)
				continue;

			// This nigga right HERE just fired a BULLET MANE
			int EntityIndex = *(int*)((uintptr_t)BasePtr + 0x10) + 1;

			auto pEntity = (C_BasePlayer*)g_EntityList->GetClientEntity(EntityIndex);
			if (pEntity && pEntity->GetClientClass() &&  pEntity->GetClientClass()->m_ClassID == ClassId::ClassId_CCSPlayer && pEntity->m_iTeamNum() != g_LocalPlayer->m_iTeamNum())
			{
				QAngle EyeAngles = QAngle(*(float*)((uintptr_t)BasePtr + 0x24), *(float*)((uintptr_t)BasePtr + 0x28), 0.0f),
					CalcedAngle = Math::CalcAngle(pEntity->GetEyePos(), g_LocalPlayer->GetEyePos());

				*(float*)((uintptr_t)BasePtr + 0x24) = CalcedAngle.pitch;
				*(float*)((uintptr_t)BasePtr + 0x28) = CalcedAngle.yaw;
				*(float*)((uintptr_t)BasePtr + 0x2C) = 0;

				float
					event_time = TICKS_TO_TIME(g_GlobalVars->tickcount),
					player_time = pEntity->m_flSimulationTime();

				// Extrapolate tick to hit scouters etc
				auto lag_records = CMBacktracking::Get().m_LagRecord[pEntity->EntIndex()];

				float shot_time = event_time;
				for (auto& record : lag_records)
				{
					if (TICKS_TO_TIME(record.m_iTickCount) <= event_time)
					{
						shot_time = record.m_flSimulationTime + (event_time - TICKS_TO_TIME(record.m_iTickCount)); // also get choked from this
#ifdef _DEBUG
						g_CVar->ConsoleColorPrintf(Color(0, 255, 0, 255), "Found exact shot time: %f, ticks choked to get here: %d\n", shot_time, TIME_TO_TICKS(event_time - TICKS_TO_TIME(record.m_iTickCount)));
#endif
						break;
					}
#ifdef _DEBUG
					else
						g_CVar->ConsolePrintf("Bad curtime difference, EVENT: %f, RECORD: %f\n", event_time, TICKS_TO_TIME(record.m_iTickCount));
#endif
				}
#ifdef _DEBUG
				g_CVar->ConsolePrintf("Calced angs: %f %f, Event angs: %f %f, CURTIME_TICKOUNT: %f, SIMTIME: %f, CALCED_TIME: %f\n", CalcedAngle.pitch, CalcedAngle.yaw, EyeAngles.pitch, EyeAngles.yaw, event_time, player_time, shot_time);
#endif
				if (!lag_records.empty())
				{
					int choked = floorf((event_time - player_time) / g_GlobalVars->interval_per_tick) + 0.5;
					choked = (choked > 14 ? 14 : choked < 1 ? 0 : choked);
					pEntity->m_vecOrigin() = (lag_records.begin()->m_vecOrigin + (g_GlobalVars->interval_per_tick * lag_records.begin()->m_vecVelocity * choked));
				}

				Global::resolverModes[EntityIndex] = "Shot";
				CMBacktracking::Get().SetOverwriteTick(pEntity, CalcedAngle, shot_time, 2);
			}
		}
		ei = next;
	} while (next != NULL);

	return ret;
}

float __fastcall Handlers::GetViewModelFov_h(void* ECX, void* EDX)
{
	return g_Options.visuals_others_player_fov_viewmodel + o_GetViewmodelFov(ECX);
}

bool __fastcall Handlers::GetBool_SVCheats_h(PVOID pConVar, int edx)
{
	// xref : "Pitch: %6.1f   Yaw: %6.1f   Dist: %6.1f %16s"
	static DWORD CAM_THINK = (DWORD)Utils::PatternScan(GetModuleHandle((Global::bIsPanorama ? "client_panorama.dll" : "client.dll")), "85 C0 75 30 38 86");
	if (!pConVar)
		return false;

	if (g_Options.misc_thirdperson)
	{
		if ((DWORD)_ReturnAddress() == CAM_THINK)
			return true;
	}

	return o_GetBool(pConVar);
}

static auto check_receiving_list_return = *reinterpret_cast<uint32_t**>(Utils::PatternScan(GetModuleHandle("engine.dll"), "FF 50 34 8B 1D ? ? ? ? 85 C0 74 16 FF B6") + 0x3);
static auto read_sub_channel_data_return = *reinterpret_cast<uint32_t**>(Utils::PatternScan(GetModuleHandle("engine.dll"), "FF 50 34 85 C0 74 12 53 FF 75 0C 68") + 0x3);

bool __fastcall Handlers::ShowFragments_h(void* cvar, void* edx)
{
	if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
		return o_ShowFragments(cvar);

	static uint32_t last_fragment = 0;

	if (_ReturnAddress() == check_receiving_list_return && last_fragment > 0)
	{
		const auto data = &reinterpret_cast<uint32_t*>(g_ClientState->m_NetChannel)[0x54];
		const auto bytes_fragments = reinterpret_cast<uint32_t*>(data)[0x43];

		if (bytes_fragments == last_fragment)
		{
			auto& buffer = reinterpret_cast<uint32_t*>(data)[0x42];
			buffer = 0;
		}
	}

	if (_ReturnAddress() == read_sub_channel_data_return)
	{
		const auto data = &reinterpret_cast<uint32_t*>(g_ClientState->m_NetChannel)[0x54];
		const auto bytes_fragments = reinterpret_cast<uint32_t*>(data)[0x43];

		last_fragment = bytes_fragments;
	}

	return o_ShowFragments(cvar);
}

void __fastcall Handlers::RunCommand_h(void* ECX, void* EDX, C_BasePlayer* player, CUserCmd* cmd, IMoveHelper* helper)
{
	o_RunCommand(ECX, player, cmd, helper);

	Miscellaneous::Get().PunchAngleFix_RunCommand(player);
}

int __fastcall Handlers::SendDatagram_h(INetChannel *ECX, void *EDX, bf_write *data)
{
	if (data || g_Options.rage_fakelatency < ECX->GetLatency(FLOW_INCOMING) + ECX->GetLatency(FLOW_OUTGOING))
		return o_SendDatagram(ECX, data);

	int instate = ECX->m_nInReliableState;
	int outstate = ECX->m_nOutReliableState;
	int insequencenr = ECX->m_nInSequenceNr;

	CMBacktracking::Get().AddLatency2(ECX, g_Options.rage_fakelatency);

	int ret = o_SendDatagram(ECX, data);

	//ECX->m_nInReliableState = instate;
	//ECX->m_nOutReliableState = outstate;
	//ECX->m_nInSequenceNr = insequencenr;

	return ret;
}

bool __fastcall Handlers::WriteUsercmdDeltaToBuffer_h(IBaseClientDLL *ECX, void *EDX, int nSlot, bf_write *buf, int from, int to, bool isNewCmd)
{
	/*
	//static DWORD WriteUsercmdDeltaToBufferReturn = (DWORD)Utils::PatternScan(GetModuleHandle("engine.dll"), "84 C0 74 04 B0 01 EB 02 32 C0 8B FE 46 3B F3 7E C9 84 C0 0F 84 ? ? ? ?"); //     84 DB 0F 84 ? ? ? ? 8B 8F ? ? ? ? 8B 01 8B 40 1C FF D0

	if (nTickBaseShift <= 0 || (DWORD)_ReturnAddress() != ((DWORD)GetModuleHandleA("engine.dll") + 0xCCCA6))
		return o_WriteUsercmdDeltaToBuffer(ECX, nSlot, buf, from, to, isNewCmd);

	if (from != -1)
		return true;

	// CL_SendMove function
	auto CL_SendMove = []()
	{
		using CL_SendMove_t = void(__fastcall*)(void);
		static CL_SendMove_t CL_SendMoveF = (CL_SendMove_t)Utils::PatternScan(GetModuleHandle("engine.dll"), ("55 8B EC A1 ? ? ? ? 81 EC ? ? ? ? B9 ? ? ? ? 53 8B 98"));

		CL_SendMoveF();
	};

	// WriteUsercmd function
	auto WriteUsercmd = [](bf_write *buf, CUserCmd *in, CUserCmd *out)
	{
		//using WriteUsercmd_t = void(__fastcall*)(bf_write*, CUserCmd*, CUserCmd*);
		static DWORD WriteUsercmdF = (DWORD)Utils::PatternScan(GetModuleHandle("client.dll"), ("55 8B EC 83 E4 F8 51 53 56 8B D9 8B 0D"));

		__asm
		{
			mov     ecx, buf
			mov     edx, in
			push	out
			call    WriteUsercmdF
			add     esp, 4
		}
	};

	/*uintptr_t framePtr;
	__asm mov framePtr, ebp;
	auto msg = reinterpret_cast<CCLCMsg_Move_t*>(framePtr + 0xFCC);*/
	/*
	int *pNumBackupCommands = (int*)(reinterpret_cast<uintptr_t>(buf) - 0x30);
	int *pNumNewCommands = (int*)(reinterpret_cast<uintptr_t>(buf) - 0x2C);
	auto net_channel = g_ClientState->m_NetChannel;

	int32_t new_commands = *pNumNewCommands;

	if (!bInSendMove)
	{
		if (new_commands <= 0)
			return false;

		bInSendMove = true;
		bFirstSendMovePack = true;
		nTickBaseShift += new_commands;

		while (nTickBaseShift > 0)
		{
			CL_SendMove();
			net_channel->Transmit(false);
			bFirstSendMovePack = false;
		}

		bInSendMove = false;
		return false;
	}

	if (!bFirstSendMovePack)
	{
		int32_t loss = min(nTickBaseShift, 10);

		nTickBaseShift -= loss;
		net_channel->m_nOutSequenceNr += loss;
	}

	int32_t next_cmdnr = g_ClientState->lastoutgoingcommand + g_ClientState->chokedcommands + 1;
	int32_t total_new_commands = min(nTickBaseShift, 62);
	nTickBaseShift -= total_new_commands;

	from = -1;
	*pNumNewCommands = total_new_commands;
	*pNumBackupCommands = 0;

	for (to = next_cmdnr - new_commands + 1; to <= next_cmdnr; to++)
	{
		if (!o_WriteUsercmdDeltaToBuffer(ECX, nSlot, buf, from, to, true))
			return false;

		from = to;
	}

	CUserCmd *last_realCmd = g_Input->GetUserCmd(nSlot, from);
	CUserCmd fromCmd;

	if (last_realCmd)
		fromCmd = *last_realCmd;

	CUserCmd toCmd = fromCmd;
	toCmd.command_number++;
	toCmd.tick_count += 200;

	for (int i = new_commands; i <= total_new_commands; i++)
	{
		WriteUsercmd(buf, &toCmd, &fromCmd);
		fromCmd = toCmd;
		toCmd.command_number++;
		toCmd.tick_count++;
	}

	return true;

	LocalPlayer.bSwarm = !LocalPlayer.bSwarm;
	int TICKS_TO_SEND_IN_BATCH = !LocalPlayer.bSwarm ? 1 : (nTickBaseShift + 2);
	CUserCmd *lastcmd = lastcmd;
	BOOL bInAttack;
	BOOL bInAttack2;
	BOOL bInUse;
	BOOL bInReload;

	if (!checks::is_bad_ptr(lastcmd))
	{
		bInAttack = lastcmd->buttons & IN_ATTACK;
		bInAttack2 = lastcmd->buttons & IN_ATTACK2;
		bInUse = lastcmd->buttons & IN_USE;
		bInReload = lastcmd->buttons & IN_RELOAD;
		int bPressingARapidFirableKey = (bInAttack || (bInAttack2 && LocalPlayer.WeaponVars.IsKnife) || bInUse || bInReload);

		static int NonSwarmTickCount = 0;
		if (!LocalPlayer.bSwarm || !LocalPlayer.isastronaut)
			NonSwarmTickCount = g_GlobalVars->tickcount;

		bool backupisastronaut = LocalPlayer.isastronaut;
		bool backupswarmstate = LocalPlayer.bSwarm;
		bool setswarmtrueonexit = false;

		if (LocalPlayer.isastronaut)
		{
			++LocalPlayer.tickssincestartedbeingastronaut;
			LocalPlayer.isastronaut = true;
			DWORD cl = *(DWORD*)g_ClientState;
			int resul = *(int*)(g_ClientState->lastoutgoingcommand);

			CUserCmd *usercmds = GetUserCmdStruct(0);
			CUserCmd backupcmds[150];
			bool bShouldFire = false;
			bool bCanHoldAttack = !LocalPlayer.WeaponVars.IsGun || LocalPlayer.WeaponVars.IsFullAuto || bInUse || (LocalPlayer.WeaponVars.IsRevolver && !bInAttack2);

			if ((bPressingARapidFirableKey && RapidFireWhenAttackingWithNasaChk.Checked) && LocalPlayer.CurrentWeapon)
			{
				WeapInfo_t *weaponinfo = g_LocalPlayer->m_hActiveWeapon()->GetWeapInfo();
				if (weaponinfo)
				{
					lastcmd->buttons &= ~IN_ATTACK;
					lastcmd->buttons &= ~IN_ATTACK2;
					lastcmd->buttons &= ~IN_USE;
					lastcmd->buttons &= ~IN_RELOAD;

					if (LocalPlayer.tickssincestartedbeingastronaut > 2)
					{
						bool &bSwarm = LocalPlayer.bSwarm;

						if (!LocalPlayer.finishedrapidfire)
						{
							if (!LocalPlayer.started)
							{
								int servertime = NonSwarmTickCount;
								if (LocalPlayer.bSwarm)
									servertime -= nTickBaseShift;

								if (bSwarm && (LocalPlayer.restart || gTriggerbot.WeaponCanFire(bInAttack2)))
								{
									bShouldFire = true;
									LocalPlayer.started = true;
									LocalPlayer.restart = false;

									float flCycle = (bInAttack2 ? weaponinfo->flCycleTimeAlt : weaponinfo->flCycleTime);

									LocalPlayer.lastshottime_server = servertime;
									LocalPlayer.lastshotwasswarm = LocalPlayer.bSwarm;
									LocalPlayer.lastshottime = g_GlobalVars->tickcount;
									float fllastshottime_server = TICKS_TO_TIME(servertime);

									float timestamp;
									//float delta = fllastshottime_server - LocalPlayer.CurrentWeapon->GetNextPrimaryAttack();
									//if (delta < 0.0f || delta > Interfaces::Globals->interval_per_tick)
									timestamp = fllastshottime_server;
									//else
									//	timestamp = LocalPlayer.CurrentWeapon->GetNextPrimaryAttack();

									//LocalPlayer.nextshottime = timestamp + flCycle + TICKS_TO_TIME(RapidFireDelayTxt.iValue);


								}
							}
							else
							{
								bool stayrunning = bInUse || bInReload || LocalPlayer.WeaponVars.IsC4 || LocalPlayer.WeaponVars.IsRevolver;

								if (stayrunning)
								{
									bShouldFire = true;
									LocalPlayer.bSwarm = false;

									//Force no swarm
									TICKS_TO_SEND_IN_BATCH = 1;
									NonSwarmTickCount = g_GlobalVars->tickcount;
									bSwarm = false;
									setswarmtrueonexit = true;
								}
								else
								{
									int servertime = 1 + LocalPlayer.lastshottime_server + (g_GlobalVars->tickcount - LocalPlayer.lastshottime);
									if (LocalPlayer.lastshotwasswarm /*&& RapidFireMode2Chk.Checked*//*)
										servertime += nTickBaseShift;

									//if (bSwarm)
									//	servertime -= TICKBASE_SHIFT;

									float flservertime = TICKS_TO_TIME(servertime);

									//bool bCanHoldAttack =  /*Interfaces::Globals->tickcount - lastshottime > 1*//*);

									if (flservertime >= LocalPlayer.nextshottime && (bCanHoldAttack || (g_GlobalVars->tickcount - LocalPlayer.lastshottime) > 1))
									{
										bShouldFire = true;
										LocalPlayer.finishedrapidfire = true;

										//Force no swarm
										TICKS_TO_SEND_IN_BATCH = 1;
										NonSwarmTickCount = g_GlobalVars->tickcount;
										bSwarm = false;
										setswarmtrueonexit = true;
										LocalPlayer.lastshotwasswarm = false;

										float flCycle = (bInAttack2 ? weaponinfo->flCycleTimeAlt : weaponinfo->flCycleTime);
										servertime = NonSwarmTickCount;
										LocalPlayer.lastshottime_server = servertime;
										LocalPlayer.lastshottime = g_GlobalVars->tickcount;
										LocalPlayer.nextshottime = TICKS_TO_TIME(servertime + nTickBaseShift + 2) + flCycle; //Add 2 ticks in case we were not swarming for a long time
									}
									else
									{
										//Can't shoot yet, let tickbase raise asap
										TICKS_TO_SEND_IN_BATCH = 1;
										NonSwarmTickCount = g_GlobalVars->tickcount;
										bSwarm = false;
										setswarmtrueonexit = true;
										if (bCanHoldAttack)
											bShouldFire = true;
									}
								}
							}
						}
						else
						{
							//Force no swarm
							TICKS_TO_SEND_IN_BATCH = 1;
							NonSwarmTickCount = g_GlobalVars->tickcount;
							bSwarm = false;
							setswarmtrueonexit = true;

							int servertime = 1 + LocalPlayer.lastshottime_server + (g_GlobalVars->tickcount - LocalPlayer.lastshottime);
							if (LocalPlayer.lastshotwasswarm /*&& RapidFireMode2Chk.Checked*//*)
								servertime += nTickBaseShift;

							if (TICKS_TO_TIME(servertime) >= LocalPlayer.nextshottime)// && (bCanHoldAttack || (Interfaces::Globals->tickcount - LocalPlayer.lastshottime) > 1))
							{
								LocalPlayer.finishedrapidfire = false;
								LocalPlayer.started = false;
								LocalPlayer.restart = true;
							}
						}
					}
				}
			}
			else
			{
				LocalPlayer.finishedrapidfire = false;
				LocalPlayer.started = false;
				LocalPlayer.restart = false;
			}

			LocalPlayer.bBlockWriteUserCmdDeltaToBuffer = false;

			if (LocalPlayer.isastronaut || (bPressingARapidFirableKey && LocalPlayer.started))
			{
				int lastcommand = *(int*)(g_ClientState->lastoutgoingcommand);
				int chokedcount = *(int*)(g_ClientState->chokedcommands);

				//if (chokedcount > 0)
				//	printf("WARNING: %i CHOKED TICKS!\n", chokedcount);

				int LAST_PROCESSABLE_TICK_INDEX = max(0, min(TICKS_TO_SEND_IN_BATCH, 16) - 2); //sv_maxusrcmdprocessticks

				for (int i = 0; i < TICKS_TO_SEND_IN_BATCH; i++)
				{
					bool bIsLastProcessedTick = i == LAST_PROCESSABLE_TICK_INDEX;
					int nextcommandnr = lastcommand + chokedcount + 1;
					CUserCmd *cmd = GetUserCmd(0, nextcommandnr, true);
					if (cmd)
					{
						if (!lastcmd)
							cmd->Reset()
						else
							*cmd = *lastcmd;

						if (bShouldFire && (bIsLastProcessedTick || bCanHoldAttack))
						{
							if (bInAttack)
								cmd->buttons |= IN_ATTACK;
							if (bInAttack2)
								cmd->buttons |= IN_ATTACK2;
							if (bInReload)
								cmd->buttons |= IN_RELOAD;
							if (bInUse)
								cmd->buttons |= IN_USE;
						}

						cmd->command_number = nextcommandnr++;
						cmd->tick_count = *(int*)(g_ClientState->m_nServerCount) + TIME_TO_TICKS(0.5f) + i;

						if (TICKS_TO_SEND_IN_BATCH > 1 && i != (TICKS_TO_SEND_IN_BATCH - 1))
							chokedcount++;

						if (LocalPlayer.bSwarm && i == 0)
						{
							memcpy(backupcmds, usercmds, sizeof(CUserCmd) * 150);
						}
					}
				}

				static void* tmp = malloc(2048);
				CNetMsg_Tick_Construct(tmp, *host_computationtime, *host_computationtime_std_deviation, *(DWORD*)(cl + 0x174), *host_framestarttime_std_deviation);
				Global::netchan->SendNetMsg(tmp, false, false);

				//if (LocalPlayer.bSwarm)
				//	chan->m_nOutSequenceNr += 10;

				if (!LocalPlayer.bSwarm)
				{
					//	int packet_drop_amount = 10; //actual limit is 23 but server clamps to 10
					//	packet_drop_amount += (TICKBASE_SHIFT + 1);
					//	chan->m_nOutSequenceNr += packet_drop_amount;
				}

				Global::netchan->m_nChokedPackets = g_ClientState->chokedcommands;
				CL_SendMove_Rebuilt();

				resul = oSendDatagram(netchan, datagram);
				resul = o_SendDatagram(netchan, datagram);
				*(int*)(g_ClientState->lastoutgoingcommand) = resul;
				*(int*)(g_ClientState->chokedcommands) = 0;

				if (LocalPlayer.bSwarm)
					memcpy(usercmds, backupcmds, sizeof(CUserCmd) * 150);

				//static int lastsent = TICKS_TO_SEND_IN_BATCH;
				//if (TICKS_TO_SEND_IN_BATCH == lastsent && !setswarmtrueonexit)
				//	printf("WARNING: SENT SAME BATCH TWICE: %i\n", TICKS_TO_SEND_IN_BATCH);
				//lastsent = TICKS_TO_SEND_IN_BATCH;

				LocalPlayer.bSwarm = setswarmtrueonexit ? true : backupswarmstate;

				return resul;
			}

			LocalPlayer.isastronaut = backupisastronaut;
		}
		else
		{
			LocalPlayer.finishedrapidfire = false;
			LocalPlayer.started = false;
			LocalPlayer.restart = false;
			LocalPlayer.tickssincestartedbeingastronaut = 0;
		}
	}
	*/
}

int __stdcall Handlers::IsBoxVisible_h(const Vector &mins, const Vector &maxs)
{
	if (!memcmp(_ReturnAddress(), "\x85\xC0\x74\x2D\x83\x7D\x10\x00\x75\x1C", 10))
		return 1;

	return o_IsBoxVisible(mins, maxs);
}

bool __fastcall Handlers::IsHLTV_h(void *ECX, void *EDX)
{
	uintptr_t player;
	__asm
	{
		mov player, edi
	}

	if ((DWORD)_ReturnAddress() != Offsets::reevauluate_anim_lod)
		return o_IsHLTV(ECX);

	if (!player || player == 0x000FFFF)
		return o_IsHLTV(ECX);

	/*if (player % 10 == 0x4)
		player -= 4;*/

	*(int32_t*)(player + 0xA24) = -1;
	*(int32_t*)(player + 0xA2C) = *(int32_t*)(player + 0xA28);
	*(int32_t*)(player + 0xA28) = 0;

	return true;
}

void __stdcall Handlers::LockCursor_h() //for panorama fix
{
	if (Global::bMenuOpen)
	{
		g_VGuiSurface->UnlockCursor();
		return;
	}
	o_LockCursor(g_VGuiSurface);
}

bool __fastcall Handlers::DispatchUserMessage_h(void *thisptr, void*, int msg_type, int unk1, int nBytes, bf_read &msg_data)
{
	if (g_Options.visuals_devinfo)
	{
		if (msg_type == static_cast<int>(usermsg_type::CS_UM_VoteStart))
		{
			bf_read read = bf_read(msg_data);
			read.SetOffset(2);
			auto ent_index = read.ReadByte();
			auto vote_type = read.ReadByte();

			g_CVar->ConsoleColorPrintf(Color(0, 153, 204, 255), "CS_UM_VoteStart: index: %d, type: %d", ent_index, vote_type);
		}
	}

	if (g_Options.misc_antikick && msg_type == static_cast<int>(usermsg_type::CS_UM_VoteStart))
	{
		g_EngineClient->ExecuteClientCmd("callvote swapteams");
	}
	return o_DispatchUserMessage(thisptr, msg_type, unk1, nBytes, msg_data);
}