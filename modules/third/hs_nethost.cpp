/*
 * Copyright 2016 Michael Hazell <michaelhazell@hotmail.com>
 *
 * You may use this module as long as you follow the terms of the GPLv3
 * Originally forked from https://gist.github.com/7330c12ce7a03c030871
 *
 * Configuration:
 * module { name = "hs_nethost"; prefix = "user/"; suffix = ""; }
 * 
 * This will format a user's vhost as user/foo, and broadcast the vhost change to
 * other modules.
 */
 
#include "module.h"
 
class HSNetHost : public Module
{
	Anope::string prefix;
	Anope::string suffix;
	bool broadcast;
	Reference<BotInfo> HostServ;
	
 private:
	void SetNetHost(NickAlias *na)
	{
		Anope::string vhost = prefix + na->nc->display + suffix;
		Anope::string setter = "HostServ";
		Anope::string ident; // We don't handle Vidents
 
		if(!IRCD->IsHostValid(vhost))
			return;
 
		na->SetVhost(ident, vhost, setter);
		
		
		/* Sigh! Another issue. If a network has module:hs_group::synconset set to true, then the module
		 * will "sync" the vhost to the account. Since this is the first NickAlias, though, it will be
		 * syncing for no reason. This actually causes the vhost to be set TWICE, causing the usual
		 * "Your vhost of xx is now activated" message to be sent twice to the user. So, to fix the issue
		 * we must set the vhost manually and update the Anope state
		 */
		if (broadcast)
		{
			FOREACH_MOD(OnSetVhost, (na));
		}
		else
		{
			User *u = User::Find(na->nick);
			
			if (u && u->Account() == na->nc)
			{
				IRCD->SendVhost(u, na->GetVhostIdent(), na->GetVhostHost());
				u->vhost = na->GetVhostHost();
				u->UpdateHost();
				
				if (IRCD->CanSetVIdent && !na->GetVhostIdent().empty())
					u->SetVIdent(na->GetVhostIdent());
				
				if (!na->GetVhostIdent().empty())
				{
					u->SendMessage(HostServ, _("Your vhost of \002%s\002@\002%s\002 is now activated."), na->GetVhostIdent().c_str(), na->GetVhostHost().c_str());
				}
				else
				{
					u->SendMessage(HostServ, _("Your vhost of \002%s\002 is now activated."), na->GetVhostHost().c_str());
				}
			}
		}
	}
 
 
 
 public:
	HSNetHost(const Anope::string &modname, const Anope::string &creator):
	Module(modname, creator, THIRD)
	{
		this->SetAuthor("Techman");
		this->SetVersion("1.0");
		
		if (!IRCD || !IRCD->CanSetVHost)
			throw ModuleException("Your IRCd does not support vhosts");
	}
 
	void Prioritize() anope_override
	{
		/* We set ourselves as priority last because we don't want vhosts being added to 
		 * ns_access on registration if that's enabled by the network
		 */
		ModuleManager::SetPriority(this, PRIORITY_LAST);
	}
	
	void OnNickRegister(User *user, NickAlias *na, const Anope::string &) anope_override
	{
		/* If it's anything else, we assume they have a nick confirmation system */
		if (Config->GetModule("ns_register")->Get<const Anope::string>("registration") == "none")
			SetNetHost(na);
	}	
	
	void OnNickConfirm(User *user, NickCore *nc) anope_override
	{
		NickAlias *na = nc->aliases->at(0);
		SetNetHost(na);
	}
	
	void OnReload(Configuration::Conf *conf) anope_override
	{
		Configuration::Block *block = conf->GetModule(this);
		prefix = block->Get<Anope::string>("prefix", "");
		suffix = block->Get<Anope::string>("suffix", "");
		broadcast = block->Get<bool>("broadcast", true);
		
		/* For when using broadcast = false */
		const Anope::string &hsnick = conf->GetModule("hostserv")->Get<const Anope::string>("client");
		if (hsnick.empty())
			throw ConfigException("m_hostserv: <client> must be defined");
		BotInfo *bi = BotInfo::Find(hsnick, true);
		if (!bi)
			throw ConfigException("m_hostserv: no bot named " + hsnick);
		HostServ = bi;
	}
};
 
MODULE_INIT(HSNetHost)
