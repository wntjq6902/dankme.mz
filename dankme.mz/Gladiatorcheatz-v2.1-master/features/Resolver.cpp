#include "Resolver.hpp"
#include "AimRage.hpp"

#include "PlayerHurt.hpp"

#include "../Options.hpp"

#include "LagCompensation.hpp"
#include "RebuildGameMovement.hpp"

#define M_PI 3.14159265358979323846

void Resolver::Log()
{
	for (int i = 1; i <= g_GlobalVars->maxClients; i++)
	{
		//if (g_Options.hvh_resolver_experimental)
		{
			auto player = C_BasePlayer::GetPlayerByIndex(i);

			if (!player)
			{
				records[i].clear();
				continue;
			}
			else if (AimRage::Get().CheckTarget(i))
			{
				if (records[i].size() > 15 / g_GlobalVars->interval_per_tick)
					records[i].pop_back();

				resolvrecord record_to_store;
				record_to_store.SaveRecord(player);

				if (!(records[i].empty()))
				{
					record_to_store.shot = (record_to_store.shot && !records[i].front().was_dormant);
					record_to_store.suppresing_animation = records[i].front().suppresing_animation;

					record_to_store.moving = (record_to_store.moving && !PFakewalkDetection(player, record_to_store));
					record_to_store.was_moving = (records[i].front().was_moving && !PFakewalkDetection(player, record_to_store));

					record_to_store.update_origin = records[i].front().update_origin;
					record_to_store.last_standing_simtime = records[i].front().moving ? record_to_store.simtime : records[i].front().last_standing_simtime;
					record_to_store.moving_lby = records[i].front().moving_lby;
					record_to_store.lastlby_lby_delta = records[i].front().lastlby_lby_delta;
					record_to_store.last_moving_simtime = records[i].front().last_moving_simtime;
					record_to_store.last_update_simtime = records[i].front().last_update_simtime;
					record_to_store.last_update_angle = records[i].front().last_update_angle;
					record_to_store.saw_update = records[i].front().saw_update;
					if (abs(Math::ClampYaw(records[i].front().lby - record_to_store.lby)) > 10)
					{
						record_to_store.update = true;
						record_to_store.saw_update = true;
						record_to_store.lastlby_lby_delta = record_to_store.lby - records[i].front().lby;
					}
				}

				if (record_to_store.moving)
				{
					record_to_store.moving ? record_to_store.last_moving_simtime = record_to_store.simtime, record_to_store.was_moving = true : record_to_store.was_moving = false;
					record_to_store.moving = true;
					record_to_store.moving_lby = record_to_store.lby;
					record_to_store.update = false;
					record_to_store.last_update_angle = record_to_store.resolvedang;
					record_to_store.last_update_angle.yaw = record_to_store.lby;
				}
				if (record_to_store.was_moving)
					record_to_store.moving_lby = record_to_store.lby;
				if (!records[i].empty() && (record_to_store.lby == records[i].front().lby || records[i].front().moving_lby < -1000))
				{
					record_to_store.moving_lby_delta = records[i].front().moving_lby_delta;
					record_to_store.raw_lby_delta = records[i].front().raw_lby_delta;
				}
				else
				{
					record_to_store.moving_lby_delta = record_to_store.lby - record_to_store.moving_lby;
					record_to_store.raw_lby_delta = record_to_store.rawang.yaw - record_to_store.moving_lby;
				}

				if (player->m_fFlags() & FL_ONGROUND && !record_to_store.moving && !records[i].empty() && record_to_store.simtime != records[i].front().simtime)	//last simtime == curr simtime => choking packet
				{
					if (record_to_store.simtime - record_to_store.last_standing_simtime > 0.22f && record_to_store.was_moving)
					{
						record_to_store.update = true;
						record_to_store.saw_update = true;
						record_to_store.was_moving = false;
					}
					else if (record_to_store.simtime - record_to_store.last_update_simtime > 1.1f)
						record_to_store.update = true;
					else
						record_to_store.update = false;
				}

				if (record_to_store.update)
				{
					record_to_store.update_origin = record_to_store.origin;
					record_to_store.last_update_simtime = record_to_store.simtime;
					record_to_store.last_update_angle.yaw = record_to_store.lby;
					record_to_store.last_update_angle.pitch = SWResolver_pitch(player);
				}

				records[i].push_front(record_to_store);
			}
			else if (!records[i].empty())
			{
				//records[i].front().moving_lby = -100000;
				records[i].front().saw_update = false;
				records[i].front().was_dormant = true;
			}
		}
		/*else*/
		{
			auto &record = arr_infos[i];

			C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);
			if (!player || !player->IsAlive() || player->m_iTeamNum() == g_LocalPlayer->m_iTeamNum())
			{
				record.m_bActive = false;
				continue;
			}

			if (player->IsDormant())
			{
				bFirstUpdate[player->EntIndex()] = false;
				bSawUpdate[player->EntIndex()] = false;
				continue;
			}

			if (record.m_flSimulationTime == player->m_flSimulationTime())
				continue;

			record.SaveRecord(player);
			record.m_bActive = true;
		}
	}
}

