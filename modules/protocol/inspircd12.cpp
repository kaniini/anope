/* inspircd 1.2 functions
 *
 * (C) 2003-2010 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "services.h"
#include "modules.h"
#include "hashcomp.h"

IRCDVar myIrcd[] = {
	{"InspIRCd 1.2",	/* ircd name */
	 "+I",				/* Modes used by pseudoclients */
	 5,					/* Chan Max Symbols */
	 1,					/* SVSNICK */
	 1,					/* Vhost */
	 0,					/* Supports SNlines */
	 1,					/* Supports SQlines */
	 1,					/* Supports SZlines */
	 0,					/* Join 2 Message */
	 0,					/* Chan SQlines */
	 0,					/* Quit on Kill */
	 0,					/* SVSMODE unban */
	 1,					/* Reverse */
	 1,					/* vidents */
	 1,					/* svshold */
	 0,					/* time stamp on mode */
	 0,					/* O:LINE */
	 1,					/* UMODE */
	 1,					/* No Knock requires +i */
	 0,					/* Can remove User Channel Modes with SVSMODE */
	 0,					/* Sglines are not enforced until user reconnects */
	 1,					/* ts6 */
	 "$",				/* TLD Prefix for Global */
	 20,				/* Max number of modes we can send per line */
	 }
	,
	{NULL}
};

static bool has_servicesmod = false;
static bool has_globopsmod = false;
static bool has_svsholdmod = false;
static bool has_chghostmod = false;
static bool has_chgidentmod = false;
static bool has_hidechansmod = false;

/* Previously introduced user during burst */
static User *prev_u_intro = NULL;

/* CHGHOST */
void inspircd_cmd_chghost(const Anope::string &nick, const Anope::string &vhost)
{
	if (!has_chghostmod)
	{
		ircdproto->SendGlobops(OperServ, "CHGHOST not loaded!");
		return;
	}

	send_cmd(HostServ ? HostServ->GetUID() : TS6SID, "CHGHOST %s %s", nick.c_str(), vhost.c_str());
}

bool event_idle(const Anope::string &source, const std::vector<Anope::string> &params)
{
	BotInfo *bi = findbot(params[0]);

	send_cmd(bi ? bi->GetUID() : params[0], "IDLE %s %ld %ld", source.c_str(), static_cast<long>(start_time), bi ? (static_cast<long>(Anope::CurTime - bi->lastmsg)) : 0);
	return true;
}

static Anope::string currentpass;

/* PASS */
void inspircd_cmd_pass(const Anope::string &pass)
{
	currentpass = pass;
}

class InspIRCdProto : public IRCDProto
{
	void SendAkillDel(const XLine *x)
	{
		send_cmd(OperServ ? OperServ->GetUID() : TS6SID, "GLINE %s", x->Mask.c_str());
	}

	void SendTopic(BotInfo *whosets, Channel *c)
	{
		send_cmd(whosets->GetUID(), "FTOPIC %s %lu %s :%s", c->name.c_str(), static_cast<unsigned long>(c->topic_time + 1), c->topic_setter.c_str(), c->topic.c_str());
	}

	void SendVhostDel(User *u)
	{
		if (u->HasMode(UMODE_CLOAK))
			inspircd_cmd_chghost(u->nick, u->chost);
		else
			inspircd_cmd_chghost(u->nick, u->host);

		if (has_chgidentmod && u->GetIdent() != u->GetVIdent())
			inspircd_cmd_chgident(u->nick, u->GetIdent());
	}

	void SendAkill(const XLine *x)
	{
		// Calculate the time left before this would expire, capping it at 2 days
		time_t timeleft = x->Expires - Anope::CurTime;
		if (timeleft > 172800 || !x->Expires)
			timeleft = 172800;
		send_cmd(OperServ ? OperServ->GetUID() : TS6SID, "ADDLINE G %s@%s %s %ld %ld :%s", x->GetUser().c_str(), x->GetHost().c_str(), x->By.c_str(), static_cast<long>(Anope::CurTime), static_cast<long>(timeleft), x->Reason.c_str());
	}

	void SendSVSKillInternal(const BotInfo *source, const User *user, const Anope::string &buf)
	{
		send_cmd(source ? source->GetUID() : TS6SID, "KILL %s :%s", user->GetUID().c_str(), buf.c_str());
	}

	void SendSVSMode(const User *u, int ac, const char **av)
	{
		this->SendModeInternal(NULL, u, merge_args(ac, av));
	}

	void SendNumericInternal(const Anope::string &source, int numeric, const Anope::string &dest, const Anope::string &buf)
	{
		send_cmd(TS6SID, "PUSH %s ::%s %03d %s %s", dest.c_str(), source.c_str(), numeric, dest.c_str(), buf.c_str());
	}

	void SendModeInternal(const BotInfo *source, const Channel *dest, const Anope::string &buf)
	{
		send_cmd(source ? source->GetUID() : TS6SID, "FMODE %s %u %s", dest->name.c_str(), static_cast<unsigned>(dest->creation_time), buf.c_str());
	}

	void SendModeInternal(const BotInfo *bi, const User *u, const Anope::string &buf)
	{
		if (buf.empty())
			return;
		send_cmd(bi ? bi->GetUID() : TS6SID, "MODE %s %s", u->GetUID().c_str(), buf.c_str());
	}

