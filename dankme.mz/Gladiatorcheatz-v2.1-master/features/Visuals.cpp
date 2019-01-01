#include "Visuals.hpp"
#include "../Options.hpp"
#include "../Structs.hpp"
#include "../helpers/Math.hpp"
#include "LagCompensation.hpp"

#include "AimRage.hpp"
#include "AntiAim.hpp"
#include "Miscellaneous.hpp"
#include "Resolver.hpp"

#include "../resource.h"

#include "../misc/dlight.h"

#include <comdef.h>

#include "../imgui/imgui.h"

#include "../trusted_list.h"

extern CEEffects				*g_Effects;

namespace Visuals
{
	vgui::HFont weapon_font;
	vgui::HFont ui_font;
	vgui::HFont ui_font_small;
	vgui::HFont watermark_font;
	vgui::HFont spectatorlist_font;
	vgui::HFont indicator_font;
	VisualsStruct ESP_ctx;
	float ESP_Fade[64];
	Vector2D recoil_screenpos;
	C_BasePlayer* local_observed;
	C_BasePlayer *carrier;
}

RECT GetBBox(C_BaseEntity* ent, Vector pointstransf[])
{
	RECT rect{};
	auto collideable = ent->GetCollideable();

	if (!collideable)
		return rect;

	auto min = collideable->OBBMins();
	auto max = collideable->OBBMaxs();

	const matrix3x4_t &trans = ent->m_rgflCoordinateFrame();

	Vector points[] =
	{
		Vector(min.x, min.y, min.z),
		Vector(min.x, max.y, min.z),
		Vector(max.x, max.y, min.z),
		Vector(max.x, min.y, min.z),
		Vector(max.x, max.y, max.z),
		Vector(min.x, max.y, max.z),
		Vector(min.x, min.y, max.z),
		Vector(max.x, min.y, max.z)
	};

	Vector pointsTransformed[8];
	for (int i = 0; i < 8; i++) {
		Math::VectorTransform(points[i], trans, pointsTransformed[i]);
	}

	Vector pos = ent->m_vecOrigin();
	Vector screen_points[8] = {};

	for (int i = 0; i < 8; i++)
		if (!Math::WorldToScreen(pointsTransformed[i], screen_points[i]))
			return rect;
		else
			pointstransf[i] = screen_points[i];

	auto left = screen_points[0].x;
	auto top = screen_points[0].y;
	auto right = screen_points[0].x;
	auto bottom = screen_points[0].y;

	for (int i = 1; i < 8; i++)
	{
		if (left > screen_points[i].x)
			left = screen_points[i].x;
		if (top < screen_points[i].y)
			top = screen_points[i].y;
		if (right < screen_points[i].x)
			right = screen_points[i].x;
		if (bottom > screen_points[i].y)
			bottom = screen_points[i].y;
	}
	return RECT{ (long)left, (long)top, (long)right, (long)bottom };
}

void Visuals::RenderNadeEsp(C_BaseCombatWeapon* nade)
{
	if (!g_Options.esp_grenades)
		return;

	const model_t* model = nade->GetModel();
	if (!model)
		return;

	studiohdr_t* hdr = g_MdlInfo->GetStudiomodel(model);
	if (!hdr)
		return;

	Color Nadecolor;
	std::string entityName = hdr->szName, icon_character;
	switch (nade->GetClientClass()->m_ClassID)
	{
	case 9:
		if (entityName[16] == 's')
		{
			Nadecolor = Color(255, 255, 0, 200);
			entityName = "Flash";
			icon_character = "G";
		}
		else
		{
			Nadecolor = Color(255, 0, 0, 200);
			entityName = "Frag";
			icon_character = "H";
		}
		break;
	case 134:
		Nadecolor = Color(170, 170, 170, 200);
		entityName = "Smoke";
		icon_character = "P";
		break;
	case 98:
		Nadecolor = Color(255, 0, 0, 200);
		entityName = "Fire";
		icon_character = "P";
		break;
	case 41:
		Nadecolor = Color(255, 255, 0, 200);
		entityName = "Decoy";
		icon_character = "G";
		break;
	default:
		return;
	}

	Vector points_transformed[8];
	RECT size = GetBBox(nade, points_transformed);
	if (size.right == 0 || size.bottom == 0)
		return;

	int width, height, width_icon, height_icon;
	Visuals::GetTextSize(ui_font, entityName.c_str(), width, height);
	Visuals::GetTextSize(weapon_font, icon_character.c_str(), width_icon, height_icon);

	// + distance? just make it customizable
	switch (g_Options.esp_grenades_type)
	{
	case 1:
		g_VGuiSurface->DrawSetColor(Color(20, 20, 20, 240));
		g_VGuiSurface->DrawLine(size.left - 1, size.bottom - 1, size.left - 1, size.top + 1);
		g_VGuiSurface->DrawLine(size.right + 1, size.top + 1, size.right + 1, size.bottom - 1);
		g_VGuiSurface->DrawLine(size.left - 1, size.top + 1, size.right + 1, size.top + 1);
		g_VGuiSurface->DrawLine(size.right + 1, size.bottom - 1, size.left + -1, size.bottom - 1);

		g_VGuiSurface->DrawSetColor(Nadecolor);
		g_VGuiSurface->DrawLine(size.left, size.bottom, size.left, size.top);
		g_VGuiSurface->DrawLine(size.left, size.top, size.right, size.top);
		g_VGuiSurface->DrawLine(size.right, size.top, size.right, size.bottom);
		g_VGuiSurface->DrawLine(size.right, size.bottom, size.left, size.bottom);
	case 0:
		DrawString(ui_font, size.left + ((size.right - size.left) * 0.5), size.bottom + (size.top - size.bottom) + height * 0.5f + 2, Nadecolor, FONT_CENTER, entityName.c_str());
		break;
	case 3:
		DrawString(ui_font, size.left + ((size.right - size.left) * 0.5), size.bottom + (size.top - size.bottom) + height_icon * 0.5f + 1, Nadecolor, FONT_CENTER, entityName.c_str());
	case 2:
		DrawString(weapon_font, size.left + ((size.right - size.left) * 0.5), size.bottom + (size.top - size.bottom), Nadecolor, FONT_CENTER, icon_character.c_str());
		break;
	}
}

void Visuals::DrawhealthIcon(int x, int y, C_BasePlayer* player)
{
	Color cross_color = Color(255, 25, 50, 255);

	// If higher health than normal, flash the icon to notify.
	if (player && player->m_iHealth() > 100)
	{
		if (((float)g_GlobalVars->curtime - (int)(g_GlobalVars->curtime)) > 0.5f)
			cross_color = Color(255, 255, 0, 200);
	}

	g_VGuiSurface->DrawSetColor(cross_color); // Cross
	g_VGuiSurface->DrawFilledRect(x + 4, y + 2, x + 6, y + 10);// top-bottom
	g_VGuiSurface->DrawFilledRect(x + 1, y + 5, x + 4, y + 7);// left
	g_VGuiSurface->DrawFilledRect(x + 6, y + 5, x + 9, y + 7);// right

	g_VGuiSurface->DrawSetColor(Color(0, 0, 0, 255)); // Outline
	g_VGuiSurface->DrawLine(x + 3, y + 1, x + 6, y + 1); // top
	g_VGuiSurface->DrawLine(x + 3, y + 1, x + 3, y + 4);
	g_VGuiSurface->DrawLine(x + 6, y + 1, x + 6, y + 4);
	g_VGuiSurface->DrawLine(x + 3, y + 7, x + 3, y + 10);// bottom
	g_VGuiSurface->DrawLine(x + 6, y + 7, x + 6, y + 10);
	g_VGuiSurface->DrawLine(x + 3, y + 10, x + 6, y + 10);
	g_VGuiSurface->DrawLine(x, y + 4, x, y + 7);// left
	g_VGuiSurface->DrawLine(x, y + 4, x + 3, y + 4);
	g_VGuiSurface->DrawLine(x, y + 7, x + 3, y + 7);
	g_VGuiSurface->DrawLine(x + 9, y + 4, x + 9, y + 7);// right
	g_VGuiSurface->DrawLine(x + 6, y + 4, x + 9, y + 4);
	g_VGuiSurface->DrawLine(x + 6, y + 7, x + 9, y + 7);
}

bool Visuals::InitFont()
{
	ui_font = g_VGuiSurface->CreateFont_();
	g_VGuiSurface->SetFontGlyphSet(ui_font, "Smallest Pixel-7", 10, 100, 0, 0, FONTFLAG_OUTLINE);	//g_VGuiSurface->SetFontGlyphSet(ui_font, "Bell Gothic", 10, 450, 0, 0, FONTFLAG_OUTLINE); 	//g_VGuiSurface->SetFontGlyphSet(ui_font, "Courier New", 14, 0, 0, 0, FONTFLAG_OUTLINE); // Styles

	ui_font_small = g_VGuiSurface->CreateFont_();
	g_VGuiSurface->SetFontGlyphSet(ui_font_small, "Bell Gothic", 7, 10, 0, 0, FONTFLAG_OUTLINE);

	watermark_font = g_VGuiSurface->CreateFont_();
	g_VGuiSurface->SetFontGlyphSet(watermark_font, "Verdana", 16, 700, 0, 0, FONTFLAG_OUTLINE | FONTFLAG_DROPSHADOW);

	weapon_font = g_VGuiSurface->CreateFont_();// 0xA1;
	g_VGuiSurface->SetFontGlyphSet(weapon_font, "Counter-Strike", 16, 500, 0, 0, FONTFLAG_OUTLINE | FONTFLAG_ANTIALIAS);

	spectatorlist_font = g_VGuiSurface->CreateFont_();
	g_VGuiSurface->SetFontGlyphSet(spectatorlist_font, "Tahoma", 14, 350, 0, 0, FONTFLAG_OUTLINE);

	indicator_font = g_VGuiSurface->CreateFont_();
	g_VGuiSurface->SetFontGlyphSet(indicator_font, "Arial", 30, 500, 0, 0, FONTFLAG_OUTLINE | FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW);

	return true;
}

bool Visuals::IsVisibleScan(C_BasePlayer *player)
{
	matrix3x4_t boneMatrix[MAXSTUDIOBONES];
	Vector eyePos = g_LocalPlayer->GetEyePos();

	CGameTrace tr;
	Ray_t ray;
	CTraceFilter filter;
	filter.pSkip = g_LocalPlayer;

	if (!player->SetupBonesExperimental(boneMatrix, MAXSTUDIOBONES, BONE_USED_BY_HITBOX, 0.0f))
	{
		return false;
	}

	auto studio_model = g_MdlInfo->GetStudiomodel(player->GetModel());
	if (!studio_model)
	{
		return false;
	}

	int scan_hitboxes[] = {
		HITBOX_HEAD,
		HITBOX_LEFT_FOREARM,
		HITBOX_LEFT_UPPER_ARM,
		HITBOX_LEFT_FOOT,
		HITBOX_RIGHT_FOOT,
		HITBOX_LEFT_CALF,
		HITBOX_RIGHT_CALF,
		HITBOX_CHEST,
		HITBOX_STOMACH
	};
	
	for (int i = 0; i < ARRAYSIZE(scan_hitboxes); i++)
	{
		auto hitbox = studio_model->pHitboxSet(player->m_nHitboxSet())->pHitbox(scan_hitboxes[i]);
		if (!checks::is_bad_ptr(hitbox))
		{
			auto
				min = Vector{},
				max = Vector{};

			Math::VectorTransform(hitbox->bbmin, boneMatrix[hitbox->bone], min);
			Math::VectorTransform(hitbox->bbmax, boneMatrix[hitbox->bone], max);

			ray.Init(eyePos, (min + max) * 0.5);
			g_EngineTrace->TraceRay(ray, MASK_SHOT | CONTENTS_GRATE, &filter, &tr);

			if (tr.hit_entity == player || tr.fraction > 0.97f)
				return true;
		}
	}
	return false;
}

bool Visuals::ValidPlayer(C_BasePlayer *player, bool count_step)
{
	int idx = player->EntIndex();
	constexpr float frequency = 0.35f / 0.5f;
	float step = frequency * g_GlobalVars->frametime;
	if (!player->IsAlive())
		return false;

	// Don't render esp if in firstperson viewing player.
	if (player == local_observed)
	{
		if (g_LocalPlayer->m_iObserverMode() == 4)
			return false;
	}

	if (player == g_LocalPlayer)
	{
		if (!g_Input->m_fCameraInThirdPerson)
			return false;
	}

	if (count_step)
	{
		if (!player->IsDormant()) {
			if (ESP_Fade[idx] < 1.f)
				ESP_Fade[idx] += step;
		}
		else {
			if (ESP_Fade[idx] > 0.f)
				ESP_Fade[idx] -= step;
		}
		ESP_Fade[idx] = (ESP_Fade[idx] > 1.f ? 1.f : ESP_Fade[idx] < 0.f ? 0.f : ESP_Fade[idx]);
	}

	return (ESP_Fade[idx] > 0.f);
}