void Resolver::Resolve()
{
	EventHandler.process();		//run shots missed counter before we continue
	for (int i = 1; i <= g_GlobalVars->maxClients; i++)
	{
		if (1)
		{
			//SelfWrittenResolver(i);
			//SelfWrittenResolverV2(i);
			//SelfWrittenResolverV3(i);
			REALSelfWrittenResolver(i);
		}
		else
		{
			auto &record = arr_infos[i];
			if (!record.m_bActive)
				continue;

			C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);
			if (!player || !player->IsAlive() || player->IsDormant() || player == g_LocalPlayer)
				continue;

			if (record.m_flVelocity == 0.f && player->m_vecVelocity().Length2D() != 0.f)
			{
				Math::VectorAngles(player->m_vecVelocity(), record.m_angDirectionFirstMoving);
				record.m_nCorrectedFakewalkIdx = 0;
			}

			auto firedShots = g_LocalPlayer->m_iShotsFired();

			if (g_Options.debug_fliponkey)
			{
				float_t new_yaw = player->m_flLowerBodyYawTarget();
				if (g_InputSystem->IsButtonDown(g_Options.debug_flipkey))
					new_yaw += 180.f;
				new_yaw = Math::ClampYaw(new_yaw);
				player->m_angEyeAngles().yaw = new_yaw;
				return;
			}

			if (g_Options.hvh_resolver_override && g_InputSystem->IsButtonDown(g_Options.hvh_resolver_override_key))
			{
				Override(); //needs an improvement sometimes fucked up xD

				Global::resolverModes[player->EntIndex()] = "Overriding";

				return;
			}

			AnimationLayer curBalanceLayer, prevBalanceLayer;

			ResolveInfo curtickrecord;
			curtickrecord.SaveRecord(player);

			/*if (((player->m_fFlags() & FL_ONGROUND) && (IsFakewalking(player, curtickrecord) || (player->m_vecVelocity().Length2D() > 0.1f && player->m_vecVelocity().Length2D() < 45.f && !(player->m_fFlags() & FL_DUCKING)))) && ! g_Options.hvh_resolver_experimental) //Fakewalk, shiftwalk check // We have to rework the fakewalk resolving, it sucks :D
			{
				float_t new_yaw = ResolveFakewalk(player, curtickrecord);
				new_yaw = Math::ClampYaw(new_yaw);

				player->m_angEyeAngles().yaw = new_yaw;

				//Global::resolverModes[player->EntIndex()] = "Fakewalking";

				continue;
			}*/
			if (IsEntityMoving(player) && !(/*g_Options.hvh_resolver_experimental &&*/ (player->m_fFlags() & FL_ONGROUND && (IsFakewalking(player, curtickrecord) || (player->m_vecVelocity().Length2D() > 0.1f && player->m_vecVelocity().Length2D() < 45.f && !(player->m_fFlags() & FL_DUCKING))))))
			{
				float_t new_yaw = player->m_flLowerBodyYawTarget();
				new_yaw = Math::ClampYaw(new_yaw);

				player->m_angEyeAngles().yaw = new_yaw;

				record.m_flStandingTime = player->m_flSimulationTime();
				record.m_flMovingLBY = player->m_flLowerBodyYawTarget();
				record.m_bIsMoving = true;

				Global::resolverModes[player->EntIndex()] = "Moving";

				continue;
			}
			ConVar *nospread = g_CVar->FindVar("weapon_accuracy_nospread");
			if (!player->m_fFlags() & FL_ONGROUND && nospread->GetBool())
			{
				float_t new_yaw = player->m_flLowerBodyYawTarget();
				new_yaw = ResolveBruteforce(player, new_yaw);
				new_yaw = Math::ClampYaw(new_yaw);

				player->m_angEyeAngles().yaw = new_yaw;

				continue;
			}
			if (IsAdjustingBalance(player, curtickrecord, &curBalanceLayer))
			{
				if (fabsf(LBYDelta(curtickrecord)) > 35.f)
				{
					float
						flAnimTime = curBalanceLayer.m_flCycle,	// no matter how accurate fakehead resolvers are, backtrack are always more accurate
						flSimTime = player->m_flSimulationTime();

					if (flAnimTime < 0.01f && prevBalanceLayer.m_flCycle > 0.01f && g_Options.rage_lagcompensation && CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(flSimTime - flAnimTime)))
					{
						CMBacktracking::Get().SetOverwriteTick(player, QAngle(player->m_angEyeAngles().pitch, player->m_flLowerBodyYawTarget(), 0), (flSimTime - flAnimTime), 2);
					}

					float_t new_yaw = player->m_flLowerBodyYawTarget() + record.m_flLbyDelta;
					new_yaw = Math::ClampYaw(new_yaw);

					player->m_angEyeAngles().yaw = new_yaw;

					Global::resolverModes[player->EntIndex()] = "Fakehead (delta > 35)";
				}
				if (IsAdjustingBalance(player, record, &prevBalanceLayer))
				{
					//if (!(g_Options.hvh_resolver_experimental && ExperimentalLBYResolver(player, record)))
					{
						if ((prevBalanceLayer.m_flCycle != curBalanceLayer.m_flCycle) || curBalanceLayer.m_flWeight == 1.f)
						{
							float
								flAnimTime = curBalanceLayer.m_flCycle,
								flSimTime = player->m_flSimulationTime();

							if (flAnimTime < 0.01f && prevBalanceLayer.m_flCycle > 0.01f && g_Options.rage_lagcompensation && CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(flSimTime - flAnimTime)))
							{
								CMBacktracking::Get().SetOverwriteTick(player, QAngle(player->m_angEyeAngles().pitch, player->m_flLowerBodyYawTarget(), 0), (flSimTime - flAnimTime), 2);
							}

							float_t new_yaw = player->m_flLowerBodyYawTarget();
							new_yaw = Math::ClampYaw(new_yaw);

							player->m_angEyeAngles().yaw = new_yaw;

							Global::resolverModes[player->EntIndex()] = "Breaking LBY";

							continue;
						}
						else if (curBalanceLayer.m_flWeight == 0.f && (prevBalanceLayer.m_flCycle > 0.92f && curBalanceLayer.m_flCycle > 0.92f)) // breaking lby with delta < 120
						{
							if (player->m_flSimulationTime() >= record.m_flStandingTime + 0.22f && record.m_bIsMoving)
							{
								record.m_flLbyDelta = record.m_flLowerBodyYawTarget - player->m_flLowerBodyYawTarget();

								float
									flAnimTime = curBalanceLayer.m_flCycle,
									flSimTime = player->m_flSimulationTime();

								if (flAnimTime < 0.01f && prevBalanceLayer.m_flCycle > 0.01f && g_Options.rage_lagcompensation && CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(flSimTime - flAnimTime)))
								{
									CMBacktracking::Get().SetOverwriteTick(player, QAngle(player->m_angEyeAngles().pitch, player->m_flLowerBodyYawTarget(), 0), (flSimTime - flAnimTime), 2);
								}

								float_t new_yaw = player->m_flLowerBodyYawTarget() + record.m_flLbyDelta;
								new_yaw = Math::ClampYaw(new_yaw);

								player->m_angEyeAngles().yaw = new_yaw;

								record.m_bIsMoving = false;

								Global::resolverModes[player->EntIndex()] = "Breaking LBY (delta < 120)";

								continue;
							}

							if (player->m_flSimulationTime() >= record.m_flStandingTime + 1.32f && std::fabsf(record.m_flLbyDelta) < 35.f)
							{
								record.m_flLbyDelta = record.m_flMovingLBY - player->m_flLowerBodyYawTarget();
								float_t new_yaw = player->m_flLowerBodyYawTarget() + record.m_flLbyDelta;
								new_yaw = Math::ClampYaw(new_yaw);

								player->m_angEyeAngles().yaw = new_yaw;

								record.m_bIsMoving = false;

								Global::resolverModes[player->EntIndex()] = "LBY delta < 35";

								continue;
							}
						}
					}
				}
				else
				{
					float_t new_yaw = player->m_flLowerBodyYawTarget();
					new_yaw = Math::ClampYaw(new_yaw);

					player->m_angEyeAngles().yaw = new_yaw;

					Global::resolverModes[player->EntIndex()] = "Other";

					continue;
				}
			}
			if (player->m_flSimulationTime() >= record.m_flStandingTime + 0.22f && record.m_bIsMoving)
			{
				record.m_flLbyDelta = record.m_flLowerBodyYawTarget - player->m_flLowerBodyYawTarget();

				float
					flAnimTime = curBalanceLayer.m_flCycle,
					flSimTime = player->m_flSimulationTime();

				if (flAnimTime < 0.01f && prevBalanceLayer.m_flCycle > 0.01f && g_Options.rage_lagcompensation && CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(flSimTime - flAnimTime)))
				{
					CMBacktracking::Get().SetOverwriteTick(player, QAngle(player->m_angEyeAngles().pitch, player->m_flLowerBodyYawTarget(), 0), (flSimTime - flAnimTime), 2);
				}

				float_t new_yaw = player->m_flLowerBodyYawTarget() + record.m_flLbyDelta;
				new_yaw = Math::ClampYaw(new_yaw);

				player->m_angEyeAngles().yaw = new_yaw;

				record.m_bIsMoving = false;

				Global::resolverModes[player->EntIndex()] = "Breaking LBY (delta < 120)";

				continue;
			}
			if (player->m_flSimulationTime() >= record.m_flStandingTime + 1.32f && std::fabsf(record.m_flLbyDelta) < 35.f)
			{
				record.m_flLbyDelta = record.m_flMovingLBY - player->m_flLowerBodyYawTarget();
				float_t new_yaw = player->m_flLowerBodyYawTarget() + record.m_flLbyDelta;
				new_yaw = Math::ClampYaw(new_yaw);

				player->m_angEyeAngles().yaw = new_yaw;

				record.m_bIsMoving = false;

				Global::resolverModes[player->EntIndex()] = "LBY delta < 35";

				continue;
			}

			float_t new_yaw = player->m_flLowerBodyYawTarget() + record.m_flLbyDelta;
			new_yaw = Math::ClampYaw(new_yaw);

			player->m_angEyeAngles().yaw = new_yaw;
		}
	}
}

