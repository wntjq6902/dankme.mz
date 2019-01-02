#ifndef hwid_list
#define hwid_list

#include <vector>
#include <utility>
#include <string>

enum user_types
{
	USER_TYPE_ROOT,	  //project owner
	USER_TYPE_DEV,	  //developer
	USER_TYPE_MOD,	  //moderator
	USER_TYPE_TRUSTED //trusted
};

static std::vector<std::pair<std::string, std::pair<std::string, int>>> hwids =
{
	{ "bf8d0f92058c7b74aa455dc0b629eb08e0df5f7ec2cc36f248e7f165ceff1455", {"ERRORNAME", USER_TYPE_ROOT} },
	{ "525f820c36a21cfbf27c7ff587f12df2163ca8c86020766f1b38f61c5cec50c9",{ "Lea", USER_TYPE_DEV } },
	{ "4902d2269dafdd8bdde89ec417039f86f08b0d928e79f4ab7916556733b8e0c1", {"Faith", USER_TYPE_TRUSTED} },
	{ "48c7bee35ad7223e5945c18bdb877152713426915f29f1278cfab7c14146c972", {"Soda", USER_TYPE_TRUSTED} },
	{ "7ac5b099bfc7dfe856289f991272d89f9059e98b4e818c4b39b22deb102e9316", {"Shizuru", USER_TYPE_TRUSTED}},
	{ "d3f1d3f12c8440c3e52015efceed200532b7d7bb101d49ae9772a2f9edf17cfe", {"H1GH", USER_TYPE_TRUSTED}},
};

inline std::string user_type_to_string(int type)
{
	switch (type)
	{
	case USER_TYPE_ROOT:
		return "Owner";
	case USER_TYPE_DEV:
		return "Developer";
	case USER_TYPE_MOD:
		return "Moderator";
	case USER_TYPE_TRUSTED:
		return "Trusted User";
	default:
		return "GTFO Cracker Faggot";
	}
}
#endif