bool Visuals::Begin(C_BasePlayer *player)
{
	ESP_ctx.player = player;
	ESP_ctx.bEnemy = g_LocalPlayer->m_iTeamNum() != player->m_iTeamNum();
	ESP_ctx.isVisible = Visuals::IsVisibleScan(player);
	local_observed = (C_BasePlayer*)g_EntityList->GetClientEntityFromHandle(g_LocalPlayer->m_hObserverTarget());

	int idx = player->EntIndex();
	bool playerTeam = player->m_iTeamNum() == 2;

	if (!ESP_ctx.bEnemy && g_Options.esp_enemies_only)
		return false;

	if (!player->m_bGunGameImmunity())
	{
		if (ESP_ctx.isVisible)
		{
			if (player == g_LocalPlayer) {
				ESP_ctx.clr_fill.SetColor(g_Options.esp_player_fill_color_local_visiable);
				ESP_ctx.clr.SetColor(g_Options.esp_player_bbox_color_local_visiable);
			}
			else {
				ESP_ctx.clr_fill.SetColor(playerTeam ? g_Options.esp_player_fill_color_t_visible : g_Options.esp_player_fill_color_ct_visible);
				ESP_ctx.clr.SetColor(playerTeam ? g_Options.esp_player_bbox_color_t_visible : g_Options.esp_player_bbox_color_ct_visible);
			}
		}
		else
		{
			if (player == g_LocalPlayer) {
				ESP_ctx.clr_fill.SetColor(g_Options.esp_player_fill_color_local);
				ESP_ctx.clr.SetColor(g_Options.esp_player_bbox_color_local);
			}
			else {
				ESP_ctx.clr_fill.SetColor(playerTeam ? g_Options.esp_player_fill_color_t : g_Options.esp_player_fill_color_ct);
				ESP_ctx.clr.SetColor(playerTeam ? g_Options.esp_player_bbox_color_t : g_Options.esp_player_bbox_color_ct);
			}
			ESP_ctx.clr.SetAlpha(255);
		}
		ESP_ctx.clr.SetAlpha(ESP_ctx.clr.a() * ESP_Fade[idx]);
		ESP_ctx.clr_fill.SetAlpha(g_Options.esp_fill_amount * ESP_Fade[idx]);
		ESP_ctx.clr_text = Color(245, 245, 245, (int)(ESP_ctx.clr.a() * ESP_Fade[idx]));
	}
	else
	{
		// Set all colors to grey if immune.
		ESP_ctx.clr.SetColor(166, 169, 174, (int)(225 * ESP_Fade[idx]));
		ESP_ctx.clr_text.SetColor(166, 169, 174, (int)(225 * ESP_Fade[idx]));
		ESP_ctx.clr_fill.SetColor(166, 169, 174, (int)(g_Options.esp_fill_amount * ESP_Fade[idx]));
	}

	// Do some touch ups if local player and scoped
	if (player == g_LocalPlayer && g_LocalPlayer->m_bIsScoped())
	{
		ESP_ctx.clr.SetAlpha(ESP_ctx.clr.a() * 0.1f);
		ESP_ctx.clr_text.SetAlpha(ESP_ctx.clr_text.a() * 0.1f);
		ESP_ctx.clr_fill.SetAlpha(ESP_ctx.clr_fill.a() * 0.1f);
	}

	Vector head = player->GetAbsOrigin() + Vector(0, 0, player->GetCollideable()->OBBMaxs().z);
	Vector origin = player->GetAbsOrigin();
	origin.z -= 5;

	if (g_Options.visuals_others_dlight)
	{
		int temp1 = ESP_ctx.clr.r(), temp2 = ESP_ctx.clr.g(), temp3 = ESP_ctx.clr.b();
		dlight_t* dLight = g_Effects->CL_AllocDlight(player->EntIndex());
		dLight->key = player->EntIndex();
		dLight->color.r = (unsigned char)temp1;
		dLight->color.g = (unsigned char)temp2;
		dLight->color.b = (unsigned char)temp3;
		dLight->color.exponent = true;
		dLight->flags = DLIGHT_NO_MODEL_ILLUMINATION;
		dLight->m_Direction = player->m_vecOrigin();
		dLight->origin = player->m_vecOrigin();

		if (player->IsWeapon()) dLight->radius = 250.f;
		else if (player->IsPlayer()) dLight->radius = 750.f;
		else dLight->radius = 500.f;
		
		dLight->die = g_GlobalVars->curtime + 0.5f;
		dLight->decay = 20.0f;
	}

	if (!Math::WorldToScreen(head, ESP_ctx.head_pos) ||
		!Math::WorldToScreen(origin, ESP_ctx.feet_pos))
		return false;

	auto h = fabs(ESP_ctx.head_pos.y - ESP_ctx.feet_pos.y);
	auto w = h / 1.65f;

	switch (g_Options.esp_player_boundstype)
	{
	case 0:
		ESP_ctx.bbox.left = static_cast<long>(ESP_ctx.feet_pos.x - w * 0.5f);
		ESP_ctx.bbox.right = static_cast<long>(ESP_ctx.bbox.left + w);
		ESP_ctx.bbox.bottom = (ESP_ctx.feet_pos.y > ESP_ctx.head_pos.y ? static_cast<long>(ESP_ctx.feet_pos.y) : static_cast<long>(ESP_ctx.head_pos.y));
		ESP_ctx.bbox.top = (ESP_ctx.feet_pos.y > ESP_ctx.head_pos.y ? static_cast<long>(ESP_ctx.head_pos.y) : static_cast<long>(ESP_ctx.feet_pos.y));
		break;
	case 1:
	{
		Vector points_transformed[8];
		RECT BBox = GetBBox(player, points_transformed);
		ESP_ctx.bbox = BBox;
		ESP_ctx.bbox.top = BBox.bottom;
		ESP_ctx.bbox.bottom = BBox.top;
		break;
	}
	}

	if (ESP_ctx.player == carrier)
	{
		DrawString(ui_font, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + 5, Color(255, 255, 255, 255), FONT_LEFT, "Bomb Carrier");
	}

	return true;
}

void Visuals::RenderFill()
{
	g_VGuiSurface->DrawSetColor(ESP_ctx.clr_fill);
	g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.left + 2, ESP_ctx.bbox.top + 2, ESP_ctx.bbox.right - 2, ESP_ctx.bbox.bottom - 2);
}

void Visuals::RenderBox()
{
	float
		length_horizontal = (ESP_ctx.bbox.right - ESP_ctx.bbox.left) * 0.2f,
		length_vertical = (ESP_ctx.bbox.bottom - ESP_ctx.bbox.top) * 0.2f;

	Color col_black = Color(0, 0, 0, (int)(255.f * ESP_Fade[ESP_ctx.player->EntIndex()]));
	switch (g_Options.esp_player_boxtype)
	{
	case 0:
		break;

	case 1:
		g_VGuiSurface->DrawSetColor(ESP_ctx.clr);
		g_VGuiSurface->DrawOutlinedRect(ESP_ctx.bbox.left, ESP_ctx.bbox.top, ESP_ctx.bbox.right, ESP_ctx.bbox.bottom);
		g_VGuiSurface->DrawSetColor(col_black);
		g_VGuiSurface->DrawOutlinedRect(ESP_ctx.bbox.left - 1, ESP_ctx.bbox.top - 1, ESP_ctx.bbox.right + 1, ESP_ctx.bbox.bottom + 1);
		g_VGuiSurface->DrawOutlinedRect(ESP_ctx.bbox.left + 1, ESP_ctx.bbox.top + 1, ESP_ctx.bbox.right - 1, ESP_ctx.bbox.bottom - 1);
		break;

	case 2:
		g_VGuiSurface->DrawSetColor(col_black);
		g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.left - 1, ESP_ctx.bbox.top - 1, ESP_ctx.bbox.left + 1 + length_horizontal, ESP_ctx.bbox.top + 2);
		g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.right - 1 - length_horizontal, ESP_ctx.bbox.top - 1, ESP_ctx.bbox.right + 1, ESP_ctx.bbox.top + 2);
		g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.left - 1, ESP_ctx.bbox.bottom - 2, ESP_ctx.bbox.left + 1 + length_horizontal, ESP_ctx.bbox.bottom + 1);
		g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.right - 1 - length_horizontal, ESP_ctx.bbox.bottom - 2, ESP_ctx.bbox.right + 1, ESP_ctx.bbox.bottom + 1);

		g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.left - 1, ESP_ctx.bbox.top + 2, ESP_ctx.bbox.left + 2, ESP_ctx.bbox.top + 1 + length_vertical);
		g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.right - 2, ESP_ctx.bbox.top + 2, ESP_ctx.bbox.right + 1, ESP_ctx.bbox.top + 1 + length_vertical);
		g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.left - 1, ESP_ctx.bbox.bottom - 1 - length_vertical, ESP_ctx.bbox.left + 2, ESP_ctx.bbox.bottom - 2);
		g_VGuiSurface->DrawFilledRect(ESP_ctx.bbox.right - 2, ESP_ctx.bbox.bottom - 1 - length_vertical, ESP_ctx.bbox.right + 1, ESP_ctx.bbox.bottom - 2);

		g_VGuiSurface->DrawSetColor(ESP_ctx.clr);
		g_VGuiSurface->DrawLine(ESP_ctx.bbox.left, ESP_ctx.bbox.top, ESP_ctx.bbox.left + length_horizontal - 1, ESP_ctx.bbox.top);
		g_VGuiSurface->DrawLine(ESP_ctx.bbox.right - length_horizontal, ESP_ctx.bbox.top, ESP_ctx.bbox.right - 1, ESP_ctx.bbox.top);
		g_VGuiSurface->DrawLine(ESP_ctx.bbox.left, ESP_ctx.bbox.bottom - 1, ESP_ctx.bbox.left + length_horizontal - 1, ESP_ctx.bbox.bottom - 1);
		g_VGuiSurface->DrawLine(ESP_ctx.bbox.right - length_horizontal, ESP_ctx.bbox.bottom - 1, ESP_ctx.bbox.right - 1, ESP_ctx.bbox.bottom - 1);

		g_VGuiSurface->DrawLine(ESP_ctx.bbox.left, ESP_ctx.bbox.top, ESP_ctx.bbox.left, ESP_ctx.bbox.top + length_vertical - 1);
		g_VGuiSurface->DrawLine(ESP_ctx.bbox.right - 1, ESP_ctx.bbox.top, ESP_ctx.bbox.right - 1, ESP_ctx.bbox.top + length_vertical - 1);
		g_VGuiSurface->DrawLine(ESP_ctx.bbox.left, ESP_ctx.bbox.bottom - length_vertical, ESP_ctx.bbox.left, ESP_ctx.bbox.bottom - 1);
		g_VGuiSurface->DrawLine(ESP_ctx.bbox.right - 1, ESP_ctx.bbox.bottom - length_vertical, ESP_ctx.bbox.right - 1, ESP_ctx.bbox.bottom - 1);
		break;
	}
}

void Visuals::RenderName()
{
	wchar_t buf[128];
	std::string name = ESP_ctx.player->GetName(),
		s_name = (name.length() > 0 ? name : "##ERROR_empty_name");

	if (MultiByteToWideChar(CP_UTF8, 0, s_name.c_str(), -1, buf, 128) > 0)
	{
		int tw, th;
		g_VGuiSurface->GetTextSize(ui_font, buf, tw, th);

		g_VGuiSurface->DrawSetTextFont(ui_font);
		g_VGuiSurface->DrawSetTextColor(ESP_ctx.clr_text);
		g_VGuiSurface->DrawSetTextPos(ESP_ctx.bbox.left + (ESP_ctx.bbox.right - ESP_ctx.bbox.left) * 0.5 - tw * 0.5, ESP_ctx.bbox.top - th + 1);
		g_VGuiSurface->DrawPrintText(buf, wcslen(buf));
	}
}

