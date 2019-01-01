#include "PlayerHurt.hpp"

#include "../Structs.hpp"
#include "Visuals.hpp"
#include "Resolver.hpp"
#include "AimRage.hpp"
#include "LagCompensation.hpp"

#include "../Options.hpp"

#include <chrono>

std::vector<std::string> tt_tap =	//damm you, linker error
{
	"You just got tapped by an garbage self pasta meme, go get an refund",
	"self pasta meme > all",
	"Your p2c sucks ass",
	"Really? Getting tapped by paste meme? Damn that's horrable cheat",
	"learncpp.com owns me and all",
	"1",
	"1 tap by learncpp.com",
	"1 tap by unknowncheats.me",
	"1 tap by mpgh.net",
	"That's 1 tap on my book.",
	"I bet even ayyware can tap you",
	"I bet even texashook can tap you",
	"I bet even ezfrags can tap you",
	"forgot to toggle aa buddy?",
	"[dankme.mz] Hit gay fag in the head for 100 dmg (used No Fake resolver)"
};

std::vector<std::string> tt_tap_legit =
{
	"1 tap by learncpp.com",
	"1 tap by unknowncheats.me",
	"1 tap by mpgh.net",
	"You can't cheat on VAC secured server!",
	"You can't cheat on SMAC secured server!",
	"Don't worry, i'll get banned for sure!",
	"CSGO is a dead game why tf do people still play this shit",
	"admin? are you here? if you are, type ''!ban @all 0''",
	"Valve Anti Cheat? More like Valve Allow Cheat",
	"SourceMod Anti Cheat? More like SourceMod Allow Cheat",
	"VACnet is a joke. it doesn't do anything"
};

std::vector<std::string> tt_kill_legit =
{
	"Don't be noob! learncpp.com!",
	"www.AYYWARE.net | Premium CSGO MEME Cheat",
	"Stop being noob! Get unknowncheats.me!",
	"You can't cheat on VAC secured server!",
	"You can't cheat on SMAC secured server!",
	"Don't worry, i'll get banned for sure!",
	"CSGO is a dead game why tf do people still play this shit",
	"admin? are you here? if you are, type ''!ban @all 0''",
	"Valve Anti Cheat? More like Valve Allow Cheat",
	"SourceMod Anti Cheat? More like SourceMod Allow Cheat",
	"VACnet is a joke. it doesn't do anything"
};

std::vector<std::string> tt_baim =
{
	"baim > all",
	"baim is gay, except when i'm the one doing it",
	"hiding head against paste? you afraid you're gonna get tapped? by paste?",
	"wanna learn what makes p100 resolver? ''Global::Should_Baim[playerindex] = true;''",
};

std::vector<std::string> tt_tapped =
{
	"Fucking lag",
	"Nice 1 way",
	"ESP Broke",
	"LBY Backtrack is just the best",
	"I'm getting like 50% loss wtf is wrong with this server",
	"Nice serverside",
	"I was typing stop",
	"pure luck"
};

std::vector<std::string> tt_hittedhead =
{
	"I just FUCKING HATE LBY backtrack",
	"ahh my lby breaker broke from high ping",
	"I bet that was bruteforce and you were just lucky.",
	"B1G BACKTRACK",
	"nice, my esp is doing it again."
};

std::vector<std::string> tt_baimed =
{
	"you remember when i said ''baim is gay, except when i'm the one doing it''? yep. you're gay.",
	"baim on pasted antiaim? you can't resolve this?",
	"Hey, look! p2c is baiming paste cheat! it can't resolve pasted antiaim!",
	"ahh these p2c with thier p100 resolver.. you can't miss when you baim!",
	"why are you baiming? can't resolve pasted antiaim? go get an refund"
};

auto HitgroupToString = [](int hitgroup) -> std::string
{
	switch (hitgroup)
	{
	case HITGROUP_GENERIC:
		return "generic";
	case HITGROUP_HEAD:
		return "head";
	case HITGROUP_CHEST:
		return "chest";
	case HITGROUP_STOMACH:
		return "stomach";
	case HITGROUP_LEFTARM:
		return "left arm";
	case HITGROUP_RIGHTARM:
		return "right arm";
	case HITGROUP_LEFTLEG:
		return "left leg";
	case HITGROUP_RIGHTLEG:
		return "right leg";
	case 8:
		return "unknown";
	case HITGROUP_GEAR:
		return "gear";
	}
};