void Resolver::FakelagFix()
{
	static bool m_bWasInAir[65] = { false };
	for (int i = 0; i < g_EngineClient->GetMaxClients(); i++)
	{
		auto player = C_BasePlayer::GetPlayerByIndex(i);
		if (checks::is_bad_ptr(player) || !player->IsPlayer() ||
			!player->IsAlive() || player->IsDormant() || player == g_LocalPlayer)
			continue;

		Global::FakelagFixed[i] = false;

		Global::PlayersChockedPackets[i] = TIME_TO_TICKS((player->m_flOldSimulationTime() + g_GlobalVars->interval_per_tick) - player->m_flSimulationTime());

		static Vector origin[65] = { Vector(0, 0, 0) }, velocity[65] = { Vector(0, 0, 0) };
		static Vector origin_backup[65] = { Vector(0, 0, 0) }, velocity_backup[65] = { Vector(0, 0, 0) };
		static int flag[65] = { 0 };
		static int flag_backup[65] = { 0 };

		if (Global::PlayersChockedPackets[i] > 1)
		{
			if (Global::PlayersChockedPackets[i] <= 14)
			{
				//RebuildGameMovement::Get().VelocityExtrapolate(player, origin[i], velocity[i], flag[i], m_bWasInAir[i]);
				RebuildGameMovement::Get().FullWalkMove(player);
				origin[i] = player->GetAbsOrigin();	//rebuild movement changes abs origin

				m_bWasInAir[i] = (flag[i] & FL_ONGROUND) && !(player->m_fFlags() & FL_ONGROUND);

				if (player->m_vecOrigin().DistTo(origin_backup[i]) > 64)
				{
					player->SetAbsOrigin(origin_backup[i]);
					player->m_vecOrigin() = origin_backup[i];
					player->m_vecVelocity() = velocity_backup[i];
					player->m_fFlags() = flag_backup[i];
				}
				else
				{
					player->m_vecOrigin() = origin[i];
					player->m_vecVelocity() = velocity[i];
					player->m_fFlags() = flag[i];
					Global::FakelagFixed[i] = true;
				}
			}
		}
		else
		{
			origin[i] = player->m_vecOrigin();
			velocity[i] = player->m_vecVelocity();
			flag[i] = player->m_fFlags();

			origin_backup[i] = origin[i];
			velocity_backup[i] = velocity[i];
			flag_backup[i] = flag[i];

			Global::FakelagUnfixedPos[i] = origin[i];
		}
	}
}

void Resolver::Override()
{
	if (!g_Options.hvh_resolver_override)
		return;

	if (!g_InputSystem->IsButtonDown(g_Options.hvh_resolver_override_key))
		return;

	int w, h, cx, cy;

	g_EngineClient->GetScreenSize(w, h);

	cx = w / 2;
	cy = h / 2;

	Vector crosshair = Vector(cx, cy, 0);

	C_BasePlayer * nearest_player = nullptr;
	float bestFoV = 0;
	Vector bestHead2D;

	for (int i = 1; i <= g_GlobalVars->maxClients; i++) //0 is always the world entity
	{
		C_BasePlayer *player = (C_BasePlayer*)g_EntityList->GetClientEntity(i);

		if (!CMBacktracking::Get().IsPlayerValid(player)) //ghetto
			continue;

		Vector headPos3D = player->GetBonePos(HITBOX_HEAD), headPos2D;

		if (!Math::WorldToScreen(headPos3D, headPos2D))
			continue;

		float FoV2D = crosshair.DistTo(headPos2D);

		if (!nearest_player || FoV2D < bestFoV)
		{
			nearest_player = player;
			bestFoV = FoV2D;
			bestHead2D = headPos2D;
		}
	}

	if (nearest_player) //use pointers and avoid double calling of GetClientEntity
	{
		int minX = cx - (w / 10), maxX = cx + (w / 10);

		if (bestHead2D.x < minX || bestHead2D.x > maxX)
			return;

		int totalWidth = maxX - minX;

		int playerX = bestHead2D.x - minX;

		int yawCorrection = -(((playerX * 360) / totalWidth) - 180);

		float_t new_yaw = yawCorrection;

		Math::ClampYaw(new_yaw);

		nearest_player->m_angEyeAngles().yaw += new_yaw;
	}
}

float_t Resolver::ResolveFakewalk(C_BasePlayer *player, ResolveInfo &curtickrecord)	//high chance of missing, but still try to tap atleast 2 shot
{
	auto &record = arr_infos[player->EntIndex()];

	float_t yaw;
	int32_t correctedFakewalkIdx = record.m_nCorrectedFakewalkIdx;

	if (correctedFakewalkIdx < 2)
	{
		yaw = record.m_angDirectionFirstMoving.yaw + 180.f;	//from mutiny, no idea why but it works?
		Global::resolverModes[player->EntIndex()] = "Fakewalking stage 1";
	}
	else if (correctedFakewalkIdx < 4)
	{
		yaw = player->m_flLowerBodyYawTarget() + record.m_flLbyDelta;
		Global::resolverModes[player->EntIndex()] = "Fakewalking stage 2";
	}
	else if (correctedFakewalkIdx < 6)
	{
		yaw = record.m_angDirectionFirstMoving.yaw;
		Global::resolverModes[player->EntIndex()] = "Fakewalking stage 3";
	}
	else
	{
		QAngle dir;
		Math::VectorAngles(curtickrecord.m_vecVelocity, dir);

		//yaw = dir.yaw;
		yaw = ResolveBruteforce(player, dir.yaw);	//goes full on retarded and brute everything
	}

	return yaw;
}

float_t Resolver::ResolveBruteforce(C_BasePlayer *player, float baseangle)
{
	static float hitang[65] = { -999 };
	static float lastang[65] = { -999 };

	int index = player->EntIndex();

	if (Global::hit_while_brute[index] && missed_shots[index] < 5)
	{
		if (hitang[index] < -200)
			hitang[index] = lastang[index];

		Global::resolverModes[index] = "Fake: Bruteforce";
		return hitang[index];
	}
	else
		hitang[index] = -999;

	lastang[index] = baseangle;
	if(missed_shots[index] < 5 && abs(Math::ClampYaw(player->m_flLowerBodyYawTarget() - baseangle)) < 10)
	{
		switch(missed_shots[index] % 6)
		{
			case 0:
				lastang[index] += 101;
				break;
			case 1:
				lastang[index] += 109;
				break;
			case 2:
				lastang[index] += 119;
				break;
			case 3:
				lastang[index] -= 101;
				break;
			case 4:
				lastang[index] -= 109;
				break;
			case 5:
				lastang[index] -= 119;
				break;
		}
	}
	else
	{
		switch (missed_shots[index] % 13)
		{
		case 0:
			break;
		case 1:
		case 2:
			lastang[index] += (missed_shots[index] % 8) * 45;
			break;
		case 3:
			lastang[index] += 180;
			break;
		case 4:
		case 5:
			lastang[index] += (missed_shots[index] % 8) * 45 + 90;
			break;
		case 6:
			lastang[index] += 45;
			break;
		case 7:
			lastang[index] += 90;
			break;
		case 8:
			lastang[index] += 135;
			break;
		case 9:
			lastang[index] += 180;
			break;
		case 10:
			lastang[index] += 225;
			break;
		case 11:
			lastang[index] += 270;
			break;
		case 12:
			lastang[index] += 315;
			break;
		}
	}

	lastang[index] = Math::ClampYaw(lastang[index]);

	Global::resolverModes[index] = "Fake: Bruteforce";

	return lastang[index];
}

bool Resolver::IsEntityMoving(C_BasePlayer *player)
{
	return (player->m_vecVelocity().Length2D() > 0.1f && player->m_fFlags() & FL_ONGROUND);
} 

bool Resolver::IsAdjustingBalance(C_BasePlayer *player, ResolveInfo &record, AnimationLayer *layer)
{
	for (int i = 0; i < record.m_iLayerCount; i++)
	{
		const int activity = player->GetSequenceActivity(record.animationLayer[i].m_nSequence);
		if (activity == 979)
		{
			*layer = record.animationLayer[i];
			return true;
		}
	}
	return false;
}

bool Resolver::IsAdjustingStopMoving(C_BasePlayer *player, ResolveInfo &record, AnimationLayer *layer)
{
	for (int i = 0; i < record.m_iLayerCount; i++)
	{
		const int activity = player->GetSequenceActivity(record.animationLayer[i].m_nSequence);
		if (activity == 980)
		{
			*layer = record.animationLayer[i];
			return true;
		}
	}
	return false;
}

