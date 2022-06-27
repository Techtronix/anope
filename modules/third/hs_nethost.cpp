/*
 * HostServ NetHost: Add default vhosts for new users using their account names
 *
 * Copyright (C) 2016-2022 Michael Hazell <michaelhazell@hotmail.com>
 *
 * You may use this module as long as you follow the terms of the GPLv3
 * Originally forked from https://gist.github.com/7330c12ce7a03c030871
 *
 * Configuration:
 * module { name = "hs_nethost"; prefix = "user/"; suffix = ""; hashprefix = "/x-"; setifnone = true; }
 * command { service = "HostServ"; name = "SETNETHOSTS"; command = "hostserv/setnethosts"; permission = "hostserv/setnethosts"; }
 *
 * This will format a user's vhost as user/foo (or user/foo-bar/x-XXXX), and broadcast the vhost change to
 * other modules.
 */

#include "module.h"

// Static variables that will be set by the configuration
static bool setifnone;
static Anope::string hashprefix;
static Anope::string prefix;
static Anope::string suffix;

// Static functions, used by the command and the module itself
/**
 * @brief Generate a hex string of a NickCore ID
 * 
 * @param nc The NickCore (account)
 * @return Anope::string 
 */
static Anope::string GenerateHash(NickCore *nc)
{
	std::stringstream ss;
	ss << std::hex << nc->GetId();
	return ss.str();
}

/**
 * @brief Get the NickAlias that matches the display nick of the NickCore
 * 
 * @param nc The NickCore (account) object
 * @return NickAlias* 
 */
static NickAlias *GetMatchingNAFromNC(NickCore *nc)
{
	for (std::vector<NickAlias *>::iterator it = nc->aliases->begin(); it != nc->aliases->end(); ++it)
	{
		if ((*it)->nick == nc->display)
			return *it;
	}
	return NULL;
}

/**
 * @brief Set a network host
 * 
 * @param na The NickAlias to set the host on
 * @param force Should the host be forcefully set?
 * Forcing a vhost to be set will override the check to see a host has been manually set by something other than HostServ.
 */
static void SetNetHost(NickAlias *na, bool force = false)
{
	// If the NickAlias has an existing host not set by this module, do not touch it
	// This avoids overwriting manually set/requested vhosts
	if (na->HasVhost() && (na->GetVhostCreator() != "HostServ") && !force)
		return;

	Anope::string nick = na->nick;
	Anope::string vhost;
	bool usehash = false;

	Anope::string valid_nick_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-";

	// This operates on nick, not na->nick, so that changes can be made later.
	for (Anope::string::iterator it = nick.begin(); it != nick.end(); ++it)
	{
		if (valid_nick_chars.find(*it) == Anope::string::npos)
		{
			usehash = true;
			*it = '-';
		}
	}

	// Construct vhost
	vhost = prefix + nick + suffix;

	// If the nickname contained invalid characters, append a hash since those were replaced with a -
	if (usehash)
		vhost += hashprefix + GenerateHash(na->nc);

	if (!IRCD->IsHostValid(vhost))
	{
		Log(Config->GetClient("HostServ"), "nethost")
			<< "Tried setting vhost " << vhost << " on " << na->nick << ", but it was not valid!";
		Log(Config->GetClient("HostServ"), "nethost")
			<< "Check if your IRCd supports all of the characters in the vhost, and that Anope is configured to allow "
		       "them (check vhost_chars in services.conf).";
		return;
	}

	na->SetVhost(na->GetVhostIdent(), vhost, "HostServ");
	FOREACH_MOD(OnSetVhost, (na));
}