void Visuals::DrawRecoilCrosshair(QAngle punch)
{
	QAngle ViewAngles; g_EngineClient->GetViewAngles(ViewAngles);
	static ConVar* weapon_recoil_scale = g_CVar->FindVar("weapon_recoil_scale");

	if (checks::is_bad_ptr(weapon_recoil_scale))
	{
		weapon_recoil_scale = g_CVar->FindVar("weapon_recoil_scale");
		ViewAngles += punch * 2.f;
	}
	else
	{
		ViewAngles += punch * weapon_recoil_scale->GetFloat();
	}
	

	Vector fowardVec;
	Math::AngleVectors(ViewAngles, fowardVec);
	fowardVec *= 10000;

	Vector start = g_LocalPlayer->GetEyePos();
	Vector end, endScreen;
	
	Ray_t ray; ray.Init(start, start + fowardVec);
	CGameTrace trace;
	CTraceFilter filter; filter.pSkip = g_LocalPlayer;
	g_EngineTrace->TraceRay(ray, MASK_SHOT, &filter, &trace);

	end = trace.endpos;

	Color crosshair;
	static ConVar *r = g_CVar->FindVar("cl_crosshaircolor_r"), *g = g_CVar->FindVar("cl_crosshaircolor_g"), *b = g_CVar->FindVar("cl_crosshaircolor_b");
	if (checks::is_bad_ptr(r) || checks::is_bad_ptr(g) || checks::is_bad_ptr(b))
	{
		r = g_CVar->FindVar("cl_crosshaircolor_r"), g = g_CVar->FindVar("cl_crosshaircolor_g"), b = g_CVar->FindVar("cl_crosshaircolor_b");
		crosshair = Color(1.f, 1.f, 1.f);
	}
	else
	{
		crosshair = Color(r->GetInt(), g->GetInt(), b->GetInt());
	}
	if (Math::WorldToScreen(end, endScreen))
	{
		g_VGuiSurface->DrawSetColor(crosshair);
		g_VGuiSurface->DrawLine(endScreen.x - 5, endScreen.y, endScreen.x + 5, endScreen.y);
		g_VGuiSurface->DrawLine(endScreen.x, endScreen.y - 5, endScreen.x, endScreen.y + 5);

		recoil_screenpos = Vector2D(endScreen.x, endScreen.y);
	}
}

void draw_gay_circle(uint8_t* p, int resolution, float alpha_power = 1.f)
{
	const auto max_radius = resolution / 2;
	const auto max_relative_alpha = pow(max_radius, alpha_power);
	int screen_w, screen_h;
	g_EngineClient->GetScreenSize(screen_w, screen_h);
	for (auto i = 0; i < resolution; ++i)
	{
		for (auto j = 0; j < resolution; ++j)
		{
			const auto cy = i - max_radius;
			const auto cx = j - max_radius;
			const auto radius = std::sqrt(cx * cx + cy * cy);
			p[i * resolution + j] = {
				//0x00u, 0x00u, 0x00u,
				static_cast<unsigned char>(radius > max_radius ? 0x00u : uint8_t(pow(radius, alpha_power) / max_relative_alpha * 0xFF))
			};
		}
	}
}

void Visuals::DrawSpreadCrosshair()
{
	C_BaseCombatWeapon *weapon = g_LocalPlayer->m_hActiveWeapon();
	if (checks::is_bad_ptr(weapon) || !g_LocalPlayer->IsAlive() || weapon->IsMiscellaneousWeapon())
		return;

	int screen_w, screen_h;
	g_EngineClient->GetScreenSize(screen_w, screen_h);

	float accuracy = weapon->GetInaccuracy() * 550.f;

	static ConVar *r = g_CVar->FindVar("cl_crosshaircolor_r"), *g = g_CVar->FindVar("cl_crosshaircolor_g"), *b = g_CVar->FindVar("cl_crosshaircolor_b");
	static int rc = 255, gc = 255, bc = 255;
	if (checks::is_bad_ptr(r) || checks::is_bad_ptr(g) || checks::is_bad_ptr(b))
	{
		r = g_CVar->FindVar("cl_crosshaircolor_r"), g = g_CVar->FindVar("cl_crosshaircolor_g"), b = g_CVar->FindVar("cl_crosshaircolor_b");
	}
	else
	{
		rc = r->GetInt(), gc = g->GetInt(), bc = b->GetInt();
	}

	if (g_Options.removals_novisualrecoil)
	{
		DrawFilledCircle(recoil_screenpos, Color(rc / 4, gc / 4, bc / 4, 128), accuracy, 60);
		g_VGuiSurface->DrawSetColor(Color(rc, gc, bc));
		g_VGuiSurface->DrawOutlinedCircle(recoil_screenpos.x, recoil_screenpos.y, accuracy, 60);
	}
	else
	{
		DrawFilledCircle(Vector2D(screen_w / 2, screen_h / 2), Color(rc / 4, gc / 4, bc / 4, 128), accuracy, 60);
		g_VGuiSurface->DrawSetColor(Color(rc, gc, bc));
		g_VGuiSurface->DrawOutlinedCircle(screen_w / 2, screen_h / 2, accuracy, 60);
	}
}

void Visuals::DrawAngleLines()
{
	{
		int screen_w, screen_h;
		QAngle eyeangles;
		g_EngineClient->GetScreenSize(screen_w, screen_h); g_EngineClient->GetViewAngles(eyeangles);

		DrawFilledCircle(Vector2D(screen_w / 2, screen_h / 7), Color(130, 130, 130, 185), 50, 60);
		g_VGuiSurface->DrawSetColor(Color::Black); g_VGuiSurface->DrawOutlinedCircle(screen_w / 2, screen_h / 7, 50, 60);

		Vector outvec;
		QAngle ang = QAngle(0, Global::realyaw, 0);
		Math::AngleVectors(QAngle(0, 270, 0) - ang + QAngle(0, eyeangles.yaw, 0), outvec);
		Vector targetvec = Vector(screen_w / 2, screen_h / 7, 0) + (outvec * 30.f);
		g_VGuiSurface->DrawSetColor(Color::Blue);
		g_VGuiSurface->DrawLine(screen_w / 2, screen_h / 7, targetvec.x, targetvec.y);
		//DrawString(ui_font, true, targetvec.x, targetvec.y, Color::Blue, "Real");

		ang = QAngle(0, Global::fakeyaw, 0);
		Math::AngleVectors(QAngle(0, 270, 0) - ang + QAngle(0, eyeangles.yaw, 0), outvec);
		targetvec = Vector(screen_w / 2, screen_h / 7, 0) + (outvec * 30.f);
		g_VGuiSurface->DrawSetColor(Color::Orange);
		g_VGuiSurface->DrawLine(screen_w / 2, screen_h / 7, targetvec.x, targetvec.y);
		//DrawString(ui_font, true, targetvec.x, targetvec.y, Color::Orange, "Fake");

		ang = QAngle(0, g_LocalPlayer->m_flLowerBodyYawTarget(), 0);
		Math::AngleVectors(QAngle(0, 270, 0) - ang + QAngle(0, eyeangles.yaw, 0), outvec);
		targetvec = Vector(screen_w / 2, screen_h / 7, 0) + (outvec * 30.f);
		g_VGuiSurface->DrawSetColor(Color::Green);
		g_VGuiSurface->DrawLine(screen_w / 2, screen_h / 7, targetvec.x, targetvec.y);
		//DrawString(ui_font, true, targetvec.x, targetvec.y, Color::Green, "LBY");

		DrawFilledCircle(Vector2D(screen_w / 2, screen_h / 7), Color(255, 255, 255), 2, 60);
		g_VGuiSurface->DrawSetColor(Color::Black); g_VGuiSurface->DrawOutlinedCircle(screen_w / 2, screen_h / 7, 2, 60);
	}

	if (g_Options.misc_thirdperson)
	{
		Vector src3D, dst3D, forward, down, src, dst;
		trace_t tr;
		Ray_t ray;
		CTraceFilter filter;

		filter.pSkip = g_LocalPlayer;
		Math::AngleVectors(QAngle(0, g_LocalPlayer->m_flLowerBodyYawTarget(), 0), forward);
		Math::AngleVectors(QAngle(90.f, 0, 0), down);
		ray.Init(g_LocalPlayer->m_vecOrigin(), g_LocalPlayer->m_vecOrigin() + (down * 4096));
		g_EngineTrace->TraceRay(ray, CONTENTS_SOLID, &filter, &tr);

		src3D = tr.endpos;
		dst3D = src3D + (forward * 50.f);

		ray.Init(src3D, dst3D);
		g_EngineTrace->TraceRay(ray, 0, &filter, &tr);

		if (Math::WorldToScreen(src3D, src) && Math::WorldToScreen(tr.endpos, dst))
		{
			g_VGuiSurface->DrawSetColor(Color::Green);
			g_VGuiSurface->DrawLine(src.x, src.y, dst.x, dst.y);
			DrawString(ui_font, true, dst.x, dst.y, Color::Green, "LBY");
		}

		Math::AngleVectors(QAngle(0, Global::fakeyaw, 0), forward);
		dst3D = src3D + (forward * 50.f);

		ray.Init(src3D, dst3D);
		g_EngineTrace->TraceRay(ray, 0, &filter, &tr);

		if (Math::WorldToScreen(src3D, src) && Math::WorldToScreen(tr.endpos, dst))
		{
			g_VGuiSurface->DrawSetColor(Color::Orange);
			g_VGuiSurface->DrawLine(src.x, src.y, dst.x, dst.y);
			DrawString(ui_font, true, dst.x, dst.y, Color::Orange, "Fake");
		}

		Math::AngleVectors(QAngle(0, Global::realyaw, 0), forward);
		dst3D = src3D + (forward * 50.f);

		ray.Init(src3D, dst3D);
		g_EngineTrace->TraceRay(ray, 0, &filter, &tr);

		if (Math::WorldToScreen(src3D, src) && Math::WorldToScreen(tr.endpos, dst))
		{
			g_VGuiSurface->DrawSetColor(Color::Blue);
			g_VGuiSurface->DrawLine(src.x, src.y, dst.x, dst.y);
			DrawString(ui_font, true, dst.x, dst.y, Color::Blue, "Real");
		}

		src3D = g_LocalPlayer->GetEyePos();

		Math::AngleVectors(QAngle(Global::realpitch, Global::realyaw, 0), forward);
		dst3D = src3D + (forward * 25.f);

		ray.Init(src3D, dst3D);
		g_EngineTrace->TraceRay(ray, 0, &filter, &tr);

		if (Math::WorldToScreen(src3D, src) && Math::WorldToScreen(tr.endpos, dst))
		{
			g_VGuiSurface->DrawSetColor(Color::Blue);
			g_VGuiSurface->DrawLine(src.x, src.y, dst.x, dst.y);
		}

		Math::AngleVectors(QAngle(Global::fakepitch, Global::fakeyaw, 0), forward);
		dst3D = src3D + (forward * 25.f);

		ray.Init(src3D, dst3D);
		g_EngineTrace->TraceRay(ray, 0, &filter, &tr);

		if (Math::WorldToScreen(src3D, src) && Math::WorldToScreen(tr.endpos, dst))
		{
			g_VGuiSurface->DrawSetColor(Color::Orange);
			g_VGuiSurface->DrawLine(src.x, src.y, dst.x, dst.y);
		}
	}
}

void Visuals::DrawWatermark()
{
	long curTime;
	if (Global::use_ud_vmt)
		curTime = g_GlobalVars->realtime * 1000;	//idk why we were using chrono in the first place but i'm sticking with that
	else
		curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	float hue = curTime / 100;

	ImVec4 temp = ImColor::HSV(hue / 360.f, 1, 1);
	Color color = Color(temp.x, temp.y, temp.z);

	if (g_Options.visuals_others_watermark)
	{
		DrawString(watermark_font, 10, 10, color, FONT_LEFT, "dankme.mz");
		DrawString(watermark_font, 10, 23, Color::White, FONT_LEFT, "welcome back, %s %s", user_type_to_string(hwids.at(Global::iHwidIndex).second.second).c_str(), hwids.at(Global::iHwidIndex).second.first.c_str());
	}
	else
	{
		DrawString(watermark_font, 10, 10, Color::White, FONT_LEFT, "welcome back, %s %s", user_type_to_string(hwids.at(Global::iHwidIndex).second.second).c_str(), hwids.at(Global::iHwidIndex).second.first.c_str());
	}
}

void Visuals::DrawWeaponRange()
{
	Vector prev_scr_pos, scr_pos, prev_full_scr_pos, full_scr_pos;
	C_BaseCombatWeapon *local_weapon = g_LocalPlayer->m_hActiveWeapon();
	if (checks::is_bad_ptr(local_weapon) || local_weapon->m_iItemDefinitionIndex() != WEAPON_TASER)
		return;

	float step = M_PI * 2.0 / 511;
	float rad = 178.f;
	Vector origin = g_LocalPlayer->GetEyePos();
	static float hue_offset = 0;
	for (float rotation = 0; rotation <= (M_PI * 2.0) + step; rotation += step)
	{
		Vector pos(rad * cos(rotation) + origin.x, rad * sin(rotation) + origin.y, origin.z);
		
		float hue = RAD2DEG(rotation) + hue_offset;
		ImVec4 temp = ImColor::HSV(hue / 360.f, 1, 1);
		Color color = Color(temp.x, temp.y, temp.z);

		g_VGuiSurface->DrawSetColor(color);

		Ray_t ray;
		trace_t trace;
		CTraceFilterWorldAndPropsOnly filter;

		ray.Init(origin, pos);

		g_EngineTrace->TraceRay(ray, CONTENTS_SOLID | CONTENTS_WINDOW | CONTENTS_MOVEABLE, &filter, &trace);

		if (Math::WorldToScreen(pos, full_scr_pos))
		{
			if (trace.fraction < 0.999 && prev_full_scr_pos.IsValid())
			{
				g_VGuiSurface->DrawLine(prev_full_scr_pos.x, prev_full_scr_pos.y, full_scr_pos.x, full_scr_pos.y);
			}
			prev_full_scr_pos = full_scr_pos;
		}
		if (Math::WorldToScreen(trace.endpos, scr_pos))
		{
			if (prev_scr_pos.IsValid())
			{
				g_VGuiSurface->DrawLine(prev_scr_pos.x, prev_scr_pos.y, scr_pos.x, scr_pos.y);
			}
			prev_scr_pos = scr_pos;
		}
	}
	hue_offset += 0.25;
}

