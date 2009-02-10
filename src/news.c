
/* News functions.
 *
 * (C) 2003-2009 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 *
 * $Id$
 *
 */

#include "services.h"
#include "pseudo.h"

#define MSG_MAX		11
/*************************************************************************/

int32 nnews = 0;
int32 news_size = 0;
NewsItem *news = NULL;

/*************************************************************************/



/*************************************************************************/
/****************************** Statistics *******************************/
/*************************************************************************/

void get_news_stats(long *nrec, long *memuse)
{
	long mem;
	int i;

	mem = sizeof(NewsItem) * news_size;
	for (i = 0; i < nnews; i++)
		mem += strlen(news[i].text) + 1;
	*nrec = nnews;
	*memuse = mem;
}

/*************************************************************************/
/*********************** News item loading/saving ************************/
/*************************************************************************/

#define SAFE(x) do {					\
	if ((x) < 0) {					\
	if (!forceload)					\
		fatal("Read error on %s", NewsDBName);	\
	nnews = i;					\
	break;						\
	}							\
} while (0)

void load_news()
{
	dbFILE *f;
	int i;
	uint16 n;
	uint32 tmp32;

	if (!(f = open_db(s_OperServ, NewsDBName, "r", NEWS_VERSION)))
		return;
	switch (i = get_file_version(f)) {
	case 9:
	case 8:
	case 7:
		SAFE(read_int16(&n, f));
		nnews = n;
		if (nnews < 8)
			news_size = 16;
		else if (nnews >= 16384)
			news_size = 32767;
		else
			news_size = 2 * nnews;
		news = static_cast<NewsItem *>(scalloc(sizeof(*news) * news_size, 1));
		if (!nnews) {
			close_db(f);
			return;
		}
		for (i = 0; i < nnews; i++) {
			SAFE(read_int16(&news[i].type, f));
			SAFE(read_int32(&news[i].num, f));
			SAFE(read_string(&news[i].text, f));
			SAFE(read_buffer(news[i].who, f));
			SAFE(read_int32(&tmp32, f));
			news[i].time = tmp32;
		}
		break;

	default:
		fatal("Unsupported version (%d) on %s", i, NewsDBName);
	}						   /* switch (ver) */

	close_db(f);
}

#undef SAFE

/*************************************************************************/

#define SAFE(x) do {						\
	if ((x) < 0) {						\
	restore_db(f);						\
	log_perror("Write error on %s", NewsDBName);		\
	if (time(NULL) - lastwarn > WarningTimeout) {		\
		ircdproto->SendGlobops(NULL, "Write error on %s: %s", NewsDBName,	\
			strerror(errno));			\
		lastwarn = time(NULL);				\
	}							\
	return;							\
	}								\
} while (0)

void save_news()
{
	dbFILE *f;
	int i;
	static time_t lastwarn = 0;

	if (!(f = open_db(s_OperServ, NewsDBName, "w", NEWS_VERSION)))
		return;
	SAFE(write_int16(nnews, f));
	for (i = 0; i < nnews; i++) {
		SAFE(write_int16(news[i].type, f));
		SAFE(write_int32(news[i].num, f));
		SAFE(write_string(news[i].text, f));
		SAFE(write_buffer(news[i].who, f));
		SAFE(write_int32(news[i].time, f));
	}
	close_db(f);
}

#undef SAFE

/*************************************************************************/
/***************************** News display ******************************/
/*************************************************************************/

void display_news(User * u, int16 type)
{
	int msg;

	if (type == NEWS_LOGON) {
		msg = NEWS_LOGON_TEXT;
	} else if (type == NEWS_OPER) {
		msg = NEWS_OPER_TEXT;
	} else if (type == NEWS_RANDOM) {
		msg = NEWS_RANDOM_TEXT;
	} else {
		alog("news: Invalid type (%d) to display_news()", type);
		return;
	}

	if (type == NEWS_RANDOM) {
		static int current_news = -1;
		int count = 0;

		if (!nnews)
			return;

		while (count++ < nnews) {
			if (++current_news >= nnews)
				current_news = 0;

			if (news[current_news].type == type) {
				struct tm *tm;
				char timebuf[64];

				tm = localtime(&news[current_news].time);
				strftime_lang(timebuf, sizeof(timebuf), u,
							  STRFTIME_SHORT_DATE_FORMAT, tm);
				notice_lang(s_GlobalNoticer, u, msg, timebuf,
							news[current_news].text);

				return;
			}
		}
	} else {
		int i;
		unsigned count = 0;

		for (i = nnews - 1; i >= 0; i--) {
			if (count >= NewsCount)
				break;
			if (news[i].type == type)
				count++;
		}
		while (++i < nnews) {
			if (news[i].type == type) {
				struct tm *tm;
				char timebuf[64];

				tm = localtime(&news[i].time);
				strftime_lang(timebuf, sizeof(timebuf), u,
							  STRFTIME_SHORT_DATE_FORMAT, tm);
				notice_lang(s_GlobalNoticer, u, msg, timebuf,
							news[i].text);
			}
		}
	}
}

/*************************************************************************/
/***************************** News editing ******************************/
/*************************************************************************/

/* Actually add a news item.  Return the number assigned to the item, or -1
 * if the news list is full (32767 items).
 */

int add_newsitem(User * u, const char *text, short type)
{
	int i, num;

	if (nnews >= 32767)
		return -1;

	if (nnews >= news_size) {
		if (news_size < 8)
			news_size = 8;
		else
			news_size *= 2;
		news = static_cast<NewsItem *>(srealloc(news, sizeof(*news) * news_size));
	}
	num = 0;
	for (i = nnews - 1; i >= 0; i--) {
		if (news[i].type == type) {
			num = news[i].num;
			break;
		}
	}
	news[nnews].type = type;
	news[nnews].num = num + 1;
	news[nnews].text = sstrdup(text);
	news[nnews].time = time(NULL);
	strscpy(news[nnews].who, u->nick, NICKMAX);
	nnews++;
	return num + 1;
}

/*************************************************************************/

/* Actually delete a news item.  If `num' is 0, delete all news items of
 * the given type.  Returns the number of items deleted.
 */

int del_newsitem(int num, short type)
{
	int i;
	int count = 0;

	for (i = 0; i < nnews; i++) {
		if (news[i].type == type && (num == 0 || news[i].num == num)) {
			delete [] news[i].text;
			count++;
			nnews--;
			if (i < nnews)
				memcpy(news + i, news + i + 1,
					   sizeof(*news) * (nnews - i));
			i--;
		}
	}
	return count;
}

/*************************************************************************/