	void SendClientIntroduction(const User *u, const Anope::string &modes)
	{
		send_cmd(TS6SID, "UID %s %ld %s %s %s %s 0.0.0.0 %ld %s :%s", u->GetUID().c_str(), static_cast<long>(u->timestamp), u->nick.c_str(), u->host.c_str(), u->host.c_str(), u->GetIdent().c_str(), static_cast<long>(u->my_signon), modes.c_str(), u->realname.c_str());
	}

	void SendKickInternal(const BotInfo *source, const Channel *chan, const User *user, const Anope::string &buf)
	{
		if (!buf.empty())
			send_cmd(source->GetUID(), "KICK %s %s :%s", chan->name.c_str(), user->GetUID().c_str(), buf.c_str());
		else
			send_cmd(source->GetUID(), "KICK %s %s :%s", chan->name.c_str(), user->GetUID().c_str(), user->nick.c_str());
	}

	void SendNoticeChanopsInternal(const BotInfo *source, const Channel *dest, const Anope::string &buf)
	{
		send_cmd(TS6SID, "NOTICE @%s :%s", dest->name.c_str(), buf.c_str());
	}

	/* SERVER services-dev.chatspike.net password 0 :Description here */
	void SendServer(const Server *server)
	{
		send_cmd("", "SERVER %s %s %d %s :%s", server->GetName().c_str(), currentpass.c_str(), server->GetHops(), server->GetSID().c_str(), server->GetDescription().c_str());
	}

	/* JOIN */
	void SendJoin(const BotInfo *user, const Anope::string &channel, time_t chantime)
	{
		send_cmd(TS6SID, "FJOIN %s %ld + :,%s", channel.c_str(), static_cast<long>(chantime), user->GetUID().c_str());
	}

	void SendJoin(BotInfo *user, const ChannelContainer *cc)
	{
		send_cmd(TS6SID, "FJOIN %s %ld +%s :%s,%s", cc->chan->name.c_str(), static_cast<long>(cc->chan->creation_time), cc->chan->GetModes(true, true).c_str(), cc->Status->BuildCharPrefixList().c_str(), user->GetUID().c_str());
	}

	/* UNSQLINE */
	void SendSQLineDel(const XLine *x)
	{
		send_cmd(TS6SID, "DELLINE Q %s", x->Mask.c_str());
	}

	/* SQLINE */
	void SendSQLine(const XLine *x)
	{
		send_cmd(TS6SID, "ADDLINE Q %s %s %ld 0 :%s", x->Mask.c_str(), Config->s_OperServ.c_str(), static_cast<long>(Anope::CurTime), x->Reason.c_str());
	}

	/* SQUIT */
	void SendSquit(const Anope::string &servname, const Anope::string &message)
	{
		send_cmd(TS6SID, "SQUIT %s :%s", servname.c_str(), message.c_str());
	}

	/* Functions that use serval cmd functions */

	void SendVhost(User *u, const Anope::string &vIdent, const Anope::string &vhost)
	{
		if (!vIdent.empty())
			inspircd_cmd_chgident(u->nick, vIdent);
		if (!vhost.empty())
			inspircd_cmd_chghost(u->nick, vhost);
	}

	void SendConnect()
	{
		inspircd_cmd_pass(uplink_server->password);
		SendServer(Me);
		send_cmd(TS6SID, "BURST");
		send_cmd(TS6SID, "VERSION :Anope-%s %s :%s - (%s) -- %s", Anope::Version().c_str(), Config->ServerName.c_str(), ircd->name, Config->EncModuleList.begin()->c_str(), Anope::Build().c_str());
	}

	/* CHGIDENT */
	void inspircd_cmd_chgident(const Anope::string &nick, const Anope::string &vIdent)
	{
		if (!has_chgidentmod)
			ircdproto->SendGlobops(OperServ, "CHGIDENT not loaded!");
		else
			send_cmd(HostServ ? HostServ->GetUID() : TS6SID, "CHGIDENT %s %s", nick.c_str(), vIdent.c_str());
	}

	/* SVSHOLD - set */
	void SendSVSHold(const Anope::string &nick)
	{
		send_cmd(NickServ->GetUID(), "SVSHOLD %s %u :Being held for registered user", nick.c_str(), static_cast<unsigned>(Config->NSReleaseTimeout));
	}

	/* SVSHOLD - release */
	void SendSVSHoldDel(const Anope::string &nick)
	{
		send_cmd(NickServ->GetUID(), "SVSHOLD %s", nick.c_str());
	}

	/* UNSZLINE */
	void SendSZLineDel(const XLine *x)
	{
		send_cmd(TS6SID, "DELLINE Z %s", x->Mask.c_str());
	}

	/* SZLINE */
	void SendSZLine(const XLine *x)
	{
		send_cmd(TS6SID, "ADDLINE Z %s %s %ld 0 :%s", x->Mask.c_str(), x->By.c_str(), static_cast<long>(Anope::CurTime), x->Reason.c_str());
	}

	void SendSVSJoin(const Anope::string &source, const Anope::string &nick, const Anope::string &chan, const Anope::string &)
	{
		User *u = finduser(nick);
		BotInfo *bi = findbot(source);
		send_cmd(bi->GetUID(), "SVSJOIN %s %s", u->GetUID().c_str(), chan.c_str());
	}

