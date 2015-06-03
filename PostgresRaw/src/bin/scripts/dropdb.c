/*-------------------------------------------------------------------------
 *
 * dropdb
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/bin/scripts/dropdb.c,v 1.28 2010/01/02 16:58:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include "common.h"
#include "dumputils.h"


static void help(const char *progname);


int
main(int argc, char *argv[])
{
	static struct option long_options[] = {
		{"host", required_argument, NULL, 'h'},
		{"port", required_argument, NULL, 'p'},
		{"username", required_argument, NULL, 'U'},
		{"no-password", no_argument, NULL, 'w'},
		{"password", no_argument, NULL, 'W'},
		{"echo", no_argument, NULL, 'e'},
		{"interactive", no_argument, NULL, 'i'},
		{NULL, 0, NULL, 0}
	};

	const char *progname;
	int			optindex;
	int			c;

	char	   *dbname = NULL;
	char	   *host = NULL;
	char	   *port = NULL;
	char	   *username = NULL;
	enum trivalue prompt_password = TRI_DEFAULT;
	bool		echo = false;
	bool		interactive = false;

	PQExpBufferData sql;

	PGconn	   *conn;
	PGresult   *result;

	progname = get_progname(argv[0]);
	set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pgscripts"));

	handle_help_version_opts(argc, argv, "dropdb", help);

	while ((c = getopt_long(argc, argv, "h:p:U:wWei", long_options, &optindex)) != -1)
	{
		switch (c)
		{
			case 'h':
				host = optarg;
				break;
			case 'p':
				port = optarg;
				break;
			case 'U':
				username = optarg;
				break;
			case 'w':
				prompt_password = TRI_NO;
				break;
			case 'W':
				prompt_password = TRI_YES;
				break;
			case 'e':
				echo = true;
				break;
			case 'i':
				interactive = true;
				break;
			default:
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
		}
	}

	switch (argc - optind)
	{
		case 0:
			fprintf(stderr, _("%s: missing required argument database name\n"), progname);
			fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
			exit(1);
		case 1:
			dbname = argv[optind];
			break;
		default:
			fprintf(stderr, _("%s: too many command-line arguments (first is \"%s\")\n"),
					progname, argv[optind + 1]);
			fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
			exit(1);
	}

	if (interactive)
	{
		printf(_("Database \"%s\" will be permanently removed.\n"), dbname);
		if (!yesno_prompt("Are you sure?"))
			exit(0);
	}

	initPQExpBuffer(&sql);

	appendPQExpBuffer(&sql, "DROP DATABASE %s;\n",
					  fmtId(dbname));

	conn = connectDatabase(strcmp(dbname, "postgres") == 0 ? "template1" : "postgres",
						   host, port, username, prompt_password, progname);

	if (echo)
		printf("%s", sql.data);
	result = PQexec(conn, sql.data);
	if (PQresultStatus(result) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, _("%s: database removal failed: %s"),
				progname, PQerrorMessage(conn));
		PQfinish(conn);
		exit(1);
	}

	PQclear(result);
	PQfinish(conn);
	exit(0);
}


static void
help(const char *progname)
{
	printf(_("%s removes a PostgreSQL database.\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION]... DBNAME\n"), progname);
	printf(_("\nOptions:\n"));
	printf(_("  -e, --echo                show the commands being sent to the server\n"));
	printf(_("  -i, --interactive         prompt before deleting anything\n"));
	printf(_("  --help                    show this help, then exit\n"));
	printf(_("  --version                 output version information, then exit\n"));
	printf(_("\nConnection options:\n"));
	printf(_("  -h, --host=HOSTNAME       database server host or socket directory\n"));
	printf(_("  -p, --port=PORT           database server port\n"));
	printf(_("  -U, --username=USERNAME   user name to connect as\n"));
	printf(_("  -w, --no-password         never prompt for password\n"));
	printf(_("  -W, --password            force password prompt\n"));
	printf(_("\nReport bugs to <pgsql-bugs@postgresql.org>.\n"));
}
