#include "module.h"

class ModuleInfoAccount : public Module
{

 public:
	ModuleInfoAccount(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, THIRD)
	{
	}

	void OnNickInfo(CommandSource &source, NickAlias *na, InfoFormatter &info, bool show_hidden) anope_override
	{
		info[_("Account")] = na->nc->display;

		if (na->nc->aliases->size() > 1)
		{
			Anope::string nicks;
			for (unsigned i = 0; i < na->nc->aliases->size(); ++i)
			{
				NickAlias *na2 = na->nc->aliases->at(i);

				if (!nicks.empty())
					nicks += ", ";
				nicks += na2->nick;
			}

			info[_("Grouped nicknames")] = nicks;
		}
	}
};

MODULE_INIT(ModuleInfoAccount)