	void SendSVSPart(const Anope::string &source, const Anope::string &nick, const Anope::string &chan)
	{
		User *u = finduser(nick);
		BotInfo *bi = findbot(source);
		send_cmd(bi->GetUID(), "SVSPART %s %s", u->GetUID().c_str(), chan.c_str());
	}

	void SendSWhois(const Anope::string &, const Anope::string &who, const Anope::string &mask)
	{
		User *u = finduser(who);

		send_cmd(TS6SID, "METADATA %s swhois :%s", u->GetUID().c_str(), mask.c_str());
	}

	void SendBOB()
	{
		send_cmd(TS6SID, "BURST %ld", static_cast<long>(Anope::CurTime));
	}

	void SendEOB()
	{
		send_cmd(TS6SID, "ENDBURST");
	}

	void SendGlobopsInternal(BotInfo *source, const Anope::string &buf)
	{
		if (has_globopsmod)
			send_cmd(source ? source->GetUID() : TS6SID, "SNONOTICE g :%s", buf.c_str());
		else
			send_cmd(source ? source->GetUID() : TS6SID, "SNONOTICE A :%s", buf.c_str());
	}

	void SendAccountLogin(const User *u, const NickCore *account)
	{
		send_cmd(TS6SID, "METADATA %s accountname :%s", u->GetUID().c_str(), account->display.c_str());
	}

	void SendAccountLogout(const User *u, const NickCore *account)
	{
		send_cmd(TS6SID, "METADATA %s accountname :", u->GetUID().c_str());
	}

	bool IsNickValid(const Anope::string &nick)
	{
		/* InspIRCd, like TS6, uses UIDs on collision, so... */
		if (isdigit(nick[0]))
			return false;

		return true;
	}
} ircd_proto;

bool event_ftopic(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* :source FTOPIC channel ts setby :topic */
	if (params.size() < 4)
		return true;

	Channel *c = findchan(params[0]);
	if (!c)
	{
		Log(LOG_DEBUG) << "TOPIC " << params[3] << " for nonexistent channel " << params[0];
		return true;
	}

	c->ChangeTopicInternal(params[2], params[3], Anope::string(params[1]).is_pos_number_only() ? convertTo<time_t>(params[1]) : Anope::CurTime);

	return true;
}

bool event_mode(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params[0][0] == '#' || params[0][0] == '&')
		do_cmode(source, params[0], params[2], params[1]);
	else
	{
		/* InspIRCd lets opers change another
		   users modes, we have to kludge this
		   as it slightly breaks RFC1459
		 */
		User *u = finduser(source);
		User *u2 = finduser(params[0]);

		// This can happen with server-origin modes.
		if (!u)
			u = u2;

		// if it's still null, drop it like fire.
		// most likely situation was that server introduced a nick which we subsequently akilled
		if (!u || !u2)
			return true;

		do_umode(u->nick, u2->nick, params[1]);
	}
	return true;
}

bool event_opertype(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* opertype is equivalent to mode +o because servers
	   dont do this directly */
	User *u = finduser(source);
	if (u && !is_oper(u))
	{
		std::vector<Anope::string> newparams;
		newparams.push_back(source);
		newparams.push_back("+o");
		return event_mode(source, newparams);
	}

	return true;
}

bool event_fmode(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* :source FMODE #test 12345678 +nto foo */
	if (params.size() < 3)
		return true;

	Channel *c = findchan(params[0]);
	if (!c)
		return true;
	
	/* Checking the TS for validity to avoid desyncs */
	time_t ts = Anope::string(params[1]).is_pos_number_only() ? convertTo<time_t>(params[1]) : 0;
	if (c->creation_time > ts)
	{
		/* Our TS is bigger, we should lower it */
		c->creation_time = ts;
		c->Reset();
	}
	else if (c->creation_time < ts)
		/* The TS we got is bigger, we should ignore this message. */
		return true;

	/* TS's are equal now, so we can proceed with parsing */
	std::vector<Anope::string> newparams;
	newparams.push_back(params[0]);
	newparams.push_back(params[1]);
	Anope::string modes = params[2];
	for (unsigned n = 3; n < params.size(); ++n)
		modes += " " + params[n];
	newparams.push_back(modes);

	return event_mode(source, newparams);
}

/*
 * [Nov 03 22:31:57.695076 2009] debug: Received: :964 FJOIN #test 1223763723 +BPSnt :,964AAAAAB ,964AAAAAC ,966AAAAAA
 *
 * 0: name
 * 1: channel ts (when it was created, see protocol docs for more info)
 * 2: channel modes + params (NOTE: this may definitely be more than one param!)
 * last: users
 */