bool Resolver::IsFakewalking(C_BasePlayer *player, ResolveInfo &record)
{
	bool
		bFakewalking = false,
		stage1 = false,			// stages needed cause we are iterating all layers, eitherwise won't work :)
		stage2 = false,
		stage3 = false;

	for (int i = 0; i < record.m_iLayerCount; i++)
	{
		if (record.animationLayer[i].m_nSequence == 26 && record.animationLayer[i].m_flWeight < 0.4f)
			stage1 = true;
		if (record.animationLayer[i].m_nSequence == 7 && record.animationLayer[i].m_flWeight > 0.001f)
			stage2 = true;
		if (record.animationLayer[i].m_nSequence == 2 && record.animationLayer[i].m_flWeight == 0)
			stage3 = true;
	}

	if (stage1 && stage2)
		if (stage3 || (player->m_fFlags() & FL_DUCKING)) // since weight from stage3 can be 0 aswell when crouching, we need this kind of check, cause you can fakewalk while crouching, thats why it's nested under stage1 and stage2
			bFakewalking = true;
		else
			bFakewalking = false;
	else
		bFakewalking = false;

	return bFakewalking;
}

bool Resolver::PFakewalkDetection(C_BasePlayer *player, resolvrecord &record)
{
	bool weight981, weightseq2, weightlay12, pbratelay6 = false;
	for (int i = 0; i < record.layercount; i++)
	{
		const int activity = player->GetSequenceActivity(record.animationLayer[i].m_nSequence);
		if (activity == 981 && record.animationLayer[i].m_flWeight == 1)
			weight981 = true;
		if (record.animationLayer[i].m_nSequence == 2 && record.animationLayer[i].m_flWeight == 0)
			weightseq2 = true;
		if (i == 12 && record.animationLayer[i].m_flWeight > 0)
			weightlay12 = true;
		if (i == 6 && record.animationLayer[i].m_flPlaybackRate < 0.001)
			pbratelay6 = true;
	}
	if (player->m_vecVelocity().Length2D() > 100.f || player->m_vecVelocity().Length2D() < 0.1f || !(player->m_fFlags() & FL_ONGROUND)) return false;
	else if (weight981 && weightseq2 && weightlay12 && pbratelay6)
	{
		if (!records[player->EntIndex()].empty()) records[player->EntIndex()].front().suppresing_animation = false;
		return true;
	}
	else //animation-less method
	{
		static int choked[65] = { 0 };
		static int last_choke[65] = { 0 };
		static Vector last_origin[65] = { Vector(0, 0, 0) };

		int choke = TIME_TO_TICKS((player->m_flOldSimulationTime() + g_GlobalVars->interval_per_tick) - player->m_flSimulationTime());
		static bool returnval[65] = { false };

		if (choke < last_choke[player->EntIndex()])
		{
			choked[player->EntIndex()] = last_choke[player->EntIndex()];
			if (!last_origin[player->EntIndex()].IsZero())
			{
				Vector delta = player->m_vecOrigin() - last_origin[player->EntIndex()];
				float calced_vel = delta.Length2D() / choked[player->EntIndex()];
				returnval[player->EntIndex()] = calced_vel != player->m_vecVelocity().Length2D() * g_GlobalVars->interval_per_tick;
			}
			last_origin[player->EntIndex()] = player->m_vecOrigin();
		}
		last_choke[player->EntIndex()] = choke;

		if (choked[player->EntIndex()] < 3) return false;
		if (returnval[player->EntIndex()] && !records[player->EntIndex()].empty()) records[player->EntIndex()].front().suppresing_animation = true;

		return returnval[player->EntIndex()];
	}
}
float Resolver::GetLBYByCompairingTicks(int playerindex)
{
	int modulo = 1;
	//int diffrence = exp_res_infos[playerindex].Get_Diffrent_LBYs(10.f);
	int diffrence = Get_Diffrent_LBYs(10.f, playerindex);
	int step = modulo * diffrence;
	for (auto var : records[playerindex])
	{
		for (int last_tick = var.tickcount; last_tick <= g_GlobalVars->tickcount; last_tick += step)
		{
			if (last_tick == g_GlobalVars->tickcount)
				return var.lby;
		}
	}
	return -1000;
}
float Resolver::GetDeltaByCompairingTicks(int playerindex)
{
	int modulo = 1;
	//int diffrence = exp_res_infos[playerindex].Get_Diffrent_Deltas(10.f);
	int diffrence = Get_Diffrent_Deltas(10.f, playerindex);
	int step = modulo * diffrence;
	for (auto var : records[playerindex])
	{
		for (int last_tick = var.tickcount; last_tick <= g_GlobalVars->tickcount; last_tick += step)
		{
			if (last_tick == g_GlobalVars->tickcount)
				return var.moving_lby_delta;
		}
	}
	return -1000;
}
bool Resolver::Choking_Packets(int i)
{
	float delta = records[i].front().simtime - records[i].at(1).simtime;
	return delta != max(g_GlobalVars->interval_per_tick, g_GlobalVars->frametime);
}
bool Resolver::Has_Static_Real(float tolerance, int i)
{
	auto minmax = std::minmax_element(std::begin(records[i]), std::end(records[i]), [](const resolvrecord& t1, const resolvrecord& t2)
	{ return t1.lby < t2.lby; });
	return (fabs(minmax.first->lby - minmax.second->lby) <= tolerance);
}
bool Resolver::Has_Static_Yaw_Difference(float tolerance, int i)
{
	if (Fake_Unusuable(tolerance, i))
		return false;

	return GetDelta(records[i].at(0).rawang.yaw, records[i].at(1).rawang.yaw) < tolerance;
}
bool Resolver::Has_Steady_Difference(float tolerance, int i)
{
	size_t misses = 0;
	for (size_t x = 0; x < records[i].size() - 1; x++) {
		float tickdif = static_cast<float>(records[i].at(x).simtime - records[i].at(x + 1).tickcount);
		float lbydif = GetDelta(records[i].at(x).lby, records[i].at(x + 1).lby);
		float ntickdif = static_cast<float>(g_GlobalVars->tickcount - records[i].at(x).tickcount);
		if (((lbydif / tickdif) * ntickdif) > tolerance) misses++;
	}
	return (misses <= (records[i].size() / 3));
}
int Resolver::Get_Diffrent_Deltas(float tolerance, int i)
{
	std::vector<float>var;
	float avg = 0;
	float total = 0;
	for (int x = 0; x < records[i].size(); x++)
	{
		total += records[i].at(x).moving_lby_delta;
		avg = total / (x + 1);
		if (std::fabsf(records[i].at(x).moving_lby_delta - avg) > tolerance)
			var.push_back(records[i].at(x).moving_lby_delta);
	}
	return var.size();
}
int Resolver::Get_Diffrent_Fakes(float tolerance, int i)
{
	std::vector<float>var;
	float avg = 0;
	float total = 0;
	for (int x = 0; x < records[i].size(); x++)
	{
		total += records[i].at(x).rawang.yaw;
		avg = total / (x + 1);
		if (std::fabsf(records[i].at(x).rawang.yaw - avg) > tolerance)
			var.push_back(records[i].at(x).rawang.yaw);
	}
	return var.size();
}
int Resolver::Get_Diffrent_LBYs(float tolerance, int i)
{
	std::vector<float>var;
	float avg = 0;
	float total = 0;
	for (int x = 0; x < records[i].size(); x++)
	{
		total += records[i].at(x).lby;
		avg = total / (x + 1);
		if (std::fabsf(records[i].at(x).lby - avg) > tolerance)
			var.push_back(records[i].at(x).lby);
	}
	return var.size();
}

bool Resolver::Delta_Keeps_Changing(float tolerance, int i)
{
	return (Get_Diffrent_Deltas(tolerance, i) > (int)records[i].size() / 2);
}
bool Resolver::Fake_Unusuable(float tolerance, int i)
{
	return (Get_Diffrent_Fakes(tolerance, i) > (int)records[i].size() / 2);
}
bool Resolver::LBY_Keeps_Changing(float tolerance, int i)
{
	return (Get_Diffrent_LBYs(tolerance, i) > (int)records[i].size() / 2);
}