auto HitboxToHitGroup = [](int hitbox) -> int
{
	switch (hitbox)
	{
	case HITBOX_HEAD:
		return HITGROUP_HEAD;
	case HITBOX_CHEST:
	case HITBOX_LOWER_CHEST:
	case HITBOX_UPPER_CHEST:
		return HITGROUP_CHEST;
	case HITBOX_STOMACH:
	case HITBOX_PELVIS:
		return HITGROUP_STOMACH;
	case HITBOX_LEFT_FOOT:
	case HITBOX_LEFT_THIGH:
	case HITBOX_LEFT_CALF:
		return HITGROUP_LEFTLEG;
	case HITBOX_RIGHT_FOOT:
	case HITBOX_RIGHT_THIGH:
	case HITBOX_RIGHT_CALF:
		return HITGROUP_RIGHTLEG;
	case HITBOX_LEFT_HAND:
	case HITBOX_LEFT_FOREARM:
	case HITBOX_LEFT_UPPER_ARM:
		return HITGROUP_LEFTARM;
	case HITBOX_RIGHT_HAND:
	case HITBOX_RIGHT_FOREARM:
	case HITBOX_RIGHT_UPPER_ARM:
		return HITGROUP_RIGHTARM;
	default:
		return HITGROUP_GENERIC;
	}
};

void PlayerHurtEvent::PushEvent(EventInfo event)
{
	eventInfo.emplace_back(event);
}