void Visuals::DrawResolverModes()
{
	if (Resolver::Get().records[ESP_ctx.player->EntIndex()].empty()) return;
	ESP_ctx.resolver_offset = (ESP_ctx.player == carrier) ? 15 : 5;
	if (ESP_ctx.bEnemy || g_Options.rage_friendlyfire)// on enemies only
	{
		if (ESP_ctx.bbox.top + ESP_ctx.resolver_offset + 30 > ESP_ctx.bbox.bottom)
		{
			ESP_ctx.resolver_offset = (ESP_ctx.player == carrier) ? 13 : 3;

			DrawString(ui_font_small, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 255, 255, 255), FONT_LEFT, (Global::resolverModes[ESP_ctx.player->EntIndex()]).c_str());
			ESP_ctx.resolver_offset += 7;
			DrawString(ui_font_small, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 255, 255, 255), FONT_LEFT, "D: %f", Resolver::Get().records[ESP_ctx.player->EntIndex()].front().moving_lby_delta);
			ESP_ctx.resolver_offset += 7;
			DrawString(ui_font_small, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 255, 255, 255), FONT_LEFT, "U: %f", 1.1f - (ESP_ctx.player->m_flSimulationTime() - Resolver::Get().records[ESP_ctx.player->EntIndex()].front().last_update_simtime));
			ESP_ctx.resolver_offset += 7;
			DrawString(ui_font_small, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 255, 255, 255), FONT_LEFT, "S: %d", Resolver::Get().missed_shots[ESP_ctx.player->EntIndex()]);

			for (auto i = CMBacktracking::Get().m_LagRecord[ESP_ctx.player->EntIndex()].begin(); i != CMBacktracking::Get().m_LagRecord[ESP_ctx.player->EntIndex()].end(); i++)
			{
				if (CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(i->m_flSimulationTime)))
				{
					if (i->m_strResolveMode == "No Fake" || i->m_strResolveMode == "LBY Update" || i->m_strResolveMode == "Shot")
					{
						ESP_ctx.resolver_offset += 7;
						DrawString(ui_font_small, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(0, 255, 0, 255), FONT_LEFT, "Backtrackable");
						break;
					}
				}
			}

			if (Global::bBaim[ESP_ctx.player->EntIndex()])
			{
				ESP_ctx.resolver_offset += 7;
				DrawString(ui_font_small, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 0, 0, 255), FONT_LEFT, "baim");
			}
			if (Resolver::Get().records[ESP_ctx.player->EntIndex()].front().suppresing_animation)
			{
				ESP_ctx.resolver_offset += 7;
				DrawString(ui_font_small, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 0, 0, 255), FONT_LEFT, "Animation Suppresion");
			}
		}
		else
		{
			DrawString(ui_font, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 255, 255, 255), FONT_LEFT, (Global::resolverModes[ESP_ctx.player->EntIndex()]).c_str());
			ESP_ctx.resolver_offset += 10;
			DrawString(ui_font, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 255, 255, 255), FONT_LEFT, "D: %f", Resolver::Get().records[ESP_ctx.player->EntIndex()].front().moving_lby_delta);
			ESP_ctx.resolver_offset += 10;
			DrawString(ui_font, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 255, 255, 255), FONT_LEFT, "U: %f", 1.11f - (ESP_ctx.player->m_flSimulationTime() - Resolver::Get().records[ESP_ctx.player->EntIndex()].front().last_update_simtime));
			ESP_ctx.resolver_offset += 10;
			DrawString(ui_font, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 255, 255, 255), FONT_LEFT, "S: %d", Resolver::Get().missed_shots[ESP_ctx.player->EntIndex()]);

			for (auto i = CMBacktracking::Get().m_LagRecord[ESP_ctx.player->EntIndex()].begin(); i != CMBacktracking::Get().m_LagRecord[ESP_ctx.player->EntIndex()].end(); i++)
			{
				if (CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(i->m_flSimulationTime)))
				{
					if (i->m_strResolveMode == "No Fake" || i->m_strResolveMode == "LBY Update")
					{
						ESP_ctx.resolver_offset += 10;
						DrawString(ui_font, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(0, 255, 0, 255), FONT_LEFT, "Backtrackable");
						break;
					}
				}
			}

			if (Global::bBaim[ESP_ctx.player->EntIndex()])
			{
				ESP_ctx.resolver_offset += 10;
				DrawString(ui_font, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 0, 0, 255), FONT_LEFT, "baim");
			}
			if (Resolver::Get().records[ESP_ctx.player->EntIndex()].front().suppresing_animation)
			{
				ESP_ctx.resolver_offset += 10;
				DrawString(ui_font, ESP_ctx.bbox.right + 5, ESP_ctx.bbox.top + ESP_ctx.resolver_offset, Color(255, 0, 0, 255), FONT_LEFT, "Animation Suppresion");
			}
		}
	}
}

void Visuals::DrawCapsuleOverlay(int idx, float duration)
{
	if (idx == g_EngineClient->GetLocalPlayer())
		return;

	auto player = C_BasePlayer::GetPlayerByIndex(idx);
	if (!player)
		return;

	if ((g_Options.rage_lagcompensation || g_Options.legit_backtrack) && CMBacktracking::Get().m_LagRecord[player->EntIndex()].empty())
		return;

	matrix3x4_t boneMatrix_actual[MAXSTUDIOBONES];
	if (!player->SetupBonesExperimental(boneMatrix_actual, MAXSTUDIOBONES, BONE_USED_BY_HITBOX, g_EngineClient->GetLastTimeStamp()))
		return;

	studiohdr_t *studioHdr = g_MdlInfo->GetStudiomodel(player->GetModel());
	if (studioHdr)
	{
		matrix3x4_t boneMatrix[MAXSTUDIOBONES];
		std::memcpy(boneMatrix, CMBacktracking::Get().current_record[player->EntIndex()].matrix, sizeof(CMBacktracking::Get().current_record[player->EntIndex()].matrix));

		//matrix3x4_t ForwardboneMatrix[MAXSTUDIOBONES];
		//std::memcpy(ForwardboneMatrix, CMBacktracking::Get().current_forwardtracking_record[player->EntIndex()].matrix, sizeof(CMBacktracking::Get().current_forwardtracking_record[player->EntIndex()].matrix));

		mstudiohitboxset_t *set = studioHdr->pHitboxSet(player->m_nHitboxSet());
		if (set)
		{
			for (int i = 0; i < set->numhitboxes; i++)
			{
				mstudiobbox_t *hitbox = set->pHitbox(i);
				if (hitbox)
				{
					if (hitbox->m_flRadius == -1.0f)
					{
						Vector position, position_actual, position_forward;
						QAngle angles, angles_actual, angles_forward;
						MatrixAngles(boneMatrix[hitbox->bone], angles, position);
						MatrixAngles(boneMatrix_actual[hitbox->bone], angles_actual, position_actual);
						//MatrixAngles(ForwardboneMatrix[hitbox->bone], angles_forward, position_forward);

						g_DebugOverlay->AddBoxOverlay(position, hitbox->bbmin, hitbox->bbmax, angles, 255, 0, 0, 150, duration);
						g_DebugOverlay->AddBoxOverlay(position_forward, hitbox->bbmin, hitbox->bbmax, angles_forward, 255, 255, 0, 150, duration);

						if (g_Options.esp_lagcompensated_hitboxes_type == 1)
							g_DebugOverlay->AddBoxOverlay(position_actual, hitbox->bbmin, hitbox->bbmax, angles_actual, 0, 0, 255, 150, duration);
					}
					else
					{
						Vector min, max,
							min_actual, max_actual, min_forward, max_forward;

						Math::VectorTransform(hitbox->bbmin, boneMatrix[hitbox->bone], min);
						Math::VectorTransform(hitbox->bbmax, boneMatrix[hitbox->bone], max);

						//Math::VectorTransform(hitbox->bbmin, ForwardboneMatrix[hitbox->bone], min_forward);
						//Math::VectorTransform(hitbox->bbmax, ForwardboneMatrix[hitbox->bone], max_forward);

						Math::VectorTransform(hitbox->bbmin, boneMatrix_actual[hitbox->bone], min_actual);
						Math::VectorTransform(hitbox->bbmax, boneMatrix_actual[hitbox->bone], max_actual);

						g_DebugOverlay->AddCapsuleOverlay(min, max, hitbox->m_flRadius, 255, 0, 0, 150, duration);
						g_DebugOverlay->AddCapsuleOverlay(min_forward, max_forward, hitbox->m_flRadius, 255, 255, 0, 150, duration);

						if (g_Options.esp_lagcompensated_hitboxes_type == 1)
							g_DebugOverlay->AddCapsuleOverlay(min_actual, max_actual, hitbox->m_flRadius, 0, 0, 255, 150, duration);
					}
				}
			}
		}
	}
}

void Visuals::RenderHealth()
{
	int health = ESP_ctx.player->m_iHealth();
	if (health > 100)
		health = 100;

	float box_h = (float)fabs(ESP_ctx.bbox.bottom - ESP_ctx.bbox.top);
	float off = 8;

	auto height = box_h - (((box_h * health) / 100));

	int x = ESP_ctx.bbox.left - off;
	int y = ESP_ctx.bbox.top;
	int w = 4;
	int h = box_h;

	DrawhealthIcon(x - 12, y, ESP_ctx.player);

	Color col_black = Color(0, 0, 0, (int)(255.f * ESP_Fade[ESP_ctx.player->EntIndex()]));
	g_VGuiSurface->DrawSetColor(col_black);
	g_VGuiSurface->DrawFilledRect(x, y, x + w, y + h);

	g_VGuiSurface->DrawSetColor(Color((255 - health * int(2.55f)), (health * int(2.55f)), 0, (int)(180.f * ESP_Fade[ESP_ctx.player->EntIndex()])));
	g_VGuiSurface->DrawFilledRect(x + 1, y + height + 1, x + w - 1, y + h - 1);
}

void Visuals::RenderWeapon()
{
	wchar_t buf[80];
	auto clean_item_name = [](const char *name) -> const char*
	{
		if (name[0] == 'C')
			name++;

		auto start = strstr(name, "Weapon");
		if (start != nullptr)
			name = start + 6;

		return name;
	};

	auto weapon = ESP_ctx.player->m_hActiveWeapon().Get();

	if (!weapon) return;

	if (weapon->m_hOwnerEntity().IsValid())
	{
		auto name = clean_item_name(weapon->GetClientClass()->m_pNetworkName);

		int iWeaponID = weapon->m_iItemDefinitionIndex();
		if (MultiByteToWideChar(CP_UTF8, 0, name, -1, buf, 80) > 0)
		{
			int tw, th;
			g_VGuiSurface->GetTextSize(ui_font, buf, tw, th);

			g_VGuiSurface->DrawSetTextFont(ui_font);
			g_VGuiSurface->DrawSetTextColor(ESP_ctx.clr_text);
			g_VGuiSurface->DrawSetTextPos(ESP_ctx.bbox.left + (ESP_ctx.bbox.right - ESP_ctx.bbox.left) * 0.5 - tw * 0.5, ESP_ctx.bbox.bottom + 1);
			g_VGuiSurface->DrawPrintText(buf, wcslen(buf));
		}
	}
}

void Visuals::RenderSnapline()
{
	int width, height;
	g_EngineClient->GetScreenSize(width, height);

	int screen_x = width * 0.5f,
		screen_y = height,
		target_x = ESP_ctx.bbox.left + (ESP_ctx.bbox.right - ESP_ctx.bbox.left) * 0.5,
		target_y = ESP_ctx.bbox.bottom,
		max_length = height * 0.3f;

	if (target_x == 0 ||
		target_y == 0)
		return;

	float length = sqrt(pow(target_x - screen_x, 2) + pow(target_y - screen_y, 2));
	if (length > max_length)
	{
		float
			x_normalized = (target_x - screen_x) / length,
			y_normalized = (target_y - screen_y) / length;
		target_x = screen_x + x_normalized * max_length;
		target_y = screen_y + y_normalized * max_length;
		DrawCircle(target_x + x_normalized * 3.5f, target_y + y_normalized * 3.5f, 3.5f, 12, ESP_ctx.clr);
	}

	g_VGuiSurface->DrawSetColor(ESP_ctx.clr);
	g_VGuiSurface->DrawLine(screen_x, screen_y, target_x, target_y);
}