bool Resolver::IsFreestanding_thickness(C_BasePlayer *player, float &angle)
{
	float bestrotation, highestthickness, radius = 0.f;
	Vector besthead, headpos, eyepos, origin;
	float step = M_PI * 2.0 / 15;

	auto checkWallThickness = [&](Vector newhead) -> float
	{
		Ray_t ray;
		trace_t trace1, trace2;
		Vector endpos1, endpos2;
		Vector eyepos = g_LocalPlayer->GetEyePos();
		CTraceFilterSkipTwoEntities filter(g_LocalPlayer, player);

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

		float add = newhead.DistTo(eyepos) - (player->GetAbsOrigin() + player->m_vecViewOffset()).DistTo(eyepos) + 3.f;
		return endpos1.DistTo(endpos2) + add / 3;
	};

	origin = player->GetAbsOrigin();
	eyepos = origin + player->m_vecViewOffset();
	headpos = player->GetHitboxPos(0);

	radius = Vector(headpos - origin).Length2D();

	for (float rotation = 0; rotation < (M_PI * 2.0); rotation += step)
	{
		float thickness = 0.f;
		Vector newhead(radius * cos(rotation) + eyepos.x, radius * sin(rotation) + eyepos.y, eyepos.z);

		thickness = checkWallThickness(newhead);

		if (thickness > highestthickness)
		{
			highestthickness = thickness;
			bestrotation = rotation;
			besthead = newhead;
		}
	}
	angle = RAD2DEG(bestrotation);
	return (highestthickness != 0 && besthead.IsValid());
}

int Resolver::IsFreestanding_awall(C_BasePlayer *player)
{
	Vector direction_1, direction_2, direction_3;
	Math::AngleVectors(QAngle(0, Math::CalcAngle(g_LocalPlayer->m_vecOrigin(), player->m_vecOrigin()).yaw - 90, 0), direction_1);
	Math::AngleVectors(QAngle(0, Math::CalcAngle(g_LocalPlayer->m_vecOrigin(), player->m_vecOrigin()).yaw + 90, 0), direction_2);
	Math::AngleVectors(QAngle(0, Math::CalcAngle(g_LocalPlayer->m_vecOrigin(), player->m_vecOrigin()).yaw + 180, 0), direction_3);

	trace_t trace;
	Ray_t ray;
	CTraceFilter filter;
	filter.pSkip = player;

	auto eyepos_1 = player->GetEyePos() + (direction_1 * 128);
	auto eyepos_2 = player->GetEyePos() + (direction_2 * 128);
	auto eyepos_3 = player->GetEyePos() + (direction_3 * 128);

	ray.Init(player->GetEyePos(), eyepos_1);
	g_EngineTrace->TraceRay(ray, MASK_SHOT_HULL, &filter, &trace);
	eyepos_1 = trace.endpos;

	ray.Init(player->GetEyePos(), eyepos_2);
	g_EngineTrace->TraceRay(ray, MASK_SHOT_HULL, &filter, &trace);
	eyepos_2 = trace.endpos;

	ray.Init(player->GetEyePos(), eyepos_3);
	g_EngineTrace->TraceRay(ray, MASK_SHOT_HULL, &filter, &trace);
	eyepos_3 = trace.endpos;


	//float damage1 = AimRage::Get().GetDamageVec(eyepos_1, g_LocalPlayer, HITBOX_HEAD, false, true, nullptr, player);
	//float damage2 = AimRage::Get().GetDamageVec(eyepos_2, g_LocalPlayer, HITBOX_HEAD, false, true, nullptr, player);
	float damage1 = AimRage::Get().GetDamageVec2(g_LocalPlayer->GetEyePos(), eyepos_1, g_LocalPlayer, player, HITBOX_HEAD).damage;
	float damage2 = AimRage::Get().GetDamageVec2(g_LocalPlayer->GetEyePos(), eyepos_2, g_LocalPlayer, player, HITBOX_HEAD).damage;
	float damage3 = AimRage::Get().GetDamageVec2(g_LocalPlayer->GetEyePos(), eyepos_3, g_LocalPlayer, player, HITBOX_HEAD).damage;

	if (std::fabsf(damage1 - damage2) < 20)
	{
		if (std::fabsf(((damage1 + damage2) / 2) - damage3) > 30)
			return 3;
		else
			return 0;
	}
	else if (damage1 < damage2)
	{
		return 1;
	}
	else
	{
		return 2;
	}
}

bool Resolver::IsFreestanding_walldt(C_BasePlayer *player, float &angle)
{
	trace_t trace;
	Ray_t ray;
	CTraceFilterWorldOnly filter;

	static constexpr float trace_distance = 25.f;
	const auto head_position = player->GetAbsOrigin() + player->m_vecViewOffset();

	float last_fraction = 1.f;
	std::deque<float> angles;
	for (int i = 0; i < 360; i += 2)
	{
		Vector direction;
		Math::AngleVectors(QAngle(0, i, 0), direction);

		ray.Init(head_position, head_position + (direction * trace_distance));
		g_EngineTrace->TraceRay(ray, MASK_ALL, &filter, &trace);

		if (trace.fraction > last_fraction)
		{
			angles.push_front(i - 2);
		}
		else
			last_fraction = trace.fraction;
	}
	for (int i = 0; i < angles.size(); i++)
	{
		if (std::fabsf((player->m_flLowerBodyYawTarget() + 180) - angles.at(i)) < 35)
		{
			angle = angles.at(i);
			return true;
		}
	}
	return false;
}

bool Resolver::IsBreakingLBY(C_BasePlayer *player)
{
	auto poses = player->m_flPoseParameter();
	return ((abs(poses[11] - 0.5f) > 0.302f));
}

float Resolver::SWResolver_pitch(C_BasePlayer *player)
{
	static auto weapon_accuracy_nospread = g_CVar->FindVar("weapon_accuracy_nospread");
	float returnval = networkedPlayerPitch[player->EntIndex()];

	if (weapon_accuracy_nospread->GetBool())
	{
		static bool cant_find_pitch[65] = { false };
		if (missed_shots[player->EntIndex()] > 5)
		{
			if (missed_shots[player->EntIndex()] > 30 || cant_find_pitch[player->EntIndex()])
			{
				switch (missed_shots[player->EntIndex()] % 9)
				{
				case 0: break;
				case 1: returnval *= -1; break;
				case 2: returnval *= 0.5; break;
				case 3: returnval *= -0.5; break;
				case 4: returnval *= 1.5; break;
				case 5: returnval *= -1.5; break;
				case 6: returnval = 89.f; break;
				case 7: returnval = -89.f; break;
				case 8: returnval = 0; break;
				}
			}
			else
			{
				float temp_var = Math::FindSmallestFake(returnval, missed_shots[player->EntIndex()] % 3);
				if (temp_var < -2000) cant_find_pitch[player->EntIndex()] = true;
				return Math::ComputeBodyPitch(temp_var);
			}
		}
		else
		{
			cant_find_pitch[player->EntIndex()] = false;
		}
	}

	return Math::ClampPitch(returnval);
}

bool Resolver::Is979MEME(C_BasePlayer *player)
{
	int animlayers = player->GetNumAnimOverlays();

	for (int i = 0; i < animlayers; i++)
	{
		if (player->GetSequenceActivity(player->GetAnimOverlay(i)->m_nSequence) == 979)
		{
			return true;
		}
	}

	return false;
}

float Resolver::AnimationResolve(C_BasePlayer *player)
{
	if (records[player->EntIndex()].front().suppresing_animation)
		return ResolveBruteforce(player, player->m_flLowerBodyYawTarget());

	if (!IsBreakingLBY(player))
	{
		Global::resolverModes[player->EntIndex()] = "LBY Not Broken (Anim)";
		return player->m_flLowerBodyYawTarget();
	}
	else
	{
		Global::resolverModes[player->EntIndex()] = "Animation Brute";
		auto poses = Animation::Get().GetPlayerAnimationInfo(player->EntIndex()).m_flPoseParameters;
		if (abs(poses[11] - 0.5) > 0.499f) //60+ delta
		{
			switch (missed_shots[player->EntIndex()] % 6)
			{
			case 0: return Math::ClampYaw(player->m_flLowerBodyYawTarget() + 130); break;
			case 1: return Math::ClampYaw(player->m_flLowerBodyYawTarget() - 130); break;
			case 2: return Math::ClampYaw(player->m_flLowerBodyYawTarget() - 110); break;
			case 3: return Math::ClampYaw(player->m_flLowerBodyYawTarget() + 150); break;
			case 4: return Math::ClampYaw(player->m_flLowerBodyYawTarget() - 150); break;
			case 5: return Math::ClampYaw(player->m_flLowerBodyYawTarget() + 70); break;
			case 6: return Math::ClampYaw(player->m_flLowerBodyYawTarget() - 70); break;
			}
		}
		else
		{								   //35-60 delta
			switch (missed_shots[player->EntIndex()] % 4)
			{
			case 0: return Math::ClampYaw(player->m_flLowerBodyYawTarget() + 47.5); break;
			case 1: return Math::ClampYaw(player->m_flLowerBodyYawTarget() + 55); break;
			case 2: return Math::ClampYaw(player->m_flLowerBodyYawTarget() - 47.5); break;
			case 3: return Math::ClampYaw(player->m_flLowerBodyYawTarget() - 55); break;
			}
		}
	}
}