class CommandHSSetNetHosts : public Command
{
 public:
	CommandHSSetNetHosts(Module *creator, const Anope::string &sname = "hostserv/setnethosts") : Command(creator, sname, 0, 1)
	{
		this->SetDesc(_("Set the NetHost for all users."));
		this->SetSyntax(_("[ALL]"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
	{
		// Figure out if all user hosts have to be changed
		bool all = params.size() > 0 && params[0].equals_ci("ALL");
		
		for (nickcore_map::const_iterator it = NickCoreList->begin(), it_end = NickCoreList->end(); it != it_end; ++it)
		{
			// Easy alias so that the code is simpler later
			NickCore *nc = it->second;

			// If the account is an operator, skip
			if (nc->o)
				continue;

			// Get the corresponding NickAlias that matches the NickCore display nick
			NickAlias *na = GetMatchingNAFromNC(nc);
			if (!na)
				continue;

			if (all)
				SetNetHost(na, true);
			else
				SetNetHost(na);
		}
		if (all)
			Log(LOG_ADMIN, source, this) << "on all users, ignoring existing vhosts";
		else
			Log(LOG_ADMIN, source, this);
	}

	bool OnHelp(CommandSource &source, const Anope::string &) anope_override
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Set the NetHost for all users.\n"
		               "Users with an existing vhost not set by HostServ will be ignored unless ALL is specified."));
		return true;
	}
};

class HSNetHost : public Module
{
	CommandHSSetNetHosts commandhssetnethosts;

 public:
	HSNetHost(const Anope::string &modname, const Anope::string &creator):
	Module(modname, creator, THIRD), commandhssetnethosts(this)
	{
		this->SetAuthor("Techman");
		this->SetVersion("2.1.0");

		if (!IRCD || !IRCD->CanSetVHost)
			throw ModuleException("Your IRCd does not support vhosts");

		if (!ModuleManager::FindModule("hostserv"))
			throw ModuleException("HostServ is required for this module to be effective.");
	}

	void Prioritize() anope_override
	{
		// We set ourselves as priority last because we don't want vhosts being added to
		// ns_access on registration if that's enabled by the network
		ModuleManager::SetPriority(this, PRIORITY_LAST);
	}

	void OnChangeCoreDisplay(NickCore *nc, const Anope::string &newdisplay) anope_override
	{
		// Send the matching NickAlias so that nick is set
		// This requires newdisplay, so this loop is different
		for (std::vector<NickAlias *>::const_iterator it = nc->aliases->begin(); it != nc->aliases->end(); ++it)
		{
			if ((*it)->nick == newdisplay)
			{
				SetNetHost(*it);
				break;
			}
		}
	}

	void OnNickRegister(User *, NickAlias *na, const Anope::string &) anope_override
	{
		// If it's anything else, we assume they have a nick confirmation system
		if (Config->GetModule("ns_register")->Get<const Anope::string>("registration") == "none")
			SetNetHost(na);
	}

	void OnNickConfirm(User *, NickCore *nc) anope_override
	{
		// It is assumed there is only one NickAlias in the account
		NickAlias *na = nc->aliases->at(0);
		SetNetHost(na);
	}

	void OnNickIdentify(User *u) anope_override
	{
		// If not configured to set a nethost when no vhost is present, just quit
		if (!setifnone)
			return;

		NickCore *nc = u->Account();
		if (!nc)
			return;

		// If the account is marked as unconfirmed, quit
		if (nc->HasExt("UNCONFIRMED"))
			return;

		// Send the NickAlias that matches the account name
		NickAlias *na = GetMatchingNAFromNC(nc);
		if (!na->HasVhost())
			SetNetHost(na);
	}

	void OnUserLogin(User *u) anope_override
	{
		OnNickIdentify(u);
	}

	void OnReload(Configuration::Conf *conf) anope_override
	{
		Configuration::Block *block = conf->GetModule(this);
		setifnone = block->Get<bool>("setifnone", "false");
		hashprefix = block->Get<const Anope::string>("hashprefix", "");
		prefix = block->Get<const Anope::string>("prefix", "");
		suffix = block->Get<const Anope::string>("suffix", "");
	}
};

MODULE_INIT(HSNetHost)
