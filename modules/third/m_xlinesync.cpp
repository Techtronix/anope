/*
 * X-line sync: Sync InspIRCd X-lines with Anope X-Lines
 * Currently supports: CBAN,Q,G,Z,R-lines -> AKILL, SQLine
 *
 * Copyright (C) 2021 Michael Hazell <michaelhazell@hotmail.com>
 *
 * You may use this module as long as you follow the terms of the GPLv3
 * Parts based on m_xlinetoakill by genius3000
 *
 * Configuration:
 * module { name = "m_xlinesync"; silent = false; }
 * command { service = "OperServ"; name = "XLINESYNC"; command = "operserv/xlinesync"; permission = "operserv/xlinesync" ; }
 */

#include "module.h"

static ServiceReference<XLineManager> akills("XLineManager", "xlinemanager/sgline");
static ServiceReference<XLineManager> sqlines("XLineManager", "xlinemanager/sqline");

enum InspIRCdXLineType
{
	TYPE_CBAN,
	TYPE_GLINE,
	TYPE_QLINE,
	TYPE_RLINE,
	TYPE_ZLINE
};

class CommandOSXLineSync : public Command
{
 public:
	CommandOSXLineSync(Module *creator) : Command(creator, "operserv/xlinesync", 1, 1)
	{
		this->SetDesc(_("Sync X-Lines in Anope to the IRCd"));
		this->SetSyntax(_("SYNC"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
	{
		// Send AKILLs
		for (size_t i = 0; i < akills->GetCount(); ++i)
		{
			XLine *x = akills->GetEntry(i);
			if (x)
				akills->Send(NULL, x);
		}

		// Send SQLines
		for (size_t i = 0; i < sqlines->GetCount(); ++i)
		{
			XLine *x = sqlines->GetEntry(i);
			if (x)
				sqlines->Send(NULL, x);
		}
		source.Reply("Synced AKILLs and SQLines with the IRCd");
		Log(LOG_ADMIN, source, this) << "to sync AKILLs and SQLines with the IRCd";
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) anope_override
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Sync X-Lines in Anope to the IRCd. This currently supports AKILL and SQLINE."));
		return true;
	}
};

class XLineSync : public Module
{
	BotInfo *OperServ;
	CommandOSXLineSync commandosxlinesync;
	bool silent;

 public:
	XLineSync(const Anope::string &modname, const Anope::string &creator)
		: Module(modname, creator, THIRD)
		, OperServ(NULL)
		, commandosxlinesync(this)
	{
		if (Anope::VersionMajor() != 2 || Anope::VersionMinor() != 0)
			throw ModuleException("This module only works on Anope 2.0.x.");

		if (IRCD->GetProtocolName().find("InspIRCd") == Anope::string::npos)
			throw ModuleException("This module only works on InspIRCd.");

		if (!ModuleManager::FindModule("operserv") || !ModuleManager::FindModule("os_akill") || !ModuleManager::FindModule("os_sxline"))
			throw ModuleException("This module requires OperServ, as well as the os_akill and os_sxline modules.");

		this->SetAuthor("Techman");
		this->SetVersion("1.1.0");
	}

	void OnReload(Configuration::Conf *conf) anope_override
	{
		OperServ = conf->GetClient("OperServ");
		if (!OperServ)
			throw ModuleException("OperServ could not be found.");

		Configuration::Block *block = conf->GetModule(this);
		silent = block->Get<bool>("silent", "false");

	}

	EventReturn OnMessage(MessageSource &source, Anope::string &command, std::vector<Anope::string> &params)
	{
		// If it isn't InspIRCd-related X-line additions or removals, move on
		if ((command != "ADDLINE" && command != "DELLINE") || params.size() < 2 || !akills || !sqlines)
			return EVENT_CONTINUE;

		// Pull out the X-line type and the mask
		const Anope::string &rawtype = params[0];
		Anope::string mask = params[1];

		// Now, set the type for processing later
		int etype;

		if (rawtype == "CBAN")
			etype = TYPE_CBAN;
		else if (rawtype == "G")
			etype  = TYPE_GLINE;
		else if (rawtype == "Q")
			etype = TYPE_QLINE;
		else if (rawtype == "R")
			etype = TYPE_RLINE;
		else if (rawtype == "Z")
			etype = TYPE_ZLINE;

		// Are we adding or removing? Parameter count will us
		if (command == "ADDLINE" && params.size() == 6)
		{
			// Pull out common variables that will be used for all types
			const Anope::string setby = params[2];
			time_t settime = convertTo<time_t>(params[3]);
			time_t duration = convertTo<time_t>(params[4]);
			const Anope::string reason = params[5];
			time_t expires = (duration == 0) ? duration : settime + duration;

			switch (etype)
			{
				// CBAN = Channel Q-line
				case TYPE_CBAN:
				case TYPE_QLINE:
				{
					// Check if entry alreay exists
					if (sqlines->HasEntry(mask))
						return EVENT_CONTINUE;

					XLine *x = new XLine(mask, setby, expires, reason, XLineManager::GenerateUID());
					sqlines->AddXLine(x);
					if (!silent)
						Log(OperServ, "sqline/sync") << "X-line (" << rawtype << ") sync added SQLINE on " << mask << " (" << reason << "), expires in " << (expires ? Anope::Duration(expires - settime) : "never") << " [set by " << setby << "]";
					break;
				}

				// R-Lines are sent as 'n!u@h\sreal\sname' and need to be '/n!u@h#real name/'
				case TYPE_RLINE:
				{
					size_t space = mask.find("\\s");
					if (space != Anope::string::npos)
					{
						mask = mask.replace(space, 2, "#");
						mask = mask.replace_all_cs("\\s", " ");
					}
					mask = "/" + mask + "/";

					if (akills->HasEntry(mask))
						return EVENT_CONTINUE;

					XLine *x = new XLine(mask, setby, expires, reason, XLineManager::GenerateUID());
					akills->AddXLine(x);
					if (!silent)
						Log(OperServ, "akill/sync") << "X-line (" << rawtype << ") sync added AKILL on " << mask << " (" << reason << "), expires in " << (expires ? Anope::Duration(expires - settime) : "never") << " [set by " << setby << "]";
					break;
				}
				case TYPE_ZLINE:
				{
					// InspIRCd Z-lines are just IPs, akills need to be *@IP
					mask = "*@" + mask;
				}
				case TYPE_GLINE:
				{
					// No conversions necessary for G-lines
					if (akills->HasEntry(mask))
						return EVENT_CONTINUE;

					XLine *x = new XLine(mask, setby, expires, reason, XLineManager::GenerateUID());
					akills->AddXLine(x);
					if (!silent)
						Log(OperServ, "akill/sync") << "X-line (" << rawtype << ") sync added AKILL on " << mask << " (" << reason << "), expires in " << (expires ? Anope::Duration(expires - settime) : "never") << " [set by " << setby << "]";
					break;
				}

				default:
					break;
			}
		}
		else if (command == "DELLINE")
		{
			switch (etype)
			{
				case TYPE_CBAN:
				case TYPE_QLINE:
				{
					// Find matching X-Line, if it exists
					XLine *x = sqlines->HasEntry(mask);
					if (!x)
						return EVENT_CONTINUE;

					sqlines->DelXLine(x);
					if (!silent)
						Log(OperServ, "sqline/sync") << "X-line (" << rawtype << ") sync removed SQLINE on " << mask;
					break;
				}

				case TYPE_RLINE:
				{
					size_t space = mask.find("\\s");
					if (space != Anope::string::npos)
					{
						mask = mask.replace(space, 2, "#");
						mask = mask.replace_all_cs("\\s", " ");
					}
					mask = "/" + mask + "/";

					// Find matching X-Line if it exists
					XLine *x = akills->HasEntry(mask);
					if (!x)
						return EVENT_CONTINUE;

					akills->DelXLine(x);
					if (!silent)
						Log(OperServ, "akill/sync") << "X-line (" << rawtype << ") sync removed AKILL on " << mask;
					break;
				}

				case TYPE_ZLINE:
				{
					mask = "*@" + mask;
				}
				case TYPE_GLINE:
				{
					XLine *x = akills->HasEntry(mask);
					if (!x)
						return EVENT_CONTINUE;

					akills->DelXLine(x);
					if (!silent)
						Log(OperServ, "akill/sync") << "X-line (" << rawtype << ") sync removed AKILL on " << mask;
					break;
				}

				default:
					break;
			}
		}
		// Standard protocol modules do nothing with ADDLINE and DELLINE,
		// allow other modules to act on these though.
		return EVENT_CONTINUE;
	}
};

MODULE_INIT(XLineSync)