float Resolver::OnAirBrute(C_BasePlayer *player)
{
	return Math::ClampYaw(player->m_flLowerBodyYawTarget() + ((missed_shots[player->EntIndex()] % 14) * 25.7f));
}

void Resolver::REALSelfWrittenResolver(int playerindex)
{
	if (!AimRage::Get().CheckTarget(playerindex))
		return;

	auto player = C_BasePlayer::GetPlayerByIndex(playerindex);
	float freestanding_yaw;
	int freestanding_awall = IsFreestanding_awall(player);
	bool freestanding_thickness = false;
	//if (!freestanding_awall)
		//freestanding_thickness = IsFreestanding_thickness(player, freestanding_yaw);
	int last_clip[65] = { 0 };

	if (records[playerindex].size() < 2)
	{
		Global::resolverModes[player->EntIndex()] = "Fail: Not enough records";
		player->m_angEyeAngles().pitch = SWResolver_pitch(player);
		return;
	}

	bool hs_only = false;

	auto record = records[playerindex].front();

	AnimationLayer curBalanceLayer, prevBalanceLayer;

	ResolveInfo curtickrecord;
	curtickrecord.SaveRecord(player);

	QAngle viewangles;
	g_EngineClient->GetViewAngles(viewangles);

	float at_target_yaw = Math::CalcAngle(g_LocalPlayer->GetAbsOrigin(), player->GetAbsOrigin()).yaw;

	if (g_Options.hvh_resolver_override && g_InputSystem->IsButtonDown(g_Options.hvh_resolver_override_key) && std::fabsf(Math::ClampYaw(viewangles.yaw - at_target_yaw)) < 50)
	{
		auto rotate = [](float lby, float yaw)	//better override
		{
			float delta = Math::ClampYaw(lby - yaw);
			if (fabs(delta) < 25.f)
				return lby;

			if (delta > 0.f)
				return yaw + 25.f;

			return yaw;
		};

		player->m_angEyeAngles().yaw = Math::ClampYaw(rotate(player->m_flLowerBodyYawTarget(), Math::ClampYaw(viewangles.yaw - at_target_yaw) > 0) ? at_target_yaw + 90.f : at_target_yaw - 90.f);

		Global::resolverModes[player->EntIndex()] = "Overriding";

		return;
	}

	static ConVar *weapon_accuracy_nospread = g_CVar->FindVar("weapon_accuracy_nospread"), *mp_damage_headshot_only = g_CVar->FindVar("mp_damage_headshot_only");
	if (checks::is_bad_ptr(weapon_accuracy_nospread))
		weapon_accuracy_nospread = g_CVar->FindVar("weapon_accuracy_nospread");
	if (checks::is_bad_ptr(mp_damage_headshot_only))
		mp_damage_headshot_only = g_CVar->FindVar("mp_damage_headshot_only");
	if (checks::is_bad_ptr(weapon_accuracy_nospread) || checks::is_bad_ptr(mp_damage_headshot_only))
		hs_only = false;
	else
		hs_only = (weapon_accuracy_nospread->GetBool() || mp_damage_headshot_only->GetBool());

	Global::bBaim[playerindex] = missed_shots[playerindex] > 10 && !hs_only;

	/*
	if (Choking_Packets(playerindex))	// has fake, need resolving
	{
		/* yaw resolver starts */
		/*
		if (!record.suppresing_animation)
		{
			if (IsBreakingLBY(player))
			{
				for (int i = 1; abs(record.moving_lby_delta) < 5 && i < records[playerindex].size(); i++)								//they can be supressing animation but idc
				{																														//why whould they ever have no lby breaker when they has anim supression lol
					records[playerindex].front().moving_lby_delta = records[playerindex].at(i).moving_lby_delta;
					record = records[playerindex].front();
				}
			}
			else
			{
				for (int i = 1; abs(record.moving_lby_delta) > 35 && i < records[playerindex].size(); i++)
				{
					records[playerindex].front().moving_lby_delta = records[playerindex].at(i).moving_lby_delta;
					record = records[playerindex].front();
				}
			}
		}
		*//*
		float cloest_dest = 9999.f; bool using_recorded_angle = false;
		if (missed_shots[playerindex] < 2 || Has_Static_Real(15, playerindex))
		{
			for (auto i = angle_records.begin(); i != angle_records.end(); i++)
			{
				if (i->handle != &player->GetRefEHandle() || i->position.DistTo(record.origin) > 64.f || i->position.DistTo(record.origin) > cloest_dest)
					continue;

				using_recorded_angle = true;
				Global::resolverModes[playerindex] = "Angle Recording";
				player->m_angEyeAngles() = i->angle;
				cloest_dest = i->position.DistTo(record.origin);
			}
		}
		
		if (record.shot)
		{
			Global::resolverModes[playerindex] = "Shot";
			if (!EventHandler.records2.empty()) player->m_angEyeAngles() = EventHandler.records2.back().direction;
			CMBacktracking::Get().SetOverwriteTick(player, player->m_angEyeAngles(), player->m_hActiveWeapon().Get()->m_fLastShotTime(), 2);
		}
		else if (!(player->m_fFlags() & FL_ONGROUND) && missed_shots[playerindex] > 1)
		{
			player->m_angEyeAngles().yaw = OnAirBrute(player);
			Global::resolverModes[playerindex] = "On-Air";
			Global::bBaim[playerindex] = (!hs_only && missed_shots[playerindex] > 5);
		}
		else if (record.moving && player->m_fFlags() & FL_ONGROUND && (!record.suppresing_animation || missed_shots[playerindex] < 2))
		{
			Global::resolverModes[playerindex] = "Moving";
			player->m_angEyeAngles().yaw = player->m_flLowerBodyYawTarget();
		}
		else if (record.was_moving && player->m_fFlags() & FL_ONGROUND && (!record.suppresing_animation || missed_shots[playerindex] < 2))
		{
			Global::resolverModes[playerindex] = "Was Moving";
			player->m_angEyeAngles().yaw = player->m_flLowerBodyYawTarget();
		}
		else if (record.update && record.saw_update)
		{
			Global::resolverModes[playerindex] = "LBY Update";
			player->m_angEyeAngles().yaw = player->m_flLowerBodyYawTarget();
			CMBacktracking::Get().SetOverwriteTick(player, record.last_update_angle, record.last_update_simtime, 2);
		}
		/*
		else if (abs(floor(record.lastlby_lby_delta) - record.lastlby_lby_delta) < 0.001 && abs(ceil(record.lastlby_lby_delta) - record.lastlby_lby_delta) < 0.001 && abs(record.lastlby_lby_delta) > 35 && missed_shots[playerindex] < 10)
		{
			Global::resolverModes[playerindex] = "Faulty LBY Breaker";
			player->m_angEyeAngles().yaw = Math::ClampYaw(player->m_flLowerBodyYawTarget() + record.lastlby_lby_delta);
		}
		*//*
		else if (!record.update && !using_recorded_angle && freestanding_awall)
		{
			Global::resolverModes[playerindex] = "Fake: Reverse Freestanding";
			switch (freestanding_awall)
			{
			case 0:
				player->m_angEyeAngles().yaw = freestanding_yaw;
				break;
			case 1:
				player->m_angEyeAngles().yaw = Math::ClampYaw(Math::CalcAngle(g_LocalPlayer->m_vecOrigin(), player->m_vecOrigin()).yaw - 90);
				break;
			case 2:
				player->m_angEyeAngles().yaw = Math::ClampYaw(Math::CalcAngle(g_LocalPlayer->m_vecOrigin(), player->m_vecOrigin()).yaw + 90);
				break;
			case 3:
				player->m_angEyeAngles().yaw = Math::ClampYaw(Math::CalcAngle(g_LocalPlayer->m_vecOrigin(), player->m_vecOrigin()).yaw + 180);
				break;
			}
		}
		else if (missed_shots[playerindex] < 5 && !using_recorded_angle) //using delta resolvo meme in 2018 is bad, but i don't have proper animation resolver so try delta resolver first then animation brute
		{
			if (record.moving_lby > -1000)		 //we got the magical delta
			{
				float tickdiff, lbydiff, tickdiff2, freestandingyaw;

				if (Has_Static_Real(15, playerindex))
				{
					Global::resolverModes[playerindex] = "Fake: Static LBY Delta";
					player->m_angEyeAngles().yaw = Math::ClampYaw(player->m_flLowerBodyYawTarget() + record.moving_lby_delta);
				}
				else if (Has_Steady_Difference(10, playerindex))
				{
					Global::resolverModes[playerindex] = "Fake: Steady Delta";
					tickdiff = record.tickcount - records[playerindex].at(1).tickcount;
					lbydiff = record.lby - records[playerindex].at(1).lby;
					tickdiff2 = g_GlobalVars->tickcount - record.tickcount;
					player->m_angEyeAngles().yaw = Math::ClampYaw((lbydiff / tickdiff) * tickdiff2);
				}
				else if (Delta_Keeps_Changing(15, playerindex))
				{
					Global::resolverModes[playerindex] = "Fake: Dynamic Delta";
					if (GetDeltaByCompairingTicks(playerindex) > -1000)
						player->m_angEyeAngles().yaw = Math::ClampYaw(player->m_flLowerBodyYawTarget() + GetDeltaByCompairingTicks(playerindex));
					else
					{
						Global::bBaim[playerindex] = !hs_only;
						player->m_angEyeAngles().yaw = ResolveBruteforce(player, player->m_flLowerBodyYawTarget());
					}
				}
				else if (LBY_Keeps_Changing(15, playerindex))
				{
					Global::resolverModes[playerindex] = "Fake: Dynamic LBY";
					if (GetLBYByCompairingTicks(playerindex) > -1000)
						player->m_angEyeAngles().yaw = Math::ClampYaw(GetLBYByCompairingTicks(playerindex));
					else
					{
						Global::bBaim[playerindex] = !hs_only;
						player->m_angEyeAngles().yaw = ResolveBruteforce(player, player->m_flLowerBodyYawTarget());
					}
				}
			}
			else								// we dont have delta!
			{
				Global::bBaim[playerindex] = !hs_only;
				player->m_angEyeAngles().yaw = AnimationResolve(player);
			}
		}
		else if (!using_recorded_angle)
		{
			player->m_angEyeAngles().yaw = AnimationResolve(player);
		}

		/* yaw resolver ends */

		/* pitch resolver starts *//*
		if (!using_recorded_angle)
		{
			player->m_angEyeAngles().pitch = SWResolver_pitch(player);
		}
		/* pitch resolver ends *//*

		records[playerindex].front().resolvedang = player->m_angEyeAngles();
	}
	else								// not choking packet = no fake = no resolving requird except for leap angle fix
	{
		player->m_angEyeAngles().pitch = SWResolver_pitch(player);
		player->m_angEyeAngles().yaw = Math::ClampYaw(networkedPlayerYaw[playerindex]);
		CMBacktracking::Get().SetOverwriteTick(player, player->m_angEyeAngles(), player->m_flSimulationTime(), 2);
		Global::resolverModes[playerindex] = "No Fake";
		return;
	}
	*/
	
	player->m_angEyeAngles().pitch = SWResolver_pitch(player);
	if (missed_shots[playerindex] < 3 || player->m_angEyeAngles().pitch < -50.f)	//thank you valve for ruining all the work i've been putting into resolvers, lby backtrack doesnt even work anymore lmaooooo
	{
		player->m_angEyeAngles().yaw = Math::ClampYaw(networkedPlayerYaw[playerindex]);
		Global::resolverModes[playerindex] = "No Fake";
		return;
	}
	else
	{
		player->m_angEyeAngles().yaw = Math::ClampYaw(networkedPlayerYaw[playerindex] + ((missed_shots[playerindex] % 4) * 19)) * ((missed_shots[playerindex] % 2) * 2 - 1);
		Global::resolverModes[playerindex] = "Bruteforce";
		return;
	}
}

