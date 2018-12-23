
#include "module.h"

class CommandRedirect: public Command
{
    std::vector<Anope::string> defaults;

public:
  CommandRedirect(Module *creator) : Command(creator, "chanserv/redirect", 3, 3)
  {
    this->SetDesc(_("Redirects nick from channel."));
    this->SetSyntax(_("\037nick\037 \037channel\037"));
  }

  void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
  {
    const Anope::string &chan = params[0];
    const Anope::string &target = params[1];
    const Anope::string &targetChan = params[2];

    User *u = source.GetUser();
    ChannelInfo *ci = ChannelInfo::Find(chan);
    Channel *c = Channel::Find(chan);
    User *u2 = User::Find(target, true);

    if (!c)
    {
      source.Reply(CHAN_X_NOT_IN_USE, chan.c_str());
      return;
    }
    else if (!ci)
    {
      source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
      return;
    }

    if (!u2)
    {
      source.Reply(_("Can't find target: %s"), target.c_str());
      return;
    }

    AccessGroup u_access = source.AccessFor(ci);

    if (!u_access.HasPriv("KICK") && !source.HasPriv("chanserv/kick"))
			source.Reply(ACCESS_DENIED);
    else if (u2)
    {
	AccessGroup u2_access = ci->AccessFor(u2);
	if (u != u2 && ci->HasExt("PEACE") && u2_access >= u_access && !source.HasPriv("chanserv/kick"))
	  source.Reply(ACCESS_DENIED);
	else if (u2->IsProtected())
	  source.Reply(ACCESS_DENIED);
	else if (!c->FindUser(u2))
	  source.Reply(NICK_X_NOT_ON_CHAN, u2->nick.c_str(), c->name.c_str());
	else
	{
	  source.Reply(_("Move %s form %s to %s"),u2->nick.c_str(),chan.c_str(),targetChan.c_str());
	  IRCD->SendSVSPart(*source.service, u2, chan,"");
	  IRCD->SendSVSJoin(*source.service, u2, targetChan,"");
	}
    }
  }

  bool OnHelp(CommandSource &source, const Anope::string &subcommand) anope_override
  {
	  this->SendSyntax(source);
	  source.Reply(" ");
	  source.Reply(_("Parts given nick from channel and forces him to join to a specified channel\n"
	    "\n"
			"Examples: \n"
			"!redirect luser #support          - redirects nick luser from active channel to a #support\n"
			"/cs redirect #main luser #support - redirects nick luser from #main to a #support"));
	  return true;
  }
};

class CommandTeleport: public Command
{
    std::vector<Anope::string> defaults;

public:
  CommandTeleport(Module *creator) : Command(creator, "chanserv/teleport", 3, 3)
  {
    this->SetDesc(_("Teleports nick from channel."));
    this->SetSyntax(_("\037nick\037 \037channel\037"));
  }

  void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
  {
    const Anope::string &chan = params[0];
    const Anope::string &target = params[1];
    const Anope::string &targetChan = params[2];
    //Anope::string reason = params.size() > 2 ? params[2] : "Requested";

    User *u = source.GetUser();
    ChannelInfo *ci = ChannelInfo::Find(chan);
    Channel *c = Channel::Find(chan);
    User *u2 = User::Find(target, true);

    Configuration::Block *block = Config->GetCommand(source);
    const Anope::string &mode = block->Get<Anope::string>("mode", "BAN");

    if (!c)
    {
      source.Reply(CHAN_X_NOT_IN_USE, chan.c_str());
      return;
    }
    else if (!ci)
    {
      source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
      return;
    }

    if (!u2)
    {
      source.Reply(_("Can't find target: %s"), target.c_str());
      return;
    }

    AccessGroup u_access = source.AccessFor(ci);

    if (!u_access.HasPriv("KICK") && !source.HasPriv("chanserv/kick"))
			source.Reply(ACCESS_DENIED);
    else if (u2)
    {
	AccessGroup u2_access = ci->AccessFor(u2);
	if (u != u2 && ci->HasExt("PEACE") && u2_access >= u_access && !source.HasPriv("chanserv/kick"))
	  source.Reply(ACCESS_DENIED);
	else if (u2->IsProtected())
	  source.Reply(ACCESS_DENIED);
	else if (!c->FindUser(u2))
	  source.Reply(NICK_X_NOT_ON_CHAN, u2->nick.c_str(), c->name.c_str());
	else
	{
	  source.Reply(_("Teleported %s form %s to %s"),u2->nick.c_str(),chan.c_str(),targetChan.c_str());

	  Anope::string mask = ci->GetIdealBan(u2);
	  if (!c->HasMode(mode,mask))
	  {
	    c->SetMode(NULL,mode,mask);
	  }
	  IRCD->SendSVSPart(*source.service, u2, chan,"");
	  IRCD->SendSVSJoin(*source.service, u2, targetChan,"");
	}
    }
  }

  bool OnHelp(CommandSource &source, const Anope::string &subcommand) anope_override
  {
	  this->SendSyntax(source);
	  source.Reply(" ");
	  source.Reply(_("Ban and parts given nick from channel \n"
			  "and forces him to join to a specified channel\n"
			"\n"
			"Examples: \n"
			"!teleport luser #support          - teleports nick luser from active channel to a #support\n"
			"/cs teleport #main luser #support - teleports nick luser from #main to a #support"));
	  return true;
  }
};

class CSREDTEL: public Module
{
  CommandRedirect commandRedirect;
  CommandTeleport commandTeleport;

public:
  CSREDTEL(const Anope::string &modname, const Anope::string &creator) : Module(modname,creator,VENDOR), commandRedirect(this), commandTeleport(this)
  {
    if (Anope::VersionMajor() < 2)
    {
      throw ModuleException("Requires version 2 of Anope.");
    }

    this->SetAuthor("Darkus");
    this->SetVersion("0.0.1");
  }
};

MODULE_INIT(CSREDTEL)