void PlayerHurtEvent::FireGameEvent(IGameEvent *event)
{
	static LagRecord last_LR[65];

	if (!g_LocalPlayer || !event)
		return;

	if (!strcmp(event->GetName(), "player_hurt"))
	{
		if (g_Options.visuals_others_hitmarker)
		{
			if (g_EngineClient->GetPlayerForUserID(event->GetInt("attacker")) == g_EngineClient->GetLocalPlayer() &&
				g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) != g_EngineClient->GetLocalPlayer() &&
				g_LocalPlayer->IsAlive())
			{
				bool done = false;
				for (auto i = hitMarkerInfo.rbegin(); i != hitMarkerInfo.rend(); i++)
				{
					if (i->m_iIndex == g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) && i->m_iDmg == 0)
					{
						i->m_iDmg = event->GetInt("dmg_health");
						i->m_iHitbox = event->GetInt("hitgroup");
						done = true;
						break;
					}
				}
				if (!done)
				{
					std::string mode = ((last_LR[g_EngineClient->GetPlayerForUserID(event->GetInt("userid"))] == CMBacktracking::Get().current_record[g_EngineClient->GetPlayerForUserID(event->GetInt("userid"))]) ? (Global::resolverModes[g_EngineClient->GetPlayerForUserID(event->GetInt("userid"))]) : (CMBacktracking::Get().current_record[g_EngineClient->GetPlayerForUserID(event->GetInt("userid"))].m_strResolveMode));
					last_LR[g_EngineClient->GetPlayerForUserID(event->GetInt("userid"))] = CMBacktracking::Get().current_record[g_EngineClient->GetPlayerForUserID(event->GetInt("userid"))];
					hitMarkerInfo.push_back({ g_GlobalVars->curtime, g_LocalPlayer->m_hActiveWeapon().Get()->m_flNextPrimaryAttack(), g_GlobalVars->curtime + 5.0f, event->GetInt("dmg_health"), g_EngineClient->GetPlayerForUserID(event->GetInt("userid")), event->GetInt("hitgroup"), mode });
				}

				if (event->GetInt("hitgroup") == 1)
				{
					g_EngineClient->ExecuteClientCmd((event->GetInt("health")) ? ("play buttons\\arena_switch_press_02.wav") : ("play player\\headshot1.wav"));
				}
				else
				{
					g_EngineClient->ExecuteClientCmd((event->GetInt("health")) ? ("play buttons\\arena_switch_press_02.wav") : ("play ui\\deathnotice.wav"));
				}
			}
		}

		if (g_Options.esp_lagcompensated_hitboxes)
		{
			int32_t attacker = g_EngineClient->GetPlayerForUserID(event->GetInt("attacker"));
			int32_t userid = g_EngineClient->GetPlayerForUserID(event->GetInt("userid"));

			if (attacker == g_EngineClient->GetLocalPlayer() && userid != g_EngineClient->GetLocalPlayer())
				Visuals::DrawCapsuleOverlay(userid, 0.8f);
		}

		if (g_Options.misc_logevents || g_Options.hvh_resolver || g_Options.misc_trashchat)
		{
			EventInfo info;
			std::string msg;
			std::stringstream msg2;

			auto enemy = event->GetInt("userid");
			auto attacker = event->GetInt("attacker");
			auto remaining_health = event->GetString("health");
			auto dmg_to_health = event->GetString("dmg_health");
			auto hitgroup = event->GetInt("hitgroup");

			auto enemy_index = g_EngineClient->GetPlayerForUserID(enemy);
			auto attacker_index = g_EngineClient->GetPlayerForUserID(attacker);
			auto pEnemy = C_BasePlayer::GetPlayerByIndex(enemy_index);
			auto pAttacker = C_BasePlayer::GetPlayerByIndex(attacker_index);

			player_info_t attacker_info;
			player_info_t enemy_info;

			std::string str;

			if (pEnemy && pAttacker && g_EngineClient->GetPlayerInfo(attacker_index, &attacker_info) && g_EngineClient->GetPlayerInfo(enemy_index, &enemy_info))
			{
				static long timestamp = 0;
				long curTime;
				if (Global::use_ud_vmt)
					curTime = g_GlobalVars->realtime * 1000;	//idk why we were using chrono in the first place but i'm sticking with that
				else
					curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

				if (attacker_index == g_EngineClient->GetLocalPlayer())
				{
					std::string mode = ((last_LR[enemy_index] == CMBacktracking::Get().current_record[enemy_index]) ? (Global::resolverModes[enemy_index]) : (CMBacktracking::Get().current_record[enemy_index].m_strResolveMode));
					if (g_Options.misc_logevents)
					{
						info.m_flExpTime = g_GlobalVars->curtime + 4.f;
						std::string szHitgroup = HitgroupToString(hitgroup);
						msg2 << "Hit " << enemy_info.szName << " in the " << szHitgroup << " for " << dmg_to_health << " dmg " << "(" << remaining_health << " health remaining";

						if (g_Options.hvh_resolver)
						{
							msg2 << ", using " << mode << ".)";
						}
						else
						{
							msg2 << ")";
						}
						info.m_szMessage = msg2.str();
						eventInfo.emplace_back(info);

						g_CVar->ConsoleColorPrintf(Color(50, 122, 239), "[dankme.mz]");
						g_CVar->ConsoleDPrintf(" ""Hit"" ");
						g_CVar->ConsoleColorPrintf(Color(255, 255, 255), "%s", enemy_info.szName);
						g_CVar->ConsoleColorPrintf(Color(255, 255, 255), " in the %s", szHitgroup.c_str());
						g_CVar->ConsoleDPrintf(" ""for"" ");
						g_CVar->ConsoleColorPrintf(Color(255, 255, 255), "%s dmg", dmg_to_health);
						g_CVar->ConsoleColorPrintf(Color(255, 255, 255), " (%s health remaining", remaining_health);
						if (g_Options.hvh_resolver)
							g_CVar->ConsoleColorPrintf(Color(255, 255, 255), ", using %s.)\n", mode.c_str());
						else
							g_CVar->ConsoleColorPrintf(Color(255, 255, 255), ")\n");
					}

					if (g_Options.rage_enabled && g_Options.hvh_resolver)
					{
						//if (event->GetInt("health") < 1)
						//	Resolver::Get().missed_shots[enemy_index] = 0;
						//else
						//	Resolver::Get().missed_shots[enemy_index]--;

						if (hitgroup == 1 && event->GetInt("health") == 0 && !(mode == "LBY Update" || mode == "No Fakes" || mode == "Shot" || Global::resolverModes[enemy_index].find("Moving") != std::string::npos) && !Resolver::Get().records[enemy_index].empty())
						{
							auto record = Resolver::Get().records[enemy_index].front(); bool should_record = true;
							for (auto i = Resolver::Get().angle_records.begin(); i != Resolver::Get().angle_records.end(); i++)
							{
								if (i->handle == &pEnemy->GetRefEHandle() && i->position.DistTo((last_LR[enemy_index] == CMBacktracking::Get().current_record[enemy_index]) ? record.origin : CMBacktracking::Get().current_record[enemy_index].m_vecOrigin) < 128.f)
								{
									should_record = false;
									break;
								}
							}

							if (should_record)
							{
								angle_recording temp; temp.SaveRecord(pEnemy, (last_LR[enemy_index] == CMBacktracking::Get().current_record[enemy_index]) ? record.origin : CMBacktracking::Get().current_record[enemy_index].m_vecOrigin, record.resolvedang);
								Resolver::Get().angle_records.push_front(temp);
							}
						}
					}

					if (g_Options.hvh_resolver && hitgroup == 1)
					{
						Global::hit_while_brute[enemy_index] = true;
					}

					if (g_Options.misc_trashchat == 1)
					{
						if ((curTime - timestamp) > 850)
						{
							if (hitgroup == 1 && (event->GetInt("dmg_health") >= 100 || event->GetInt("health") == 0))
							{
								msg = tt_tap[Utils::RandomInt(0, tt_tap.size() - 1)];
							}
							else if (hitgroup == 1)
							{
								msg.append(dmg_to_health);
								if (event->GetInt("dmg_health") < 75)
									msg.append(" damage on head? that's bullshit");
								else
									msg.append(" damage on head.. that was close");
							}
							else if (hitgroup > 3 && hitgroup < 8)
							{
								if (hitgroup < 6)
									msg = "dankme.mz repasted - destroying your hand since 2018";
								else
									msg = "dankme.mz repasted - destroying your toe since 2018";
							}
							else if (hitgroup == 0)
							{
								if (AimRage::Get().local_weapon->IsWeaponNonAim())
									msg = "pasted knifebot!";
								else
									msg = "dankme.mz repasted - now with p nade prediction!";
							}
							else
							{
								msg = tt_baim[Utils::RandomInt(0, tt_baim.size() - 1)];
							}
							str.append("say ");
							str.append(msg);
							g_EngineClient->ExecuteClientCmd(str.c_str());

							timestamp = curTime;
						}
					}
					last_LR[enemy_index] = CMBacktracking::Get().current_record[enemy_index];
				}
				else if (enemy_index == g_EngineClient->GetLocalPlayer())
				{
					if (g_Options.misc_trashchat == 1)
					{
						if ((curTime - timestamp) > 850)
						{
							auto awall1 = AimRage::Get().GetDamageVec2(pEnemy->GetEyePos(), g_LocalPlayer->GetEyePos(), g_LocalPlayer, pEnemy, HITBOX_HEAD);
							auto awall2 = AimRage::Get().GetDamageVec2(g_LocalPlayer->GetEyePos(), pEnemy->GetEyePos(), g_LocalPlayer, pEnemy, HITBOX_HEAD);
							if (awall1.damage - awall2.damage > 25 || (awall1.damage > 10 && awall2.damage < 5))
							{
								msg = "[dankme.mz] ";
								msg += pEnemy->GetName();
								msg += " is hiding in 1 way spot like a fucking coward. (";
								msg += awall1.damage;
								msg += "maximum damage possable on he(she)'s spot, ";
								msg += awall2.damage;
								msg += "on my side.)";
							}
							else if (pEnemy->IsDormant())
							{
								msg = "[dankme.mz] ";
								msg += pEnemy->GetName();
								msg += " is hiding in 1 way spot like a fucking coward.";
							}
							else if (event->GetInt("dmg_health") >= 100)
							{
								msg = tt_tapped[Utils::RandomInt(0, tt_tapped.size() - 1)];
							}
							else if (hitgroup == 1)
							{
								msg = tt_hittedhead[Utils::RandomInt(0, tt_hittedhead.size() - 1)];
							}
							else if (hitgroup == 0)
							{
								msg = "ohh, nade? you're too scared to peek at paste cheat?";
							}
							else
							{
								msg = tt_baimed[Utils::RandomInt(0, tt_baimed.size() - 1)];
							}
							str.append("say ");
							str.append(msg);
							g_EngineClient->ExecuteClientCmd(str.c_str());

							timestamp = curTime;
						}
					}
				}
			}
		}
		else
			if (eventInfo.size() > 0)
				eventInfo.clear();

		if (g_Options.hvh_resolver)
		{
			int32_t userid = event->GetInt("userid");
			auto player = C_BasePlayer::GetPlayerByUserId(userid);
			if (!player)
				return;

			int32_t idx = player->EntIndex();
			auto &player_recs = Resolver::Get().arr_infos[idx];

			if (!player->IsDormant())
			{
				int32_t local_id = g_EngineClient->GetLocalPlayer();
				int32_t attacker = g_EngineClient->GetPlayerForUserID(event->GetInt("attacker"));

				if (attacker == local_id)
				{
					bool airJump = !(player->m_fFlags() & FL_ONGROUND) && player->m_vecVelocity().Length2D() > 1.0f;
					int32_t tickcount = g_GlobalVars->tickcount;

					if (tickHitWall == tickcount)
					{
						player_recs.m_nShotsMissed = originalShotsMissed;
						player_recs.m_nCorrectedFakewalkIdx = originalCorrectedFakewalkIdx;
					}
					if (tickcount != tickHitPlayer)
					{
						tickHitPlayer = tickcount;
						player_recs.m_nShotsMissed = 0;

						if (!airJump)
						{
							if (++player_recs.m_nCorrectedFakewalkIdx > 7)
								player_recs.m_nCorrectedFakewalkIdx = 0;
						}
					}
				}
			}
		}
	}

	if (!strcmp(event->GetName(), "weapon_fire"))
	{
		if (g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) == g_EngineClient->GetLocalPlayer())
		{
			//if (g_Options.rage_enabled && g_Options.hvh_resolver && !AimRage::Get().local_weapon->IsMiscellaneousWeapon() && AimRage::Get().CheckTarget(Global::aimbot_target)) Resolver::Get().missed_shots[Global::aimbot_target]++;
			if (g_Options.visuals_others_hitmarker && g_LocalPlayer->IsAlive() && AimRage::Get().CheckTarget(Global::aimbot_target))
			{
				std::string mode = ((last_LR[Global::aimbot_target] == CMBacktracking::Get().current_record[Global::aimbot_target]) ? (Global::resolverModes[Global::aimbot_target]) : (CMBacktracking::Get().current_record[Global::aimbot_target].m_strResolveMode));
				last_LR[Global::aimbot_target] = CMBacktracking::Get().current_record[Global::aimbot_target];
				hitMarkerInfo.push_back({ g_GlobalVars->curtime, g_LocalPlayer->m_hActiveWeapon().Get()->m_flNextPrimaryAttack(), g_GlobalVars->curtime + 5.0f, 0, Global::aimbot_target, HitboxToHitGroup(Global::aim_hitbox), mode });
			}
		}
		else if (g_Options.misc_trashchat == 1)
		{
			std::string weapon = event->GetString("weapon");
			if (weapon.find("awp") != std::string::npos)
			{
				auto pEnemy = C_BasePlayer::GetPlayerByIndex(g_EngineClient->GetPlayerForUserID(event->GetInt("userid")));
				std::string str = "say ";
				str += pEnemy->GetName();
				str += " is gay enough to use ";
				str += weapon;
				str += " on hvh server.";
				g_EngineClient->ExecuteClientCmd(str.c_str());
			}
		}
	}

	if (!strcmp(event->GetName(), "player_death") && g_Options.misc_trashchat == 2)
	{
		static float timestamp = 0;
		int local = g_EngineClient->GetLocalPlayer();
		int target = g_EngineClient->GetPlayerForUserID(event->GetInt("userid"));
		int killer = g_EngineClient->GetPlayerForUserID(event->GetInt("attacker"));
		bool hs = event->GetBool("headshot");
		bool pen = event->GetInt("penetrated") > 2;
		if (local == target || local == killer)
		{
			if (g_GlobalVars->curtime - timestamp > 0.85)
			{
				timestamp = g_GlobalVars->curtime;
				std::string str = "say ";
				if (local == killer)
				{
					str += (hs) ? (tt_tap_legit[Utils::RandomInt(0, tt_tap_legit.size() - 1)]) : (tt_kill_legit[Utils::RandomInt(0, tt_kill_legit.size() - 1)]);
				}
				else
				{
					auto pEnemy = C_BasePlayer::GetPlayerByIndex(killer);

					if (hs)
					{
						str += "Nice resolver, ";
						str += pEnemy->GetName();
						str += ". wanna hvh?";
					}
					else if (pen)
					{
						str += "Nice autowall, ";
						str += pEnemy->GetName();
						str += ". wanna hvh?";
					}
					else
					{
						str += "You just got lucky, ";
						str += pEnemy->GetName();
						str += ".";
					}
				}
				g_EngineClient->ExecuteClientCmd(str.c_str());
			}
		}
	}
	if (!strcmp(event->GetName(), "grenade_thrown") && g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) != g_EngineClient->GetLocalPlayer() && g_Options.misc_trashchat == 1)
	{
		std::string weapon = event->GetString("weapon");
		if (weapon.find("smoke") == std::string::npos && weapon.find("flash") == std::string::npos && weapon.find("decoy") == std::string::npos)
		{
			auto pEnemy = C_BasePlayer::GetPlayerByIndex(g_EngineClient->GetPlayerForUserID(event->GetInt("userid")));
			std::string str = "say ";
			str += pEnemy->GetName();
			str += " is gay enough to use ";
			str += weapon;
			str += " on hvh server.";
			g_EngineClient->ExecuteClientCmd(str.c_str());
		}
	}
}

