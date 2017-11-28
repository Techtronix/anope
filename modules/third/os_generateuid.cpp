/* os_generateuid: Generate an XLine ID manually
 *
 * Copyright 2016 Michael Hazell <michaelhazell@hotmail.com>
 *
 * You may use this module under the terms of docs/COPYING,
 * located in the Anope source directory
 *
 * module { name = "os_generateuid"; lowercase = no; }
 * command { name = "GENERATEUID"; service = "OperServ"; command = "operserv/generateuid"; }
 * 
 * If you want to use shorthand, like /os genuid or /os genid, make another command {} block
 * with the name changed to what you want. Don't change the actual command = "" area.
 */
#include "module.h"

class CommandOSGenerateUID : public Command
{
	bool lowercase;
 public:
	CommandOSGenerateUID(Module *creator, const Anope::string &sname = "operserv/generateuid") : Command(creator, sname, 0, 0)
	{
		this->SetDesc(_("Manually generate an AKILL ID"));
		this->SetSyntax(_(" "));
	}
	
	void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
	{
		lowercase = Config->GetModule(this->owner)->Get<bool>("lowercase");
		Anope::string id = XLineManager::GenerateUID();
		
		if (lowercase == true)
			source.Reply(_("Generated ID: " + id.lower()));
		else
			source.Reply(_("Generated ID: " + id));
	}
	
	bool OnHelp (CommandSource &source, const Anope::string &subcommand) anope_override
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Manually generate an AKILL ID"));
		return true;
	}
};

class OSGenerateUID : public Module
{
	CommandOSGenerateUID commandosgenerateuid;
	bool lowercase;
 public:
	OSGenerateUID(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, THIRD), commandosgenerateuid(this)
	{
		this->SetAuthor("Techman");
		this->SetVersion("1.0.0");
	}
	
	void OnReload(Configuration::Conf *conf) anope_override
	{
		Configuration::Block *block = conf->GetModule(this);
		lowercase = block->Get<bool>("lowercase", "false");
	}
};

MODULE_INIT(OSGenerateUID)