void Visuals::RenderOffscreenESP()
{
	for (int i = 1; i < g_EngineClient->GetMaxClients(); i++)
	{
		auto player = C_BasePlayer::GetPlayerByIndex(i);

		if (!player || !player->IsPlayer() || player->IsDormant() || !player->IsAlive() || (g_Options.esp_enemies_only && player->m_iTeamNum() == g_LocalPlayer->m_iTeamNum()))
			continue;

		Vector poopvec;
		QAngle eyeangles;
		int screen_w, screen_h;

		g_EngineClient->GetScreenSize(screen_w, screen_h);
		g_EngineClient->GetViewAngles(eyeangles);
		QAngle newangle = Math::CalcAngle(Vector(g_LocalPlayer->m_vecOrigin().x, g_LocalPlayer->m_vecOrigin().y, 0), Vector(player->m_vecOrigin().x, player->m_vecOrigin().y, 0));
		Math::AngleVectors(QAngle(0, 270, 0) - newangle + QAngle(0, eyeangles.yaw, 0), poopvec);
		auto circlevec = Vector(screen_w / 2, screen_h / 2, 0) + (poopvec * ((screen_h / 2) - (screen_h / 10)));
		DrawTriangle(circlevec.x, circlevec.y, 10.f, 12.5f, 270 - newangle.yaw + eyeangles.yaw, ESP_ctx.clr);
	}
}

void Visuals::RenderSkelet()
{
	studiohdr_t *studioHdr = g_MdlInfo->GetStudiomodel(ESP_ctx.player->GetModel());
	if (studioHdr)
	{
		static matrix3x4_t boneToWorldOut[128];
		if (ESP_ctx.player->SetupBonesExperimental(boneToWorldOut, MAXSTUDIOBONES, BONE_USED_BY_HITBOX, g_GlobalVars->curtime))
		//if(ESP_ctx.player->HandleBoneSetup(BONE_USED_BY_HITBOX, boneToWorldOut))
		{
			for (int i = 0; i < studioHdr->numbones; i++)
			{
				mstudiobone_t *bone = studioHdr->pBone(i);
				if (!bone || !(bone->flags & BONE_USED_BY_HITBOX) || bone->parent == -1)
					continue;

				Vector bonePos1;
				if (!Math::WorldToScreen(Vector(boneToWorldOut[i][0][3], boneToWorldOut[i][1][3], boneToWorldOut[i][2][3]), bonePos1))
					continue;

				Vector bonePos2;
				if (!Math::WorldToScreen(Vector(boneToWorldOut[bone->parent][0][3], boneToWorldOut[bone->parent][1][3], boneToWorldOut[bone->parent][2][3]), bonePos2))
					continue;

				g_VGuiSurface->DrawSetColor(ESP_ctx.clr);
				g_VGuiSurface->DrawLine((int)bonePos1.x, (int)bonePos1.y, (int)bonePos2.x, (int)bonePos2.y);
			}
		}
	}
}

void Visuals::RenderLBYLC() {
	RECT scrn = GetViewport();
	static Vector lastpos = Vector(1, 1, 1);
	static Vector curpos = Vector(1, 1, 1);
	static float lcbreak_dest = 4096;

	int offset = 0;

	if (Global::fps < 1.1 / g_GlobalVars->interval_per_tick)
	{
		g_VGuiSurface->DrawSetTextColor((Global::fps < 1 / g_GlobalVars->interval_per_tick) ? (Color::Red) : (Color::Orange));
		const wchar_t *buf4 = L"FPS";
		g_VGuiSurface->DrawSetTextFont(indicator_font);
		g_VGuiSurface->DrawSetTextPos(scrn.left + 10, scrn.bottom - 50 + offset);
		g_VGuiSurface->DrawPrintText(buf4, wcslen(buf4));
		offset -= 30;
	}

	auto nci = g_EngineClient->GetNetChannelInfo();
	static auto sv_maxunlag = g_CVar->FindVar("sv_maxunlag");
	if (!checks::is_bad_ptr(nci) && !checks::is_bad_ptr(sv_maxunlag))
	{
		if (nci->GetLatency(FLOW_INCOMING) + nci->GetLatency(FLOW_OUTGOING) > sv_maxunlag->GetFloat() * 0.75)
		{
			g_VGuiSurface->DrawSetTextColor((nci->GetLatency(FLOW_INCOMING) + nci->GetLatency(FLOW_OUTGOING) > sv_maxunlag->GetFloat()) ? (Color::Red) : (Color::Orange));
			const wchar_t *buf4 = L"PING";
			g_VGuiSurface->DrawSetTextFont(indicator_font);
			g_VGuiSurface->DrawSetTextPos(scrn.left + 10, scrn.bottom - 50 + offset);
			g_VGuiSurface->DrawPrintText(buf4, wcslen(buf4));
			offset -= 30;
		}
	}

	if (AntiAim::Get().IsFreestanding())
	{
		g_VGuiSurface->DrawSetTextColor(Color::Green);	//we're freestanding (duh)

		QAngle viewangle; g_EngineClient->GetViewAngles(viewangle);
		int dir = 0;
		for (; dir < 8; dir++)
		{
			if (abs(Math::ClampYaw(AntiAim::Get().GetFreestandingYaw() - Math::ClampYaw(viewangle.yaw + 45 * dir))) < 22.5)
				break;
		}
		std::wstring buf3;
		switch (dir)
		{
		case 0:
			buf3 = L" ¡è ";
			break;
		case 1:
			buf3 = L"¢Ø";
			break;
		case 2:
			buf3 = L"¡ç";
			break;
		case 3:
			buf3 = L"¢×";
			break;
		case 4:
			buf3 = L" ¡é ";
			break;
		case 5:
			buf3 = L"¢Ù";
			break;
		case 6:
			buf3 = L"¡æ";
			break;
		case 7:
			buf3 = L"¢Ö";
			break;
		}
		buf3 += L" Freestanding";
		g_VGuiSurface->DrawSetTextFont(indicator_font);
		g_VGuiSurface->DrawSetTextPos(scrn.left + 10, scrn.bottom - 50 + offset);
		g_VGuiSurface->DrawPrintText(buf3.c_str(), buf3.length());
		offset -= 30;
	}

	Vector tempvec = curpos - lastpos;
	if (g_ClientState->chokedcommands - Global::prevChoked > 2)
	{
		const wchar_t *buf2 = L"LC";

		if (pow(tempvec.Length2D(), 2) > lcbreak_dest)
		{
			g_VGuiSurface->DrawSetTextColor(Color::Green);	//we are breaking lc
		}
		else
		{
			g_VGuiSurface->DrawSetTextColor(Color::Red);
		}

		g_VGuiSurface->DrawSetTextFont(indicator_font);
		g_VGuiSurface->DrawSetTextPos(scrn.left + 10, scrn.bottom - 50 + offset);
		g_VGuiSurface->DrawPrintText(buf2, wcslen(buf2));
		offset -= 30;
	}

	const wchar_t *buf = L"LBY";
	float delta = std::fabsf(Math::ClampYaw(g_LocalPlayer->m_flLowerBodyYawTarget() - Global::realyaw));	//we're clampping this cause reasons
	if (delta > 35 && !((g_LocalPlayer->m_fFlags() & FL_ONGROUND) && (g_LocalPlayer->m_vecVelocity().Length2D() >= 0.1f)))
	{
		if (g_GlobalVars->curtime - Global::LastLBYUpdate > 0.2f)
			g_VGuiSurface->DrawSetTextColor(Color::Green);		//we are breaking lby and can't get backtracked
		else
			g_VGuiSurface->DrawSetTextColor(Color::Orange);		//we are breaking lby but can get backtracked
	}
	else
	{
		g_VGuiSurface->DrawSetTextColor(Color::Red);
	}

	g_VGuiSurface->DrawSetTextFont(indicator_font);
	g_VGuiSurface->DrawSetTextPos(scrn.left + 10, scrn.bottom - 50 + offset);
	g_VGuiSurface->DrawPrintText(buf, wcslen(buf));
	int w, h; GetTextSize(indicator_font, "LBY", w, h);

	offset -= 30;

	float lby_ratio = (Global::NextLBYUpdate - (AimRage::Get().GetTickbase() * g_GlobalVars->interval_per_tick)) / 1.1, lc_ratio = (pow(tempvec.Length2D(), 2) / lcbreak_dest);
	tempvec = g_LocalPlayer->m_vecOrigin() - curpos; float lc_ratio2 = (pow(tempvec.Length2D(), 2) / lcbreak_dest);
	g_VGuiSurface->DrawSetColor(Color(25, 25, 25));
	g_VGuiSurface->DrawOutlinedRect(scrn.left, scrn.bottom - 20, scrn.left + (scrn.right / 6), scrn.bottom);
	g_VGuiSurface->DrawSetColor(Color(50, 50, 50));
	g_VGuiSurface->DrawFilledRect(scrn.left + 1, scrn.bottom - 19, scrn.left + (scrn.right / 6) - 1, scrn.bottom - 1);

	ImVec4 temp = ImColor::HSV(lby_ratio * 0.333, 1, 1);
	Color color = Color(temp.x, temp.y, temp.z);
	g_VGuiSurface->DrawSetColor(color);
	g_VGuiSurface->DrawFilledRect(scrn.left + 2, scrn.bottom - 18, scrn.left + ((scrn.right / 6) * lby_ratio) - 2, scrn.bottom - 12);

	temp = ImColor::HSV(min(lc_ratio, 1) * 0.333, 1, 1);
	color = Color(temp.x, temp.y, temp.z , 0.5f);
	g_VGuiSurface->DrawSetColor(color);
	g_VGuiSurface->DrawFilledRect(scrn.left + 2, scrn.bottom - 8, (scrn.left + ((scrn.right / 6) * min(lc_ratio, 1))) - 2, scrn.bottom - 2);

	temp = ImColor::HSV(min(lc_ratio2, 1) * 0.333, 1, 1);
	color = Color(temp.x, temp.y, temp.z, 0.5f);
	g_VGuiSurface->DrawSetColor(color);
	g_VGuiSurface->DrawFilledRect(scrn.left + 2, scrn.bottom - 8, (scrn.left + ((scrn.right / 6) * min(lc_ratio2, 1))) - 2, scrn.bottom - 2);


	if (!Miscellaneous::Get().GetChocked())
	{
		lastpos = curpos;
		curpos = g_LocalPlayer->m_vecOrigin();
	}
}

void Visuals::RenderBacktrackedSkelet()
{
	if (!g_Options.rage_lagcompensation && !g_Options.legit_backtrack)
		return;

	auto records = &CMBacktracking::Get().m_LagRecord[ESP_ctx.player->EntIndex()];
	if (records->size() < 2)
		return;

	Vector previous_screenpos;
	for (auto record = records->begin(); record != records->end(); record++)
	{
		if (!CMBacktracking::Get().IsTickValid(TIME_TO_TICKS(record->m_flSimulationTime)))
			continue;

		Vector screen_pos;
		if (!Math::WorldToScreen(record->m_vecHeadSpot, screen_pos))
			continue;

		if (previous_screenpos.IsValid())
		{
			if (*record == CMBacktracking::Get().m_RestoreLagRecord[ESP_ctx.player->EntIndex()].first)
				g_VGuiSurface->DrawSetColor(Color(255, 255, 0, 255));
			else
				g_VGuiSurface->DrawSetColor(Color(255, 255, 255, 255));
			g_VGuiSurface->DrawLine(screen_pos.x, screen_pos.y, previous_screenpos.x, previous_screenpos.y);
		}

		previous_screenpos = screen_pos;
	}
}