void PredictResolve::log(C_BasePlayer* player)
{

}

float PredictResolve::predict(C_BasePlayer* player)
{
	int index = player->EntIndex();
	auto currecord = record[index];
	
	switch (currecord.CurrentPredictMode)
	{
	case PredictResolveModes::Static:
		return (player->m_flLowerBodyYawTarget() + currecord.DeltaFromPrediction);
	case PredictResolveModes::Spin:
		return predictSpin(player);
	case PredictResolveModes::Flips:
		return predictFlips(player);
	case PredictResolveModes::Freestand:
		return predictFreestand(player) + currecord.DeltaFromPrediction;
	case PredictResolveModes::FuckIt:
		return predictFuckIt(player);
	}
}

float PredictResolve::predictSpin(C_BasePlayer* player)
{
	int index = player->EntIndex();
	auto currecord = record[index];
	float TimeDelta = currecord.LbyPredictedUpdate - currecord.LbyLastUpdate;
	Global::resolverModes[index] = "Predicted Spin";
	return (player->m_flLowerBodyYawTarget() + (currecord.SpinSpeed * TIME_TO_TICKS(TimeDelta)));
}

float PredictResolve::predictFlips(C_BasePlayer* player)
{
	int index = player->EntIndex();
	auto currecord = record[index];

	return 0.f;
}

float PredictResolve::predictFreestand(C_BasePlayer* player)
{
	int index = player->EntIndex();
	auto currecord = record[index];

	if (!g_LocalPlayer || !g_LocalPlayer->IsAlive()) return 0.f;
}

float PredictResolve::predictFuckIt(C_BasePlayer* player)
{
	int index = player->EntIndex();
	auto currecord = record[index];
	int MissedShot = Resolver::Get().missed_shots[index];

	float returnval;

	switch (MissedShot % 5)
	{
	case 0: currecord.DeltaFromPrediction = 0; currecord.SpinSpeed = 10; break;
	case 1: currecord.DeltaFromPrediction = 180.f; currecord.SpinSpeed = 30; break;
	case 2: currecord.DeltaFromPrediction = 90.f; currecord.SpinSpeed = -30; break;
	case 3: currecord.DeltaFromPrediction = -90.f; currecord.SpinSpeed = -10; break;
	case 4: currecord.DeltaFromPrediction = Utils::RandomFloat(-180.f, 180.f); currecord.SpinSpeed = Utils::RandomFloat(-270.f, 270.f);  break;
	}

	record[index] = currecord;

	switch (MissedShot % 4)
	{
	case 0: returnval = (player->m_flLowerBodyYawTarget() + currecord.DeltaFromPrediction); break;
	case 1: returnval = predictSpin(player); break;
	case 2: returnval = predictFlips(player); break;
	case 3: returnval = predictFreestand(player) + currecord.DeltaFromPrediction; break;
	}

	Global::resolverModes[index] = "Fake: Bruteforce";
}