bool event_fjoin(const Anope::string &source, const std::vector<Anope::string> &params)
{
	Channel *c = findchan(params[0]);
	time_t ts = Anope::string(params[1]).is_pos_number_only() ? convertTo<time_t>(params[1]) : 0;
	bool keep_their_modes = true;

	if (!c)
	{
		c = new Channel(params[0], ts);
		c->SetFlag(CH_SYNCING);
	}
	/* Our creation time is newer than what the server gave us */
	else if (c->creation_time > ts)
	{
		c->creation_time = ts;
		c->Reset();

		/* Reset mlock */
		check_modes(c);
	}
	/* Their TS is newer than ours, our modes > theirs, unset their modes if need be */
	else if (ts > c->creation_time)
		keep_their_modes = false;

	/* If we need to keep their modes, and this FJOIN string contains modes */
	if (keep_their_modes && params.size() >= 3)
	{
		Anope::string modes;
		for (unsigned i = 2; i < params.size() - 1; ++i)
			modes += " " + params[i];
		if (!modes.empty())
			modes.erase(modes.begin());
		/* Set the modes internally */
		c->SetModesInternal(NULL, modes);
	}

	spacesepstream sep(params[params.size() - 1]);
	Anope::string buf;
	while (sep.GetToken(buf))
	{
		std::list<ChannelMode *> Status;

		/* Loop through prefixes and find modes for them */
		while (buf[0] != ',')
		{
			ChannelMode *cm = ModeManager::FindChannelModeByChar(buf[0]);
			if (!cm)
			{
				Log() << "Receeved unknown mode prefix " << buf[0] << " in FJOIN string";
				buf.erase(buf.begin());
				continue;
			}

			buf.erase(buf.begin());
			if (keep_their_modes)
				Status.push_back(cm);
		}
		buf.erase(buf.begin());

		User *u = finduser(buf);
		if (!u)
		{
			Log(LOG_DEBUG) << "FJOIN for nonexistant user " << buf << " on " << c->name;
			continue;
		}

		EventReturn MOD_RESULT;
		FOREACH_RESULT(I_OnPreJoinChannel, OnPreJoinChannel(u, c));

		/* Add the user to the channel */
		c->JoinUser(u);

		/* Update their status internally on the channel
		 * This will enforce secureops etc on the user
		 */
		for (std::list<ChannelMode *>::iterator it = Status.begin(), it_end = Status.end(); it != it_end; ++it)
			c->SetModeInternal(*it, buf);

		/* Now set whatever modes this user is allowed to have on the channel */
		chan_set_correct_modes(u, c, 1);

		/* Check to see if modules want the user to join, if they do
		 * check to see if they are allowed to join (CheckKick will kick/ban them)
		 * Don't trigger OnJoinChannel event then as the user will be destroyed
		 */
		if (MOD_RESULT != EVENT_STOP && c->ci && c->ci->CheckKick(u))
			continue;

		FOREACH_MOD(I_OnJoinChannel, OnJoinChannel(u, c));
	}

	/* Channel is done syncing */
	if (c->HasFlag(CH_SYNCING))
	{
		/* Unset the syncing flag */
		c->UnsetFlag(CH_SYNCING);
		c->Sync();
	}

	return true;
}

/* Events */
bool event_ping(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params.size() == 1)
		ircdproto->SendPong("", params[0]);
	else if (params.size() == 2)
		ircdproto->SendPong(params[1], params[0]);

	return true;
}

bool event_time(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params.size() < 2)
		return true;

	send_cmd(TS6SID, "TIME %s %s %ld", source.c_str(), params[1].c_str(), static_cast<long>(Anope::CurTime));

	/* We handled it, don't pass it on to the core..
	 * The core doesn't understand our syntax anyways.. ~ Viper */
	return MOD_STOP;
}

bool event_436(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (!params.empty())
		m_nickcoll(params[0]);
	return true;
}

bool event_away(const Anope::string &source, const std::vector<Anope::string> &params)
{
	m_away(source, !params.empty() ? params[0] : "");
	return true;
}

/* Taken from hybrid.c, topic syntax is identical */
bool event_topic(const Anope::string &source, const std::vector<Anope::string> &params)
{
	Channel *c = findchan(params[0]);

	if (!c)
	{
		Log() << "TOPIC " << params[1] << " for nonexistent channel " << params[0];
		return true;
	}

	c->ChangeTopicInternal(source, (params.size() > 1 ? params[1] : ""), Anope::CurTime);

	return true;
}

bool event_squit(const Anope::string &source, const std::vector<Anope::string> &params)
{
	do_squit(source, params[0]);
	return true;
}

bool event_rsquit(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* On InspIRCd we must send a SQUIT when we recieve RSQUIT for a server we have juped */
	Server *s = Server::Find(params[0]);
	if (s && s->HasFlag(SERVER_JUPED))
		send_cmd(TS6SID, "SQUIT %s :%s", s->GetSID().c_str(), params.size() > 1 ? params[1].c_str() : "");

	do_squit(source, params[0]);

	return true;
}

bool event_quit(const Anope::string &source, const std::vector<Anope::string> &params)
{
	do_quit(source, params[0]);
	return true;
}

bool event_kill(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(params[0]);
	BotInfo *bi = findbot(params[0]);
	m_kill(u ? u->nick : (bi ? bi->nick : params[0]), params[1]);
	return true;
}

bool event_kick(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params.size() > 2)
		do_kick(source, params[0], params[1], params[2]);
	return true;
}

bool event_join(const Anope::string &source, const std::vector<Anope::string> &params)
{
	do_join(source, params[0], (params.size() > 1 ? params[1] : ""));
	return true;
}

bool event_motd(const Anope::string &source, const std::vector<Anope::string> &params)
{
	m_motd(source);
	return true;
}

bool event_setname(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);
	if (!u)
	{
		Log(LOG_DEBUG) << "SETNAME for nonexistent user " << source;
		return true;
	}

	u->SetRealname(params[0]);
	return true;
}