void Visuals::RenderWeapon(C_BaseCombatWeapon *entity)
{
	wchar_t buf[80];
	auto clean_item_name = [](const char *name) -> const char*
	{
		if (name[0] == 'C')
			name++;

		auto start = strstr(name, "Weapon");
		if (start != nullptr)
			name = start + 6;

		return name;
	};

	if (entity->m_hOwnerEntity().IsValid() ||
		entity->m_vecOrigin() == Vector(0, 0, 0))
		return;

	Vector pointsTransformed[8];
	auto bbox = GetBBox(entity, pointsTransformed);
	if (bbox.right == 0 || bbox.bottom == 0)
		return;

	g_VGuiSurface->DrawSetColor(Color::Green);
	switch (g_Options.esp_dropped_weapons)
	{
	case 2:
		g_VGuiSurface->DrawLine(bbox.left, bbox.top, bbox.right, bbox.top);
		g_VGuiSurface->DrawLine(bbox.left, bbox.bottom, bbox.right, bbox.bottom);
		g_VGuiSurface->DrawLine(bbox.left, bbox.top, bbox.left, bbox.bottom);
		g_VGuiSurface->DrawLine(bbox.right, bbox.top, bbox.right, bbox.bottom);
		break;
	case 3:
		g_VGuiSurface->DrawLine(pointsTransformed[0].x, pointsTransformed[0].y, pointsTransformed[1].x, pointsTransformed[1].y);
		g_VGuiSurface->DrawLine(pointsTransformed[0].x, pointsTransformed[0].y, pointsTransformed[6].x, pointsTransformed[6].y);
		g_VGuiSurface->DrawLine(pointsTransformed[1].x, pointsTransformed[1].y, pointsTransformed[5].x, pointsTransformed[5].y);
		g_VGuiSurface->DrawLine(pointsTransformed[6].x, pointsTransformed[6].y, pointsTransformed[5].x, pointsTransformed[5].y);

		g_VGuiSurface->DrawLine(pointsTransformed[2].x, pointsTransformed[2].y, pointsTransformed[1].x, pointsTransformed[1].y);
		g_VGuiSurface->DrawLine(pointsTransformed[4].x, pointsTransformed[4].y, pointsTransformed[5].x, pointsTransformed[5].y);
		g_VGuiSurface->DrawLine(pointsTransformed[6].x, pointsTransformed[6].y, pointsTransformed[7].x, pointsTransformed[7].y);
		g_VGuiSurface->DrawLine(pointsTransformed[3].x, pointsTransformed[3].y, pointsTransformed[0].x, pointsTransformed[0].y);

		g_VGuiSurface->DrawLine(pointsTransformed[3].x, pointsTransformed[3].y, pointsTransformed[2].x, pointsTransformed[2].y);
		g_VGuiSurface->DrawLine(pointsTransformed[2].x, pointsTransformed[2].y, pointsTransformed[4].x, pointsTransformed[4].y);
		g_VGuiSurface->DrawLine(pointsTransformed[7].x, pointsTransformed[7].y, pointsTransformed[4].x, pointsTransformed[4].y);
		g_VGuiSurface->DrawLine(pointsTransformed[7].x, pointsTransformed[7].y, pointsTransformed[3].x, pointsTransformed[3].y);
		break;
	}

	auto name = clean_item_name(entity->GetClientClass()->m_pNetworkName);
	if (MultiByteToWideChar(CP_UTF8, 0, name, -1, buf, 80) > 0)
	{
		int w = bbox.right - bbox.left;
		int tw, th;
		g_VGuiSurface->GetTextSize(ui_font, buf, tw, th);

		g_VGuiSurface->DrawSetTextFont(ui_font);
		g_VGuiSurface->DrawSetTextColor(Color::Green);
		g_VGuiSurface->DrawSetTextPos(bbox.left + ((bbox.right - bbox.left) / 2) - (tw / 2), bbox.top + 1);
		g_VGuiSurface->DrawPrintText(buf, wcslen(buf));
	}

	if (g_Options.visuals_others_dlight)
	{
		dlight_t* dLight = g_Effects->CL_AllocDlight(entity->EntIndex());
		dLight->key = entity->EntIndex();
		dLight->color.r = (unsigned char) 70;
		dLight->color.g = (unsigned char) 255;
		dLight->color.b = (unsigned char) 70;
		dLight->color.exponent = true;
		dLight->flags = DLIGHT_NO_MODEL_ILLUMINATION;
		dLight->m_Direction = entity->GetAbsOrigin();
		dLight->origin = entity->GetAbsOrigin();
		dLight->radius = 500.f;
		dLight->die = g_GlobalVars->curtime + 0.1f;
		dLight->decay = 20.0f;
	}
}

void Visuals::RenderPlantedC4(C_BaseEntity *entity)
{
	Vector screen_points[8];
	auto bbox = GetBBox(entity, screen_points);

	if (bbox.right == 0 || bbox.bottom == 0)
		return;

	g_VGuiSurface->DrawSetColor(Color::Red);
	g_VGuiSurface->DrawLine(bbox.left, bbox.bottom, bbox.left, bbox.top);
	g_VGuiSurface->DrawLine(bbox.left, bbox.top, bbox.right, bbox.top);
	g_VGuiSurface->DrawLine(bbox.right, bbox.top, bbox.right, bbox.bottom);
	g_VGuiSurface->DrawLine(bbox.right, bbox.bottom, bbox.left, bbox.bottom);

	const wchar_t *buf = L"C4";

	int w = bbox.right - bbox.left;
	int tw, th;
	g_VGuiSurface->GetTextSize(ui_font, buf, tw, th);

	g_VGuiSurface->DrawSetTextFont(ui_font);
	g_VGuiSurface->DrawSetTextColor(Color::Red);
	g_VGuiSurface->DrawSetTextPos((bbox.left + w * 0.5f) - tw * 0.5f, bbox.bottom + 1);
	g_VGuiSurface->DrawPrintText(buf, wcslen(buf));
}

float bomb_Armor(float flDamage, int ArmorValue)
{
	float flArmorRatio = 0.5f;
	float flArmorBonus = 0.5f;
	if (ArmorValue > 0) {
		float flNew = flDamage * flArmorRatio;
		float flArmor = (flDamage - flNew) * flArmorBonus;

		if (flArmor > static_cast<float>(ArmorValue)) {
			flArmor = static_cast<float>(ArmorValue) * (1.f / flArmorBonus);
			flNew = flDamage - flArmor;
		}

		flDamage = flNew;
	}
	return flDamage;
}

void Visuals::RenderC4()
{
	Vector screen_points[8];

	C_BasePlayer* entity = nullptr;
	for (int i = 66; i < g_EntityList->GetHighestEntityIndex(); i++)
	{
		auto temp = C_BasePlayer::GetPlayerByIndex(i);
		if (!temp)
			continue;
		if (temp->GetClientClass()->m_ClassID == ClassId::ClassId_CC4 || temp->GetClientClass()->m_ClassID == ClassId::ClassId_CPlantedC4)
			entity = temp;
	}
	if (entity == nullptr || !entity || (entity->GetClientClass()->m_ClassID != ClassId::ClassId_CC4 && entity->GetClientClass()->m_ClassID != ClassId::ClassId_CPlantedC4))
		return;

	C_BaseCombatWeapon *weapon = (C_BaseCombatWeapon*)entity;
	CBaseHandle parent = weapon->m_hOwnerEntity();
	if ((parent.IsValid() || (entity->GetAbsOrigin().x == 0 && entity->GetAbsOrigin().y == 0 && entity->GetAbsOrigin().z == 0)) && entity->GetClientClass()->m_ClassID == ClassId::ClassId_CC4)
	{
		auto parentent = (C_BasePlayer*)g_EntityList->GetClientEntityFromHandle(parent);
		if (parentent && parentent->IsAlive())
			carrier = parentent;
	}
	else
	{
		carrier = nullptr;

		auto bbox = GetBBox(entity, screen_points);

		int w, tw, th;
		if (bbox.right != 0 && bbox.bottom != 0)
		{

			g_VGuiSurface->DrawSetColor(Color::Red);
			g_VGuiSurface->DrawLine(bbox.left, bbox.bottom, bbox.left, bbox.top);
			g_VGuiSurface->DrawLine(bbox.left, bbox.top, bbox.right, bbox.top);
			g_VGuiSurface->DrawLine(bbox.right, bbox.top, bbox.right, bbox.bottom);
			g_VGuiSurface->DrawLine(bbox.right, bbox.bottom, bbox.left, bbox.bottom);

			const wchar_t *buf = L"C4";

			w = bbox.right - bbox.left;
			g_VGuiSurface->GetTextSize(ui_font, buf, tw, th);

			g_VGuiSurface->DrawSetTextFont(ui_font);
			g_VGuiSurface->DrawSetTextColor(Color::Red);
			g_VGuiSurface->DrawSetTextPos((bbox.left + w * 0.5f) - tw * 0.5f, bbox.bottom + 1);
			g_VGuiSurface->DrawPrintText(buf, wcslen(buf));
		}

		if (g_Options.visuals_others_dlight)
		{
			dlight_t* dLight = g_Effects->CL_AllocDlight(entity->EntIndex());
			dLight->key = entity->EntIndex();
			dLight->color.r = (unsigned char) 255;
			dLight->color.g = (unsigned char) 0;
			dLight->color.b = (unsigned char) 0;
			dLight->color.exponent = true;
			dLight->flags = DLIGHT_NO_MODEL_ILLUMINATION;
			dLight->m_Direction = entity->GetAbsOrigin();
			dLight->origin = entity->GetAbsOrigin();
			dLight->radius = 500.f;
			dLight->die = g_GlobalVars->curtime + 0.1f;
			dLight->decay = 20.0f;
		}

		if (entity->GetClientClass()->m_ClassID == ClassId::ClassId_CPlantedC4)
		{
			C_CSBomb* bomb = (C_CSBomb*)entity;
			float flBlow = bomb->m_flC4Blow();
			float TimeRemaining = flBlow - ((g_LocalPlayer->IsAlive()) ? (TICKS_TO_TIME(AimRage::Get().GetTickbase())) : (g_GlobalVars->curtime));
			float DefuseTime = bomb->m_flDefuseCountDown() - ((g_LocalPlayer->IsAlive()) ? (TICKS_TO_TIME(AimRage::Get().GetTickbase())) : (g_GlobalVars->curtime));
			std::string temp;  _bstr_t output;

			float flDistance = g_LocalPlayer->m_vecOrigin().DistTo(entity->m_vecOrigin()), a = 450.7f, b = 75.68f, c = 789.2f, d = ((flDistance - b) / c);
			float flDamage = a * exp(-d * d);

			int damage = max(floorf(bomb_Armor(flDamage, g_LocalPlayer->m_ArmorValue())), 0);

			int scrw, scrh; g_EngineClient->GetScreenSize(scrw, scrh);

			bool local_is_t = g_LocalPlayer->m_iTeamNum() == 2;

			Vector boxpos1 = Vector(scrw / 3, scrh - (scrh / 7), 0), boxpos2 = boxpos1; boxpos2.x *= 2; boxpos2.y -= scrh / 112;

			float lenght = boxpos2.x - boxpos1.x;

			float c4time = g_CVar->FindVar("mp_c4timer")->GetFloat();
			float timepercent = TimeRemaining / c4time;
			float defusepercent = DefuseTime / 10;
			C_BasePlayer *defuser = bomb->m_hBombDefuser();

			g_VGuiSurface->DrawSetColor(Color(40, 40, 40));
			g_VGuiSurface->DrawFilledRect(boxpos1.x, boxpos2.y, boxpos2.x, boxpos1.y);
			g_VGuiSurface->DrawSetColor(Color(10, 10, 10));
			g_VGuiSurface->DrawFilledRect(boxpos1.x + 1, boxpos2.y + 1, boxpos2.x - 1, boxpos1.y - 1);
			
			int offset = 0;

			if (checks::is_bad_ptr(defuser))
			{
				g_VGuiSurface->DrawSetColor(Color(255, 170, 0));
				g_VGuiSurface->DrawFilledRect(boxpos1.x + 2, boxpos2.y + 2, (boxpos1.x + lenght * timepercent) - 2, boxpos1.y - 2);
			}
			else
			{
				g_VGuiSurface->DrawSetColor(Color(255, 170, 0));
				g_VGuiSurface->DrawFilledRect(boxpos1.x + 2, boxpos2.y + 2, (boxpos1.x + lenght * timepercent) - 2, boxpos1.y - 2);

				g_VGuiSurface->DrawSetColor(Color(0, 170, 255));
				g_VGuiSurface->DrawFilledRect(boxpos1.x + 2, boxpos2.y + 2, (boxpos1.x + (lenght * (10 / c4time)) * defusepercent) - 2, boxpos1.y - 2);

				g_VGuiSurface->DrawSetColor(Color(0, 110, 255));
				g_VGuiSurface->DrawFilledRect(boxpos1.x + 2, boxpos2.y + 2, (boxpos1.x + min(((lenght * (10 / c4time)) * defusepercent), lenght * timepercent) - 2), boxpos1.y - 2);

				if ((bomb->m_flDefuseCountDown() - ((g_LocalPlayer->IsAlive()) ? (TICKS_TO_TIME(AimRage::Get().GetTickbase())) : (g_GlobalVars->curtime))) < TimeRemaining || !local_is_t)
				offset = 10;
			}

			g_VGuiSurface->DrawSetColor(Color(255, 255, 255, 128));
			g_VGuiSurface->DrawLine((boxpos1.x + lenght * (10 / c4time)), boxpos2.y + 3, (boxpos1.x + lenght * (10 / c4time)), boxpos1.y - 3);
			g_VGuiSurface->DrawLine((boxpos1.x + lenght * (5 / c4time)), boxpos2.y + 3, (boxpos1.x + lenght * (5 / c4time)), boxpos1.y - 3);

			g_VGuiSurface->DrawSetTextFont(ui_font);
			g_VGuiSurface->DrawSetTextColor(Color(255, 255, 255));

			temp = (char)(bomb->m_nBombSite() + 0x41); // bombsite netvar is 0 when a, 1 when b. ascii 0x41 = A, 0x42 = B
			temp += " Site";
			output = temp.c_str();
			g_VGuiSurface->DrawSetTextPos(boxpos1.x, boxpos1.y - 40 - offset);
			g_VGuiSurface->DrawPrintText(output, wcslen(output));

			temp = "Bomb Timer: ";
			temp += std::to_string(TimeRemaining);
			if (!checks::is_bad_ptr(defuser))
			{
				temp += "  Defuse Timer: ";
				temp += std::to_string(DefuseTime);
			}
			output = temp.c_str();
			g_VGuiSurface->DrawSetTextPos(boxpos1.x, boxpos1.y - 30 - offset);
			g_VGuiSurface->DrawPrintText(output, wcslen(output));

			temp = "Bomb Damage: ";
			temp += std::to_string(damage);
			if (damage >= g_LocalPlayer->m_iHealth())
			{
				temp += " (Lethal)"; g_VGuiSurface->DrawSetTextColor(Color(255, 0, 0));
			}
			else g_VGuiSurface->DrawSetTextColor(Color(0, 255, 0));

			output = temp.c_str();
			g_VGuiSurface->DrawSetTextPos(boxpos1.x, boxpos1.y - 20 - offset);
			g_VGuiSurface->DrawPrintText(output, wcslen(output));

			if (!checks::is_bad_ptr(defuser))
			{
				g_VGuiSurface->DrawSetTextPos(boxpos1.x, boxpos1.y - 20);
				if ((bomb->m_flDefuseCountDown() - ((g_LocalPlayer->IsAlive()) ? (TICKS_TO_TIME(AimRage::Get().GetTickbase())) : (g_GlobalVars->curtime))) < TimeRemaining)
				{
					temp = (local_is_t) ? ("Getting Defused!") : ("Defusable");
					g_VGuiSurface->DrawSetTextColor((local_is_t) ? (Color(255 , 0 , 0 )) : (Color(0, 255, 0)));
				}
				else if (!local_is_t)
				{
					temp = "Defusable";
					g_VGuiSurface->DrawSetTextColor(Color(255, 0, 0));
				}

				output = temp.c_str();
				g_VGuiSurface->DrawPrintText(output, wcslen(output));
			}
		}
	}
}