void AimbotBulletImpactEvent::FireGameEvent(IGameEvent *event)
{
	if (!g_LocalPlayer || !event || records.empty())
		return;

	if (!strcmp(event->GetName(), "player_hurt"))
	{
		if (g_EngineClient->GetPlayerForUserID(event->GetInt("attacker")) == g_EngineClient->GetLocalPlayer() &&
			g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) == records.front().target)
		{
			records.front().processed = true;
			records.front().hit = true;
			if (event->GetInt("health") < 1) Resolver::Get().missed_shots[g_EngineClient->GetPlayerForUserID(event->GetInt("userid"))] = 0;
		}
	}

	if (!strcmp(event->GetName(), "bullet_impact"))
	{
		if (g_EngineClient->GetPlayerForUserID(event->GetInt("userid")) == g_EngineClient->GetLocalPlayer())
		{
			records.front().processed = true;
			records.front().impacts.push_back(Vector(event->GetFloat("x"), event->GetFloat("y"), event->GetFloat("z")));
		}
		else
		{
			if (records2.empty() || abs(records2.back().time - g_GlobalVars->curtime) > 0.1f)
				records2.push_back(g_EngineClient->GetPlayerForUserID(event->GetInt("userid")));

			records2.back().impacts.push_back(Vector(event->GetFloat("x"), event->GetFloat("y"), event->GetFloat("z")));
			records2.back().direction = Math::CalcAngle(records2.back().src, records2.back().impacts.front());
			for (auto i = records2.back().impacts.begin(); i != records2.back().impacts.end(); i++)
			{
				records2.back().direction += Math::CalcAngle(records2.back().src, *i);
				records2.back().direction /= 2;
			}
		}
	}
}

int AimbotBulletImpactEvent::GetEventDebugID(void)
{
	return EVENT_DEBUG_ID_INIT;
}

void AimbotBulletImpactEvent::RegisterSelf()
{
	g_GameEvents->AddListener(this, "player_hurt", false);
	g_GameEvents->AddListener(this, "bullet_impact", false);
}

void AimbotBulletImpactEvent::UnregisterSelf()
{
	g_GameEvents->RemoveListener(this);
}

void SetupCapsule(const Vector& vecMin, const Vector& vecMax, float flRadius, int hitgroup, std::vector<CSphere>&m_cSpheres)
{
	auto vecDelta = (vecMax - vecMin);
	Math::NormalizeVector(vecDelta);
	auto vecCenter = vecMin;

	CSphere Sphere = CSphere{ vecMin, flRadius, hitgroup };
	m_cSpheres.push_back(Sphere);

	for (size_t i = 1; i < std::floor(vecMin.DistTo(vecMax)); ++i)
	{
		CSphere Sphere = CSphere{ vecMin + vecDelta * static_cast<float>(i), flRadius, hitgroup };
		m_cSpheres.push_back(CSphere{ Sphere });
	}

	CSphere UsedSphere = CSphere{ vecMin, flRadius, hitgroup };
	m_cSpheres.push_back(UsedSphere);
}

bool IntersectInfiniteRayWithSphere(const Vector &vecRayOrigin, const Vector &vecRayDelta, const Vector &vecSphereCenter, float flRadius, float *pT1, float *pT2)
{
	Vector vecSphereToRay;
	VectorSubtract(vecRayOrigin, vecSphereCenter, vecSphereToRay);

	float a = vecRayDelta.Dot(vecRayDelta);

	// This would occur in the case of a zero-length ray
	if (a == 0.0f) {
		*pT1 = *pT2 = 0.0f;
		return vecSphereToRay.LengthSqr() <= flRadius * flRadius;
	}

	float b = 2 * vecSphereToRay.Dot(vecRayDelta);
	float c = vecSphereToRay.Dot(vecSphereToRay) - flRadius * flRadius;
	float flDiscrim = b * b - 4 * a * c;
	if (flDiscrim < 0.0f)
		return false;

	flDiscrim = sqrt(flDiscrim);
	float oo2a = 0.5f / a;
	*pT1 = (-b - flDiscrim) * oo2a;
	*pT2 = (-b + flDiscrim) * oo2a;
	return true;
}

bool CSphere::intersectsRay(const Ray_t& ray, Vector& vecIntersection)
{
	float T1, T2;
	if (!IntersectInfiniteRayWithSphere(ray.m_Start, ray.m_Delta, m_vecCenter, m_flRadius, &T1, &T2))
		return false;

	if (T1 > 1.0f || T2 < 0.0f)
		return false;

	// Clamp it!
	if (T1 < 0.0f)
		T1 = 0.0f;
	if (T2 > 1.0f)
		T2 = 1.0f;

	vecIntersection = ray.m_Start + ray.m_Delta * T1;

	return true;
}

void AimbotBulletImpactEvent::process()
{
	static LagRecord last_LR[65];

	if (records.empty())
		return;

	for (auto i = records.begin(); i != records.end();)
	{
		auto player = C_BasePlayer::GetPlayerByIndex(i->target);
		if (!i->processed || abs(i->time - g_GlobalVars->curtime) < max(g_GlobalVars->interval_per_tick, g_GlobalVars->frametime) * 2)
		{
			i++;
			continue;
		}

		if (i->hit)
		{
			records.clear();
			break;
		}

		std::string mode = ((last_LR[i->target] == CMBacktracking::Get().current_record[i->target]) ? (Global::resolverModes[i->target]) : (CMBacktracking::Get().current_record[i->target].m_strResolveMode));

		bool trace_hit = false;

		Vector impact_to_use;
		float biggest_lenght = 0.f;

		auto matrix = CMBacktracking::Get().current_record[i->target].matrix;
		if (last_LR[i->target] == CMBacktracking::Get().current_record[i->target])
		{
			player->SetupBonesExperimental(matrix, 128, BONE_USED_BY_HITBOX, g_EngineClient->GetLastTimeStamp());
		}

		std::vector<CSphere> m_cSpheres;

		Ray_t Ray;
		trace_t Trace;

		studiohdr_t *studioHdr = g_MdlInfo->GetStudiomodel(player->GetModel());
		mstudiohitboxset_t *set = studioHdr->pHitboxSet(player->m_nHitboxSet());

		while (!i->impacts.empty())
		{
			if ((i->src - i->impacts.front()).Length() > biggest_lenght)
			{
				impact_to_use = i->impacts.front();
				biggest_lenght = (i->src - i->impacts.front()).Length();
			}
			i->impacts.erase(i->impacts.begin());
		}

		for (int h = 0; h < set->numhitboxes; h++)
		{
			auto hitbox = set->pHitbox(h);
			if (hitbox->m_flRadius != -1.f)
			{
				Vector min, max;

				Math::VectorTransform(hitbox->bbmin, matrix[hitbox->bone], min);
				Math::VectorTransform(hitbox->bbmax, matrix[hitbox->bone], max);

				SetupCapsule(min, max, hitbox->m_flRadius, hitbox->group, m_cSpheres);
			}
		}

		Ray.Init(i->src, impact_to_use);

		Vector intersectpos;
		for (auto& i : m_cSpheres)
		{
			if (i.intersectsRay(Ray, intersectpos))
			{
				trace_hit = true;
				break;
			}
		}

		EventInfo info;
		info.m_flExpTime = g_GlobalVars->curtime + 4.f;

		if (trace_hit)
		{
			Resolver::Get().missed_shots[i->target]++;

			g_CVar->ConsoleColorPrintf(Color(50, 122, 239), "[dankme.mz]");
			g_CVar->ConsoleColorPrintf(Color(255, 0, 0), " Missed shot due to bad resolver. ");
			g_CVar->ConsoleColorPrintf(Color(50, 122, 239), "(%f Damage expacted, used %s)\n", Global::lastdmg[i->target], mode.c_str());

			info.m_szMessage = "Missed shot due to bad resolver. (" + std::to_string(Global::lastdmg[i->target]) + " Damage expacted, used " + mode + ")";
			PlayerHurtEvent::Get().PushEvent(info);
		}
		else
		{
			g_CVar->ConsoleColorPrintf(Color(50, 122, 239), "[dankme.mz]");
			g_CVar->ConsoleColorPrintf(Color(255, 255, 255), " Missed shot due to spread.\n");

			info.m_szMessage = "Missed shot due to spread.";
			PlayerHurtEvent::Get().PushEvent(info);
		}

		last_LR[i->target] = CMBacktracking::Get().current_record[i->target];
		//i = records.erase(i);
		records.clear();
		break;
	}
}