bool event_chgname(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);

	if (!u)
	{
		Log(LOG_DEBUG) << "FNAME for nonexistent user " << source;
		return true;
	}

	u->SetRealname(params[0]);
	return true;
}

bool event_setident(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);

	if (!u)
	{
		Log(LOG_DEBUG) << "SETIDENT for nonexistent user " << source;
		return true;
	}

	u->SetIdent(params[0]);
	return true;
}

bool event_chgident(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(params[0]);

	if (!u)
	{
		Log(LOG_DEBUG) << "CHGIDENT for nonexistent user " << params[0];
		return true;
	}

	u->SetIdent(params[1]);
	return true;
}

bool event_sethost(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);

	if (!u)
	{
		Log(LOG_DEBUG) << "SETHOST for nonexistent user " << source;
		return true;
	}

	u->SetDisplayedHost(params[0]);
	return true;
}

bool event_nick(const Anope::string &source, const std::vector<Anope::string> &params)
{
	do_nick(source, params[0], "", "", "", "", 0, "", "", "", "");
	return true;
}

/*
 * [Nov 03 22:09:58.176252 2009] debug: Received: :964 UID 964AAAAAC 1225746297 w00t2 localhost testnet.user w00t 127.0.0.1 1225746302 +iosw +ACGJKLNOQcdfgjklnoqtx :Robin Burchell <w00t@inspircd.org>
 * 0: uid
 * 1: ts
 * 2: nick
 * 3: host
 * 4: dhost
 * 5: ident
 * 6: ip
 * 7: signon
 * 8+: modes and params -- IMPORTANT, some modes (e.g. +s) may have parameters. So don't assume a fixed position of realname!
 * last: realname
 */

bool event_uid(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *user;
	Server *s = Server::Find(source);
	time_t ts = convertTo<time_t>(params[1]);

	/* Check if the previously introduced user was Id'd for the nickgroup of the nick he s currently using.
	 * If not, validate the user.  ~ Viper*/
	user = prev_u_intro;
	prev_u_intro = NULL;
	if (user && !user->server->IsSynced())
	{
		NickAlias *na = findnick(user->nick);

		if (!na || na->nc != user->Account())
		{
			validate_user(user);
			if (user->HasMode(UMODE_REGISTERED))
				user->RemoveMode(NickServ, UMODE_REGISTERED);
		}
		else
			/* Set them +r (to negate a possible -r on the stack) */
			user->SetMode(NickServ, UMODE_REGISTERED);
	}

	Anope::string modes = params[8];
	for (unsigned i = 9; i < params.size() - 1; ++i)
		modes += " " + params[i];
	user = do_nick("", params[2], params[5], params[3], s->GetName(), params[params.size() - 1], ts, params[6], params[4], params[0], modes);
	if (user)
	{
		if (!user->server->IsSynced())
			prev_u_intro = user;
		else
			validate_user(user);
	}

	return true;
}

bool event_chghost(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);

	if (!u)
	{
		Log(LOG_DEBUG) << "FHOST for nonexistent user " << source;
		return true;
	}

	u->SetDisplayedHost(params[0]);
	return true;
}

/*
 * [Nov 04 00:08:46.308435 2009] debug: Received: SERVER irc.inspircd.com pass 0 964 :Testnet Central!
 * 0: name
 * 1: pass
 * 2: hops
 * 3: numeric
 * 4: desc
 */
bool event_server(const Anope::string &source, const std::vector<Anope::string> &params)
{
	do_server(source, params[0], Anope::string(params[2]).is_pos_number_only() ? convertTo<unsigned>(params[2]) : 0, params[4], params[3]);
	return true;
}

bool event_privmsg(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (!finduser(source))
		return true; // likely a message from a server, which can happen.

	m_privmsg(source, params[0], params[1]);
	return true;
}

bool event_part(const Anope::string &source, const std::vector<Anope::string> &params)
{
	do_part(source, params[0], (params.size() > 1 ? params[1] : ""));
	return true;
}

bool event_whois(const Anope::string &source, const std::vector<Anope::string> &params)
{
	m_whois(source, params[0]);
	return true;
}

bool event_metadata(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params.size() < 3)
		return true;
	if (params[1].equals_cs("accountname"))
	{
		User *u = finduser(params[0]);
		NickCore *nc = findcore(params[2]);
		if (u && nc)
		{
			u->Login(nc);
		}
	}

	return true;
}