void Visuals::RenderSpectatorList()
{
	RECT scrn = GetViewport();
	int cnt = 0;
	static int longest_lenght = 0;

	for (int i = 1; i <= g_EntityList->GetHighestEntityIndex(); i++)
	{
		C_BasePlayer *player = C_BasePlayer::GetPlayerByIndex(i);

		if (!player || player == nullptr)
			continue;

		player_info_t player_info;
		if (player != g_LocalPlayer)
		{
			if (g_EngineClient->GetPlayerInfo(i, &player_info) && !player->IsAlive() && !player->IsDormant())
			{
				auto observer_target = player->m_hObserverTarget();
				if (!observer_target)
					continue;

				auto target = observer_target.Get();
				if (!target)
					continue;

				player_info_t player_info2;
				if (g_EngineClient->GetPlayerInfo(target->EntIndex(), &player_info2))
				{
					char player_name[255] = { 0 };
					sprintf_s(player_name, "%s => %s", player_info.szName, player_info2.szName);

					int w, h;
					GetTextSize(spectatorlist_font, player_name, w, h);

					if (longest_lenght < w)
						longest_lenght = w;

					g_VGuiSurface->DrawSetColor(Color(31, 44, 54, 140));
					DrawString(spectatorlist_font, false, scrn.right - w - 4, (scrn.bottom / 2) + (16 * cnt), target->EntIndex() == g_LocalPlayer->EntIndex() ? Color(240, 70, 80, 255) : Color(255, 255, 255, 255), player_name);
					++cnt;
				}
			}
		}
	}
	if (cnt)
	{
		DrawOutlinedRect(scrn.right - (longest_lenght + 1), (scrn.bottom / 2) - 1, longest_lenght + 2, (16 * cnt) + 2, Color(9, 82, 128, 255));
		DrawOutlinedRect(scrn.right - longest_lenght, (scrn.bottom / 2), longest_lenght, (16 * cnt), Color(90, 90, 90, 160));
	}
	longest_lenght = 0;
}

std::string get_hitbox_name(int hitbox)
{
	switch (hitbox)
	{
	case HITBOX_HEAD:
		return "Head";
	case HITBOX_CHEST:
	case HITBOX_LOWER_CHEST:
	case HITBOX_UPPER_CHEST:
		return "Body";
	case HITBOX_STOMACH:
	case HITBOX_PELVIS:
		return "Stomach";
	case HITBOX_LEFT_FOOT:
	case HITBOX_RIGHT_FOOT:
		return "Toe";
	case HITBOX_LEFT_THIGH:
	case HITBOX_RIGHT_THIGH:
	case HITBOX_LEFT_CALF:
	case HITBOX_RIGHT_CALF:
		return "Leg";
	case HITBOX_LEFT_HAND:
	case HITBOX_RIGHT_HAND:
	case HITBOX_LEFT_FOREARM:
	case HITBOX_RIGHT_FOREARM:
	case HITBOX_LEFT_UPPER_ARM:
	case HITBOX_RIGHT_UPPER_ARM:
		return "Arm";
	default:
		return "Unknown";
	}
}

void DrawDebugLine(Vector src, Vector end, Color color)
{
	BeamInfo_t beamInfo;

	beamInfo.m_nType = TE_BEAMPOINTS;
	beamInfo.m_pszModelName = "sprites/purplelaser1.vmt";
	beamInfo.m_flHaloScale = 6.0f;
	beamInfo.m_nModelIndex = -1;
	beamInfo.m_flLife = 10.f;
	beamInfo.m_flWidth = 0.25f;
	beamInfo.m_flEndWidth = 0.25f;
	beamInfo.m_flFadeLength = 0.5f;
	beamInfo.m_flAmplitude = 0.0f;
	beamInfo.m_flBrightness = 255.0f;
	beamInfo.m_flSpeed = 0.0f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.f;
	beamInfo.m_flRed = color.r();
	beamInfo.m_flGreen = color.g();
	beamInfo.m_flBlue = color.b();
	beamInfo.m_nSegments = -1;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE;

	beamInfo.m_vecStart = src;
	beamInfo.m_vecEnd = end;

	auto beam = g_RenderBeams->CreateBeamPoints(beamInfo);
	if (beam)
	{
		g_RenderBeams->DrawBeam(beam);
	}
}

void Visuals::RenderDevInfo()
{
	RECT scrn = GetViewport();
	static float realrealang = 0; //lol
	static float realfakeang = 0; //lmao
	static Vector lastAimPos = Vector(0, 0, 0);
	int w, w_temp, h;

	if (Global::vecAimpos != lastAimPos)
	{
		lastAimPos = Global::vecAimpos;
		Color color;
		if (Global::userCMD->buttons & IN_ATTACK || Global::userCMD->buttons & IN_ATTACK2)
		{
			color = Color::Green;
		}
		else if (Global::bAimbotting)
		{
			color = Color::Red;
		}
		else
		{
			color = Color::Orange;
		}
		DrawDebugLine(g_LocalPlayer->GetEyePos(), Global::vecAimpos, color);
		g_DebugOverlay->AddBoxOverlay(Global::vecAimpos, Vector(-3, -3, -3), Vector(3, 3, 3), QAngle(0, 0, 0), color.r(), color.g(), color.b(), 255, 10.0f);
	}

	if (g_LocalPlayer && g_LocalPlayer->IsAlive() && Global::userCMD != nullptr) {
		if (!Global::bSendPacket)
			realrealang = Global::userCMD->viewangles.yaw;
		else
			realfakeang = Global::userCMD->viewangles.yaw;
	}

	float lbyang = g_LocalPlayer->m_flLowerBodyYawTarget();

	std::string output = "=====AA Info=====";
	_bstr_t temp = output.c_str();
	g_VGuiSurface->GetTextSize(ui_font, temp, w, h);
	DrawString(ui_font, false, scrn.left, scrn.bottom / 2, Color(255, 255, 0), output.c_str());
	output = "Target Angles";
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 10, Color(255, 255, 0), output.c_str());
	output = "Real - ";
	output += std::to_string(Global::realyaw);
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 20, Color(255, 255, 0), output.c_str());
	output = "Fake - ";
	output += std::to_string(Global::fakeyaw);
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 30, Color(255, 255, 0), output.c_str());
	output = "True Angle Values";
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 40, Color(255, 255, 0), output.c_str());
	output = "Real - ";
	output += std::to_string(realrealang);
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 50, Color(255, 255, 0), output.c_str());
	output = "Fake - ";
	output += std::to_string(realfakeang);
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 60, Color(255, 255, 0), output.c_str());
	output = "LBY - ";
	output += std::to_string(lbyang);
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 70, Color(255, 255, 0), output.c_str());
	output = "Current Server Time: ";
	output += std::to_string(TICKS_TO_TIME(AimRage::Get().GetTickbase()));
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 90, Color(0, 178, 255), output.c_str());
	output = "Last LBY Update: ";
	output += std::to_string(Global::LastLBYUpdate);
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 100, Color(255, 110, 0), output.c_str());
	output = "Next LBY Update: ";
	output += std::to_string(Global::NextLBYUpdate);
	DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 110, Color(255, 110, 0), output.c_str());
	//output = "Next LBY Update: ";
	//output += std::to_string(AntiAim::Get().GetLbyUpdateTime(false));
	//output += std::to_string();
	//DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 110, Color(76, 255, 0), output.c_str());
	//DrawString(ui_font, false, scrn.left, (scrn.bottom / 2) + 100, Color(76, 255, 0), output.c_str());

	w += 10;

	output = "============Weapon Info============";
	temp = output.c_str();
	g_VGuiSurface->GetTextSize(ui_font, temp, w_temp, h);
	DrawString(ui_font, false, scrn.left + w, scrn.bottom / 2, Color(255, 255, 0), output.c_str());
	output = "Targetting player: ";
	if(!checks::is_bad_ptr(C_BasePlayer::GetPlayerByIndex(Global::aimbot_target)))
		output += C_BasePlayer::GetPlayerByIndex(Global::aimbot_target)->GetPlayerInfo().szName;
	if (!AimRage::Get().CheckTarget(Global::aimbot_target))
		output += " (invalid)";
	DrawString(ui_font, false, scrn.left + w, (scrn.bottom / 2) + 10, Color(255, 255, 0), output.c_str());
	output = "Target hitbox: ";
	output += get_hitbox_name(Global::aim_hitbox);
	DrawString(ui_font, false, scrn.left + w, (scrn.bottom / 2) + 20, Color(255, 255, 0), output.c_str());
	output = "Shots Missed: ";
	output += std::to_string(Resolver::Get().missed_shots[Global::aimbot_target]);
	DrawString(ui_font, false, scrn.left + w, (scrn.bottom / 2) + 30, Color(255, 255, 0), output.c_str());
	output = "Last calculated hitchance: ";
	output += std::to_string(Global::lasthc);
	DrawString(ui_font, false, scrn.left + w, (scrn.bottom / 2) + 40, Color(255, 255, 0), output.c_str());
	output = "Last calculated awall damage: ";
	output += std::to_string(Global::lastawdmg);
	DrawString(ui_font, false, scrn.left + w, (scrn.bottom / 2) + 50, Color(255, 255, 0), output.c_str());
	output = "Can Fire: ";
	output += AimRage::Get().can_fire_weapon ? "True" : "False";
	DrawString(ui_font, false, scrn.left + w, (scrn.bottom / 2) + 60, Color(255, 255, 0), output.c_str());

	w += w_temp;
	w += 10;

	output = "========Network========";
	temp = output.c_str();
	g_VGuiSurface->GetTextSize(ui_font, temp, w_temp, h);
	DrawString(ui_font, false, scrn.left + w, scrn.bottom / 2, Color(255, 255, 0), output.c_str());

	INetChannelInfo *nci = g_EngineClient->GetNetChannelInfo();
	if (nci) {
		output = "ping: ";
		output += std::to_string((nci->GetLatency(FLOW_INCOMING) + nci->GetLatency(FLOW_OUTGOING)) * 1000);
		output += "ms";
		DrawString(ui_font, false, scrn.left + w, scrn.bottom / 2 + 10, Color(255, 255, 0), output.c_str());
		output = "Loss: ";
		output += std::to_string(((nci->GetAvgLoss(FLOW_INCOMING) + nci->GetAvgLoss(FLOW_OUTGOING))/2)*100);
		output += "%";
		DrawString(ui_font, false, scrn.left + w, scrn.bottom / 2 + 20, Color(255, 255, 0), output.c_str());
	}
	output = "Choke: ";
	output += std::to_string(Miscellaneous::Get().GetChocked());
	DrawString(ui_font, false, scrn.left + w, scrn.bottom / 2 + 10 + (nci ? 20 : 0), Color(255, 255, 0), output.c_str());

	w += w_temp;
	w += 10;

	output = "========Misc========";
	temp = output.c_str();
	g_VGuiSurface->GetTextSize(ui_font, temp, w_temp, h);
	DrawString(ui_font, false, scrn.left + w, scrn.bottom / 2, Color(255, 255, 0), output.c_str());
	output = "FPS: ";
	output += std::to_string((float)1 / g_GlobalVars->frametime);
	DrawString(ui_font, false, scrn.left + w, scrn.bottom / 2 + 10, Color(255, 255, 0), output.c_str());
	if (g_LocalPlayer)
	{
		output = "Velocity ";
		output += std::to_string(g_LocalPlayer->m_vecVelocity().Length2D());
		DrawString(ui_font, false, scrn.left + w, scrn.bottom / 2 + 20, Color(255, 255, 0), output.c_str());
	}
}