int PlayerHurtEvent::GetEventDebugID(void)
{
	return EVENT_DEBUG_ID_INIT;
}

void PlayerHurtEvent::RegisterSelf()
{
	g_GameEvents->AddListener(this, "player_hurt", false);
	g_GameEvents->AddListener(this, "player_death", false);
	g_GameEvents->AddListener(this, "grenade_thrown", false);
	g_GameEvents->AddListener(this, "weapon_fire", false);
	g_GameEvents->AddListener(this, "bullet_impact", false);
}

void PlayerHurtEvent::UnregisterSelf()
{
	g_GameEvents->RemoveListener(this);
}

void PlayerHurtEvent::Paint(void)
{
	static int width = 0, height = 0;
	if (width == 0 || height == 0)
		g_EngineClient->GetScreenSize(width, height);

	RECT scrn = Visuals::GetViewport();

	if (eventInfo.size() > 15)
		eventInfo.erase(eventInfo.begin() + 1);

	float alpha = 0.f;

	if (g_Options.visuals_others_hitmarker)
	{
		for (size_t i = 0; i < hitMarkerInfo.size(); i++)
		{
			float diff = hitMarkerInfo.at(i).m_flExpTime - g_GlobalVars->curtime;

			if (diff < 0.f || diff > 6.0f)
			{
				hitMarkerInfo.erase(hitMarkerInfo.begin() + i);
				continue;
			}
		}

		const float h = 14;
		Vector2D Pos = { (float)width / 2 + 18, (float)height / 2 + 18  - (h * hitMarkerInfo.size())};

		Vector2D curpos = Pos;
		for (size_t i = 0; i < hitMarkerInfo.size(); i++)
		{
			float diff = hitMarkerInfo.at(i).m_flExpTime - g_GlobalVars->curtime;
			float height_offset = (diff < 1.0f) ? (pow(((1.0f - diff) * 100), 1.2) * -1) : (0);
			alpha = min((hitMarkerInfo.at(i).m_flExpTime - g_GlobalVars->curtime) / 1.0f, 0.3f);

			int w1, w2, w3, h1, h2, h3; Visuals::GetTextSize(Visuals::ui_font, std::to_string(hitMarkerInfo.at(i).m_iDmg).c_str(), w1, h1); Visuals::GetTextSize(Visuals::ui_font, hitMarkerInfo.at(i).m_szMethod.c_str(), w2, h2); Visuals::GetTextSize(Visuals::ui_font, HitgroupToString(hitMarkerInfo.at(i).m_iHitbox).c_str(), w3, h3);
			if (g_Options.hvh_resolver) w1 += 3;
			else w2 = h2 = 0;

			g_VGuiSurface->DrawSetColor(Color(255, 255, 255, (int)(alpha * 255.f))); g_VGuiSurface->DrawFilledRect(curpos.x, curpos.y - (h1 / 2) + height_offset, curpos.x + 10 + w1 + w2 + w3, curpos.y + 4 + (h1 / 2) + height_offset);
			curpos.y += h;
		}
		curpos = Pos;
		for (size_t i = 0; i < hitMarkerInfo.size(); i++)
		{
			float diff = hitMarkerInfo.at(i).m_flExpTime - g_GlobalVars->curtime;
			float height_offset = (diff < 1.0f) ? (pow(((1.0f - diff) * 100), 1.2) * -1) : (0);
			alpha = min((hitMarkerInfo.at(i).m_flExpTime - g_GlobalVars->curtime) / 1.0f, 1.0f);
			int w1, w2, w3, h1, h2, h3; Visuals::GetTextSize(Visuals::ui_font, std::to_string(hitMarkerInfo.at(i).m_iDmg).c_str(), w1, h1); Visuals::GetTextSize(Visuals::ui_font, hitMarkerInfo.at(i).m_szMethod.c_str(), w2, h2); Visuals::GetTextSize(Visuals::ui_font, HitgroupToString(hitMarkerInfo.at(i).m_iHitbox).c_str(), w3, h3);
			if (g_Options.hvh_resolver) w1 += 3;
			else w2 = h2 = 0;

			Visuals::DrawString(Visuals::ui_font, curpos.x + 2, curpos.y + 2 + height_offset, Color(255, 255, 0, (int)(alpha * 255.f)), FONT_LEFT, HitgroupToString(hitMarkerInfo.at(i).m_iHitbox).c_str());
			Visuals::DrawString(Visuals::ui_font, curpos.x + 5 + w3, curpos.y + 2 + height_offset, Color(255, 255, 0, (int)(alpha * 255.f)), FONT_LEFT, std::to_string(hitMarkerInfo.at(i).m_iDmg).c_str());
			if (g_Options.hvh_resolver) Visuals::DrawString(Visuals::ui_font, curpos.x + 5 + w1 + w3, curpos.y + 2 + height_offset, Color(255, 255, 0, (int)(alpha * 255.f)), FONT_LEFT, hitMarkerInfo.at(i).m_szMethod.c_str());
			curpos.y += h;
		}

		if (hitMarkerInfo.size() > 0)
		{
			int lineSize = 12;
			alpha = 0;
			for (auto i = hitMarkerInfo.rbegin(); i != hitMarkerInfo.rend(); i++)
			{
				if(i->m_iDmg > 0)
					alpha = min(((i->m_flCurTime + 0.8f) - g_GlobalVars->curtime) / 0.8f, 0);
			}
			g_VGuiSurface->DrawSetColor(Color(255, 255, 255, (int)(alpha * 255.f)));
			g_VGuiSurface->DrawLine(width / 2 - lineSize / 2, height / 2 - lineSize / 2, width / 2 + lineSize / 2, height / 2 + lineSize / 2);
			g_VGuiSurface->DrawLine(width / 2 + lineSize / 2, height / 2 - lineSize / 2, width / 2 - lineSize / 2, height / 2 + lineSize / 2);
		}
	}

	if (g_Options.misc_logevents)
	{
		for (size_t i = 0; i < eventInfo.size(); i++)
		{
			float diff = eventInfo[i].m_flExpTime - g_GlobalVars->curtime;
			if (eventInfo[i].m_flExpTime < g_GlobalVars->curtime)
			{
				eventInfo.erase(eventInfo.begin() + i);
				continue;
			}

			alpha = 0.8f - diff / 0.8f;

			Visuals::DrawString(5, 5, (scrn.top + (g_Options.visuals_others_watermark ? 39 : 26)) + (9.5 * i), Color(255, 255, 255), FONT_LEFT, eventInfo[i].m_szMessage.c_str());
		}
	}
}