bool event_capab(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params[0].equals_cs("START"))
	{
		/* reset CAPAB */
		has_servicesmod = false;
		has_globopsmod = false;
		has_svsholdmod = false;
		has_chghostmod = false;
		has_chgidentmod = false;
		has_hidechansmod = false;
	}
	else if (params[0].equals_cs("MODULES"))
	{
		if (params[1].find("m_globops.so") != Anope::string::npos)
			has_globopsmod = true;
		if (params[1].find("m_services_account.so") != Anope::string::npos)
			has_servicesmod = true;
		if (params[1].find("m_svshold.so") != Anope::string::npos)
			has_svsholdmod = true;
		if (params[1].find("m_chghost.so") != Anope::string::npos)
			has_chghostmod = true;
		if (params[1].find("m_chgident.so") != Anope::string::npos)
			has_chgidentmod = true;
		if (params[1].find("m_hidechans.so") != Anope::string::npos)
			has_hidechansmod = true;
		if (params[1].find("m_servprotect.so") != Anope::string::npos)
			ircd->pseudoclient_mode = "+Ik";
	}
	else if (params[0].equals_cs("CAPABILITIES"))
	{
		spacesepstream ssep(params[1]);
		Anope::string capab;
		while (ssep.GetToken(capab))
		{
			if (capab.find("CHANMODES") != Anope::string::npos)
			{
				Anope::string modes(capab.begin() + 10, capab.end());
				commasepstream sep(modes);
				Anope::string modebuf;

				sep.GetToken(modebuf);
				for (size_t t = 0, end = modebuf.length(); t < end; ++t)
				{
					switch (modebuf[t])
					{
						case 'b':
							ModeManager::AddChannelMode(new ChannelModeBan('b'));
							continue;
						case 'e':
							ModeManager::AddChannelMode(new ChannelModeExcept('e'));
							continue;
						case 'I':
							ModeManager::AddChannelMode(new ChannelModeInvex('I'));
							continue;
						/* InspIRCd sends q and a here if they have no prefixes */
						case 'q':
							ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_OWNER, "CMODE_OWNER", 'q', '@'));
							continue;
						case 'a':
							ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_PROTECT , "CMODE_PROTECT", 'a', '@'));
							continue;
						// XXX list modes needs a bit of a rewrite, we need to be able to support +g here
						default:
							ModeManager::AddChannelMode(new ChannelModeList(CMODE_END, "", modebuf[t]));
					}
				}

				sep.GetToken(modebuf);
				for (size_t t = 0, end = modebuf.length(); t < end; ++t)
				{
					switch (modebuf[t])
					{
						case 'k':
							ModeManager::AddChannelMode(new ChannelModeKey('k'));
							continue;
						default:
							ModeManager::AddChannelMode(new ChannelModeParam(CMODE_END, "", modebuf[t]));
					}
				}

				sep.GetToken(modebuf);
				for (size_t t = 0, end = modebuf.length(); t < end; ++t)
				{
					switch (modebuf[t])
					{
						case 'F':
							ModeManager::AddChannelMode(new ChannelModeParam(CMODE_NICKFLOOD, "CMODE_NICKFLOOD", 'F', true));
							continue;
						case 'J':
							ModeManager::AddChannelMode(new ChannelModeParam(CMODE_NOREJOIN, "CMODE_NOREJOIN", 'J', true));
							continue;
						case 'L':
							ModeManager::AddChannelMode(new ChannelModeParam(CMODE_REDIRECT, "CMODE_REDIRECT", 'L', true));
							continue;
						case 'f':
							ModeManager::AddChannelMode(new ChannelModeFlood('f', true));
							continue;
						case 'j':
							ModeManager::AddChannelMode(new ChannelModeParam(CMODE_JOINFLOOD, "CMODE_JOINFLOOD", 'j', true));
							continue;
						case 'l':
							ModeManager::AddChannelMode(new ChannelModeParam(CMODE_LIMIT, "CMODE_LIMIT", 'l', true));
							continue;
						default:
							ModeManager::AddChannelMode(new ChannelModeParam(CMODE_END, "", modebuf[t], true));
					}
				}

				sep.GetToken(modebuf);
				for (size_t t = 0, end = modebuf.length(); t < end; ++t)
				{
					switch (modebuf[t])
					{
						case 'A':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_ALLINVITE, "CMODE_ALLINVITE", 'A'));
							continue;
						case 'B':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_BLOCKCAPS, "CMODE_BLOCKCAPS", 'B'));
							continue;
						case 'C':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_NOCTCP, "CMODE_NOCTCP", 'C'));
							continue;
						case 'D':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_DELAYEDJOIN, "CMODE_DELAYEDJOIN", 'D'));
							continue;
						case 'G':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_FILTER, "CMODE_FILTER", 'G'));
							continue;
						case 'K':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_NOKNOCK, "CMODE_NOKNOCK", 'K'));
							continue;
						case 'M':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_REGMODERATED, "CMODE_REGMODERATED", 'M'));
							continue;
						case 'N':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_NONICK, "CMODE_NONICK", 'N'));
							continue;
						case 'O':
							ModeManager::AddChannelMode(new ChannelModeOper('O'));
							continue;
						case 'P':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_PERM, "CMODE_PERM", 'P'));
							continue;
						case 'Q':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_NOKICK, "CMODE_NOKICK", 'Q'));
							continue;
						case 'R':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_REGISTEREDONLY, "CMODE_REGISTEREDONLY", 'R'));
							continue;
						case 'S':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_STRIPCOLOR, "CMODE_STRIPCOLOR", 'S'));
							continue;
						case 'T':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_NONOTICE, "CMODE_NONOTICE", 'T'));
							continue;
						case 'c':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_BLOCKCOLOR, "CMODE_BLOCKCOLOR", 'c'));
							continue;
						case 'i':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_INVITE, "CMODE_INVITE", 'i'));
							continue;
						case 'm':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_MODERATED, "CMODE_MODERATED", 'm'));
							continue;
						case 'n':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_NOEXTERNAL, "CMODE_NOEXTERNAL", 'n'));
							continue;
						case 'p':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_PRIVATE, "CMODE_PRIVATE", 'p'));
							continue;
						case 'r':
							ModeManager::AddChannelMode(new ChannelModeRegistered('r'));
							continue;
						case 's':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_SECRET, "CMODE_SECRET", 's'));
							continue;
						case 't':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_TOPIC, "CMODE_TOPIC", 't'));
							continue;
						case 'u':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_AUDITORIUM, "CMODE_AUDITORIUM", 'u'));
							continue;
						case 'z':
							ModeManager::AddChannelMode(new ChannelMode(CMODE_SSL, "CMODE_SSL", 'z'));
							continue;
						default:
							ModeManager::AddChannelMode(new ChannelMode(CMODE_END, "", modebuf[t]));
					}
				}
			}
			else if (capab.find("USERMODES") != Anope::string::npos)
			{
				Anope::string modes(capab.begin() + 10, capab.end());
				commasepstream sep(modes);
				Anope::string modebuf;

				while (sep.GetToken(modebuf))
				{
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						switch (modebuf[t])
						{
							case 'h':
								ModeManager::AddUserMode(new UserMode(UMODE_HELPOP, "UMODE_HELPOP", 'h'));
								continue;
							case 'B':
								ModeManager::AddUserMode(new UserMode(UMODE_BOT, "UMODE_BOT", 'B'));
								continue;
							case 'G':
								ModeManager::AddUserMode(new UserMode(UMODE_FILTER, "UMODE_FILTER", 'G'));
								continue;
							case 'H':
								ModeManager::AddUserMode(new UserMode(UMODE_HIDEOPER, "UMODE_HIDEOPER", 'H'));
								continue;
							case 'I':
								ModeManager::AddUserMode(new UserMode(UMODE_PRIV, "UMODE_PRIV", 'I'));
								continue;
							case 'Q':
								ModeManager::AddUserMode(new UserMode(UMODE_HIDDEN, "UMODE_HIDDEN", 'Q'));
								continue;
							case 'R':
								ModeManager::AddUserMode(new UserMode(UMODE_REGPRIV, "UMODE_REGPRIV", 'R'));
								continue;
							case 'S':
								ModeManager::AddUserMode(new UserMode(UMODE_STRIPCOLOR, "UMODE_STRIPCOLOR", 'S'));
								continue;
							case 'W':
								ModeManager::AddUserMode(new UserMode(UMODE_WHOIS, "UMODE_WHOIS", 'W'));
								continue;
							case 'c':
								ModeManager::AddUserMode(new UserMode(UMODE_COMMONCHANS, "UMODE_COMMONCHANS", 'c'));
								continue;
							case 'g':
								ModeManager::AddUserMode(new UserMode(UMODE_CALLERID, "UMODE_CALLERID", 'g'));
								continue;
							case 'i':
								ModeManager::AddUserMode(new UserMode(UMODE_INVIS, "UMODE_INVIS", 'i'));
								continue;
							case 'k':
								ModeManager::AddUserMode(new UserMode(UMODE_PROTECTED, "UMODE_PROTECTED", 'k'));
								continue;
							case 'o':
								ModeManager::AddUserMode(new UserMode(UMODE_OPER, "UMODE_OPER", 'o'));
								continue;
							case 'r':
								ModeManager::AddUserMode(new UserMode(UMODE_REGISTERED, "UMODE_REGISTERED", 'r'));
								continue;
							case 'w':
								ModeManager::AddUserMode(new UserMode(UMODE_WALLOPS, "UMODE_WALLOPS", 'w'));
								continue;
							case 'x':
								ModeManager::AddUserMode(new UserMode(UMODE_CLOAK, "UMODE_CLOAK", 'x'));
								continue;
							case 'd':
								ModeManager::AddUserMode(new UserMode(UMODE_DEAF, "UMODE_DEAF", 'd'));
								continue;
							default:
								ModeManager::AddUserMode(new UserMode(UMODE_END, "", modebuf[t]));
						}
					}
				}
			}
			else if (capab.find("PREFIX=(") != Anope::string::npos)
			{
				Anope::string modes(capab.begin() + 8, capab.begin() + capab.find(')'));
				Anope::string chars(capab.begin() + capab.find(')') + 1, capab.end());

				for (size_t t = 0, end = modes.length(); t < end; ++t)
				{
					switch (modes[t])
					{
						case 'q':
							ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_OWNER, "CMODE_OWNER", 'q', chars[t]));
							continue;
						case 'a':
							ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_PROTECT, "CMODE_PROTECT", 'a', chars[t]));
							continue;
						case 'o':
							ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_OP, "CMODE_OP", 'o', chars[t]));
							continue;
						case 'h':
							ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_HALFOP, "CMODE_HALFOP", 'h', chars[t]));
							continue;
						case 'v':
							ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_VOICE, "CMODE_VOICE", 'v', chars[t]));
							continue;
					}
				}
			}
			else if (capab.find("MAXMODES=") != Anope::string::npos)
			{
				Anope::string maxmodes(capab.begin() + 9, capab.end());
				ircd->maxmodes = maxmodes.is_pos_number_only() ? convertTo<unsigned>(maxmodes) : 3;
			}
		}
	}
	else if (params[0].equals_cs("END"))
	{
		if (!has_globopsmod)
		{
			send_cmd("", "ERROR :m_globops is not loaded. This is required by Anope");
			quitmsg = "Remote server does not have the m_globops module loaded, and this is required.";
			quitting = true;
			return MOD_STOP;
		}
		if (!has_servicesmod)
		{
			send_cmd("", "ERROR :m_services_account.so is not loaded. This is required by Anope");
			quitmsg = "ERROR: Remote server does not have the m_services_account module loaded, and this is required.";
			quitting = true;
			return MOD_STOP;
		}
		if (!has_hidechansmod)
		{
			send_cmd("", "ERROR :m_hidechans.so is not loaded. This is required by Anope");
			quitmsg = "ERROR: Remote server does not have the m_hidechans module loaded, and this is required.";
			quitting = true;
			return MOD_STOP;
		}
		if (!has_svsholdmod)
			ircdproto->SendGlobops(OperServ, "SVSHOLD missing, Usage disabled until module is loaded.");
		if (!has_chghostmod)
			ircdproto->SendGlobops(OperServ, "CHGHOST missing, Usage disabled until module is loaded.");
		if (!has_chgidentmod)
			ircdproto->SendGlobops(OperServ, "CHGIDENT missing, Usage disabled until module is loaded.");
		ircd->svshold = has_svsholdmod;
	}

	CapabParse(params);

	return true;
}