void Visuals::Polygon(int count, Vertex_t* Vertexs, Color color)
{	//thanks for the broken ass function who ever coded this originally
	static int Texture = g_VGuiSurface->CreateNewTextureID(true);
	//unsigned char buffer[4] = { color.r(), color.g(), color.b(), color.a() };
	unsigned char buffer[4] = { 255, 255, 255, 255 };

	g_VGuiSurface->DrawSetTextureRGBA(Texture, buffer, 1, 1);
	//g_VGuiSurface->DrawSetColor(Color(255, 255, 255, 255));
	g_VGuiSurface->DrawSetColor(color);
	g_VGuiSurface->DrawSetTexture(Texture);

	g_VGuiSurface->DrawTexturedPolygon(count, Vertexs);
}

void Visuals::PolygonOutline(int count, Vertex_t* Vertexs, Color color, Color colorLine)
{
	static int x[128];
	static int y[128];

	Polygon(count, Vertexs, color);

	for (int i = 0; i < count; i++)
	{
		x[i] = Vertexs[i].m_Position.x;
		y[i] = Vertexs[i].m_Position.y;
	}

	PolyLine(x, y, count, colorLine);
}

void Visuals::PolyLine(int count, Vertex_t* Vertexs, Color colorLine)
{
	static int x[128];
	static int y[128];

	for (int i = 0; i < count; i++)
	{
		x[i] = Vertexs[i].m_Position.x;
		y[i] = Vertexs[i].m_Position.y;
	}

	PolyLine(x, y, count, colorLine);
}

void Visuals::PolyLine(int *x, int *y, int count, Color color)
{
	g_VGuiSurface->DrawSetColor(color);
	g_VGuiSurface->DrawPolyLine(x, y, count);
}

void Visuals::Draw3DCube(float scalar, QAngle angles, Vector middle_origin, Color outline)
{
	Vector forward, right, up;
	Math::AngleVectors(angles, forward, right, up);

	Vector points[8];
	points[0] = middle_origin - (right * scalar) + (up * scalar) - (forward * scalar); // BLT
	points[1] = middle_origin + (right * scalar) + (up * scalar) - (forward * scalar); // BRT
	points[2] = middle_origin - (right * scalar) - (up * scalar) - (forward * scalar); // BLB
	points[3] = middle_origin + (right * scalar) - (up * scalar) - (forward * scalar); // BRB

	points[4] = middle_origin - (right * scalar) + (up * scalar) + (forward * scalar); // FLT
	points[5] = middle_origin + (right * scalar) + (up * scalar) + (forward * scalar); // FRT
	points[6] = middle_origin - (right * scalar) - (up * scalar) + (forward * scalar); // FLB
	points[7] = middle_origin + (right * scalar) - (up * scalar) + (forward * scalar); // FRB

	Vector points_screen[8];
	for (int i = 0; i < 8; i++)
		if (!Math::WorldToScreen(points[i], points_screen[i]))
			return;

	g_VGuiSurface->DrawSetColor(outline);

	// Back frame
	g_VGuiSurface->DrawLine(points_screen[0].x, points_screen[0].y, points_screen[1].x, points_screen[1].y);
	g_VGuiSurface->DrawLine(points_screen[0].x, points_screen[0].y, points_screen[2].x, points_screen[2].y);
	g_VGuiSurface->DrawLine(points_screen[3].x, points_screen[3].y, points_screen[1].x, points_screen[1].y);
	g_VGuiSurface->DrawLine(points_screen[3].x, points_screen[3].y, points_screen[2].x, points_screen[2].y);

	// Frame connector
	g_VGuiSurface->DrawLine(points_screen[0].x, points_screen[0].y, points_screen[4].x, points_screen[4].y);
	g_VGuiSurface->DrawLine(points_screen[1].x, points_screen[1].y, points_screen[5].x, points_screen[5].y);
	g_VGuiSurface->DrawLine(points_screen[2].x, points_screen[2].y, points_screen[6].x, points_screen[6].y);
	g_VGuiSurface->DrawLine(points_screen[3].x, points_screen[3].y, points_screen[7].x, points_screen[7].y);

	// Front frame
	g_VGuiSurface->DrawLine(points_screen[4].x, points_screen[4].y, points_screen[5].x, points_screen[5].y);
	g_VGuiSurface->DrawLine(points_screen[4].x, points_screen[4].y, points_screen[6].x, points_screen[6].y);
	g_VGuiSurface->DrawLine(points_screen[7].x, points_screen[7].y, points_screen[5].x, points_screen[5].y);
	g_VGuiSurface->DrawLine(points_screen[7].x, points_screen[7].y, points_screen[6].x, points_screen[6].y);
}

void Visuals::FillRGBA(int x, int y, int w, int h, Color c)
{
	g_VGuiSurface->DrawSetColor(c);
	g_VGuiSurface->DrawFilledRect(x, y, x + w, y + h);
}
void Visuals::DrawFilledCircle(Vector2D center, Color color, float radius, float points)
{
	std::vector<Vertex_t> vertices;
	float step = (float)M_PI * 2.0f / points;

	for (float a = 0; a < (M_PI * 2.0f); a += step)
		vertices.push_back(Vertex_t(Vector2D(radius * cosf(a) + center.x, radius * sinf(a) + center.y)));

	Polygon((int)points, vertices.data(), color);
}
void Visuals::DrawTriangle(int x, int y, float w, float h, float angle, Color color)
{
	std::vector<Vertex_t> vertices;
	float rad = DEG2RAD(angle), rad_flip = DEG2RAD(angle + 90);

	vertices.push_back(Vertex_t(Vector2D((w / 2) * cos(rad_flip) + x, (w / 2) * sin(rad_flip) + y)));
	vertices.push_back(Vertex_t(Vector2D((-w / 2) * cos(rad_flip) + x, (-w / 2) * sin(rad_flip) + y)));
	vertices.push_back(Vertex_t(Vector2D(h * cos(rad) + x, h * sin(rad) + y)));

	Polygon(3, vertices.data(), color);
}
void Visuals::DrawSemiCircle(Vector2D center, Color color, float radius, float startang, float endang, float points)
{
	/*
	if (endang < startang)
		return;

	std::vector<Vertex_t> vertices;
	float total_angle = endang - startang;
	float step = (float)M_PI * 2.0f / points;
	float real_points = step * DEG2RAD(total_angle);

	vertices.push_back(Vertex_t(center));

	for (float a = DEG2RAD(startang); a < DEG2RAD(endang); a += step)
	{
		vertices.push_back(Vertex_t(Vector2D(radius * cosf(a) + center.x, radius * sinf(a) + center.y)));
	}

	Polygon((int)real_points + 1, vertices.data(), color);
	*/
}
void Visuals::BorderBox(int x, int y, int w, int h, Color color, int thickness)
{
	FillRGBA(x, y, w, thickness, color);
	FillRGBA(x, y, thickness, h, color);
	FillRGBA(x + w, y, thickness, h, color);
	FillRGBA(x, y + h, w + thickness, thickness, color);
}

__inline void Visuals::DrawFilledRect(int x, int y, int w, int h)
{
	g_VGuiSurface->DrawFilledRect(x, y, x + w, y + h);
}

void Visuals::DrawRectOutlined(int x, int y, int w, int h, Color color, Color outlinedColor, int thickness)
{
	FillRGBA(x, y, w, h, color);
	BorderBox(x - 1, y - 1, w + 1, h + 1, outlinedColor, thickness);
}

void Visuals::DrawString(unsigned long font, int x, int y, Color color, unsigned long alignment, const char* msg, ...)
{
	FUNCTION_GUARD;

	va_list va_alist;
	char buf[1024];
	va_start(va_alist, msg);
	_vsnprintf(buf, sizeof(buf), msg, va_alist);
	va_end(va_alist);
	wchar_t wbuf[1024];
	MultiByteToWideChar(CP_UTF8, 0, buf, 256, wbuf, 256);

	int r = 255, g = 255, b = 255, a = 255;
	color.GetColor(r, g, b, a);

	int width, height;
	g_VGuiSurface->GetTextSize(font, wbuf, width, height);

	if (alignment & FONT_RIGHT)
		x -= width;
	if (alignment & FONT_CENTER)
		x -= width / 2;

	g_VGuiSurface->DrawSetTextFont(font);
	g_VGuiSurface->DrawSetTextColor(r, g, b, a);
	g_VGuiSurface->DrawSetTextPos(x, y - height / 2);
	g_VGuiSurface->DrawPrintText(wbuf, wcslen(wbuf));
}

void Visuals::DrawString(unsigned long font, bool center, int x, int y, Color c, const char *fmt, ...)
{
	wchar_t *pszStringWide = reinterpret_cast< wchar_t* >(malloc((strlen(fmt) + 1) * sizeof(wchar_t)));

	mbstowcs(pszStringWide, fmt, (strlen(fmt) + 1) * sizeof(wchar_t));

	TextW(center, font, x, y, c, pszStringWide);

	free(pszStringWide);
}

void Visuals::TextW(bool center, unsigned long font, int x, int y, Color c, wchar_t *pszString)
{
	if (center)
	{
		int wide, tall;
		g_VGuiSurface->GetTextSize(font, pszString, wide, tall);
		x -= wide / 2;
		y -= tall / 2;
	}
	g_VGuiSurface->DrawSetTextColor(c);
	g_VGuiSurface->DrawSetTextFont(font);
	g_VGuiSurface->DrawSetTextPos(x, y);
	g_VGuiSurface->DrawPrintText(pszString, (int)wcslen(pszString), FONT_DRAW_DEFAULT);
}

void Visuals::DrawCircle(int x, int y, float r, int step, Color color)
{
	float Step = PI * 2.0 / step;
	for (float a = 0; a < (PI*2.0); a += Step)
	{
		float x1 = r * cos(a) + x;
		float y1 = r * sin(a) + y;
		float x2 = r * cos(a + Step) + x;
		float y2 = r * sin(a + Step) + y;
		g_VGuiSurface->DrawSetColor(color);
		g_VGuiSurface->DrawLine(x1, y1, x2, y2);
	}
}

void Visuals::DrawOutlinedRect(int x, int y, int w, int h, Color &c)
{
	g_VGuiSurface->DrawSetColor(c);
	g_VGuiSurface->DrawOutlinedRect(x, y, x + w, y + h);
}

void Visuals::GetTextSize(unsigned long font, const char *txt, int &width, int &height)
{
	FUNCTION_GUARD;

	if (checks::is_bad_const_ptr(txt) || strlen(txt) > 50)	//p100 anti-ayyware crasher
	{
		width = 0;
		height = 0;
		return;
	}

	size_t origsize = strlen(txt) + 1;
	const size_t newsize = 100;
	size_t convertedChars = 0;
	wchar_t wcstring[newsize];
	int x, y;

	mbstowcs_s(&convertedChars, wcstring, origsize, txt, _TRUNCATE);

	g_VGuiSurface->GetTextSize(font, wcstring, x, y);

	width = x;
	height = y;
}

void Visuals::Radar()
{
	static DWORD m_bSpotted = NetMngr::Get().getOffs("CBaseEntity", "m_bSpotted");
	if(!m_bSpotted)
		m_bSpotted = NetMngr::Get().getOffs("CBaseEntity", "m_bSpotted");
	else
		*(bool*)((DWORD)(ESP_ctx.player) + m_bSpotted) = true;
}

RECT Visuals::GetViewport()
{
	RECT viewport = { 0, 0, 0, 0 };
	int w, h;
	g_EngineClient->GetScreenSize(w, h);
	viewport.right = w; viewport.bottom = h;

	return viewport;
}