bool event_endburst(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = prev_u_intro;
	Server *s = Server::Find(source);

	if (!s)
		throw CoreException("Got ENDBURST without a source");

	/* Check if the previously introduced user was Id'd for the nickgroup of the nick he s currently using.
	 * If not, validate the user. ~ Viper*/
	prev_u_intro = NULL;
	if (u && !u->server->IsSynced())
	{
		NickAlias *na = findnick(u->nick);

		if (!na || na->nc != u->Account())
		{
			validate_user(u);
			if (u->HasMode(UMODE_REGISTERED))
				u->RemoveMode(NickServ, UMODE_REGISTERED);
		}
		else
			/* Set them +r (to negate a possible -r on the stack) */
			u->SetMode(NickServ, UMODE_REGISTERED);
	}

	Log(LOG_DEBUG) << "Processed ENDBURST for " << s->GetName();

	s->Sync(true);
	return true;
}

bool ChannelModeFlood::IsValid(const Anope::string &value) const
{
	Anope::string rest;
	if (!value.empty() && value[0] != ':' && convertTo<int>(value[0] == '*' ? value.substr(1) : value, rest, false) > 0 && rest[0] == ':' && rest.length() > 1 && convertTo<int>(rest.substr(1), rest, false) > 0 && rest.empty())
		return true;
	else
		return false;
}

class ProtoInspIRCd : public Module
{
	Message message_endburst, message_436, message_away, message_join, message_kick, message_kill, message_mode, message_motd,
		message_nick, message_uid, message_capab, message_part, message_ping, message_time, message_privmsg, message_quit,
		message_server, message_squit, message_rsquit, message_topic, message_whois, message_svsmode, message_fhost,
		message_chgident, message_fname, message_sethost, message_setident, message_setname, message_fjoin, message_fmode,
		message_ftopic, message_opertype, message_idle, message_metadata;
 public:
	ProtoInspIRCd(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator),
		message_endburst("ENDBURST", event_endburst), message_436("436", event_436), message_away("AWAY", event_away),
		message_join("JOIN", event_join), message_kick("KICK", event_kick), message_kill("KILL", event_kill),
		message_mode("MODE", event_mode), message_motd("MOTD", event_motd), message_nick("NICK", event_nick),
		message_uid("UID", event_uid), message_capab("CAPAB", event_capab), message_part("PART", event_part),
		message_ping("PING", event_ping), message_time("TIME", event_time), message_privmsg("PRIVMSG", event_privmsg),
		message_quit("QUIT", event_quit), message_server("SERVER", event_server), message_squit("SQUIT", event_squit),
		message_rsquit("RSQUIT", event_rsquit), message_topic("TOPIC", event_topic), message_whois("WHOIS", event_whois),
		message_svsmode("SVSMODE", event_mode), message_fhost("FHOST", event_chghost),
		message_chgident("CHGIDENT", event_chgident), message_fname("FNAME", event_chgname),
		message_sethost("SETHOST", event_sethost), message_setident("SETIDENT", event_setident),
		message_setname("SETNAME", event_setname), message_fjoin("FJOIN", event_fjoin), message_fmode("FMODE", event_fmode),
		message_ftopic("FTOPIC", event_ftopic), message_opertype("OPERTYPE", event_opertype), message_idle("IDLE", event_idle),
		message_metadata("METADATA", event_metadata)
	{
		this->SetAuthor("Anope");
		this->SetType(PROTOCOL);

		if (!Config->Numeric.empty())
			TS6SID = Config->Numeric;

		pmodule_ircd_var(myIrcd);

		CapabType c[] = { CAPAB_NOQUIT, CAPAB_SSJ3, CAPAB_NICK2, CAPAB_VL, CAPAB_TLKEXT };
		for (unsigned i = 0; i < 5; ++i)
			Capab.SetFlag(c[i]);

		pmodule_ircd_proto(&ircd_proto);

		ModuleManager::Attach(I_OnUserNickChange, this);
	}

	void OnUserNickChange(User *u, const Anope::string &)
	{
		/* InspIRCd 1.2 doesn't set -r on nick change, remove -r here. Note that if we have to set +r later
		 * this will cancel out this -r, resulting in no mode changes.
		 */
		u->RemoveMode(NickServ, UMODE_REGISTERED);
	}
};

MODULE_INIT(ProtoInspIRCd)
