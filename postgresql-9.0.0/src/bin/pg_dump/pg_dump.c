/*-------------------------------------------------------------------------
 *
 * pg_dump.c
 *	  pg_dump is a utility for dumping out a postgres database
 *	  into a script file.
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	pg_dump will read the system catalogs in a database and dump out a
 *	script that reproduces the schema in terms of SQL that is understood
 *	by PostgreSQL
 *
 *	Note that pg_dump runs in a serializable transaction, so it sees a
 *	consistent snapshot of the database including system catalogs.
 *	However, it relies in part on various specialized backend functions
 *	like pg_get_indexdef(), and those things tend to run on SnapshotNow
 *	time, ie they look at the currently committed state.  So it is
 *	possible to get 'cache lookup failed' error if someone performs DDL
 *	changes while a dump is happening. The window for this sort of thing
 *	is from the beginning of the serializable transaction to
 *	getSchemaData() (when pg_dump acquires AccessShareLock on every
 *	table it intends to dump). It isn't very large, but it can happen.
 *
 *	http://archives.postgresql.org/pgsql-bugs/2010-02/msg00187.php
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/bin/pg_dump/pg_dump.c,v 1.581.2.2 2010/08/13 14:38:12 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <unistd.h>
#include <ctype.h>
#ifdef ENABLE_NLS
#include <locale.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "getopt_long.h"

#include "access/attnum.h"
#include "access/sysattr.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_class.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "libpq/libpq-fs.h"

#include "pg_backup_archiver.h"
#include "dumputils.h"

extern char *optarg;
extern int	optind,
			opterr;


typedef struct
{
	const char *descr;			/* comment for an object */
	Oid			classoid;		/* object class (catalog OID) */
	Oid			objoid;			/* object OID */
	int			objsubid;		/* subobject (table column #) */
} CommentItem;


/* global decls */
bool		g_verbose;			/* User wants verbose narration of our
								 * activities. */
Archive    *g_fout;				/* the script file */
PGconn	   *g_conn;				/* the database connection */

/* various user-settable parameters */
bool		schemaOnly;
bool		dataOnly;
bool		aclsSkip;
const char *lockWaitTimeout;

/* subquery used to convert user ID (eg, datdba) to user name */
static const char *username_subquery;

/* obsolete as of 7.3: */
static Oid	g_last_builtin_oid; /* value of the last builtin oid */

/*
 * Object inclusion/exclusion lists
 *
 * The string lists record the patterns given by command-line switches,
 * which we then convert to lists of OIDs of matching objects.
 */
static SimpleStringList schema_include_patterns = {NULL, NULL};
static SimpleOidList schema_include_oids = {NULL, NULL};
static SimpleStringList schema_exclude_patterns = {NULL, NULL};
static SimpleOidList schema_exclude_oids = {NULL, NULL};

static SimpleStringList table_include_patterns = {NULL, NULL};
static SimpleOidList table_include_oids = {NULL, NULL};
static SimpleStringList table_exclude_patterns = {NULL, NULL};
static SimpleOidList table_exclude_oids = {NULL, NULL};

/* default, if no "inclusion" switches appear, is to dump everything */
static bool include_everything = true;

char		g_opaque_type[10];	/* name for the opaque type */

/* placeholders for the delimiters for comments */
char		g_comment_start[10];
char		g_comment_end[10];

static const CatalogId nilCatalogId = {0, 0};

/* these are to avoid passing around info for findNamespace() */
static NamespaceInfo *g_namespaces;
static int	g_numNamespaces;

/* flags for various command-line long options */
static int	binary_upgrade = 0;
static int	disable_dollar_quoting = 0;
static int	dump_inserts = 0;
static int	column_inserts = 0;


static void help(const char *progname);
static void expand_schema_name_patterns(SimpleStringList *patterns,
							SimpleOidList *oids);
static void expand_table_name_patterns(SimpleStringList *patterns,
						   SimpleOidList *oids);
static NamespaceInfo *findNamespace(Oid nsoid, Oid objoid);
static void dumpTableData(Archive *fout, TableDataInfo *tdinfo);
static void guessConstraintInheritance(TableInfo *tblinfo, int numTables);
static void dumpComment(Archive *fout, const char *target,
			const char *namespace, const char *owner,
			CatalogId catalogId, int subid, DumpId dumpId);
static int findComments(Archive *fout, Oid classoid, Oid objoid,
			 CommentItem **items);
static int	collectComments(Archive *fout, CommentItem **items);
static void dumpDumpableObject(Archive *fout, DumpableObject *dobj);
static void dumpNamespace(Archive *fout, NamespaceInfo *nspinfo);
static void dumpType(Archive *fout, TypeInfo *tyinfo);
static void dumpBaseType(Archive *fout, TypeInfo *tyinfo);
static void dumpEnumType(Archive *fout, TypeInfo *tyinfo);
static void dumpDomain(Archive *fout, TypeInfo *tyinfo);
static void dumpCompositeType(Archive *fout, TypeInfo *tyinfo);
static void dumpCompositeTypeColComments(Archive *fout, TypeInfo *tyinfo);
static void dumpShellType(Archive *fout, ShellTypeInfo *stinfo);
static void dumpProcLang(Archive *fout, ProcLangInfo *plang);
static void dumpFunc(Archive *fout, FuncInfo *finfo);
static void dumpCast(Archive *fout, CastInfo *cast);
static void dumpOpr(Archive *fout, OprInfo *oprinfo);
static void dumpOpclass(Archive *fout, OpclassInfo *opcinfo);
static void dumpOpfamily(Archive *fout, OpfamilyInfo *opfinfo);
static void dumpConversion(Archive *fout, ConvInfo *convinfo);
static void dumpRule(Archive *fout, RuleInfo *rinfo);
static void dumpAgg(Archive *fout, AggInfo *agginfo);
static void dumpTrigger(Archive *fout, TriggerInfo *tginfo);
static void dumpTable(Archive *fout, TableInfo *tbinfo);
static void dumpTableSchema(Archive *fout, TableInfo *tbinfo);
static void dumpAttrDef(Archive *fout, AttrDefInfo *adinfo);
static void dumpSequence(Archive *fout, TableInfo *tbinfo);
static void dumpIndex(Archive *fout, IndxInfo *indxinfo);
static void dumpConstraint(Archive *fout, ConstraintInfo *coninfo);
static void dumpTableConstraintComment(Archive *fout, ConstraintInfo *coninfo);
static void dumpTSParser(Archive *fout, TSParserInfo *prsinfo);
static void dumpTSDictionary(Archive *fout, TSDictInfo *dictinfo);
static void dumpTSTemplate(Archive *fout, TSTemplateInfo *tmplinfo);
static void dumpTSConfig(Archive *fout, TSConfigInfo *cfginfo);
static void dumpForeignDataWrapper(Archive *fout, FdwInfo *fdwinfo);
static void dumpForeignServer(Archive *fout, ForeignServerInfo *srvinfo);
static void dumpUserMappings(Archive *fout,
				 const char *servername, const char *namespace,
				 const char *owner, CatalogId catalogId, DumpId dumpId);
static void dumpDefaultACL(Archive *fout, DefaultACLInfo *daclinfo);

static void dumpACL(Archive *fout, CatalogId objCatId, DumpId objDumpId,
		const char *type, const char *name, const char *subname,
		const char *tag, const char *nspname, const char *owner,
		const char *acls);

static void getDependencies(void);
static void getDomainConstraints(TypeInfo *tyinfo);
static void getTableData(TableInfo *tblinfo, int numTables, bool oids);
static void getTableDataFKConstraints(void);
static char *format_function_arguments(FuncInfo *finfo, char *funcargs);
static char *format_function_arguments_old(FuncInfo *finfo, int nallargs,
							  char **allargtypes,
							  char **argmodes,
							  char **argnames);
static char *format_function_signature(FuncInfo *finfo, bool honor_quotes);
static const char *convertRegProcReference(const char *proc);
static const char *convertOperatorReference(const char *opr);
static const char *convertTSFunction(Oid funcOid);
static Oid	findLastBuiltinOid_V71(const char *);
static Oid	findLastBuiltinOid_V70(void);
static void selectSourceSchema(const char *schemaName);
static char *getFormattedTypeName(Oid oid, OidOptions opts);
static char *myFormatType(const char *typname, int32 typmod);
static const char *fmtQualifiedId(const char *schema, const char *id);
static void getBlobs(Archive *AH);
static void dumpBlob(Archive *AH, BlobInfo *binfo);
static int	dumpBlobs(Archive *AH, void *arg);
static void dumpDatabase(Archive *AH);
static void dumpEncoding(Archive *AH);
static void dumpStdStrings(Archive *AH);
static void binary_upgrade_set_type_oids_by_type_oid(
								PQExpBuffer upgrade_buffer, Oid pg_type_oid);
static bool binary_upgrade_set_type_oids_by_rel_oid(
								 PQExpBuffer upgrade_buffer, Oid pg_rel_oid);
static void binary_upgrade_set_relfilenodes(PQExpBuffer upgrade_buffer,
								Oid pg_class_oid, bool is_index);
static const char *getAttrName(int attrnum, TableInfo *tblInfo);
static const char *fmtCopyColumnList(const TableInfo *ti);
static void do_sql_command(PGconn *conn, const char *query);
static void check_sql_result(PGresult *res, PGconn *conn, const char *query,
				 ExecStatusType expected);


int
main(int argc, char **argv)
{
	int			c;
	const char *filename = NULL;
	const char *format = "p";
	const char *dbname = NULL;
	const char *pghost = NULL;
	const char *pgport = NULL;
	const char *username = NULL;
	const char *dumpencoding = NULL;
	const char *std_strings;
	bool		oids = false;
	TableInfo  *tblinfo;
	int			numTables;
	DumpableObject **dobjs;
	int			numObjs;
	int			i;
	enum trivalue prompt_password = TRI_DEFAULT;
	int			compressLevel = -1;
	int			plainText = 0;
	int			outputClean = 0;
	int			outputCreateDB = 0;
	bool		outputBlobs = false;
	int			outputNoOwner = 0;
	char	   *outputSuperuser = NULL;
	char	   *use_role = NULL;
	int			my_version;
	int			optindex;
	RestoreOptions *ropt;

	static int	disable_triggers = 0;
	static int	outputNoTablespaces = 0;
	static int	use_setsessauth = 0;

	static struct option long_options[] = {
		{"data-only", no_argument, NULL, 'a'},
		{"blobs", no_argument, NULL, 'b'},
		{"clean", no_argument, NULL, 'c'},
		{"create", no_argument, NULL, 'C'},
		{"file", required_argument, NULL, 'f'},
		{"format", required_argument, NULL, 'F'},
		{"host", required_argument, NULL, 'h'},
		{"ignore-version", no_argument, NULL, 'i'},
		{"no-reconnect", no_argument, NULL, 'R'},
		{"oids", no_argument, NULL, 'o'},
		{"no-owner", no_argument, NULL, 'O'},
		{"port", required_argument, NULL, 'p'},
		{"schema", required_argument, NULL, 'n'},
		{"exclude-schema", required_argument, NULL, 'N'},
		{"schema-only", no_argument, NULL, 's'},
		{"superuser", required_argument, NULL, 'S'},
		{"table", required_argument, NULL, 't'},
		{"exclude-table", required_argument, NULL, 'T'},
		{"no-password", no_argument, NULL, 'w'},
		{"password", no_argument, NULL, 'W'},
		{"username", required_argument, NULL, 'U'},
		{"verbose", no_argument, NULL, 'v'},
		{"no-privileges", no_argument, NULL, 'x'},
		{"no-acl", no_argument, NULL, 'x'},
		{"compress", required_argument, NULL, 'Z'},
		{"encoding", required_argument, NULL, 'E'},
		{"help", no_argument, NULL, '?'},
		{"version", no_argument, NULL, 'V'},

		/*
		 * the following options don't have an equivalent short option letter
		 */
		{"attribute-inserts", no_argument, &column_inserts, 1},
		{"binary-upgrade", no_argument, &binary_upgrade, 1},
		{"column-inserts", no_argument, &column_inserts, 1},
		{"disable-dollar-quoting", no_argument, &disable_dollar_quoting, 1},
		{"disable-triggers", no_argument, &disable_triggers, 1},
		{"inserts", no_argument, &dump_inserts, 1},
		{"lock-wait-timeout", required_argument, NULL, 2},
		{"no-tablespaces", no_argument, &outputNoTablespaces, 1},
		{"role", required_argument, NULL, 3},
		{"use-set-session-authorization", no_argument, &use_setsessauth, 1},

		{NULL, 0, NULL, 0}
	};

	set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_dump"));

	g_verbose = false;

	strcpy(g_comment_start, "-- ");
	g_comment_end[0] = '\0';
	strcpy(g_opaque_type, "opaque");

	dataOnly = schemaOnly = false;
	lockWaitTimeout = NULL;

	progname = get_progname(argv[0]);

	/* Set default options based on progname */
	if (strcmp(progname, "pg_backup") == 0)
		format = "c";

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			help(progname);
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_dump (PostgreSQL) " PG_VERSION);
			exit(0);
		}
	}

	while ((c = getopt_long(argc, argv, "abcCE:f:F:h:in:N:oOp:RsS:t:T:U:vwWxX:Z:",
							long_options, &optindex)) != -1)
	{
		switch (c)
		{
			case 'a':			/* Dump data only */
				dataOnly = true;
				break;

			case 'b':			/* Dump blobs */
				outputBlobs = true;
				break;

			case 'c':			/* clean (i.e., drop) schema prior to create */
				outputClean = 1;
				break;

			case 'C':			/* Create DB */
				outputCreateDB = 1;
				break;

			case 'E':			/* Dump encoding */
				dumpencoding = optarg;
				break;

			case 'f':
				filename = optarg;
				break;

			case 'F':
				format = optarg;
				break;

			case 'h':			/* server host */
				pghost = optarg;
				break;

			case 'i':
				/* ignored, deprecated option */
				break;

			case 'n':			/* include schema(s) */
				simple_string_list_append(&schema_include_patterns, optarg);
				include_everything = false;
				break;

			case 'N':			/* exclude schema(s) */
				simple_string_list_append(&schema_exclude_patterns, optarg);
				break;

			case 'o':			/* Dump oids */
				oids = true;
				break;

			case 'O':			/* Don't reconnect to match owner */
				outputNoOwner = 1;
				break;

			case 'p':			/* server port */
				pgport = optarg;
				break;

			case 'R':
				/* no-op, still accepted for backwards compatibility */
				break;

			case 's':			/* dump schema only */
				schemaOnly = true;
				break;

			case 'S':			/* Username for superuser in plain text output */
				outputSuperuser = strdup(optarg);
				break;

			case 't':			/* include table(s) */
				simple_string_list_append(&table_include_patterns, optarg);
				include_everything = false;
				break;

			case 'T':			/* exclude table(s) */
				simple_string_list_append(&table_exclude_patterns, optarg);
				break;

			case 'U':
				username = optarg;
				break;

			case 'v':			/* verbose */
				g_verbose = true;
				break;

			case 'w':
				prompt_password = TRI_NO;
				break;

			case 'W':
				prompt_password = TRI_YES;
				break;

			case 'x':			/* skip ACL dump */
				aclsSkip = true;
				break;

			case 'X':
				/* -X is a deprecated alternative to long options */
				if (strcmp(optarg, "disable-dollar-quoting") == 0)
					disable_dollar_quoting = 1;
				else if (strcmp(optarg, "disable-triggers") == 0)
					disable_triggers = 1;
				else if (strcmp(optarg, "no-tablespaces") == 0)
					outputNoTablespaces = 1;
				else if (strcmp(optarg, "use-set-session-authorization") == 0)
					use_setsessauth = 1;
				else
				{
					fprintf(stderr,
							_("%s: invalid -X option -- %s\n"),
							progname, optarg);
					fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
					exit(1);
				}
				break;

			case 'Z':			/* Compression Level */
				compressLevel = atoi(optarg);
				break;

			case 0:
				/* This covers the long options equivalent to -X xxx. */
				break;

			case 2:				/* lock-wait-timeout */
				lockWaitTimeout = optarg;
				break;

			case 3:				/* SET ROLE */
				use_role = optarg;
				break;

			default:
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
		}
	}

	/* Get database name from command line */
	if (optind < argc)
		dbname = argv[optind++];

	/* Complain if any arguments remain */
	if (optind < argc)
	{
		fprintf(stderr, _("%s: too many command-line arguments (first is \"%s\")\n"),
				progname, argv[optind]);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit(1);
	}

	/* --column-inserts implies --inserts */
	if (column_inserts)
		dump_inserts = 1;

	if (dataOnly && schemaOnly)
	{
		write_msg(NULL, "options -s/--schema-only and -a/--data-only cannot be used together\n");
		exit(1);
	}

	if (dataOnly && outputClean)
	{
		write_msg(NULL, "options -c/--clean and -a/--data-only cannot be used together\n");
		exit(1);
	}

	if (dump_inserts && oids)
	{
		write_msg(NULL, "options --inserts/--column-inserts and -o/--oids cannot be used together\n");
		write_msg(NULL, "(The INSERT command cannot set OIDs.)\n");
		exit(1);
	}

	/* open the output file */
	if (pg_strcasecmp(format, "a") == 0 || pg_strcasecmp(format, "append") == 0)
	{
		/* This is used by pg_dumpall, and is not documented */
		plainText = 1;
		g_fout = CreateArchive(filename, archNull, 0, archModeAppend);
	}
	else if (pg_strcasecmp(format, "c") == 0 || pg_strcasecmp(format, "custom") == 0)
		g_fout = CreateArchive(filename, archCustom, compressLevel, archModeWrite);
	else if (pg_strcasecmp(format, "f") == 0 || pg_strcasecmp(format, "file") == 0)
	{
		/*
		 * Dump files into the current directory; for demonstration only, not
		 * documented.
		 */
		g_fout = CreateArchive(filename, archFiles, compressLevel, archModeWrite);
	}
	else if (pg_strcasecmp(format, "p") == 0 || pg_strcasecmp(format, "plain") == 0)
	{
		plainText = 1;
		g_fout = CreateArchive(filename, archNull, 0, archModeWrite);
	}
	else if (pg_strcasecmp(format, "t") == 0 || pg_strcasecmp(format, "tar") == 0)
		g_fout = CreateArchive(filename, archTar, compressLevel, archModeWrite);
	else
	{
		write_msg(NULL, "invalid output format \"%s\" specified\n", format);
		exit(1);
	}

	if (g_fout == NULL)
	{
		write_msg(NULL, "could not open output file \"%s\" for writing\n", filename);
		exit(1);
	}

	/* Let the archiver know how noisy to be */
	g_fout->verbose = g_verbose;

	my_version = parse_version(PG_VERSION);
	if (my_version < 0)
	{
		write_msg(NULL, "could not parse version string \"%s\"\n", PG_VERSION);
		exit(1);
	}

	/*
	 * We allow the server to be back to 7.0, and up to any minor release of
	 * our own major version.  (See also version check in pg_dumpall.c.)
	 */
	g_fout->minRemoteVersion = 70000;
	g_fout->maxRemoteVersion = (my_version / 100) * 100 + 99;

	/*
	 * Open the database using the Archiver, so it knows about it. Errors mean
	 * death.
	 */
	g_conn = ConnectDatabase(g_fout, dbname, pghost, pgport,
							 username, prompt_password);

	/* Set the client encoding if requested */
	if (dumpencoding)
	{
		if (PQsetClientEncoding(g_conn, dumpencoding) < 0)
		{
			write_msg(NULL, "invalid client encoding \"%s\" specified\n",
					  dumpencoding);
			exit(1);
		}
	}

	/*
	 * Get the active encoding and the standard_conforming_strings setting, so
	 * we know how to escape strings.
	 */
	g_fout->encoding = PQclientEncoding(g_conn);

	std_strings = PQparameterStatus(g_conn, "standard_conforming_strings");
	g_fout->std_strings = (std_strings && strcmp(std_strings, "on") == 0);

	/* Set the role if requested */
	if (use_role && g_fout->remoteVersion >= 80100)
	{
		PQExpBuffer query = createPQExpBuffer();

		appendPQExpBuffer(query, "SET ROLE %s", fmtId(use_role));
		do_sql_command(g_conn, query->data);
		destroyPQExpBuffer(query);
	}

	/* Set the datestyle to ISO to ensure the dump's portability */
	do_sql_command(g_conn, "SET DATESTYLE = ISO");

	/* Likewise, avoid using sql_standard intervalstyle */
	if (g_fout->remoteVersion >= 80400)
		do_sql_command(g_conn, "SET INTERVALSTYLE = POSTGRES");

	/*
	 * If supported, set extra_float_digits so that we can dump float data
	 * exactly (given correctly implemented float I/O code, anyway)
	 */
	if (g_fout->remoteVersion >= 90000)
		do_sql_command(g_conn, "SET extra_float_digits TO 3");
	else if (g_fout->remoteVersion >= 70400)
		do_sql_command(g_conn, "SET extra_float_digits TO 2");

	/*
	 * If synchronized scanning is supported, disable it, to prevent
	 * unpredictable changes in row ordering across a dump and reload.
	 */
	if (g_fout->remoteVersion >= 80300)
		do_sql_command(g_conn, "SET synchronize_seqscans TO off");

	/*
	 * Disable timeouts if supported.
	 */
	if (g_fout->remoteVersion >= 70300)
		do_sql_command(g_conn, "SET statement_timeout = 0");

	/*
	 * Start serializable transaction to dump consistent data.
	 */
	do_sql_command(g_conn, "BEGIN");

	do_sql_command(g_conn, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE");

	/* Select the appropriate subquery to convert user IDs to names */
	if (g_fout->remoteVersion >= 80100)
		username_subquery = "SELECT rolname FROM pg_catalog.pg_roles WHERE oid =";
	else if (g_fout->remoteVersion >= 70300)
		username_subquery = "SELECT usename FROM pg_catalog.pg_user WHERE usesysid =";
	else
		username_subquery = "SELECT usename FROM pg_user WHERE usesysid =";

	/* Find the last built-in OID, if needed */
	if (g_fout->remoteVersion < 70300)
	{
		if (g_fout->remoteVersion >= 70100)
			g_last_builtin_oid = findLastBuiltinOid_V71(PQdb(g_conn));
		else
			g_last_builtin_oid = findLastBuiltinOid_V70();
		if (g_verbose)
			write_msg(NULL, "last built-in OID is %u\n", g_last_builtin_oid);
	}

	/* Expand schema selection patterns into OID lists */
	if (schema_include_patterns.head != NULL)
	{
		expand_schema_name_patterns(&schema_include_patterns,
									&schema_include_oids);
		if (schema_include_oids.head == NULL)
		{
			write_msg(NULL, "No matching schemas were found\n");
			exit_nicely();
		}
	}
	expand_schema_name_patterns(&schema_exclude_patterns,
								&schema_exclude_oids);
	/* non-matching exclusion patterns aren't an error */

	/* Expand table selection patterns into OID lists */
	if (table_include_patterns.head != NULL)
	{
		expand_table_name_patterns(&table_include_patterns,
								   &table_include_oids);
		if (table_include_oids.head == NULL)
		{
			write_msg(NULL, "No matching tables were found\n");
			exit_nicely();
		}
	}
	expand_table_name_patterns(&table_exclude_patterns,
							   &table_exclude_oids);
	/* non-matching exclusion patterns aren't an error */

	/*
	 * Dumping blobs is now default unless we saw an inclusion switch or -s
	 * ... but even if we did see one of these, -b turns it back on.
	 */
	if (include_everything && !schemaOnly)
		outputBlobs = true;

	/*
	 * Now scan the database and create DumpableObject structs for all the
	 * objects we intend to dump.
	 */
	tblinfo = getSchemaData(&numTables);

	if (g_fout->remoteVersion < 80400)
		guessConstraintInheritance(tblinfo, numTables);

	if (!schemaOnly)
	{
		getTableData(tblinfo, numTables, oids);
		if (dataOnly)
			getTableDataFKConstraints();
	}

	if (outputBlobs)
		getBlobs(g_fout);

	/*
	 * Collect dependency data to assist in ordering the objects.
	 */
	getDependencies();

	/*
	 * Sort the objects into a safe dump order (no forward references).
	 *
	 * In 7.3 or later, we can rely on dependency information to help us
	 * determine a safe order, so the initial sort is mostly for cosmetic
	 * purposes: we sort by name to ensure that logically identical schemas
	 * will dump identically.  Before 7.3 we don't have dependencies and we
	 * use OID ordering as an (unreliable) guide to creation order.
	 */
	getDumpableObjects(&dobjs, &numObjs);

	if (g_fout->remoteVersion >= 70300)
		sortDumpableObjectsByTypeName(dobjs, numObjs);
	else
		sortDumpableObjectsByTypeOid(dobjs, numObjs);

	sortDumpableObjects(dobjs, numObjs);

	/*
	 * Create archive TOC entries for all the objects to be dumped, in a safe
	 * order.
	 */

	/* First the special ENCODING and STDSTRINGS entries. */
	dumpEncoding(g_fout);
	dumpStdStrings(g_fout);

	/* The database item is always next, unless we don't want it at all */
	if (include_everything && !dataOnly)
		dumpDatabase(g_fout);

	/* Now the rearrangeable objects. */
	for (i = 0; i < numObjs; i++)
		dumpDumpableObject(g_fout, dobjs[i]);

	/*
	 * And finally we can do the actual output.
	 */
	if (plainText)
	{
		ropt = NewRestoreOptions();
		ropt->filename = (char *) filename;
		ropt->dropSchema = outputClean;
		ropt->aclsSkip = aclsSkip;
		ropt->superuser = outputSuperuser;
		ropt->createDB = outputCreateDB;
		ropt->noOwner = outputNoOwner;
		ropt->noTablespace = outputNoTablespaces;
		ropt->disable_triggers = disable_triggers;
		ropt->use_setsessauth = use_setsessauth;
		ropt->dataOnly = dataOnly;

		if (compressLevel == -1)
			ropt->compression = 0;
		else
			ropt->compression = compressLevel;

		ropt->suppressDumpWarnings = true;		/* We've already shown them */

		RestoreArchive(g_fout, ropt);
	}

	CloseArchive(g_fout);

	PQfinish(g_conn);

	exit(0);
}


static void
help(const char *progname)
{
	printf(_("%s dumps a database as a text file or to other formats.\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION]... [DBNAME]\n"), progname);

	printf(_("\nGeneral options:\n"));
	printf(_("  -f, --file=FILENAME         output file name\n"));
	printf(_("  -F, --format=c|t|p          output file format (custom, tar, plain text)\n"));
	printf(_("  -v, --verbose               verbose mode\n"));
	printf(_("  -Z, --compress=0-9          compression level for compressed formats\n"));
	printf(_("  --lock-wait-timeout=TIMEOUT fail after waiting TIMEOUT for a table lock\n"));
	printf(_("  --help                      show this help, then exit\n"));
	printf(_("  --version                   output version information, then exit\n"));

	printf(_("\nOptions controlling the output content:\n"));
	printf(_("  -a, --data-only             dump only the data, not the schema\n"));
	printf(_("  -b, --blobs                 include large objects in dump\n"));
	printf(_("  -c, --clean                 clean (drop) database objects before recreating\n"));
	printf(_("  -C, --create                include commands to create database in dump\n"));
	printf(_("  -E, --encoding=ENCODING     dump the data in encoding ENCODING\n"));
	printf(_("  -n, --schema=SCHEMA         dump the named schema(s) only\n"));
	printf(_("  -N, --exclude-schema=SCHEMA do NOT dump the named schema(s)\n"));
	printf(_("  -o, --oids                  include OIDs in dump\n"));
	printf(_("  -O, --no-owner              skip restoration of object ownership in\n"
			 "                              plain-text format\n"));
	printf(_("  -s, --schema-only           dump only the schema, no data\n"));
	printf(_("  -S, --superuser=NAME        superuser user name to use in plain-text format\n"));
	printf(_("  -t, --table=TABLE           dump the named table(s) only\n"));
	printf(_("  -T, --exclude-table=TABLE   do NOT dump the named table(s)\n"));
	printf(_("  -x, --no-privileges         do not dump privileges (grant/revoke)\n"));
	printf(_("  --binary-upgrade            for use by upgrade utilities only\n"));
	printf(_("  --inserts                   dump data as INSERT commands, rather than COPY\n"));
	printf(_("  --column-inserts            dump data as INSERT commands with column names\n"));
	printf(_("  --disable-dollar-quoting    disable dollar quoting, use SQL standard quoting\n"));
	printf(_("  --disable-triggers          disable triggers during data-only restore\n"));
	printf(_("  --no-tablespaces            do not dump tablespace assignments\n"));
	printf(_("  --role=ROLENAME             do SET ROLE before dump\n"));
	printf(_("  --use-set-session-authorization\n"
			 "                              use SET SESSION AUTHORIZATION commands instead of\n"
	"                              ALTER OWNER commands to set ownership\n"));

	printf(_("\nConnection options:\n"));
	printf(_("  -h, --host=HOSTNAME      database server host or socket directory\n"));
	printf(_("  -p, --port=PORT          database server port number\n"));
	printf(_("  -U, --username=NAME      connect as specified database user\n"));
	printf(_("  -w, --no-password        never prompt for password\n"));
	printf(_("  -W, --password           force password prompt (should happen automatically)\n"));

	printf(_("\nIf no database name is supplied, then the PGDATABASE environment\n"
			 "variable value is used.\n\n"));
	printf(_("Report bugs to <pgsql-bugs@postgresql.org>.\n"));
}

void
exit_nicely(void)
{
	PQfinish(g_conn);
	if (g_verbose)
		write_msg(NULL, "*** aborted because of error\n");
	exit(1);
}

/*
 * Find the OIDs of all schemas matching the given list of patterns,
 * and append them to the given OID list.
 */
static void
expand_schema_name_patterns(SimpleStringList *patterns, SimpleOidList *oids)
{
	PQExpBuffer query;
	PGresult   *res;
	SimpleStringListCell *cell;
	int			i;

	if (patterns->head == NULL)
		return;					/* nothing to do */

	if (g_fout->remoteVersion < 70300)
	{
		write_msg(NULL, "server version must be at least 7.3 to use schema selection switches\n");
		exit_nicely();
	}

	query = createPQExpBuffer();

	/*
	 * We use UNION ALL rather than UNION; this might sometimes result in
	 * duplicate entries in the OID list, but we don't care.
	 */

	for (cell = patterns->head; cell; cell = cell->next)
	{
		if (cell != patterns->head)
			appendPQExpBuffer(query, "UNION ALL\n");
		appendPQExpBuffer(query,
						  "SELECT oid FROM pg_catalog.pg_namespace n\n");
		processSQLNamePattern(g_conn, query, cell->val, false, false,
							  NULL, "n.nspname", NULL,
							  NULL);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	for (i = 0; i < PQntuples(res); i++)
	{
		simple_oid_list_append(oids, atooid(PQgetvalue(res, i, 0)));
	}

	PQclear(res);
	destroyPQExpBuffer(query);
}

/*
 * Find the OIDs of all tables matching the given list of patterns,
 * and append them to the given OID list.
 */
static void
expand_table_name_patterns(SimpleStringList *patterns, SimpleOidList *oids)
{
	PQExpBuffer query;
	PGresult   *res;
	SimpleStringListCell *cell;
	int			i;

	if (patterns->head == NULL)
		return;					/* nothing to do */

	query = createPQExpBuffer();

	/*
	 * We use UNION ALL rather than UNION; this might sometimes result in
	 * duplicate entries in the OID list, but we don't care.
	 */

	for (cell = patterns->head; cell; cell = cell->next)
	{
		if (cell != patterns->head)
			appendPQExpBuffer(query, "UNION ALL\n");
		appendPQExpBuffer(query,
						  "SELECT c.oid"
						  "\nFROM pg_catalog.pg_class c"
		"\n     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace"
						  "\nWHERE c.relkind in ('%c', '%c', '%c')\n",
						  RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW);
		processSQLNamePattern(g_conn, query, cell->val, true, false,
							  "n.nspname", "c.relname", NULL,
							  "pg_catalog.pg_table_is_visible(c.oid)");
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	for (i = 0; i < PQntuples(res); i++)
	{
		simple_oid_list_append(oids, atooid(PQgetvalue(res, i, 0)));
	}

	PQclear(res);
	destroyPQExpBuffer(query);
}

/*
 * selectDumpableNamespace: policy-setting subroutine
 *		Mark a namespace as to be dumped or not
 */
static void
selectDumpableNamespace(NamespaceInfo *nsinfo)
{
	/*
	 * If specific tables are being dumped, do not dump any complete
	 * namespaces. If specific namespaces are being dumped, dump just those
	 * namespaces. Otherwise, dump all non-system namespaces.
	 */
	if (table_include_oids.head != NULL)
		nsinfo->dobj.dump = false;
	else if (schema_include_oids.head != NULL)
		nsinfo->dobj.dump = simple_oid_list_member(&schema_include_oids,
												   nsinfo->dobj.catId.oid);
	else if (strncmp(nsinfo->dobj.name, "pg_", 3) == 0 ||
			 strcmp(nsinfo->dobj.name, "information_schema") == 0)
		nsinfo->dobj.dump = false;
	else
		nsinfo->dobj.dump = true;

	/*
	 * In any case, a namespace can be excluded by an exclusion switch
	 */
	if (nsinfo->dobj.dump &&
		simple_oid_list_member(&schema_exclude_oids,
							   nsinfo->dobj.catId.oid))
		nsinfo->dobj.dump = false;
}

/*
 * selectDumpableTable: policy-setting subroutine
 *		Mark a table as to be dumped or not
 */
static void
selectDumpableTable(TableInfo *tbinfo)
{
	/*
	 * If specific tables are being dumped, dump just those tables; else, dump
	 * according to the parent namespace's dump flag.
	 */
	if (table_include_oids.head != NULL)
		tbinfo->dobj.dump = simple_oid_list_member(&table_include_oids,
												   tbinfo->dobj.catId.oid);
	else
		tbinfo->dobj.dump = tbinfo->dobj.namespace->dobj.dump;

	/*
	 * In any case, a table can be excluded by an exclusion switch
	 */
	if (tbinfo->dobj.dump &&
		simple_oid_list_member(&table_exclude_oids,
							   tbinfo->dobj.catId.oid))
		tbinfo->dobj.dump = false;
}

/*
 * selectDumpableType: policy-setting subroutine
 *		Mark a type as to be dumped or not
 *
 * If it's a table's rowtype or an autogenerated array type, we also apply a
 * special type code to facilitate sorting into the desired order.	(We don't
 * want to consider those to be ordinary types because that would bring tables
 * up into the datatype part of the dump order.)  Those tests should be made
 * first to ensure the objType change is applied regardless of namespace etc.
 */
static void
selectDumpableType(TypeInfo *tyinfo)
{
	/* skip complex types, except for standalone composite types */
	if (OidIsValid(tyinfo->typrelid) &&
		tyinfo->typrelkind != RELKIND_COMPOSITE_TYPE)
	{
		tyinfo->dobj.dump = false;
		tyinfo->dobj.objType = DO_DUMMY_TYPE;
	}

	/* skip auto-generated array types */
	else if (tyinfo->isArray)
	{
		tyinfo->dobj.dump = false;
		tyinfo->dobj.objType = DO_DUMMY_TYPE;
	}

	/* dump only types in dumpable namespaces */
	else if (!tyinfo->dobj.namespace->dobj.dump)
		tyinfo->dobj.dump = false;

	/* skip undefined placeholder types */
	else if (!tyinfo->isDefined)
		tyinfo->dobj.dump = false;

	else
		tyinfo->dobj.dump = true;
}

/*
 * selectDumpableDefaultACL: policy-setting subroutine
 *		Mark a default ACL as to be dumped or not
 *
 * For per-schema default ACLs, dump if the schema is to be dumped.
 * Otherwise dump if we are dumping "everything".  Note that dataOnly
 * and aclsSkip are checked separately.
 */
static void
selectDumpableDefaultACL(DefaultACLInfo *dinfo)
{
	if (dinfo->dobj.namespace)
		dinfo->dobj.dump = dinfo->dobj.namespace->dobj.dump;
	else
		dinfo->dobj.dump = include_everything;
}

/*
 * selectDumpableObject: policy-setting subroutine
 *		Mark a generic dumpable object as to be dumped or not
 *
 * Use this only for object types without a special-case routine above.
 */
static void
selectDumpableObject(DumpableObject *dobj)
{
	/*
	 * Default policy is to dump if parent namespace is dumpable, or always
	 * for non-namespace-associated items.
	 */
	if (dobj->namespace)
		dobj->dump = dobj->namespace->dobj.dump;
	else
		dobj->dump = true;
}

/*
 *	Dump a table's contents for loading using the COPY command
 *	- this routine is called by the Archiver when it wants the table
 *	  to be dumped.
 */

static int
dumpTableData_copy(Archive *fout, void *dcontext)
{
	TableDataInfo *tdinfo = (TableDataInfo *) dcontext;
	TableInfo  *tbinfo = tdinfo->tdtable;
	const char *classname = tbinfo->dobj.name;
	const bool	hasoids = tbinfo->hasoids;
	const bool	oids = tdinfo->oids;
	PQExpBuffer q = createPQExpBuffer();
	PGresult   *res;
	int			ret;
	char	   *copybuf;
	const char *column_list;

	if (g_verbose)
		write_msg(NULL, "dumping contents of table %s\n", classname);

	/*
	 * Make sure we are in proper schema.  We will qualify the table name
	 * below anyway (in case its name conflicts with a pg_catalog table); but
	 * this ensures reproducible results in case the table contains regproc,
	 * regclass, etc columns.
	 */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	/*
	 * If possible, specify the column list explicitly so that we have no
	 * possibility of retrieving data in the wrong column order.  (The default
	 * column ordering of COPY will not be what we want in certain corner
	 * cases involving ADD COLUMN and inheritance.)
	 */
	if (g_fout->remoteVersion >= 70300)
		column_list = fmtCopyColumnList(tbinfo);
	else
		column_list = "";		/* can't select columns in COPY */

	if (oids && hasoids)
	{
		appendPQExpBuffer(q, "COPY %s %s WITH OIDS TO stdout;",
						  fmtQualifiedId(tbinfo->dobj.namespace->dobj.name,
										 classname),
						  column_list);
	}
	else
	{
		appendPQExpBuffer(q, "COPY %s %s TO stdout;",
						  fmtQualifiedId(tbinfo->dobj.namespace->dobj.name,
										 classname),
						  column_list);
	}
	res = PQexec(g_conn, q->data);
	check_sql_result(res, g_conn, q->data, PGRES_COPY_OUT);
	PQclear(res);

	for (;;)
	{
		ret = PQgetCopyData(g_conn, &copybuf, 0);

		if (ret < 0)
			break;				/* done or error */

		if (copybuf)
		{
			WriteData(fout, copybuf, ret);
			PQfreemem(copybuf);
		}

		/* ----------
		 * THROTTLE:
		 *
		 * There was considerable discussion in late July, 2000 regarding
		 * slowing down pg_dump when backing up large tables. Users with both
		 * slow & fast (multi-processor) machines experienced performance
		 * degradation when doing a backup.
		 *
		 * Initial attempts based on sleeping for a number of ms for each ms
		 * of work were deemed too complex, then a simple 'sleep in each loop'
		 * implementation was suggested. The latter failed because the loop
		 * was too tight. Finally, the following was implemented:
		 *
		 * If throttle is non-zero, then
		 *		See how long since the last sleep.
		 *		Work out how long to sleep (based on ratio).
		 *		If sleep is more than 100ms, then
		 *			sleep
		 *			reset timer
		 *		EndIf
		 * EndIf
		 *
		 * where the throttle value was the number of ms to sleep per ms of
		 * work. The calculation was done in each loop.
		 *
		 * Most of the hard work is done in the backend, and this solution
		 * still did not work particularly well: on slow machines, the ratio
		 * was 50:1, and on medium paced machines, 1:1, and on fast
		 * multi-processor machines, it had little or no effect, for reasons
		 * that were unclear.
		 *
		 * Further discussion ensued, and the proposal was dropped.
		 *
		 * For those people who want this feature, it can be implemented using
		 * gettimeofday in each loop, calculating the time since last sleep,
		 * multiplying that by the sleep ratio, then if the result is more
		 * than a preset 'minimum sleep time' (say 100ms), call the 'select'
		 * function to sleep for a subsecond period ie.
		 *
		 * select(0, NULL, NULL, NULL, &tvi);
		 *
		 * This will return after the interval specified in the structure tvi.
		 * Finally, call gettimeofday again to save the 'last sleep time'.
		 * ----------
		 */
	}
	archprintf(fout, "\\.\n\n\n");

	if (ret == -2)
	{
		/* copy data transfer failed */
		write_msg(NULL, "Dumping the contents of table \"%s\" failed: PQgetCopyData() failed.\n", classname);
		write_msg(NULL, "Error message from server: %s", PQerrorMessage(g_conn));
		write_msg(NULL, "The command was: %s\n", q->data);
		exit_nicely();
	}

	/* Check command status and return to normal libpq state */
	res = PQgetResult(g_conn);
	check_sql_result(res, g_conn, q->data, PGRES_COMMAND_OK);
	PQclear(res);

	destroyPQExpBuffer(q);
	return 1;
}

static int
dumpTableData_insert(Archive *fout, void *dcontext)
{
	TableDataInfo *tdinfo = (TableDataInfo *) dcontext;
	TableInfo  *tbinfo = tdinfo->tdtable;
	const char *classname = tbinfo->dobj.name;
	PQExpBuffer q = createPQExpBuffer();
	PGresult   *res;
	int			tuple;
	int			nfields;
	int			field;

	/*
	 * Make sure we are in proper schema.  We will qualify the table name
	 * below anyway (in case its name conflicts with a pg_catalog table); but
	 * this ensures reproducible results in case the table contains regproc,
	 * regclass, etc columns.
	 */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	if (fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(q, "DECLARE _pg_dump_cursor CURSOR FOR "
						  "SELECT * FROM ONLY %s",
						  fmtQualifiedId(tbinfo->dobj.namespace->dobj.name,
										 classname));
	}
	else
	{
		appendPQExpBuffer(q, "DECLARE _pg_dump_cursor CURSOR FOR "
						  "SELECT * FROM %s",
						  fmtQualifiedId(tbinfo->dobj.namespace->dobj.name,
										 classname));
	}

	res = PQexec(g_conn, q->data);
	check_sql_result(res, g_conn, q->data, PGRES_COMMAND_OK);

	do
	{
		PQclear(res);

		res = PQexec(g_conn, "FETCH 100 FROM _pg_dump_cursor");
		check_sql_result(res, g_conn, "FETCH 100 FROM _pg_dump_cursor",
						 PGRES_TUPLES_OK);
		nfields = PQnfields(res);
		for (tuple = 0; tuple < PQntuples(res); tuple++)
		{
			archprintf(fout, "INSERT INTO %s ", fmtId(classname));
			if (nfields == 0)
			{
				/* corner case for zero-column table */
				archprintf(fout, "DEFAULT VALUES;\n");
				continue;
			}
			if (column_inserts)
			{
				resetPQExpBuffer(q);
				appendPQExpBuffer(q, "(");
				for (field = 0; field < nfields; field++)
				{
					if (field > 0)
						appendPQExpBuffer(q, ", ");
					appendPQExpBufferStr(q, fmtId(PQfname(res, field)));
				}
				appendPQExpBuffer(q, ") ");
				archputs(q->data, fout);
			}
			archprintf(fout, "VALUES (");
			for (field = 0; field < nfields; field++)
			{
				if (field > 0)
					archprintf(fout, ", ");
				if (PQgetisnull(res, tuple, field))
				{
					archprintf(fout, "NULL");
					continue;
				}

				/* XXX This code is partially duplicated in ruleutils.c */
				switch (PQftype(res, field))
				{
					case INT2OID:
					case INT4OID:
					case INT8OID:
					case OIDOID:
					case FLOAT4OID:
					case FLOAT8OID:
					case NUMERICOID:
						{
							/*
							 * These types are printed without quotes unless
							 * they contain values that aren't accepted by the
							 * scanner unquoted (e.g., 'NaN').	Note that
							 * strtod() and friends might accept NaN, so we
							 * can't use that to test.
							 *
							 * In reality we only need to defend against
							 * infinity and NaN, so we need not get too crazy
							 * about pattern matching here.
							 */
							const char *s = PQgetvalue(res, tuple, field);

							if (strspn(s, "0123456789 +-eE.") == strlen(s))
								archprintf(fout, "%s", s);
							else
								archprintf(fout, "'%s'", s);
						}
						break;

					case BITOID:
					case VARBITOID:
						archprintf(fout, "B'%s'",
								   PQgetvalue(res, tuple, field));
						break;

					case BOOLOID:
						if (strcmp(PQgetvalue(res, tuple, field), "t") == 0)
							archprintf(fout, "true");
						else
							archprintf(fout, "false");
						break;

					default:
						/* All other types are printed as string literals. */
						resetPQExpBuffer(q);
						appendStringLiteralAH(q,
											  PQgetvalue(res, tuple, field),
											  fout);
						archputs(q->data, fout);
						break;
				}
			}
			archprintf(fout, ");\n");
		}
	} while (PQntuples(res) > 0);

	PQclear(res);

	archprintf(fout, "\n\n");

	do_sql_command(g_conn, "CLOSE _pg_dump_cursor");

	destroyPQExpBuffer(q);
	return 1;
}


/*
 * dumpTableData -
 *	  dump the contents of a single table
 *
 * Actually, this just makes an ArchiveEntry for the table contents.
 */
static void
dumpTableData(Archive *fout, TableDataInfo *tdinfo)
{
	TableInfo  *tbinfo = tdinfo->tdtable;
	PQExpBuffer copyBuf = createPQExpBuffer();
	DataDumperPtr dumpFn;
	char	   *copyStmt;

	if (!dump_inserts)
	{
		/* Dump/restore using COPY */
		dumpFn = dumpTableData_copy;
		/* must use 2 steps here 'cause fmtId is nonreentrant */
		appendPQExpBuffer(copyBuf, "COPY %s ",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(copyBuf, "%s %sFROM stdin;\n",
						  fmtCopyColumnList(tbinfo),
					  (tdinfo->oids && tbinfo->hasoids) ? "WITH OIDS " : "");
		copyStmt = copyBuf->data;
	}
	else
	{
		/* Restore using INSERT */
		dumpFn = dumpTableData_insert;
		copyStmt = NULL;
	}

	ArchiveEntry(fout, tdinfo->dobj.catId, tdinfo->dobj.dumpId,
				 tbinfo->dobj.name, tbinfo->dobj.namespace->dobj.name,
				 NULL, tbinfo->rolname,
				 false, "TABLE DATA", SECTION_DATA,
				 "", "", copyStmt,
				 tdinfo->dobj.dependencies, tdinfo->dobj.nDeps,
				 dumpFn, tdinfo);

	destroyPQExpBuffer(copyBuf);
}

/*
 * getTableData -
 *	  set up dumpable objects representing the contents of tables
 */
static void
getTableData(TableInfo *tblinfo, int numTables, bool oids)
{
	int			i;

	for (i = 0; i < numTables; i++)
	{
		/* Skip VIEWs (no data to dump) */
		if (tblinfo[i].relkind == RELKIND_VIEW)
			continue;
		/* Skip SEQUENCEs (handled elsewhere) */
		if (tblinfo[i].relkind == RELKIND_SEQUENCE)
			continue;

		if (tblinfo[i].dobj.dump)
		{
			TableDataInfo *tdinfo;

			tdinfo = (TableDataInfo *) malloc(sizeof(TableDataInfo));

			tdinfo->dobj.objType = DO_TABLE_DATA;

			/*
			 * Note: use tableoid 0 so that this object won't be mistaken for
			 * something that pg_depend entries apply to.
			 */
			tdinfo->dobj.catId.tableoid = 0;
			tdinfo->dobj.catId.oid = tblinfo[i].dobj.catId.oid;
			AssignDumpId(&tdinfo->dobj);
			tdinfo->dobj.name = tblinfo[i].dobj.name;
			tdinfo->dobj.namespace = tblinfo[i].dobj.namespace;
			tdinfo->tdtable = &(tblinfo[i]);
			tdinfo->oids = oids;
			addObjectDependency(&tdinfo->dobj, tblinfo[i].dobj.dumpId);

			tblinfo[i].dataObj = tdinfo;
		}
	}
}

/*
 * getTableDataFKConstraints -
 *	  add dump-order dependencies reflecting foreign key constraints
 *
 * This code is executed only in a data-only dump --- in schema+data dumps
 * we handle foreign key issues by not creating the FK constraints until
 * after the data is loaded.  In a data-only dump, however, we want to
 * order the table data objects in such a way that a table's referenced
 * tables are restored first.  (In the presence of circular references or
 * self-references this may be impossible; we'll detect and complain about
 * that during the dependency sorting step.)
 */
static void
getTableDataFKConstraints(void)
{
	DumpableObject **dobjs;
	int			numObjs;
	int			i;

	/* Search through all the dumpable objects for FK constraints */
	getDumpableObjects(&dobjs, &numObjs);
	for (i = 0; i < numObjs; i++)
	{
		if (dobjs[i]->objType == DO_FK_CONSTRAINT)
		{
			ConstraintInfo *cinfo = (ConstraintInfo *) dobjs[i];
			TableInfo  *ftable;

			/* Not interesting unless both tables are to be dumped */
			if (cinfo->contable == NULL ||
				cinfo->contable->dataObj == NULL)
				continue;
			ftable = findTableByOid(cinfo->confrelid);
			if (ftable == NULL ||
				ftable->dataObj == NULL)
				continue;

			/*
			 * Okay, make referencing table's TABLE_DATA object depend on the
			 * referenced table's TABLE_DATA object.
			 */
			addObjectDependency(&cinfo->contable->dataObj->dobj,
								ftable->dataObj->dobj.dumpId);
		}
	}
	free(dobjs);
}


/*
 * guessConstraintInheritance:
 *	In pre-8.4 databases, we can't tell for certain which constraints
 *	are inherited.	We assume a CHECK constraint is inherited if its name
 *	matches the name of any constraint in the parent.  Originally this code
 *	tried to compare the expression texts, but that can fail for various
 *	reasons --- for example, if the parent and child tables are in different
 *	schemas, reverse-listing of function calls may produce different text
 *	(schema-qualified or not) depending on search path.
 *
 *	In 8.4 and up we can rely on the conislocal field to decide which
 *	constraints must be dumped; much safer.
 *
 *	This function assumes all conislocal flags were initialized to TRUE.
 *	It clears the flag on anything that seems to be inherited.
 */
static void
guessConstraintInheritance(TableInfo *tblinfo, int numTables)
{
	int			i,
				j,
				k;

	for (i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &(tblinfo[i]);
		int			numParents;
		TableInfo **parents;
		TableInfo  *parent;

		/* Sequences and views never have parents */
		if (tbinfo->relkind == RELKIND_SEQUENCE ||
			tbinfo->relkind == RELKIND_VIEW)
			continue;

		/* Don't bother computing anything for non-target tables, either */
		if (!tbinfo->dobj.dump)
			continue;

		numParents = tbinfo->numParents;
		parents = tbinfo->parents;

		if (numParents == 0)
			continue;			/* nothing to see here, move along */

		/* scan for inherited CHECK constraints */
		for (j = 0; j < tbinfo->ncheck; j++)
		{
			ConstraintInfo *constr;

			constr = &(tbinfo->checkexprs[j]);

			for (k = 0; k < numParents; k++)
			{
				int			l;

				parent = parents[k];
				for (l = 0; l < parent->ncheck; l++)
				{
					ConstraintInfo *pconstr = &(parent->checkexprs[l]);

					if (strcmp(pconstr->dobj.name, constr->dobj.name) == 0)
					{
						constr->conislocal = false;
						break;
					}
				}
				if (!constr->conislocal)
					break;
			}
		}
	}
}


/*
 * dumpDatabase:
 *	dump the database definition
 */
static void
dumpDatabase(Archive *AH)
{
	PQExpBuffer dbQry = createPQExpBuffer();
	PQExpBuffer delQry = createPQExpBuffer();
	PQExpBuffer creaQry = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			i_tableoid,
				i_oid,
				i_dba,
				i_encoding,
				i_collate,
				i_ctype,
				i_frozenxid,
				i_tablespace;
	CatalogId	dbCatId;
	DumpId		dbDumpId;
	const char *datname,
			   *dba,
			   *encoding,
			   *collate,
			   *ctype,
			   *tablespace;
	uint32		frozenxid;

	datname = PQdb(g_conn);

	if (g_verbose)
		write_msg(NULL, "saving database definition\n");

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* Get the database owner and parameters from pg_database */
	if (g_fout->remoteVersion >= 80400)
	{
		appendPQExpBuffer(dbQry, "SELECT tableoid, oid, "
						  "(%s datdba) AS dba, "
						  "pg_encoding_to_char(encoding) AS encoding, "
						  "datcollate, datctype, datfrozenxid, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
					  "shobj_description(oid, 'pg_database') AS description "

						  "FROM pg_database "
						  "WHERE datname = ",
						  username_subquery);
		appendStringLiteralAH(dbQry, datname, AH);
	}
	else if (g_fout->remoteVersion >= 80200)
	{
		appendPQExpBuffer(dbQry, "SELECT tableoid, oid, "
						  "(%s datdba) AS dba, "
						  "pg_encoding_to_char(encoding) AS encoding, "
					   "NULL AS datcollate, NULL AS datctype, datfrozenxid, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
					  "shobj_description(oid, 'pg_database') AS description "

						  "FROM pg_database "
						  "WHERE datname = ",
						  username_subquery);
		appendStringLiteralAH(dbQry, datname, AH);
	}
	else if (g_fout->remoteVersion >= 80000)
	{
		appendPQExpBuffer(dbQry, "SELECT tableoid, oid, "
						  "(%s datdba) AS dba, "
						  "pg_encoding_to_char(encoding) AS encoding, "
					   "NULL AS datcollate, NULL AS datctype, datfrozenxid, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace "
						  "FROM pg_database "
						  "WHERE datname = ",
						  username_subquery);
		appendStringLiteralAH(dbQry, datname, AH);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(dbQry, "SELECT tableoid, oid, "
						  "(%s datdba) AS dba, "
						  "pg_encoding_to_char(encoding) AS encoding, "
						  "NULL AS datcollate, NULL AS datctype, "
						  "0 AS datfrozenxid, "
						  "NULL AS tablespace "
						  "FROM pg_database "
						  "WHERE datname = ",
						  username_subquery);
		appendStringLiteralAH(dbQry, datname, AH);
	}
	else
	{
		appendPQExpBuffer(dbQry, "SELECT "
						  "(SELECT oid FROM pg_class WHERE relname = 'pg_database') AS tableoid, "
						  "oid, "
						  "(%s datdba) AS dba, "
						  "pg_encoding_to_char(encoding) AS encoding, "
						  "NULL AS datcollate, NULL AS datctype, "
						  "0 AS datfrozenxid, "
						  "NULL AS tablespace "
						  "FROM pg_database "
						  "WHERE datname = ",
						  username_subquery);
		appendStringLiteralAH(dbQry, datname, AH);
	}

	res = PQexec(g_conn, dbQry->data);
	check_sql_result(res, g_conn, dbQry->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	if (ntups <= 0)
	{
		write_msg(NULL, "missing pg_database entry for database \"%s\"\n",
				  datname);
		exit_nicely();
	}

	if (ntups != 1)
	{
		write_msg(NULL, "query returned more than one (%d) pg_database entry for database \"%s\"\n",
				  ntups, datname);
		exit_nicely();
	}

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_dba = PQfnumber(res, "dba");
	i_encoding = PQfnumber(res, "encoding");
	i_collate = PQfnumber(res, "datcollate");
	i_ctype = PQfnumber(res, "datctype");
	i_frozenxid = PQfnumber(res, "datfrozenxid");
	i_tablespace = PQfnumber(res, "tablespace");

	dbCatId.tableoid = atooid(PQgetvalue(res, 0, i_tableoid));
	dbCatId.oid = atooid(PQgetvalue(res, 0, i_oid));
	dba = PQgetvalue(res, 0, i_dba);
	encoding = PQgetvalue(res, 0, i_encoding);
	collate = PQgetvalue(res, 0, i_collate);
	ctype = PQgetvalue(res, 0, i_ctype);
	frozenxid = atooid(PQgetvalue(res, 0, i_frozenxid));
	tablespace = PQgetvalue(res, 0, i_tablespace);

	appendPQExpBuffer(creaQry, "CREATE DATABASE %s WITH TEMPLATE = template0",
					  fmtId(datname));
	if (strlen(encoding) > 0)
	{
		appendPQExpBuffer(creaQry, " ENCODING = ");
		appendStringLiteralAH(creaQry, encoding, AH);
	}
	if (strlen(collate) > 0)
	{
		appendPQExpBuffer(creaQry, " LC_COLLATE = ");
		appendStringLiteralAH(creaQry, collate, AH);
	}
	if (strlen(ctype) > 0)
	{
		appendPQExpBuffer(creaQry, " LC_CTYPE = ");
		appendStringLiteralAH(creaQry, ctype, AH);
	}
	if (strlen(tablespace) > 0 && strcmp(tablespace, "pg_default") != 0)
		appendPQExpBuffer(creaQry, " TABLESPACE = %s",
						  fmtId(tablespace));
	appendPQExpBuffer(creaQry, ";\n");

	if (binary_upgrade)
	{
		appendPQExpBuffer(creaQry, "\n-- For binary upgrade, set datfrozenxid.\n");
		appendPQExpBuffer(creaQry, "UPDATE pg_catalog.pg_database\n"
						  "SET datfrozenxid = '%u'\n"
						  "WHERE	datname = ",
						  frozenxid);
		appendStringLiteralAH(creaQry, datname, AH);
		appendPQExpBuffer(creaQry, ";\n");

	}

	appendPQExpBuffer(delQry, "DROP DATABASE %s;\n",
					  fmtId(datname));

	dbDumpId = createDumpId();

	ArchiveEntry(AH,
				 dbCatId,		/* catalog ID */
				 dbDumpId,		/* dump ID */
				 datname,		/* Name */
				 NULL,			/* Namespace */
				 NULL,			/* Tablespace */
				 dba,			/* Owner */
				 false,			/* with oids */
				 "DATABASE",	/* Desc */
				 SECTION_PRE_DATA,		/* Section */
				 creaQry->data, /* Create */
				 delQry->data,	/* Del */
				 NULL,			/* Copy */
				 NULL,			/* Deps */
				 0,				/* # Deps */
				 NULL,			/* Dumper */
				 NULL);			/* Dumper Arg */

	/*
	 * pg_largeobject comes from the old system intact, so set its
	 * relfrozenxid.
	 */
	if (binary_upgrade)
	{
		PGresult   *lo_res;
		PQExpBuffer loFrozenQry = createPQExpBuffer();
		PQExpBuffer loOutQry = createPQExpBuffer();
		int			i_relfrozenxid;

		appendPQExpBuffer(loFrozenQry, "SELECT relfrozenxid\n"
						  "FROM pg_catalog.pg_class\n"
						  "WHERE oid = %u;\n",
						  LargeObjectRelationId);

		lo_res = PQexec(g_conn, loFrozenQry->data);
		check_sql_result(lo_res, g_conn, loFrozenQry->data, PGRES_TUPLES_OK);

		if (PQntuples(lo_res) != 1)
		{
			write_msg(NULL, "dumpDatabase(): could not find pg_largeobject.relfrozenxid\n");
			exit_nicely();
		}

		i_relfrozenxid = PQfnumber(lo_res, "relfrozenxid");

		appendPQExpBuffer(loOutQry, "\n-- For binary upgrade, set pg_largeobject relfrozenxid.\n");
		appendPQExpBuffer(loOutQry, "UPDATE pg_catalog.pg_class\n"
						  "SET relfrozenxid = '%u'\n"
						  "WHERE oid = %u;\n",
						  atoi(PQgetvalue(lo_res, 0, i_relfrozenxid)),
						  LargeObjectRelationId);
		ArchiveEntry(AH, nilCatalogId, createDumpId(),
					 "pg_largeobject", NULL, NULL, "",
					 false, "pg_largeobject", SECTION_PRE_DATA,
					 loOutQry->data, "", NULL,
					 NULL, 0,
					 NULL, NULL);

		PQclear(lo_res);
		destroyPQExpBuffer(loFrozenQry);
		destroyPQExpBuffer(loOutQry);
	}

	/* Dump DB comment if any */
	if (g_fout->remoteVersion >= 80200)
	{
		/*
		 * 8.2 keeps comments on shared objects in a shared table, so we
		 * cannot use the dumpComment used for other database objects.
		 */
		char	   *comment = PQgetvalue(res, 0, PQfnumber(res, "description"));

		if (comment && strlen(comment))
		{
			resetPQExpBuffer(dbQry);

			/*
			 * Generates warning when loaded into a differently-named
			 * database.
			 */
			appendPQExpBuffer(dbQry, "COMMENT ON DATABASE %s IS ", fmtId(datname));
			appendStringLiteralAH(dbQry, comment, AH);
			appendPQExpBuffer(dbQry, ";\n");

			ArchiveEntry(AH, dbCatId, createDumpId(), datname, NULL, NULL,
						 dba, false, "COMMENT", SECTION_NONE,
						 dbQry->data, "", NULL,
						 &dbDumpId, 1, NULL, NULL);
		}
	}
	else
	{
		resetPQExpBuffer(dbQry);
		appendPQExpBuffer(dbQry, "DATABASE %s", fmtId(datname));
		dumpComment(AH, dbQry->data, NULL, "",
					dbCatId, 0, dbDumpId);
	}

	PQclear(res);

	destroyPQExpBuffer(dbQry);
	destroyPQExpBuffer(delQry);
	destroyPQExpBuffer(creaQry);
}


/*
 * dumpEncoding: put the correct encoding into the archive
 */
static void
dumpEncoding(Archive *AH)
{
	const char *encname = pg_encoding_to_char(AH->encoding);
	PQExpBuffer qry = createPQExpBuffer();

	if (g_verbose)
		write_msg(NULL, "saving encoding = %s\n", encname);

	appendPQExpBuffer(qry, "SET client_encoding = ");
	appendStringLiteralAH(qry, encname, AH);
	appendPQExpBuffer(qry, ";\n");

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "ENCODING", NULL, NULL, "",
				 false, "ENCODING", SECTION_PRE_DATA,
				 qry->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(qry);
}


/*
 * dumpStdStrings: put the correct escape string behavior into the archive
 */
static void
dumpStdStrings(Archive *AH)
{
	const char *stdstrings = AH->std_strings ? "on" : "off";
	PQExpBuffer qry = createPQExpBuffer();

	if (g_verbose)
		write_msg(NULL, "saving standard_conforming_strings = %s\n",
				  stdstrings);

	appendPQExpBuffer(qry, "SET standard_conforming_strings = '%s';\n",
					  stdstrings);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "STDSTRINGS", NULL, NULL, "",
				 false, "STDSTRINGS", SECTION_PRE_DATA,
				 qry->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(qry);
}


/*
 * getBlobs:
 *	Collect schema-level data about large objects
 */
static void
getBlobs(Archive *AH)
{
	PQExpBuffer blobQry = createPQExpBuffer();
	BlobInfo   *binfo;
	DumpableObject *bdata;
	PGresult   *res;
	int			ntups;
	int			i;

	/* Verbose message */
	if (g_verbose)
		write_msg(NULL, "reading large objects\n");

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* Fetch BLOB OIDs, and owner/ACL data if >= 9.0 */
	if (AH->remoteVersion >= 90000)
		appendPQExpBuffer(blobQry,
						  "SELECT oid, (%s lomowner) AS rolname, lomacl"
						  " FROM pg_largeobject_metadata",
						  username_subquery);
	else if (AH->remoteVersion >= 70100)
		appendPQExpBuffer(blobQry,
						  "SELECT DISTINCT loid, NULL::oid, NULL::oid"
						  " FROM pg_largeobject");
	else
		appendPQExpBuffer(blobQry,
						  "SELECT oid, NULL::oid, NULL::oid"
						  " FROM pg_class WHERE relkind = 'l'");

	res = PQexec(g_conn, blobQry->data);
	check_sql_result(res, g_conn, blobQry->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	if (ntups > 0)
	{
		/*
		 * Each large object has its own BLOB archive entry.
		 */
		binfo = (BlobInfo *) malloc(ntups * sizeof(BlobInfo));

		for (i = 0; i < ntups; i++)
		{
			binfo[i].dobj.objType = DO_BLOB;
			binfo[i].dobj.catId.tableoid = LargeObjectRelationId;
			binfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, 0));
			AssignDumpId(&binfo[i].dobj);

			binfo[i].dobj.name = strdup(PQgetvalue(res, i, 0));
			if (!PQgetisnull(res, i, 1))
				binfo[i].rolname = strdup(PQgetvalue(res, i, 1));
			else
				binfo[i].rolname = "";
			if (!PQgetisnull(res, i, 2))
				binfo[i].blobacl = strdup(PQgetvalue(res, i, 2));
			else
				binfo[i].blobacl = NULL;
		}

		/*
		 * If we have any large objects, a "BLOBS" archive entry is needed.
		 * This is just a placeholder for sorting; it carries no data now.
		 */
		bdata = (DumpableObject *) malloc(sizeof(DumpableObject));
		bdata->objType = DO_BLOB_DATA;
		bdata->catId = nilCatalogId;
		AssignDumpId(bdata);
		bdata->name = strdup("BLOBS");
	}

	PQclear(res);
	destroyPQExpBuffer(blobQry);
}

/*
 * dumpBlob
 *
 * dump the definition (metadata) of the given large object
 */
static void
dumpBlob(Archive *AH, BlobInfo *binfo)
{
	PQExpBuffer cquery = createPQExpBuffer();
	PQExpBuffer dquery = createPQExpBuffer();

	appendPQExpBuffer(cquery,
					  "SELECT pg_catalog.lo_create('%s');\n",
					  binfo->dobj.name);

	appendPQExpBuffer(dquery,
					  "SELECT pg_catalog.lo_unlink('%s');\n",
					  binfo->dobj.name);

	ArchiveEntry(AH, binfo->dobj.catId, binfo->dobj.dumpId,
				 binfo->dobj.name,
				 NULL, NULL,
				 binfo->rolname, false,
				 "BLOB", SECTION_PRE_DATA,
				 cquery->data, dquery->data, NULL,
				 binfo->dobj.dependencies, binfo->dobj.nDeps,
				 NULL, NULL);

	/* set up tag for comment and/or ACL */
	resetPQExpBuffer(cquery);
	appendPQExpBuffer(cquery, "LARGE OBJECT %s", binfo->dobj.name);

	/* Dump comment if any */
	dumpComment(AH, cquery->data,
				NULL, binfo->rolname,
				binfo->dobj.catId, 0, binfo->dobj.dumpId);

	/* Dump ACL if any */
	if (binfo->blobacl)
		dumpACL(AH, binfo->dobj.catId, binfo->dobj.dumpId, "LARGE OBJECT",
				binfo->dobj.name, NULL, cquery->data,
				NULL, binfo->rolname, binfo->blobacl);

	destroyPQExpBuffer(cquery);
	destroyPQExpBuffer(dquery);
}

/*
 * dumpBlobs:
 *	dump the data contents of all large objects
 */
static int
dumpBlobs(Archive *AH, void *arg)
{
	const char *blobQry;
	const char *blobFetchQry;
	PGresult   *res;
	char		buf[LOBBUFSIZE];
	int			ntups;
	int			i;
	int			cnt;

	if (g_verbose)
		write_msg(NULL, "saving large objects\n");

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/*
	 * Currently, we re-fetch all BLOB OIDs using a cursor.  Consider scanning
	 * the already-in-memory dumpable objects instead...
	 */
	if (AH->remoteVersion >= 90000)
		blobQry = "DECLARE bloboid CURSOR FOR SELECT oid FROM pg_largeobject_metadata";
	else if (AH->remoteVersion >= 70100)
		blobQry = "DECLARE bloboid CURSOR FOR SELECT DISTINCT loid FROM pg_largeobject";
	else
		blobQry = "DECLARE bloboid CURSOR FOR SELECT oid FROM pg_class WHERE relkind = 'l'";

	res = PQexec(g_conn, blobQry);
	check_sql_result(res, g_conn, blobQry, PGRES_COMMAND_OK);

	/* Command to fetch from cursor */
	blobFetchQry = "FETCH 1000 IN bloboid";

	do
	{
		PQclear(res);

		/* Do a fetch */
		res = PQexec(g_conn, blobFetchQry);
		check_sql_result(res, g_conn, blobFetchQry, PGRES_TUPLES_OK);

		/* Process the tuples, if any */
		ntups = PQntuples(res);
		for (i = 0; i < ntups; i++)
		{
			Oid			blobOid;
			int			loFd;

			blobOid = atooid(PQgetvalue(res, i, 0));
			/* Open the BLOB */
			loFd = lo_open(g_conn, blobOid, INV_READ);
			if (loFd == -1)
			{
				write_msg(NULL, "could not open large object %u: %s",
						  blobOid, PQerrorMessage(g_conn));
				exit_nicely();
			}

			StartBlob(AH, blobOid);

			/* Now read it in chunks, sending data to archive */
			do
			{
				cnt = lo_read(g_conn, loFd, buf, LOBBUFSIZE);
				if (cnt < 0)
				{
					write_msg(NULL, "error reading large object %u: %s",
							  blobOid, PQerrorMessage(g_conn));
					exit_nicely();
				}

				WriteData(AH, buf, cnt);
			} while (cnt > 0);

			lo_close(g_conn, loFd);

			EndBlob(AH, blobOid);
		}
	} while (ntups > 0);

	PQclear(res);

	return 1;
}

static void
binary_upgrade_set_type_oids_by_type_oid(PQExpBuffer upgrade_buffer,
										 Oid pg_type_oid)
{
	PQExpBuffer upgrade_query = createPQExpBuffer();
	int			ntups;
	PGresult   *upgrade_res;
	Oid			pg_type_array_oid;

	appendPQExpBuffer(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type oid\n");
	appendPQExpBuffer(upgrade_buffer,
	 "SELECT binary_upgrade.set_next_pg_type_oid('%u'::pg_catalog.oid);\n\n",
					  pg_type_oid);

	/* we only support old >= 8.3 for binary upgrades */
	appendPQExpBuffer(upgrade_query,
					  "SELECT typarray "
					  "FROM pg_catalog.pg_type "
					  "WHERE pg_type.oid = '%u'::pg_catalog.oid;",
					  pg_type_oid);

	upgrade_res = PQexec(g_conn, upgrade_query->data);
	check_sql_result(upgrade_res, g_conn, upgrade_query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, upgrade_query->data);
		exit_nicely();
	}

	pg_type_array_oid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "typarray")));

	if (OidIsValid(pg_type_array_oid))
	{
		appendPQExpBuffer(upgrade_buffer,
			   "\n-- For binary upgrade, must preserve pg_type array oid\n");
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_pg_type_array_oid('%u'::pg_catalog.oid);\n\n",
						  pg_type_array_oid);
	}

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);
}

static bool
binary_upgrade_set_type_oids_by_rel_oid(PQExpBuffer upgrade_buffer,
										Oid pg_rel_oid)
{
	PQExpBuffer upgrade_query = createPQExpBuffer();
	int			ntups;
	PGresult   *upgrade_res;
	Oid			pg_type_oid;
	bool		toast_set = false;

	/* we only support old >= 8.3 for binary upgrades */
	appendPQExpBuffer(upgrade_query,
					  "SELECT c.reltype AS crel, t.reltype AS trel "
					  "FROM pg_catalog.pg_class c "
					  "LEFT JOIN pg_catalog.pg_class t ON "
					  "  (c.reltoastrelid = t.oid) "
					  "WHERE c.oid = '%u'::pg_catalog.oid;",
					  pg_rel_oid);

	upgrade_res = PQexec(g_conn, upgrade_query->data);
	check_sql_result(upgrade_res, g_conn, upgrade_query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, upgrade_query->data);
		exit_nicely();
	}

	pg_type_oid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "crel")));

	binary_upgrade_set_type_oids_by_type_oid(upgrade_buffer, pg_type_oid);

	if (!PQgetisnull(upgrade_res, 0, PQfnumber(upgrade_res, "trel")))
	{
		/* Toast tables do not have pg_type array rows */
		Oid			pg_type_toast_oid = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "trel")));

		appendPQExpBuffer(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type toast oid\n");
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_pg_type_toast_oid('%u'::pg_catalog.oid);\n\n",
						  pg_type_toast_oid);

		toast_set = true;
	}

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);

	return toast_set;
}

static void
binary_upgrade_set_relfilenodes(PQExpBuffer upgrade_buffer, Oid pg_class_oid,
								bool is_index)
{
	PQExpBuffer upgrade_query = createPQExpBuffer();
	int			ntups;
	PGresult   *upgrade_res;
	Oid			pg_class_relfilenode;
	Oid			pg_class_reltoastrelid;
	Oid			pg_class_reltoastidxid;

	/*
	 * Note: we don't need to use pg_relation_filenode() here because this
	 * function is not intended to be used against system catalogs. Otherwise
	 * we'd have to worry about which versions pg_relation_filenode is
	 * available in.
	 */
	appendPQExpBuffer(upgrade_query,
					"SELECT c.relfilenode, c.reltoastrelid, t.reltoastidxid "
					  "FROM pg_catalog.pg_class c LEFT JOIN "
					  "pg_catalog.pg_class t ON (c.reltoastrelid = t.oid) "
					  "WHERE c.oid = '%u'::pg_catalog.oid;",
					  pg_class_oid);

	upgrade_res = PQexec(g_conn, upgrade_query->data);
	check_sql_result(upgrade_res, g_conn, upgrade_query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, upgrade_query->data);
		exit_nicely();
	}

	pg_class_relfilenode = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "relfilenode")));
	pg_class_reltoastrelid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "reltoastrelid")));
	pg_class_reltoastidxid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "reltoastidxid")));

	appendPQExpBuffer(upgrade_buffer,
					"\n-- For binary upgrade, must preserve relfilenodes\n");

	if (!is_index)
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_heap_relfilenode('%u'::pg_catalog.oid);\n",
						  pg_class_relfilenode);
	else
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_index_relfilenode('%u'::pg_catalog.oid);\n",
						  pg_class_relfilenode);

	if (OidIsValid(pg_class_reltoastrelid))
	{
		/*
		 * One complexity is that the table definition might not require the
		 * creation of a TOAST table, and the TOAST table might have been
		 * created long after table creation, when the table was loaded with
		 * wide data.  By setting the TOAST relfilenode we force creation of
		 * the TOAST heap and TOAST index by the backend so we can cleanly
		 * migrate the files during binary migration.
		 */

		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_toast_relfilenode('%u'::pg_catalog.oid);\n",
						  pg_class_reltoastrelid);

		/* every toast table has an index */
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_index_relfilenode('%u'::pg_catalog.oid);\n",
						  pg_class_reltoastidxid);
	}
	appendPQExpBuffer(upgrade_buffer, "\n");

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);
}

/*
 * getNamespaces:
 *	  read all namespaces in the system catalogs and return them in the
 * NamespaceInfo* structure
 *
 *	numNamespaces is set to the number of namespaces read in
 */
NamespaceInfo *
getNamespaces(int *numNamespaces)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	NamespaceInfo *nsinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_nspname;
	int			i_rolname;
	int			i_nspacl;

	/*
	 * Before 7.3, there are no real namespaces; create two dummy entries, one
	 * for user stuff and one for system stuff.
	 */
	if (g_fout->remoteVersion < 70300)
	{
		nsinfo = (NamespaceInfo *) malloc(2 * sizeof(NamespaceInfo));

		nsinfo[0].dobj.objType = DO_NAMESPACE;
		nsinfo[0].dobj.catId.tableoid = 0;
		nsinfo[0].dobj.catId.oid = 0;
		AssignDumpId(&nsinfo[0].dobj);
		nsinfo[0].dobj.name = strdup("public");
		nsinfo[0].rolname = strdup("");
		nsinfo[0].nspacl = strdup("");

		selectDumpableNamespace(&nsinfo[0]);

		nsinfo[1].dobj.objType = DO_NAMESPACE;
		nsinfo[1].dobj.catId.tableoid = 0;
		nsinfo[1].dobj.catId.oid = 1;
		AssignDumpId(&nsinfo[1].dobj);
		nsinfo[1].dobj.name = strdup("pg_catalog");
		nsinfo[1].rolname = strdup("");
		nsinfo[1].nspacl = strdup("");

		selectDumpableNamespace(&nsinfo[1]);

		g_namespaces = nsinfo;
		g_numNamespaces = *numNamespaces = 2;

		return nsinfo;
	}

	query = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/*
	 * we fetch all namespaces including system ones, so that every object we
	 * read in can be linked to a containing namespace.
	 */
	appendPQExpBuffer(query, "SELECT tableoid, oid, nspname, "
					  "(%s nspowner) AS rolname, "
					  "nspacl FROM pg_namespace",
					  username_subquery);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	nsinfo = (NamespaceInfo *) malloc(ntups * sizeof(NamespaceInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_nspname = PQfnumber(res, "nspname");
	i_rolname = PQfnumber(res, "rolname");
	i_nspacl = PQfnumber(res, "nspacl");

	for (i = 0; i < ntups; i++)
	{
		nsinfo[i].dobj.objType = DO_NAMESPACE;
		nsinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		nsinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&nsinfo[i].dobj);
		nsinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_nspname));
		nsinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		nsinfo[i].nspacl = strdup(PQgetvalue(res, i, i_nspacl));

		/* Decide whether to dump this namespace */
		selectDumpableNamespace(&nsinfo[i]);

		if (strlen(nsinfo[i].rolname) == 0)
			write_msg(NULL, "WARNING: owner of schema \"%s\" appears to be invalid\n",
					  nsinfo[i].dobj.name);
	}

	PQclear(res);
	destroyPQExpBuffer(query);

	g_namespaces = nsinfo;
	g_numNamespaces = *numNamespaces = ntups;

	return nsinfo;
}

/*
 * findNamespace:
 *		given a namespace OID and an object OID, look up the info read by
 *		getNamespaces
 *
 * NB: for pre-7.3 source database, we use object OID to guess whether it's
 * a system object or not.	In 7.3 and later there is no guessing.
 */
static NamespaceInfo *
findNamespace(Oid nsoid, Oid objoid)
{
	int			i;

	if (g_fout->remoteVersion >= 70300)
	{
		for (i = 0; i < g_numNamespaces; i++)
		{
			NamespaceInfo *nsinfo = &g_namespaces[i];

			if (nsoid == nsinfo->dobj.catId.oid)
				return nsinfo;
		}
		write_msg(NULL, "schema with OID %u does not exist\n", nsoid);
		exit_nicely();
	}
	else
	{
		/* This code depends on the layout set up by getNamespaces. */
		if (objoid > g_last_builtin_oid)
			i = 0;				/* user object */
		else
			i = 1;				/* system object */
		return &g_namespaces[i];
	}

	return NULL;				/* keep compiler quiet */
}

/*
 * getTypes:
 *	  read all types in the system catalogs and return them in the
 * TypeInfo* structure
 *
 *	numTypes is set to the number of types read in
 *
 * NB: this must run after getFuncs() because we assume we can do
 * findFuncByOid().
 */
TypeInfo *
getTypes(int *numTypes)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	TypeInfo   *tyinfo;
	ShellTypeInfo *stinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_typname;
	int			i_typnamespace;
	int			i_rolname;
	int			i_typinput;
	int			i_typoutput;
	int			i_typelem;
	int			i_typrelid;
	int			i_typrelkind;
	int			i_typtype;
	int			i_typisdefined;
	int			i_isarray;

	/*
	 * we include even the built-in types because those may be used as array
	 * elements by user-defined types
	 *
	 * we filter out the built-in types when we dump out the types
	 *
	 * same approach for undefined (shell) types and array types
	 *
	 * Note: as of 8.3 we can reliably detect whether a type is an
	 * auto-generated array type by checking the element type's typarray.
	 * (Before that the test is capable of generating false positives.) We
	 * still check for name beginning with '_', though, so as to avoid the
	 * cost of the subselect probe for all standard types.	This would have to
	 * be revisited if the backend ever allows renaming of array types.
	 */

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	if (g_fout->remoteVersion >= 80300)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, typname, "
						  "typnamespace, "
						  "(%s typowner) AS rolname, "
						  "typinput::oid AS typinput, "
						  "typoutput::oid AS typoutput, typelem, typrelid, "
						  "CASE WHEN typrelid = 0 THEN ' '::\"char\" "
						  "ELSE (SELECT relkind FROM pg_class WHERE oid = typrelid) END AS typrelkind, "
						  "typtype, typisdefined, "
						  "typname[0] = '_' AND typelem != 0 AND "
						  "(SELECT typarray FROM pg_type te WHERE oid = pg_type.typelem) = oid AS isarray "
						  "FROM pg_type",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, typname, "
						  "typnamespace, "
						  "(%s typowner) AS rolname, "
						  "typinput::oid AS typinput, "
						  "typoutput::oid AS typoutput, typelem, typrelid, "
						  "CASE WHEN typrelid = 0 THEN ' '::\"char\" "
						  "ELSE (SELECT relkind FROM pg_class WHERE oid = typrelid) END AS typrelkind, "
						  "typtype, typisdefined, "
						  "typname[0] = '_' AND typelem != 0 AS isarray "
						  "FROM pg_type",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, typname, "
						  "0::oid AS typnamespace, "
						  "(%s typowner) AS rolname, "
						  "typinput::oid AS typinput, "
						  "typoutput::oid AS typoutput, typelem, typrelid, "
						  "CASE WHEN typrelid = 0 THEN ' '::\"char\" "
						  "ELSE (SELECT relkind FROM pg_class WHERE oid = typrelid) END AS typrelkind, "
						  "typtype, typisdefined, "
						  "typname[0] = '_' AND typelem != 0 AS isarray "
						  "FROM pg_type",
						  username_subquery);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT "
		 "(SELECT oid FROM pg_class WHERE relname = 'pg_type') AS tableoid, "
						  "oid, typname, "
						  "0::oid AS typnamespace, "
						  "(%s typowner) AS rolname, "
						  "typinput::oid AS typinput, "
						  "typoutput::oid AS typoutput, typelem, typrelid, "
						  "CASE WHEN typrelid = 0 THEN ' '::\"char\" "
						  "ELSE (SELECT relkind FROM pg_class WHERE oid = typrelid) END AS typrelkind, "
						  "typtype, typisdefined, "
						  "typname[0] = '_' AND typelem != 0 AS isarray "
						  "FROM pg_type",
						  username_subquery);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	tyinfo = (TypeInfo *) malloc(ntups * sizeof(TypeInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_typname = PQfnumber(res, "typname");
	i_typnamespace = PQfnumber(res, "typnamespace");
	i_rolname = PQfnumber(res, "rolname");
	i_typinput = PQfnumber(res, "typinput");
	i_typoutput = PQfnumber(res, "typoutput");
	i_typelem = PQfnumber(res, "typelem");
	i_typrelid = PQfnumber(res, "typrelid");
	i_typrelkind = PQfnumber(res, "typrelkind");
	i_typtype = PQfnumber(res, "typtype");
	i_typisdefined = PQfnumber(res, "typisdefined");
	i_isarray = PQfnumber(res, "isarray");

	for (i = 0; i < ntups; i++)
	{
		tyinfo[i].dobj.objType = DO_TYPE;
		tyinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		tyinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&tyinfo[i].dobj);
		tyinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_typname));
		tyinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_typnamespace)),
												 tyinfo[i].dobj.catId.oid);
		tyinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		tyinfo[i].typelem = atooid(PQgetvalue(res, i, i_typelem));
		tyinfo[i].typrelid = atooid(PQgetvalue(res, i, i_typrelid));
		tyinfo[i].typrelkind = *PQgetvalue(res, i, i_typrelkind);
		tyinfo[i].typtype = *PQgetvalue(res, i, i_typtype);
		tyinfo[i].shellType = NULL;

		if (strcmp(PQgetvalue(res, i, i_typisdefined), "t") == 0)
			tyinfo[i].isDefined = true;
		else
			tyinfo[i].isDefined = false;

		if (strcmp(PQgetvalue(res, i, i_isarray), "t") == 0)
			tyinfo[i].isArray = true;
		else
			tyinfo[i].isArray = false;

		/* Decide whether we want to dump it */
		selectDumpableType(&tyinfo[i]);

		/*
		 * If it's a domain, fetch info about its constraints, if any
		 */
		tyinfo[i].nDomChecks = 0;
		tyinfo[i].domChecks = NULL;
		if (tyinfo[i].dobj.dump && tyinfo[i].typtype == TYPTYPE_DOMAIN)
			getDomainConstraints(&(tyinfo[i]));

		/*
		 * If it's a base type, make a DumpableObject representing a shell
		 * definition of the type.	We will need to dump that ahead of the I/O
		 * functions for the type.
		 *
		 * Note: the shell type doesn't have a catId.  You might think it
		 * should copy the base type's catId, but then it might capture the
		 * pg_depend entries for the type, which we don't want.
		 */
		if (tyinfo[i].dobj.dump && tyinfo[i].typtype == TYPTYPE_BASE)
		{
			stinfo = (ShellTypeInfo *) malloc(sizeof(ShellTypeInfo));
			stinfo->dobj.objType = DO_SHELL_TYPE;
			stinfo->dobj.catId = nilCatalogId;
			AssignDumpId(&stinfo->dobj);
			stinfo->dobj.name = strdup(tyinfo[i].dobj.name);
			stinfo->dobj.namespace = tyinfo[i].dobj.namespace;
			stinfo->baseType = &(tyinfo[i]);
			tyinfo[i].shellType = stinfo;

			/*
			 * Initially mark the shell type as not to be dumped.  We'll only
			 * dump it if the I/O functions need to be dumped; this is taken
			 * care of while sorting dependencies.
			 */
			stinfo->dobj.dump = false;

			/*
			 * However, if dumping from pre-7.3, there will be no dependency
			 * info so we have to fake it here.  We only need to worry about
			 * typinput and typoutput since the other functions only exist
			 * post-7.3.
			 */
			if (g_fout->remoteVersion < 70300)
			{
				Oid			typinput;
				Oid			typoutput;
				FuncInfo   *funcInfo;

				typinput = atooid(PQgetvalue(res, i, i_typinput));
				typoutput = atooid(PQgetvalue(res, i, i_typoutput));

				funcInfo = findFuncByOid(typinput);
				if (funcInfo && funcInfo->dobj.dump)
				{
					/* base type depends on function */
					addObjectDependency(&tyinfo[i].dobj,
										funcInfo->dobj.dumpId);
					/* function depends on shell type */
					addObjectDependency(&funcInfo->dobj,
										stinfo->dobj.dumpId);
					/* mark shell type as to be dumped */
					stinfo->dobj.dump = true;
				}

				funcInfo = findFuncByOid(typoutput);
				if (funcInfo && funcInfo->dobj.dump)
				{
					/* base type depends on function */
					addObjectDependency(&tyinfo[i].dobj,
										funcInfo->dobj.dumpId);
					/* function depends on shell type */
					addObjectDependency(&funcInfo->dobj,
										stinfo->dobj.dumpId);
					/* mark shell type as to be dumped */
					stinfo->dobj.dump = true;
				}
			}
		}

		if (strlen(tyinfo[i].rolname) == 0 && tyinfo[i].isDefined)
			write_msg(NULL, "WARNING: owner of data type \"%s\" appears to be invalid\n",
					  tyinfo[i].dobj.name);
	}

	*numTypes = ntups;

	PQclear(res);

	destroyPQExpBuffer(query);

	return tyinfo;
}

/*
 * getOperators:
 *	  read all operators in the system catalogs and return them in the
 * OprInfo* structure
 *
 *	numOprs is set to the number of operators read in
 */
OprInfo *
getOperators(int *numOprs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	OprInfo    *oprinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_oprname;
	int			i_oprnamespace;
	int			i_rolname;
	int			i_oprcode;

	/*
	 * find all operators, including builtin operators; we filter out
	 * system-defined operators at dump-out time.
	 */

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, oprname, "
						  "oprnamespace, "
						  "(%s oprowner) AS rolname, "
						  "oprcode::oid AS oprcode "
						  "FROM pg_operator",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, oprname, "
						  "0::oid AS oprnamespace, "
						  "(%s oprowner) AS rolname, "
						  "oprcode::oid AS oprcode "
						  "FROM pg_operator",
						  username_subquery);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT "
						  "(SELECT oid FROM pg_class WHERE relname = 'pg_operator') AS tableoid, "
						  "oid, oprname, "
						  "0::oid AS oprnamespace, "
						  "(%s oprowner) AS rolname, "
						  "oprcode::oid AS oprcode "
						  "FROM pg_operator",
						  username_subquery);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numOprs = ntups;

	oprinfo = (OprInfo *) malloc(ntups * sizeof(OprInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_oprname = PQfnumber(res, "oprname");
	i_oprnamespace = PQfnumber(res, "oprnamespace");
	i_rolname = PQfnumber(res, "rolname");
	i_oprcode = PQfnumber(res, "oprcode");

	for (i = 0; i < ntups; i++)
	{
		oprinfo[i].dobj.objType = DO_OPERATOR;
		oprinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		oprinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&oprinfo[i].dobj);
		oprinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_oprname));
		oprinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_oprnamespace)),
												  oprinfo[i].dobj.catId.oid);
		oprinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		oprinfo[i].oprcode = atooid(PQgetvalue(res, i, i_oprcode));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(oprinfo[i].dobj));

		if (strlen(oprinfo[i].rolname) == 0)
			write_msg(NULL, "WARNING: owner of operator \"%s\" appears to be invalid\n",
					  oprinfo[i].dobj.name);
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return oprinfo;
}

/*
 * getConversions:
 *	  read all conversions in the system catalogs and return them in the
 * ConvInfo* structure
 *
 *	numConversions is set to the number of conversions read in
 */
ConvInfo *
getConversions(int *numConversions)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	ConvInfo   *convinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_conname;
	int			i_connamespace;
	int			i_rolname;

	/* Conversions didn't exist pre-7.3 */
	if (g_fout->remoteVersion < 70300)
	{
		*numConversions = 0;
		return NULL;
	}

	/*
	 * find all conversions, including builtin conversions; we filter out
	 * system-defined conversions at dump-out time.
	 */

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT tableoid, oid, conname, "
					  "connamespace, "
					  "(%s conowner) AS rolname "
					  "FROM pg_conversion",
					  username_subquery);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numConversions = ntups;

	convinfo = (ConvInfo *) malloc(ntups * sizeof(ConvInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_conname = PQfnumber(res, "conname");
	i_connamespace = PQfnumber(res, "connamespace");
	i_rolname = PQfnumber(res, "rolname");

	for (i = 0; i < ntups; i++)
	{
		convinfo[i].dobj.objType = DO_CONVERSION;
		convinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		convinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&convinfo[i].dobj);
		convinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_conname));
		convinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_connamespace)),
												 convinfo[i].dobj.catId.oid);
		convinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(convinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return convinfo;
}

/*
 * getOpclasses:
 *	  read all opclasses in the system catalogs and return them in the
 * OpclassInfo* structure
 *
 *	numOpclasses is set to the number of opclasses read in
 */
OpclassInfo *
getOpclasses(int *numOpclasses)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	OpclassInfo *opcinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_opcname;
	int			i_opcnamespace;
	int			i_rolname;

	/*
	 * find all opclasses, including builtin opclasses; we filter out
	 * system-defined opclasses at dump-out time.
	 */

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, opcname, "
						  "opcnamespace, "
						  "(%s opcowner) AS rolname "
						  "FROM pg_opclass",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, opcname, "
						  "0::oid AS opcnamespace, "
						  "''::name AS rolname "
						  "FROM pg_opclass");
	}
	else
	{
		appendPQExpBuffer(query, "SELECT "
						  "(SELECT oid FROM pg_class WHERE relname = 'pg_opclass') AS tableoid, "
						  "oid, opcname, "
						  "0::oid AS opcnamespace, "
						  "''::name AS rolname "
						  "FROM pg_opclass");
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numOpclasses = ntups;

	opcinfo = (OpclassInfo *) malloc(ntups * sizeof(OpclassInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_opcname = PQfnumber(res, "opcname");
	i_opcnamespace = PQfnumber(res, "opcnamespace");
	i_rolname = PQfnumber(res, "rolname");

	for (i = 0; i < ntups; i++)
	{
		opcinfo[i].dobj.objType = DO_OPCLASS;
		opcinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		opcinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&opcinfo[i].dobj);
		opcinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_opcname));
		opcinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_opcnamespace)),
												  opcinfo[i].dobj.catId.oid);
		opcinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(opcinfo[i].dobj));

		if (g_fout->remoteVersion >= 70300)
		{
			if (strlen(opcinfo[i].rolname) == 0)
				write_msg(NULL, "WARNING: owner of operator class \"%s\" appears to be invalid\n",
						  opcinfo[i].dobj.name);
		}
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return opcinfo;
}

/*
 * getOpfamilies:
 *	  read all opfamilies in the system catalogs and return them in the
 * OpfamilyInfo* structure
 *
 *	numOpfamilies is set to the number of opfamilies read in
 */
OpfamilyInfo *
getOpfamilies(int *numOpfamilies)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	OpfamilyInfo *opfinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_opfname;
	int			i_opfnamespace;
	int			i_rolname;

	/* Before 8.3, there is no separate concept of opfamilies */
	if (g_fout->remoteVersion < 80300)
	{
		*numOpfamilies = 0;
		return NULL;
	}

	query = createPQExpBuffer();

	/*
	 * find all opfamilies, including builtin opfamilies; we filter out
	 * system-defined opfamilies at dump-out time.
	 */

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT tableoid, oid, opfname, "
					  "opfnamespace, "
					  "(%s opfowner) AS rolname "
					  "FROM pg_opfamily",
					  username_subquery);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numOpfamilies = ntups;

	opfinfo = (OpfamilyInfo *) malloc(ntups * sizeof(OpfamilyInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_opfname = PQfnumber(res, "opfname");
	i_opfnamespace = PQfnumber(res, "opfnamespace");
	i_rolname = PQfnumber(res, "rolname");

	for (i = 0; i < ntups; i++)
	{
		opfinfo[i].dobj.objType = DO_OPFAMILY;
		opfinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		opfinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&opfinfo[i].dobj);
		opfinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_opfname));
		opfinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_opfnamespace)),
												  opfinfo[i].dobj.catId.oid);
		opfinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(opfinfo[i].dobj));

		if (g_fout->remoteVersion >= 70300)
		{
			if (strlen(opfinfo[i].rolname) == 0)
				write_msg(NULL, "WARNING: owner of operator family \"%s\" appears to be invalid\n",
						  opfinfo[i].dobj.name);
		}
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return opfinfo;
}

/*
 * getAggregates:
 *	  read all the user-defined aggregates in the system catalogs and
 * return them in the AggInfo* structure
 *
 * numAggs is set to the number of aggregates read in
 */
AggInfo *
getAggregates(int *numAggs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	AggInfo    *agginfo;
	int			i_tableoid;
	int			i_oid;
	int			i_aggname;
	int			i_aggnamespace;
	int			i_pronargs;
	int			i_proargtypes;
	int			i_rolname;
	int			i_aggacl;

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* find all user-defined aggregates */

	if (g_fout->remoteVersion >= 80200)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, proname AS aggname, "
						  "pronamespace AS aggnamespace, "
						  "pronargs, proargtypes, "
						  "(%s proowner) AS rolname, "
						  "proacl AS aggacl "
						  "FROM pg_proc "
						  "WHERE proisagg "
						  "AND pronamespace != "
			   "(SELECT oid FROM pg_namespace WHERE nspname = 'pg_catalog')",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, proname AS aggname, "
						  "pronamespace AS aggnamespace, "
						  "CASE WHEN proargtypes[0] = 'pg_catalog.\"any\"'::pg_catalog.regtype THEN 0 ELSE 1 END AS pronargs, "
						  "proargtypes, "
						  "(%s proowner) AS rolname, "
						  "proacl AS aggacl "
						  "FROM pg_proc "
						  "WHERE proisagg "
						  "AND pronamespace != "
			   "(SELECT oid FROM pg_namespace WHERE nspname = 'pg_catalog')",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, aggname, "
						  "0::oid AS aggnamespace, "
				  "CASE WHEN aggbasetype = 0 THEN 0 ELSE 1 END AS pronargs, "
						  "aggbasetype AS proargtypes, "
						  "(%s aggowner) AS rolname, "
						  "'{=X}' AS aggacl "
						  "FROM pg_aggregate "
						  "where oid > '%u'::oid",
						  username_subquery,
						  g_last_builtin_oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT "
						  "(SELECT oid FROM pg_class WHERE relname = 'pg_aggregate') AS tableoid, "
						  "oid, aggname, "
						  "0::oid AS aggnamespace, "
				  "CASE WHEN aggbasetype = 0 THEN 0 ELSE 1 END AS pronargs, "
						  "aggbasetype AS proargtypes, "
						  "(%s aggowner) AS rolname, "
						  "'{=X}' AS aggacl "
						  "FROM pg_aggregate "
						  "where oid > '%u'::oid",
						  username_subquery,
						  g_last_builtin_oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numAggs = ntups;

	agginfo = (AggInfo *) malloc(ntups * sizeof(AggInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_aggname = PQfnumber(res, "aggname");
	i_aggnamespace = PQfnumber(res, "aggnamespace");
	i_pronargs = PQfnumber(res, "pronargs");
	i_proargtypes = PQfnumber(res, "proargtypes");
	i_rolname = PQfnumber(res, "rolname");
	i_aggacl = PQfnumber(res, "aggacl");

	for (i = 0; i < ntups; i++)
	{
		agginfo[i].aggfn.dobj.objType = DO_AGG;
		agginfo[i].aggfn.dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		agginfo[i].aggfn.dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&agginfo[i].aggfn.dobj);
		agginfo[i].aggfn.dobj.name = strdup(PQgetvalue(res, i, i_aggname));
		agginfo[i].aggfn.dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_aggnamespace)),
											agginfo[i].aggfn.dobj.catId.oid);
		agginfo[i].aggfn.rolname = strdup(PQgetvalue(res, i, i_rolname));
		if (strlen(agginfo[i].aggfn.rolname) == 0)
			write_msg(NULL, "WARNING: owner of aggregate function \"%s\" appears to be invalid\n",
					  agginfo[i].aggfn.dobj.name);
		agginfo[i].aggfn.lang = InvalidOid;		/* not currently interesting */
		agginfo[i].aggfn.prorettype = InvalidOid;		/* not saved */
		agginfo[i].aggfn.proacl = strdup(PQgetvalue(res, i, i_aggacl));
		agginfo[i].aggfn.nargs = atoi(PQgetvalue(res, i, i_pronargs));
		if (agginfo[i].aggfn.nargs == 0)
			agginfo[i].aggfn.argtypes = NULL;
		else
		{
			agginfo[i].aggfn.argtypes = (Oid *) malloc(agginfo[i].aggfn.nargs * sizeof(Oid));
			if (g_fout->remoteVersion >= 70300)
				parseOidArray(PQgetvalue(res, i, i_proargtypes),
							  agginfo[i].aggfn.argtypes,
							  agginfo[i].aggfn.nargs);
			else
				/* it's just aggbasetype */
				agginfo[i].aggfn.argtypes[0] = atooid(PQgetvalue(res, i, i_proargtypes));
		}

		/* Decide whether we want to dump it */
		selectDumpableObject(&(agginfo[i].aggfn.dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return agginfo;
}

/*
 * getFuncs:
 *	  read all the user-defined functions in the system catalogs and
 * return them in the FuncInfo* structure
 *
 * numFuncs is set to the number of functions read in
 */
FuncInfo *
getFuncs(int *numFuncs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	FuncInfo   *finfo;
	int			i_tableoid;
	int			i_oid;
	int			i_proname;
	int			i_pronamespace;
	int			i_rolname;
	int			i_prolang;
	int			i_pronargs;
	int			i_proargtypes;
	int			i_prorettype;
	int			i_proacl;

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* find all user-defined funcs */

	if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query,
						  "SELECT tableoid, oid, proname, prolang, "
						  "pronargs, proargtypes, prorettype, proacl, "
						  "pronamespace, "
						  "(%s proowner) AS rolname "
						  "FROM pg_proc "
						  "WHERE NOT proisagg "
						  "AND pronamespace != "
						  "(SELECT oid FROM pg_namespace "
						  "WHERE nspname = 'pg_catalog')",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query,
						  "SELECT tableoid, oid, proname, prolang, "
						  "pronargs, proargtypes, prorettype, "
						  "'{=X}' AS proacl, "
						  "0::oid AS pronamespace, "
						  "(%s proowner) AS rolname "
						  "FROM pg_proc "
						  "WHERE pg_proc.oid > '%u'::oid",
						  username_subquery,
						  g_last_builtin_oid);
	}
	else
	{
		appendPQExpBuffer(query,
						  "SELECT "
						  "(SELECT oid FROM pg_class "
						  " WHERE relname = 'pg_proc') AS tableoid, "
						  "oid, proname, prolang, "
						  "pronargs, proargtypes, prorettype, "
						  "'{=X}' AS proacl, "
						  "0::oid AS pronamespace, "
						  "(%s proowner) AS rolname "
						  "FROM pg_proc "
						  "where pg_proc.oid > '%u'::oid",
						  username_subquery,
						  g_last_builtin_oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numFuncs = ntups;

	finfo = (FuncInfo *) calloc(ntups, sizeof(FuncInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_proname = PQfnumber(res, "proname");
	i_pronamespace = PQfnumber(res, "pronamespace");
	i_rolname = PQfnumber(res, "rolname");
	i_prolang = PQfnumber(res, "prolang");
	i_pronargs = PQfnumber(res, "pronargs");
	i_proargtypes = PQfnumber(res, "proargtypes");
	i_prorettype = PQfnumber(res, "prorettype");
	i_proacl = PQfnumber(res, "proacl");

	for (i = 0; i < ntups; i++)
	{
		finfo[i].dobj.objType = DO_FUNC;
		finfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		finfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&finfo[i].dobj);
		finfo[i].dobj.name = strdup(PQgetvalue(res, i, i_proname));
		finfo[i].dobj.namespace =
			findNamespace(atooid(PQgetvalue(res, i, i_pronamespace)),
						  finfo[i].dobj.catId.oid);
		finfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		finfo[i].lang = atooid(PQgetvalue(res, i, i_prolang));
		finfo[i].prorettype = atooid(PQgetvalue(res, i, i_prorettype));
		finfo[i].proacl = strdup(PQgetvalue(res, i, i_proacl));
		finfo[i].nargs = atoi(PQgetvalue(res, i, i_pronargs));
		if (finfo[i].nargs == 0)
			finfo[i].argtypes = NULL;
		else
		{
			finfo[i].argtypes = (Oid *) malloc(finfo[i].nargs * sizeof(Oid));
			parseOidArray(PQgetvalue(res, i, i_proargtypes),
						  finfo[i].argtypes, finfo[i].nargs);
		}

		/* Decide whether we want to dump it */
		selectDumpableObject(&(finfo[i].dobj));

		if (strlen(finfo[i].rolname) == 0)
			write_msg(NULL,
				 "WARNING: owner of function \"%s\" appears to be invalid\n",
					  finfo[i].dobj.name);
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return finfo;
}

/*
 * getTables
 *	  read all the user-defined tables (no indexes, no catalogs)
 * in the system catalogs return them in the TableInfo* structure
 *
 * numTables is set to the number of tables read in
 */
TableInfo *
getTables(int *numTables)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	TableInfo  *tblinfo;
	int			i_reltableoid;
	int			i_reloid;
	int			i_relname;
	int			i_relnamespace;
	int			i_relkind;
	int			i_relacl;
	int			i_rolname;
	int			i_relchecks;
	int			i_relhastriggers;
	int			i_relhasindex;
	int			i_relhasrules;
	int			i_relhasoids;
	int			i_relfrozenxid;
	int			i_owning_tab;
	int			i_owning_col;
	int			i_reltablespace;
	int			i_reloptions;
	int			i_toastreloptions;
	int			i_reloftype;

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/*
	 * Find all the tables (including views and sequences).
	 *
	 * We include system catalogs, so that we can work if a user table is
	 * defined to inherit from a system catalog (pretty weird, but...)
	 *
	 * We ignore tables that are not type 'r' (ordinary relation), 'S'
	 * (sequence), 'v' (view), or 'c' (composite type).
	 *
	 * Composite-type table entries won't be dumped as such, but we have to
	 * make a DumpableObject for them so that we can track dependencies of the
	 * composite type (pg_depend entries for columns of the composite type
	 * link to the pg_class entry not the pg_type entry).
	 *
	 * Note: in this phase we should collect only a minimal amount of
	 * information about each table, basically just enough to decide if it is
	 * interesting. We must fetch all tables in this phase because otherwise
	 * we cannot correctly identify inherited columns, owned sequences, etc.
	 */

	if (g_fout->remoteVersion >= 90000)
	{
		/*
		 * Left join to pick up dependency info linking sequences to their
		 * owning column, if any (note this dependency is AUTO as of 8.2)
		 */
		appendPQExpBuffer(query,
						  "SELECT c.tableoid, c.oid, c.relname, "
						  "c.relacl, c.relkind, c.relnamespace, "
						  "(%s c.relowner) AS rolname, "
						  "c.relchecks, c.relhastriggers, "
						  "c.relhasindex, c.relhasrules, c.relhasoids, "
						  "c.relfrozenxid, "
						  "CASE WHEN c.reloftype <> 0 THEN c.reloftype::pg_catalog.regtype ELSE NULL END AS reloftype, "
						  "d.refobjid AS owning_tab, "
						  "d.refobjsubid AS owning_col, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
						"array_to_string(c.reloptions, ', ') AS reloptions, "
						  "array_to_string(array(SELECT 'toast.' || x FROM unnest(tc.reloptions) x), ', ') AS toast_reloptions "
						  "FROM pg_class c "
						  "LEFT JOIN pg_depend d ON "
						  "(c.relkind = '%c' AND "
						  "d.classid = c.tableoid AND d.objid = c.oid AND "
						  "d.objsubid = 0 AND "
						  "d.refclassid = c.tableoid AND d.deptype = 'a') "
					   "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
						  "WHERE c.relkind in ('%c', '%c', '%c', '%c') "
						  "ORDER BY c.oid",
						  username_subquery,
						  RELKIND_SEQUENCE,
						  RELKIND_RELATION, RELKIND_SEQUENCE,
						  RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
	}
	else if (g_fout->remoteVersion >= 80400)
	{
		/*
		 * Left join to pick up dependency info linking sequences to their
		 * owning column, if any (note this dependency is AUTO as of 8.2)
		 */
		appendPQExpBuffer(query,
						  "SELECT c.tableoid, c.oid, c.relname, "
						  "c.relacl, c.relkind, c.relnamespace, "
						  "(%s c.relowner) AS rolname, "
						  "c.relchecks, c.relhastriggers, "
						  "c.relhasindex, c.relhasrules, c.relhasoids, "
						  "c.relfrozenxid, "
						  "NULL AS reloftype, "
						  "d.refobjid AS owning_tab, "
						  "d.refobjsubid AS owning_col, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
						"array_to_string(c.reloptions, ', ') AS reloptions, "
						  "array_to_string(array(SELECT 'toast.' || x FROM unnest(tc.reloptions) x), ', ') AS toast_reloptions "
						  "FROM pg_class c "
						  "LEFT JOIN pg_depend d ON "
						  "(c.relkind = '%c' AND "
						  "d.classid = c.tableoid AND d.objid = c.oid AND "
						  "d.objsubid = 0 AND "
						  "d.refclassid = c.tableoid AND d.deptype = 'a') "
					   "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid) "
						  "WHERE c.relkind in ('%c', '%c', '%c', '%c') "
						  "ORDER BY c.oid",
						  username_subquery,
						  RELKIND_SEQUENCE,
						  RELKIND_RELATION, RELKIND_SEQUENCE,
						  RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
	}
	else if (g_fout->remoteVersion >= 80200)
	{
		/*
		 * Left join to pick up dependency info linking sequences to their
		 * owning column, if any (note this dependency is AUTO as of 8.2)
		 */
		appendPQExpBuffer(query,
						  "SELECT c.tableoid, c.oid, relname, "
						  "relacl, relkind, relnamespace, "
						  "(%s relowner) AS rolname, "
						  "relchecks, (reltriggers <> 0) AS relhastriggers, "
						  "relhasindex, relhasrules, relhasoids, "
						  "relfrozenxid, "
						  "NULL AS reloftype, "
						  "d.refobjid AS owning_tab, "
						  "d.refobjsubid AS owning_col, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
						"array_to_string(c.reloptions, ', ') AS reloptions, "
						  "NULL AS toast_reloptions "
						  "FROM pg_class c "
						  "LEFT JOIN pg_depend d ON "
						  "(c.relkind = '%c' AND "
						  "d.classid = c.tableoid AND d.objid = c.oid AND "
						  "d.objsubid = 0 AND "
						  "d.refclassid = c.tableoid AND d.deptype = 'a') "
						  "WHERE relkind in ('%c', '%c', '%c', '%c') "
						  "ORDER BY c.oid",
						  username_subquery,
						  RELKIND_SEQUENCE,
						  RELKIND_RELATION, RELKIND_SEQUENCE,
						  RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
	}
	else if (g_fout->remoteVersion >= 80000)
	{
		/*
		 * Left join to pick up dependency info linking sequences to their
		 * owning column, if any
		 */
		appendPQExpBuffer(query,
						  "SELECT c.tableoid, c.oid, relname, "
						  "relacl, relkind, relnamespace, "
						  "(%s relowner) AS rolname, "
						  "relchecks, (reltriggers <> 0) AS relhastriggers, "
						  "relhasindex, relhasrules, relhasoids, "
						  "0 AS relfrozenxid, "
						  "NULL AS reloftype, "
						  "d.refobjid AS owning_tab, "
						  "d.refobjsubid AS owning_col, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = c.reltablespace) AS reltablespace, "
						  "NULL AS reloptions, "
						  "NULL AS toast_reloptions "
						  "FROM pg_class c "
						  "LEFT JOIN pg_depend d ON "
						  "(c.relkind = '%c' AND "
						  "d.classid = c.tableoid AND d.objid = c.oid AND "
						  "d.objsubid = 0 AND "
						  "d.refclassid = c.tableoid AND d.deptype = 'i') "
						  "WHERE relkind in ('%c', '%c', '%c', '%c') "
						  "ORDER BY c.oid",
						  username_subquery,
						  RELKIND_SEQUENCE,
						  RELKIND_RELATION, RELKIND_SEQUENCE,
						  RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
	}
	else if (g_fout->remoteVersion >= 70300)
	{
		/*
		 * Left join to pick up dependency info linking sequences to their
		 * owning column, if any
		 */
		appendPQExpBuffer(query,
						  "SELECT c.tableoid, c.oid, relname, "
						  "relacl, relkind, relnamespace, "
						  "(%s relowner) AS rolname, "
						  "relchecks, (reltriggers <> 0) AS relhastriggers, "
						  "relhasindex, relhasrules, relhasoids, "
						  "0 AS relfrozenxid, "
						  "NULL AS reloftype, "
						  "d.refobjid AS owning_tab, "
						  "d.refobjsubid AS owning_col, "
						  "NULL AS reltablespace, "
						  "NULL AS reloptions, "
						  "NULL AS toast_reloptions "
						  "FROM pg_class c "
						  "LEFT JOIN pg_depend d ON "
						  "(c.relkind = '%c' AND "
						  "d.classid = c.tableoid AND d.objid = c.oid AND "
						  "d.objsubid = 0 AND "
						  "d.refclassid = c.tableoid AND d.deptype = 'i') "
						  "WHERE relkind IN ('%c', '%c', '%c', '%c') "
						  "ORDER BY c.oid",
						  username_subquery,
						  RELKIND_SEQUENCE,
						  RELKIND_RELATION, RELKIND_SEQUENCE,
						  RELKIND_VIEW, RELKIND_COMPOSITE_TYPE);
	}
	else if (g_fout->remoteVersion >= 70200)
	{
		appendPQExpBuffer(query,
						  "SELECT tableoid, oid, relname, relacl, relkind, "
						  "0::oid AS relnamespace, "
						  "(%s relowner) AS rolname, "
						  "relchecks, (reltriggers <> 0) AS relhastriggers, "
						  "relhasindex, relhasrules, relhasoids, "
						  "0 AS relfrozenxid, "
						  "NULL AS reloftype, "
						  "NULL::oid AS owning_tab, "
						  "NULL::int4 AS owning_col, "
						  "NULL AS reltablespace, "
						  "NULL AS reloptions, "
						  "NULL AS toast_reloptions "
						  "FROM pg_class "
						  "WHERE relkind IN ('%c', '%c', '%c') "
						  "ORDER BY oid",
						  username_subquery,
						  RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		/* all tables have oids in 7.1 */
		appendPQExpBuffer(query,
						  "SELECT tableoid, oid, relname, relacl, relkind, "
						  "0::oid AS relnamespace, "
						  "(%s relowner) AS rolname, "
						  "relchecks, (reltriggers <> 0) AS relhastriggers, "
						  "relhasindex, relhasrules, "
						  "'t'::bool AS relhasoids, "
						  "0 AS relfrozenxid, "
						  "NULL AS reloftype, "
						  "NULL::oid AS owning_tab, "
						  "NULL::int4 AS owning_col, "
						  "NULL AS reltablespace, "
						  "NULL AS reloptions, "
						  "NULL AS toast_reloptions "
						  "FROM pg_class "
						  "WHERE relkind IN ('%c', '%c', '%c') "
						  "ORDER BY oid",
						  username_subquery,
						  RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW);
	}
	else
	{
		/*
		 * Before 7.1, view relkind was not set to 'v', so we must check if we
		 * have a view by looking for a rule in pg_rewrite.
		 */
		appendPQExpBuffer(query,
						  "SELECT "
		"(SELECT oid FROM pg_class WHERE relname = 'pg_class') AS tableoid, "
						  "oid, relname, relacl, "
						  "CASE WHEN relhasrules and relkind = 'r' "
					  "  and EXISTS(SELECT rulename FROM pg_rewrite r WHERE "
					  "             r.ev_class = c.oid AND r.ev_type = '1') "
						  "THEN '%c'::\"char\" "
						  "ELSE relkind END AS relkind,"
						  "0::oid AS relnamespace, "
						  "(%s relowner) AS rolname, "
						  "relchecks, (reltriggers <> 0) AS relhastriggers, "
						  "relhasindex, relhasrules, "
						  "'t'::bool AS relhasoids, "
						  "0 as relfrozenxid, "
						  "NULL AS reloftype, "
						  "NULL::oid AS owning_tab, "
						  "NULL::int4 AS owning_col, "
						  "NULL AS reltablespace, "
						  "NULL AS reloptions, "
						  "NULL AS toast_reloptions "
						  "FROM pg_class c "
						  "WHERE relkind IN ('%c', '%c') "
						  "ORDER BY oid",
						  RELKIND_VIEW,
						  username_subquery,
						  RELKIND_RELATION, RELKIND_SEQUENCE);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numTables = ntups;

	/*
	 * Extract data from result and lock dumpable tables.  We do the locking
	 * before anything else, to minimize the window wherein a table could
	 * disappear under us.
	 *
	 * Note that we have to save info about all tables here, even when dumping
	 * only one, because we don't yet know which tables might be inheritance
	 * ancestors of the target table.
	 */
	tblinfo = (TableInfo *) calloc(ntups, sizeof(TableInfo));

	i_reltableoid = PQfnumber(res, "tableoid");
	i_reloid = PQfnumber(res, "oid");
	i_relname = PQfnumber(res, "relname");
	i_relnamespace = PQfnumber(res, "relnamespace");
	i_relacl = PQfnumber(res, "relacl");
	i_relkind = PQfnumber(res, "relkind");
	i_rolname = PQfnumber(res, "rolname");
	i_relchecks = PQfnumber(res, "relchecks");
	i_relhastriggers = PQfnumber(res, "relhastriggers");
	i_relhasindex = PQfnumber(res, "relhasindex");
	i_relhasrules = PQfnumber(res, "relhasrules");
	i_relhasoids = PQfnumber(res, "relhasoids");
	i_relfrozenxid = PQfnumber(res, "relfrozenxid");
	i_owning_tab = PQfnumber(res, "owning_tab");
	i_owning_col = PQfnumber(res, "owning_col");
	i_reltablespace = PQfnumber(res, "reltablespace");
	i_reloptions = PQfnumber(res, "reloptions");
	i_toastreloptions = PQfnumber(res, "toast_reloptions");
	i_reloftype = PQfnumber(res, "reloftype");

	if (lockWaitTimeout && g_fout->remoteVersion >= 70300)
	{
		/*
		 * Arrange to fail instead of waiting forever for a table lock.
		 *
		 * NB: this coding assumes that the only queries issued within the
		 * following loop are LOCK TABLEs; else the timeout may be undesirably
		 * applied to other things too.
		 */
		resetPQExpBuffer(query);
		appendPQExpBuffer(query, "SET statement_timeout = ");
		appendStringLiteralConn(query, lockWaitTimeout, g_conn);
		do_sql_command(g_conn, query->data);
	}

	for (i = 0; i < ntups; i++)
	{
		tblinfo[i].dobj.objType = DO_TABLE;
		tblinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_reltableoid));
		tblinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_reloid));
		AssignDumpId(&tblinfo[i].dobj);
		tblinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_relname));
		tblinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_relnamespace)),
												  tblinfo[i].dobj.catId.oid);
		tblinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		tblinfo[i].relacl = strdup(PQgetvalue(res, i, i_relacl));
		tblinfo[i].relkind = *(PQgetvalue(res, i, i_relkind));
		tblinfo[i].hasindex = (strcmp(PQgetvalue(res, i, i_relhasindex), "t") == 0);
		tblinfo[i].hasrules = (strcmp(PQgetvalue(res, i, i_relhasrules), "t") == 0);
		tblinfo[i].hastriggers = (strcmp(PQgetvalue(res, i, i_relhastriggers), "t") == 0);
		tblinfo[i].hasoids = (strcmp(PQgetvalue(res, i, i_relhasoids), "t") == 0);
		tblinfo[i].frozenxid = atooid(PQgetvalue(res, i, i_relfrozenxid));
		if (PQgetisnull(res, i, i_reloftype))
			tblinfo[i].reloftype = NULL;
		else
			tblinfo[i].reloftype = strdup(PQgetvalue(res, i, i_reloftype));
		tblinfo[i].ncheck = atoi(PQgetvalue(res, i, i_relchecks));
		if (PQgetisnull(res, i, i_owning_tab))
		{
			tblinfo[i].owning_tab = InvalidOid;
			tblinfo[i].owning_col = 0;
		}
		else
		{
			tblinfo[i].owning_tab = atooid(PQgetvalue(res, i, i_owning_tab));
			tblinfo[i].owning_col = atoi(PQgetvalue(res, i, i_owning_col));
		}
		tblinfo[i].reltablespace = strdup(PQgetvalue(res, i, i_reltablespace));
		tblinfo[i].reloptions = strdup(PQgetvalue(res, i, i_reloptions));
		tblinfo[i].toast_reloptions = strdup(PQgetvalue(res, i, i_toastreloptions));

		/* other fields were zeroed above */

		/*
		 * Decide whether we want to dump this table.
		 */
		if (tblinfo[i].relkind == RELKIND_COMPOSITE_TYPE)
			tblinfo[i].dobj.dump = false;
		else
			selectDumpableTable(&tblinfo[i]);
		tblinfo[i].interesting = tblinfo[i].dobj.dump;

		/*
		 * Read-lock target tables to make sure they aren't DROPPED or altered
		 * in schema before we get around to dumping them.
		 *
		 * Note that we don't explicitly lock parents of the target tables; we
		 * assume our lock on the child is enough to prevent schema
		 * alterations to parent tables.
		 *
		 * NOTE: it'd be kinda nice to lock views and sequences too, not only
		 * plain tables, but the backend doesn't presently allow that.
		 */
		if (tblinfo[i].dobj.dump && tblinfo[i].relkind == RELKIND_RELATION)
		{
			resetPQExpBuffer(query);
			appendPQExpBuffer(query,
							  "LOCK TABLE %s IN ACCESS SHARE MODE",
						 fmtQualifiedId(tblinfo[i].dobj.namespace->dobj.name,
										tblinfo[i].dobj.name));
			do_sql_command(g_conn, query->data);
		}

		/* Emit notice if join for owner failed */
		if (strlen(tblinfo[i].rolname) == 0)
			write_msg(NULL, "WARNING: owner of table \"%s\" appears to be invalid\n",
					  tblinfo[i].dobj.name);
	}

	if (lockWaitTimeout && g_fout->remoteVersion >= 70300)
	{
		do_sql_command(g_conn, "SET statement_timeout = 0");
	}

	PQclear(res);

	/*
	 * Force sequences that are "owned" by table columns to be dumped whenever
	 * their owning table is being dumped.
	 */
	for (i = 0; i < ntups; i++)
	{
		TableInfo  *seqinfo = &tblinfo[i];
		int			j;

		if (!OidIsValid(seqinfo->owning_tab))
			continue;			/* not an owned sequence */
		if (seqinfo->dobj.dump)
			continue;			/* no need to search */

		/* can't use findTableByOid yet, unfortunately */
		for (j = 0; j < ntups; j++)
		{
			if (tblinfo[j].dobj.catId.oid == seqinfo->owning_tab)
			{
				if (tblinfo[j].dobj.dump)
				{
					seqinfo->interesting = true;
					seqinfo->dobj.dump = true;
				}
				break;
			}
		}
	}

	destroyPQExpBuffer(query);

	return tblinfo;
}

/*
 * getInherits
 *	  read all the inheritance information
 * from the system catalogs return them in the InhInfo* structure
 *
 * numInherits is set to the number of pairs read in
 */
InhInfo *
getInherits(int *numInherits)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	InhInfo    *inhinfo;

	int			i_inhrelid;
	int			i_inhparent;

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* find all the inheritance information */

	appendPQExpBuffer(query, "SELECT inhrelid, inhparent FROM pg_inherits");

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numInherits = ntups;

	inhinfo = (InhInfo *) malloc(ntups * sizeof(InhInfo));

	i_inhrelid = PQfnumber(res, "inhrelid");
	i_inhparent = PQfnumber(res, "inhparent");

	for (i = 0; i < ntups; i++)
	{
		inhinfo[i].inhrelid = atooid(PQgetvalue(res, i, i_inhrelid));
		inhinfo[i].inhparent = atooid(PQgetvalue(res, i, i_inhparent));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return inhinfo;
}

/*
 * getIndexes
 *	  get information about every index on a dumpable table
 *
 * Note: index data is not returned directly to the caller, but it
 * does get entered into the DumpableObject tables.
 */
void
getIndexes(TableInfo tblinfo[], int numTables)
{
	int			i,
				j;
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	IndxInfo   *indxinfo;
	ConstraintInfo *constrinfo;
	int			i_tableoid,
				i_oid,
				i_indexname,
				i_indexdef,
				i_indnkeys,
				i_indkey,
				i_indisclustered,
				i_contype,
				i_conname,
				i_condeferrable,
				i_condeferred,
				i_contableoid,
				i_conoid,
				i_condef,
				i_tablespace,
				i_options;
	int			ntups;

	for (i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &tblinfo[i];

		/* Only plain tables have indexes */
		if (tbinfo->relkind != RELKIND_RELATION || !tbinfo->hasindex)
			continue;

		/* Ignore indexes of tables not to be dumped */
		if (!tbinfo->dobj.dump)
			continue;

		if (g_verbose)
			write_msg(NULL, "reading indexes for table \"%s\"\n",
					  tbinfo->dobj.name);

		/* Make sure we are in proper schema so indexdef is right */
		selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

		/*
		 * The point of the messy-looking outer join is to find a constraint
		 * that is related by an internal dependency link to the index. If we
		 * find one, create a CONSTRAINT entry linked to the INDEX entry.  We
		 * assume an index won't have more than one internal dependency.
		 *
		 * As of 9.0 we don't need to look at pg_depend but can check for a
		 * match to pg_constraint.conindid.  The check on conrelid is
		 * redundant but useful because that column is indexed while conindid
		 * is not.
		 */
		resetPQExpBuffer(query);
		if (g_fout->remoteVersion >= 90000)
		{
			appendPQExpBuffer(query,
							  "SELECT t.tableoid, t.oid, "
							  "t.relname AS indexname, "
					 "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
							  "t.relnatts AS indnkeys, "
							  "i.indkey, i.indisclustered, "
							  "c.contype, c.conname, "
							  "c.condeferrable, c.condeferred, "
							  "c.tableoid AS contableoid, "
							  "c.oid AS conoid, "
				  "pg_catalog.pg_get_constraintdef(c.oid, false) AS condef, "
							  "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
							"array_to_string(t.reloptions, ', ') AS options "
							  "FROM pg_catalog.pg_index i "
					  "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
							  "LEFT JOIN pg_catalog.pg_constraint c "
							  "ON (i.indrelid = c.conrelid AND "
							  "i.indexrelid = c.conindid AND "
							  "c.contype IN ('p','u','x')) "
							  "WHERE i.indrelid = '%u'::pg_catalog.oid "
							  "ORDER BY indexname",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 80200)
		{
			appendPQExpBuffer(query,
							  "SELECT t.tableoid, t.oid, "
							  "t.relname AS indexname, "
					 "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
							  "t.relnatts AS indnkeys, "
							  "i.indkey, i.indisclustered, "
							  "c.contype, c.conname, "
							  "c.condeferrable, c.condeferred, "
							  "c.tableoid AS contableoid, "
							  "c.oid AS conoid, "
							  "null AS condef, "
							  "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
							"array_to_string(t.reloptions, ', ') AS options "
							  "FROM pg_catalog.pg_index i "
					  "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
							  "LEFT JOIN pg_catalog.pg_depend d "
							  "ON (d.classid = t.tableoid "
							  "AND d.objid = t.oid "
							  "AND d.deptype = 'i') "
							  "LEFT JOIN pg_catalog.pg_constraint c "
							  "ON (d.refclassid = c.tableoid "
							  "AND d.refobjid = c.oid) "
							  "WHERE i.indrelid = '%u'::pg_catalog.oid "
							  "ORDER BY indexname",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 80000)
		{
			appendPQExpBuffer(query,
							  "SELECT t.tableoid, t.oid, "
							  "t.relname AS indexname, "
					 "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
							  "t.relnatts AS indnkeys, "
							  "i.indkey, i.indisclustered, "
							  "c.contype, c.conname, "
							  "c.condeferrable, c.condeferred, "
							  "c.tableoid AS contableoid, "
							  "c.oid AS conoid, "
							  "null AS condef, "
							  "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
							  "null AS options "
							  "FROM pg_catalog.pg_index i "
					  "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
							  "LEFT JOIN pg_catalog.pg_depend d "
							  "ON (d.classid = t.tableoid "
							  "AND d.objid = t.oid "
							  "AND d.deptype = 'i') "
							  "LEFT JOIN pg_catalog.pg_constraint c "
							  "ON (d.refclassid = c.tableoid "
							  "AND d.refobjid = c.oid) "
							  "WHERE i.indrelid = '%u'::pg_catalog.oid "
							  "ORDER BY indexname",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 70300)
		{
			appendPQExpBuffer(query,
							  "SELECT t.tableoid, t.oid, "
							  "t.relname AS indexname, "
					 "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
							  "t.relnatts AS indnkeys, "
							  "i.indkey, i.indisclustered, "
							  "c.contype, c.conname, "
							  "c.condeferrable, c.condeferred, "
							  "c.tableoid AS contableoid, "
							  "c.oid AS conoid, "
							  "null AS condef, "
							  "NULL AS tablespace, "
							  "null AS options "
							  "FROM pg_catalog.pg_index i "
					  "JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) "
							  "LEFT JOIN pg_catalog.pg_depend d "
							  "ON (d.classid = t.tableoid "
							  "AND d.objid = t.oid "
							  "AND d.deptype = 'i') "
							  "LEFT JOIN pg_catalog.pg_constraint c "
							  "ON (d.refclassid = c.tableoid "
							  "AND d.refobjid = c.oid) "
							  "WHERE i.indrelid = '%u'::pg_catalog.oid "
							  "ORDER BY indexname",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 70100)
		{
			appendPQExpBuffer(query,
							  "SELECT t.tableoid, t.oid, "
							  "t.relname AS indexname, "
							  "pg_get_indexdef(i.indexrelid) AS indexdef, "
							  "t.relnatts AS indnkeys, "
							  "i.indkey, false AS indisclustered, "
							  "CASE WHEN i.indisprimary THEN 'p'::char "
							  "ELSE '0'::char END AS contype, "
							  "t.relname AS conname, "
							  "false AS condeferrable, "
							  "false AS condeferred, "
							  "0::oid AS contableoid, "
							  "t.oid AS conoid, "
							  "null AS condef, "
							  "NULL AS tablespace, "
							  "null AS options "
							  "FROM pg_index i, pg_class t "
							  "WHERE t.oid = i.indexrelid "
							  "AND i.indrelid = '%u'::oid "
							  "ORDER BY indexname",
							  tbinfo->dobj.catId.oid);
		}
		else
		{
			appendPQExpBuffer(query,
							  "SELECT "
							  "(SELECT oid FROM pg_class WHERE relname = 'pg_class') AS tableoid, "
							  "t.oid, "
							  "t.relname AS indexname, "
							  "pg_get_indexdef(i.indexrelid) AS indexdef, "
							  "t.relnatts AS indnkeys, "
							  "i.indkey, false AS indisclustered, "
							  "CASE WHEN i.indisprimary THEN 'p'::char "
							  "ELSE '0'::char END AS contype, "
							  "t.relname AS conname, "
							  "false AS condeferrable, "
							  "false AS condeferred, "
							  "0::oid AS contableoid, "
							  "t.oid AS conoid, "
							  "null AS condef, "
							  "NULL AS tablespace, "
							  "null AS options "
							  "FROM pg_index i, pg_class t "
							  "WHERE t.oid = i.indexrelid "
							  "AND i.indrelid = '%u'::oid "
							  "ORDER BY indexname",
							  tbinfo->dobj.catId.oid);
		}

		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		ntups = PQntuples(res);

		i_tableoid = PQfnumber(res, "tableoid");
		i_oid = PQfnumber(res, "oid");
		i_indexname = PQfnumber(res, "indexname");
		i_indexdef = PQfnumber(res, "indexdef");
		i_indnkeys = PQfnumber(res, "indnkeys");
		i_indkey = PQfnumber(res, "indkey");
		i_indisclustered = PQfnumber(res, "indisclustered");
		i_contype = PQfnumber(res, "contype");
		i_conname = PQfnumber(res, "conname");
		i_condeferrable = PQfnumber(res, "condeferrable");
		i_condeferred = PQfnumber(res, "condeferred");
		i_contableoid = PQfnumber(res, "contableoid");
		i_conoid = PQfnumber(res, "conoid");
		i_condef = PQfnumber(res, "condef");
		i_tablespace = PQfnumber(res, "tablespace");
		i_options = PQfnumber(res, "options");

		indxinfo = (IndxInfo *) malloc(ntups * sizeof(IndxInfo));
		constrinfo = (ConstraintInfo *) malloc(ntups * sizeof(ConstraintInfo));

		for (j = 0; j < ntups; j++)
		{
			char		contype;

			indxinfo[j].dobj.objType = DO_INDEX;
			indxinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_tableoid));
			indxinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_oid));
			AssignDumpId(&indxinfo[j].dobj);
			indxinfo[j].dobj.name = strdup(PQgetvalue(res, j, i_indexname));
			indxinfo[j].dobj.namespace = tbinfo->dobj.namespace;
			indxinfo[j].indextable = tbinfo;
			indxinfo[j].indexdef = strdup(PQgetvalue(res, j, i_indexdef));
			indxinfo[j].indnkeys = atoi(PQgetvalue(res, j, i_indnkeys));
			indxinfo[j].tablespace = strdup(PQgetvalue(res, j, i_tablespace));
			indxinfo[j].options = strdup(PQgetvalue(res, j, i_options));

			/*
			 * In pre-7.4 releases, indkeys may contain more entries than
			 * indnkeys says (since indnkeys will be 1 for a functional
			 * index).	We don't actually care about this case since we don't
			 * examine indkeys except for indexes associated with PRIMARY and
			 * UNIQUE constraints, which are never functional indexes. But we
			 * have to allocate enough space to keep parseOidArray from
			 * complaining.
			 */
			indxinfo[j].indkeys = (Oid *) malloc(INDEX_MAX_KEYS * sizeof(Oid));
			parseOidArray(PQgetvalue(res, j, i_indkey),
						  indxinfo[j].indkeys, INDEX_MAX_KEYS);
			indxinfo[j].indisclustered = (PQgetvalue(res, j, i_indisclustered)[0] == 't');
			contype = *(PQgetvalue(res, j, i_contype));

			if (contype == 'p' || contype == 'u' || contype == 'x')
			{
				/*
				 * If we found a constraint matching the index, create an
				 * entry for it.
				 *
				 * In a pre-7.3 database, we take this path iff the index was
				 * marked indisprimary.
				 */
				constrinfo[j].dobj.objType = DO_CONSTRAINT;
				constrinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_contableoid));
				constrinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_conoid));
				AssignDumpId(&constrinfo[j].dobj);
				constrinfo[j].dobj.name = strdup(PQgetvalue(res, j, i_conname));
				constrinfo[j].dobj.namespace = tbinfo->dobj.namespace;
				constrinfo[j].contable = tbinfo;
				constrinfo[j].condomain = NULL;
				constrinfo[j].contype = contype;
				if (contype == 'x')
					constrinfo[j].condef = strdup(PQgetvalue(res, j, i_condef));
				else
					constrinfo[j].condef = NULL;
				constrinfo[j].confrelid = InvalidOid;
				constrinfo[j].conindex = indxinfo[j].dobj.dumpId;
				constrinfo[j].condeferrable = *(PQgetvalue(res, j, i_condeferrable)) == 't';
				constrinfo[j].condeferred = *(PQgetvalue(res, j, i_condeferred)) == 't';
				constrinfo[j].conislocal = true;
				constrinfo[j].separate = true;

				indxinfo[j].indexconstraint = constrinfo[j].dobj.dumpId;

				/* If pre-7.3 DB, better make sure table comes first */
				addObjectDependency(&constrinfo[j].dobj,
									tbinfo->dobj.dumpId);
			}
			else
			{
				/* Plain secondary index */
				indxinfo[j].indexconstraint = 0;
			}
		}

		PQclear(res);
	}

	destroyPQExpBuffer(query);
}

/*
 * getConstraints
 *
 * Get info about constraints on dumpable tables.
 *
 * Currently handles foreign keys only.
 * Unique and primary key constraints are handled with indexes,
 * while check constraints are processed in getTableAttrs().
 */
void
getConstraints(TableInfo tblinfo[], int numTables)
{
	int			i,
				j;
	ConstraintInfo *constrinfo;
	PQExpBuffer query;
	PGresult   *res;
	int			i_contableoid,
				i_conoid,
				i_conname,
				i_confrelid,
				i_condef;
	int			ntups;

	/* pg_constraint was created in 7.3, so nothing to do if older */
	if (g_fout->remoteVersion < 70300)
		return;

	query = createPQExpBuffer();

	for (i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &tblinfo[i];

		if (!tbinfo->hastriggers || !tbinfo->dobj.dump)
			continue;

		if (g_verbose)
			write_msg(NULL, "reading foreign key constraints for table \"%s\"\n",
					  tbinfo->dobj.name);

		/*
		 * select table schema to ensure constraint expr is qualified if
		 * needed
		 */
		selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

		resetPQExpBuffer(query);
		appendPQExpBuffer(query,
						  "SELECT tableoid, oid, conname, confrelid, "
						  "pg_catalog.pg_get_constraintdef(oid) AS condef "
						  "FROM pg_catalog.pg_constraint "
						  "WHERE conrelid = '%u'::pg_catalog.oid "
						  "AND contype = 'f'",
						  tbinfo->dobj.catId.oid);
		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		ntups = PQntuples(res);

		i_contableoid = PQfnumber(res, "tableoid");
		i_conoid = PQfnumber(res, "oid");
		i_conname = PQfnumber(res, "conname");
		i_confrelid = PQfnumber(res, "confrelid");
		i_condef = PQfnumber(res, "condef");

		constrinfo = (ConstraintInfo *) malloc(ntups * sizeof(ConstraintInfo));

		for (j = 0; j < ntups; j++)
		{
			constrinfo[j].dobj.objType = DO_FK_CONSTRAINT;
			constrinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_contableoid));
			constrinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_conoid));
			AssignDumpId(&constrinfo[j].dobj);
			constrinfo[j].dobj.name = strdup(PQgetvalue(res, j, i_conname));
			constrinfo[j].dobj.namespace = tbinfo->dobj.namespace;
			constrinfo[j].contable = tbinfo;
			constrinfo[j].condomain = NULL;
			constrinfo[j].contype = 'f';
			constrinfo[j].condef = strdup(PQgetvalue(res, j, i_condef));
			constrinfo[j].confrelid = atooid(PQgetvalue(res, j, i_confrelid));
			constrinfo[j].conindex = 0;
			constrinfo[j].condeferrable = false;
			constrinfo[j].condeferred = false;
			constrinfo[j].conislocal = true;
			constrinfo[j].separate = true;
		}

		PQclear(res);
	}

	destroyPQExpBuffer(query);
}

/*
 * getDomainConstraints
 *
 * Get info about constraints on a domain.
 */
static void
getDomainConstraints(TypeInfo *tyinfo)
{
	int			i;
	ConstraintInfo *constrinfo;
	PQExpBuffer query;
	PGresult   *res;
	int			i_tableoid,
				i_oid,
				i_conname,
				i_consrc;
	int			ntups;

	/* pg_constraint was created in 7.3, so nothing to do if older */
	if (g_fout->remoteVersion < 70300)
		return;

	/*
	 * select appropriate schema to ensure names in constraint are properly
	 * qualified
	 */
	selectSourceSchema(tyinfo->dobj.namespace->dobj.name);

	query = createPQExpBuffer();

	if (g_fout->remoteVersion >= 70400)
		appendPQExpBuffer(query, "SELECT tableoid, oid, conname, "
						  "pg_catalog.pg_get_constraintdef(oid) AS consrc "
						  "FROM pg_catalog.pg_constraint "
						  "WHERE contypid = '%u'::pg_catalog.oid "
						  "ORDER BY conname",
						  tyinfo->dobj.catId.oid);
	else
		appendPQExpBuffer(query, "SELECT tableoid, oid, conname, "
						  "'CHECK (' || consrc || ')' AS consrc "
						  "FROM pg_catalog.pg_constraint "
						  "WHERE contypid = '%u'::pg_catalog.oid "
						  "ORDER BY conname",
						  tyinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_conname = PQfnumber(res, "conname");
	i_consrc = PQfnumber(res, "consrc");

	constrinfo = (ConstraintInfo *) malloc(ntups * sizeof(ConstraintInfo));

	tyinfo->nDomChecks = ntups;
	tyinfo->domChecks = constrinfo;

	for (i = 0; i < ntups; i++)
	{
		constrinfo[i].dobj.objType = DO_CONSTRAINT;
		constrinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		constrinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&constrinfo[i].dobj);
		constrinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_conname));
		constrinfo[i].dobj.namespace = tyinfo->dobj.namespace;
		constrinfo[i].contable = NULL;
		constrinfo[i].condomain = tyinfo;
		constrinfo[i].contype = 'c';
		constrinfo[i].condef = strdup(PQgetvalue(res, i, i_consrc));
		constrinfo[i].confrelid = InvalidOid;
		constrinfo[i].conindex = 0;
		constrinfo[i].condeferrable = false;
		constrinfo[i].condeferred = false;
		constrinfo[i].conislocal = true;
		constrinfo[i].separate = false;

		/*
		 * Make the domain depend on the constraint, ensuring it won't be
		 * output till any constraint dependencies are OK.
		 */
		addObjectDependency(&tyinfo->dobj,
							constrinfo[i].dobj.dumpId);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
}

/*
 * getRules
 *	  get basic information about every rule in the system
 *
 * numRules is set to the number of rules read in
 */
RuleInfo *
getRules(int *numRules)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	RuleInfo   *ruleinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_rulename;
	int			i_ruletable;
	int			i_ev_type;
	int			i_is_instead;
	int			i_ev_enabled;

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	if (g_fout->remoteVersion >= 80300)
	{
		appendPQExpBuffer(query, "SELECT "
						  "tableoid, oid, rulename, "
						  "ev_class AS ruletable, ev_type, is_instead, "
						  "ev_enabled "
						  "FROM pg_rewrite "
						  "ORDER BY oid");
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT "
						  "tableoid, oid, rulename, "
						  "ev_class AS ruletable, ev_type, is_instead, "
						  "'O'::char AS ev_enabled "
						  "FROM pg_rewrite "
						  "ORDER BY oid");
	}
	else
	{
		appendPQExpBuffer(query, "SELECT "
						  "(SELECT oid FROM pg_class WHERE relname = 'pg_rewrite') AS tableoid, "
						  "oid, rulename, "
						  "ev_class AS ruletable, ev_type, is_instead, "
						  "'O'::char AS ev_enabled "
						  "FROM pg_rewrite "
						  "ORDER BY oid");
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numRules = ntups;

	ruleinfo = (RuleInfo *) malloc(ntups * sizeof(RuleInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_rulename = PQfnumber(res, "rulename");
	i_ruletable = PQfnumber(res, "ruletable");
	i_ev_type = PQfnumber(res, "ev_type");
	i_is_instead = PQfnumber(res, "is_instead");
	i_ev_enabled = PQfnumber(res, "ev_enabled");

	for (i = 0; i < ntups; i++)
	{
		Oid			ruletableoid;

		ruleinfo[i].dobj.objType = DO_RULE;
		ruleinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		ruleinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&ruleinfo[i].dobj);
		ruleinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_rulename));
		ruletableoid = atooid(PQgetvalue(res, i, i_ruletable));
		ruleinfo[i].ruletable = findTableByOid(ruletableoid);
		if (ruleinfo[i].ruletable == NULL)
		{
			write_msg(NULL, "failed sanity check, parent table OID %u of pg_rewrite entry OID %u not found\n",
					  ruletableoid,
					  ruleinfo[i].dobj.catId.oid);
			exit_nicely();
		}
		ruleinfo[i].dobj.namespace = ruleinfo[i].ruletable->dobj.namespace;
		ruleinfo[i].dobj.dump = ruleinfo[i].ruletable->dobj.dump;
		ruleinfo[i].ev_type = *(PQgetvalue(res, i, i_ev_type));
		ruleinfo[i].is_instead = *(PQgetvalue(res, i, i_is_instead)) == 't';
		ruleinfo[i].ev_enabled = *(PQgetvalue(res, i, i_ev_enabled));
		if (ruleinfo[i].ruletable)
		{
			/*
			 * If the table is a view, force its ON SELECT rule to be sorted
			 * before the view itself --- this ensures that any dependencies
			 * for the rule affect the table's positioning. Other rules are
			 * forced to appear after their table.
			 */
			if (ruleinfo[i].ruletable->relkind == RELKIND_VIEW &&
				ruleinfo[i].ev_type == '1' && ruleinfo[i].is_instead)
			{
				addObjectDependency(&ruleinfo[i].ruletable->dobj,
									ruleinfo[i].dobj.dumpId);
				/* We'll merge the rule into CREATE VIEW, if possible */
				ruleinfo[i].separate = false;
			}
			else
			{
				addObjectDependency(&ruleinfo[i].dobj,
									ruleinfo[i].ruletable->dobj.dumpId);
				ruleinfo[i].separate = true;
			}
		}
		else
			ruleinfo[i].separate = true;
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return ruleinfo;
}

/*
 * getTriggers
 *	  get information about every trigger on a dumpable table
 *
 * Note: trigger data is not returned directly to the caller, but it
 * does get entered into the DumpableObject tables.
 */
void
getTriggers(TableInfo tblinfo[], int numTables)
{
	int			i,
				j;
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	TriggerInfo *tginfo;
	int			i_tableoid,
				i_oid,
				i_tgname,
				i_tgfname,
				i_tgtype,
				i_tgnargs,
				i_tgargs,
				i_tgisconstraint,
				i_tgconstrname,
				i_tgconstrrelid,
				i_tgconstrrelname,
				i_tgenabled,
				i_tgdeferrable,
				i_tginitdeferred,
				i_tgdef;
	int			ntups;

	for (i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &tblinfo[i];

		if (!tbinfo->hastriggers || !tbinfo->dobj.dump)
			continue;

		if (g_verbose)
			write_msg(NULL, "reading triggers for table \"%s\"\n",
					  tbinfo->dobj.name);

		/*
		 * select table schema to ensure regproc name is qualified if needed
		 */
		selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

		resetPQExpBuffer(query);
		if (g_fout->remoteVersion >= 90000)
		{
			/*
			 * NB: think not to use pretty=true in pg_get_triggerdef.  It
			 * could result in non-forward-compatible dumps of WHEN clauses
			 * due to under-parenthesization.
			 */
			appendPQExpBuffer(query,
							  "SELECT tgname, "
							  "tgfoid::pg_catalog.regproc AS tgfname, "
						"pg_catalog.pg_get_triggerdef(oid, false) AS tgdef, "
							  "tgenabled, tableoid, oid "
							  "FROM pg_catalog.pg_trigger t "
							  "WHERE tgrelid = '%u'::pg_catalog.oid "
							  "AND NOT tgisinternal",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 80300)
		{
			/*
			 * We ignore triggers that are tied to a foreign-key constraint
			 */
			appendPQExpBuffer(query,
							  "SELECT tgname, "
							  "tgfoid::pg_catalog.regproc AS tgfname, "
							  "tgtype, tgnargs, tgargs, tgenabled, "
							  "tgisconstraint, tgconstrname, tgdeferrable, "
							  "tgconstrrelid, tginitdeferred, tableoid, oid, "
					 "tgconstrrelid::pg_catalog.regclass AS tgconstrrelname "
							  "FROM pg_catalog.pg_trigger t "
							  "WHERE tgrelid = '%u'::pg_catalog.oid "
							  "AND tgconstraint = 0",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 70300)
		{
			/*
			 * We ignore triggers that are tied to a foreign-key constraint,
			 * but in these versions we have to grovel through pg_constraint
			 * to find out
			 */
			appendPQExpBuffer(query,
							  "SELECT tgname, "
							  "tgfoid::pg_catalog.regproc AS tgfname, "
							  "tgtype, tgnargs, tgargs, tgenabled, "
							  "tgisconstraint, tgconstrname, tgdeferrable, "
							  "tgconstrrelid, tginitdeferred, tableoid, oid, "
					 "tgconstrrelid::pg_catalog.regclass AS tgconstrrelname "
							  "FROM pg_catalog.pg_trigger t "
							  "WHERE tgrelid = '%u'::pg_catalog.oid "
							  "AND (NOT tgisconstraint "
							  " OR NOT EXISTS"
							  "  (SELECT 1 FROM pg_catalog.pg_depend d "
							  "   JOIN pg_catalog.pg_constraint c ON (d.refclassid = c.tableoid AND d.refobjid = c.oid) "
							  "   WHERE d.classid = t.tableoid AND d.objid = t.oid AND d.deptype = 'i' AND c.contype = 'f'))",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 70100)
		{
			appendPQExpBuffer(query,
							  "SELECT tgname, tgfoid::regproc AS tgfname, "
							  "tgtype, tgnargs, tgargs, tgenabled, "
							  "tgisconstraint, tgconstrname, tgdeferrable, "
							  "tgconstrrelid, tginitdeferred, tableoid, oid, "
				  "(SELECT relname FROM pg_class WHERE oid = tgconstrrelid) "
							  "		AS tgconstrrelname "
							  "FROM pg_trigger "
							  "WHERE tgrelid = '%u'::oid",
							  tbinfo->dobj.catId.oid);
		}
		else
		{
			appendPQExpBuffer(query,
							  "SELECT tgname, tgfoid::regproc AS tgfname, "
							  "tgtype, tgnargs, tgargs, tgenabled, "
							  "tgisconstraint, tgconstrname, tgdeferrable, "
							  "tgconstrrelid, tginitdeferred, "
							  "(SELECT oid FROM pg_class WHERE relname = 'pg_trigger') AS tableoid, "
							  "oid, "
				  "(SELECT relname FROM pg_class WHERE oid = tgconstrrelid) "
							  "		AS tgconstrrelname "
							  "FROM pg_trigger "
							  "WHERE tgrelid = '%u'::oid",
							  tbinfo->dobj.catId.oid);
		}
		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		ntups = PQntuples(res);

		i_tableoid = PQfnumber(res, "tableoid");
		i_oid = PQfnumber(res, "oid");
		i_tgname = PQfnumber(res, "tgname");
		i_tgfname = PQfnumber(res, "tgfname");
		i_tgtype = PQfnumber(res, "tgtype");
		i_tgnargs = PQfnumber(res, "tgnargs");
		i_tgargs = PQfnumber(res, "tgargs");
		i_tgisconstraint = PQfnumber(res, "tgisconstraint");
		i_tgconstrname = PQfnumber(res, "tgconstrname");
		i_tgconstrrelid = PQfnumber(res, "tgconstrrelid");
		i_tgconstrrelname = PQfnumber(res, "tgconstrrelname");
		i_tgenabled = PQfnumber(res, "tgenabled");
		i_tgdeferrable = PQfnumber(res, "tgdeferrable");
		i_tginitdeferred = PQfnumber(res, "tginitdeferred");
		i_tgdef = PQfnumber(res, "tgdef");

		tginfo = (TriggerInfo *) malloc(ntups * sizeof(TriggerInfo));

		for (j = 0; j < ntups; j++)
		{
			tginfo[j].dobj.objType = DO_TRIGGER;
			tginfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_tableoid));
			tginfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_oid));
			AssignDumpId(&tginfo[j].dobj);
			tginfo[j].dobj.name = strdup(PQgetvalue(res, j, i_tgname));
			tginfo[j].dobj.namespace = tbinfo->dobj.namespace;
			tginfo[j].tgtable = tbinfo;
			tginfo[j].tgenabled = *(PQgetvalue(res, j, i_tgenabled));
			if (i_tgdef >= 0)
			{
				tginfo[j].tgdef = strdup(PQgetvalue(res, j, i_tgdef));

				/* remaining fields are not valid if we have tgdef */
				tginfo[j].tgfname = NULL;
				tginfo[j].tgtype = 0;
				tginfo[j].tgnargs = 0;
				tginfo[j].tgargs = NULL;
				tginfo[j].tgisconstraint = false;
				tginfo[j].tgdeferrable = false;
				tginfo[j].tginitdeferred = false;
				tginfo[j].tgconstrname = NULL;
				tginfo[j].tgconstrrelid = InvalidOid;
				tginfo[j].tgconstrrelname = NULL;
			}
			else
			{
				tginfo[j].tgdef = NULL;

				tginfo[j].tgfname = strdup(PQgetvalue(res, j, i_tgfname));
				tginfo[j].tgtype = atoi(PQgetvalue(res, j, i_tgtype));
				tginfo[j].tgnargs = atoi(PQgetvalue(res, j, i_tgnargs));
				tginfo[j].tgargs = strdup(PQgetvalue(res, j, i_tgargs));
				tginfo[j].tgisconstraint = *(PQgetvalue(res, j, i_tgisconstraint)) == 't';
				tginfo[j].tgdeferrable = *(PQgetvalue(res, j, i_tgdeferrable)) == 't';
				tginfo[j].tginitdeferred = *(PQgetvalue(res, j, i_tginitdeferred)) == 't';

				if (tginfo[j].tgisconstraint)
				{
					tginfo[j].tgconstrname = strdup(PQgetvalue(res, j, i_tgconstrname));
					tginfo[j].tgconstrrelid = atooid(PQgetvalue(res, j, i_tgconstrrelid));
					if (OidIsValid(tginfo[j].tgconstrrelid))
					{
						if (PQgetisnull(res, j, i_tgconstrrelname))
						{
							write_msg(NULL, "query produced null referenced table name for foreign key trigger \"%s\" on table \"%s\" (OID of table: %u)\n",
									  tginfo[j].dobj.name, tbinfo->dobj.name,
									  tginfo[j].tgconstrrelid);
							exit_nicely();
						}
						tginfo[j].tgconstrrelname = strdup(PQgetvalue(res, j, i_tgconstrrelname));
					}
					else
						tginfo[j].tgconstrrelname = NULL;
				}
				else
				{
					tginfo[j].tgconstrname = NULL;
					tginfo[j].tgconstrrelid = InvalidOid;
					tginfo[j].tgconstrrelname = NULL;
				}
			}
		}

		PQclear(res);
	}

	destroyPQExpBuffer(query);
}

/*
 * getProcLangs
 *	  get basic information about every procedural language in the system
 *
 * numProcLangs is set to the number of langs read in
 *
 * NB: this must run after getFuncs() because we assume we can do
 * findFuncByOid().
 */
ProcLangInfo *
getProcLangs(int *numProcLangs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	ProcLangInfo *planginfo;
	int			i_tableoid;
	int			i_oid;
	int			i_lanname;
	int			i_lanpltrusted;
	int			i_lanplcallfoid;
	int			i_laninline;
	int			i_lanvalidator;
	int			i_lanacl;
	int			i_lanowner;

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	if (g_fout->remoteVersion >= 90000)
	{
		/* pg_language has a laninline column */
		appendPQExpBuffer(query, "SELECT tableoid, oid, "
						  "lanname, lanpltrusted, lanplcallfoid, "
						  "laninline, lanvalidator,  lanacl, "
						  "(%s lanowner) AS lanowner "
						  "FROM pg_language "
						  "WHERE lanispl "
						  "ORDER BY oid",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 80300)
	{
		/* pg_language has a lanowner column */
		appendPQExpBuffer(query, "SELECT tableoid, oid, "
						  "lanname, lanpltrusted, lanplcallfoid, "
						  "lanvalidator,  lanacl, "
						  "(%s lanowner) AS lanowner "
						  "FROM pg_language "
						  "WHERE lanispl "
						  "ORDER BY oid",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 80100)
	{
		/* Languages are owned by the bootstrap superuser, OID 10 */
		appendPQExpBuffer(query, "SELECT tableoid, oid, *, "
						  "(%s '10') AS lanowner "
						  "FROM pg_language "
						  "WHERE lanispl "
						  "ORDER BY oid",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70400)
	{
		/* Languages are owned by the bootstrap superuser, sysid 1 */
		appendPQExpBuffer(query, "SELECT tableoid, oid, *, "
						  "(%s '1') AS lanowner "
						  "FROM pg_language "
						  "WHERE lanispl "
						  "ORDER BY oid",
						  username_subquery);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		/* No clear notion of an owner at all before 7.4 ... */
		appendPQExpBuffer(query, "SELECT tableoid, oid, * FROM pg_language "
						  "WHERE lanispl "
						  "ORDER BY oid");
	}
	else
	{
		appendPQExpBuffer(query, "SELECT "
						  "(SELECT oid FROM pg_class WHERE relname = 'pg_language') AS tableoid, "
						  "oid, * FROM pg_language "
						  "WHERE lanispl "
						  "ORDER BY oid");
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numProcLangs = ntups;

	planginfo = (ProcLangInfo *) malloc(ntups * sizeof(ProcLangInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_lanname = PQfnumber(res, "lanname");
	i_lanpltrusted = PQfnumber(res, "lanpltrusted");
	i_lanplcallfoid = PQfnumber(res, "lanplcallfoid");
	/* these may fail and return -1: */
	i_laninline = PQfnumber(res, "laninline");
	i_lanvalidator = PQfnumber(res, "lanvalidator");
	i_lanacl = PQfnumber(res, "lanacl");
	i_lanowner = PQfnumber(res, "lanowner");

	for (i = 0; i < ntups; i++)
	{
		planginfo[i].dobj.objType = DO_PROCLANG;
		planginfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		planginfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&planginfo[i].dobj);

		planginfo[i].dobj.name = strdup(PQgetvalue(res, i, i_lanname));
		planginfo[i].lanpltrusted = *(PQgetvalue(res, i, i_lanpltrusted)) == 't';
		planginfo[i].lanplcallfoid = atooid(PQgetvalue(res, i, i_lanplcallfoid));
		if (i_laninline >= 0)
			planginfo[i].laninline = atooid(PQgetvalue(res, i, i_laninline));
		else
			planginfo[i].laninline = InvalidOid;
		if (i_lanvalidator >= 0)
			planginfo[i].lanvalidator = atooid(PQgetvalue(res, i, i_lanvalidator));
		else
			planginfo[i].lanvalidator = InvalidOid;
		if (i_lanacl >= 0)
			planginfo[i].lanacl = strdup(PQgetvalue(res, i, i_lanacl));
		else
			planginfo[i].lanacl = strdup("{=U}");
		if (i_lanowner >= 0)
			planginfo[i].lanowner = strdup(PQgetvalue(res, i, i_lanowner));
		else
			planginfo[i].lanowner = strdup("");

		if (g_fout->remoteVersion < 70300)
		{
			/*
			 * We need to make a dependency to ensure the function will be
			 * dumped first.  (In 7.3 and later the regular dependency
			 * mechanism will handle this for us.)
			 */
			FuncInfo   *funcInfo = findFuncByOid(planginfo[i].lanplcallfoid);

			if (funcInfo)
				addObjectDependency(&planginfo[i].dobj,
									funcInfo->dobj.dumpId);
		}
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return planginfo;
}

/*
 * getCasts
 *	  get basic information about every cast in the system
 *
 * numCasts is set to the number of casts read in
 */
CastInfo *
getCasts(int *numCasts)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	CastInfo   *castinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_castsource;
	int			i_casttarget;
	int			i_castfunc;
	int			i_castcontext;
	int			i_castmethod;

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	if (g_fout->remoteVersion >= 80400)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, "
						  "castsource, casttarget, castfunc, castcontext, "
						  "castmethod "
						  "FROM pg_cast ORDER BY 3,4");
	}
	else if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, "
						  "castsource, casttarget, castfunc, castcontext, "
				"CASE WHEN castfunc = 0 THEN 'b' ELSE 'f' END AS castmethod "
						  "FROM pg_cast ORDER BY 3,4");
	}
	else
	{
		appendPQExpBuffer(query, "SELECT 0 AS tableoid, p.oid, "
						  "t1.oid AS castsource, t2.oid AS casttarget, "
						  "p.oid AS castfunc, 'e' AS castcontext, "
						  "'f' AS castmethod "
						  "FROM pg_type t1, pg_type t2, pg_proc p "
						  "WHERE p.pronargs = 1 AND "
						  "p.proargtypes[0] = t1.oid AND "
						  "p.prorettype = t2.oid AND p.proname = t2.typname "
						  "ORDER BY 3,4");
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numCasts = ntups;

	castinfo = (CastInfo *) malloc(ntups * sizeof(CastInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_castsource = PQfnumber(res, "castsource");
	i_casttarget = PQfnumber(res, "casttarget");
	i_castfunc = PQfnumber(res, "castfunc");
	i_castcontext = PQfnumber(res, "castcontext");
	i_castmethod = PQfnumber(res, "castmethod");

	for (i = 0; i < ntups; i++)
	{
		PQExpBufferData namebuf;
		TypeInfo   *sTypeInfo;
		TypeInfo   *tTypeInfo;

		castinfo[i].dobj.objType = DO_CAST;
		castinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		castinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&castinfo[i].dobj);
		castinfo[i].castsource = atooid(PQgetvalue(res, i, i_castsource));
		castinfo[i].casttarget = atooid(PQgetvalue(res, i, i_casttarget));
		castinfo[i].castfunc = atooid(PQgetvalue(res, i, i_castfunc));
		castinfo[i].castcontext = *(PQgetvalue(res, i, i_castcontext));
		castinfo[i].castmethod = *(PQgetvalue(res, i, i_castmethod));

		/*
		 * Try to name cast as concatenation of typnames.  This is only used
		 * for purposes of sorting.  If we fail to find either type, the name
		 * will be an empty string.
		 */
		initPQExpBuffer(&namebuf);
		sTypeInfo = findTypeByOid(castinfo[i].castsource);
		tTypeInfo = findTypeByOid(castinfo[i].casttarget);
		if (sTypeInfo && tTypeInfo)
			appendPQExpBuffer(&namebuf, "%s %s",
							  sTypeInfo->dobj.name, tTypeInfo->dobj.name);
		castinfo[i].dobj.name = namebuf.data;

		if (OidIsValid(castinfo[i].castfunc))
		{
			/*
			 * We need to make a dependency to ensure the function will be
			 * dumped first.  (In 7.3 and later the regular dependency
			 * mechanism will handle this for us.)
			 */
			FuncInfo   *funcInfo;

			funcInfo = findFuncByOid(castinfo[i].castfunc);
			if (funcInfo)
				addObjectDependency(&castinfo[i].dobj,
									funcInfo->dobj.dumpId);
		}
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return castinfo;
}

/*
 * getTableAttrs -
 *	  for each interesting table, read info about its attributes
 *	  (names, types, default values, CHECK constraints, etc)
 *
 * This is implemented in a very inefficient way right now, looping
 * through the tblinfo and doing a join per table to find the attrs and their
 * types.  However, because we want type names and so forth to be named
 * relative to the schema of each table, we couldn't do it in just one
 * query.  (Maybe one query per schema?)
 *
 *	modifies tblinfo
 */
void
getTableAttrs(TableInfo *tblinfo, int numTables)
{
	int			i,
				j;
	PQExpBuffer q = createPQExpBuffer();
	int			i_attnum;
	int			i_attname;
	int			i_atttypname;
	int			i_atttypmod;
	int			i_attstattarget;
	int			i_attstorage;
	int			i_typstorage;
	int			i_attnotnull;
	int			i_atthasdef;
	int			i_attisdropped;
	int			i_attlen;
	int			i_attalign;
	int			i_attislocal;
	int			i_attoptions;
	PGresult   *res;
	int			ntups;
	bool		hasdefaults;

	for (i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &tblinfo[i];

		/* Don't bother to collect info for sequences */
		if (tbinfo->relkind == RELKIND_SEQUENCE)
			continue;

		/* Don't bother with uninteresting tables, either */
		if (!tbinfo->interesting)
			continue;

		/*
		 * Make sure we are in proper schema for this table; this allows
		 * correct retrieval of formatted type names and default exprs
		 */
		selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

		/* find all the user attributes and their types */

		/*
		 * we must read the attribute names in attribute number order! because
		 * we will use the attnum to index into the attnames array later.  We
		 * actually ask to order by "attrelid, attnum" because (at least up to
		 * 7.3) the planner is not smart enough to realize it needn't re-sort
		 * the output of an indexscan on pg_attribute_relid_attnum_index.
		 */
		if (g_verbose)
			write_msg(NULL, "finding the columns and types of table \"%s\"\n",
					  tbinfo->dobj.name);

		resetPQExpBuffer(q);

		if (g_fout->remoteVersion >= 90000)
		{
			/* attoptions is new in 9.0 */
			appendPQExpBuffer(q, "SELECT a.attnum, a.attname, a.atttypmod, "
							  "a.attstattarget, a.attstorage, t.typstorage, "
							  "a.attnotnull, a.atthasdef, a.attisdropped, "
							  "a.attlen, a.attalign, a.attislocal, "
				  "pg_catalog.format_type(t.oid,a.atttypmod) AS atttypname, "
						   "array_to_string(attoptions, ', ') AS attoptions "
			 "FROM pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_type t "
							  "ON a.atttypid = t.oid "
							  "WHERE a.attrelid = '%u'::pg_catalog.oid "
							  "AND a.attnum > 0::pg_catalog.int2 "
							  "ORDER BY a.attrelid, a.attnum",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 70300)
		{
			/* need left join here to not fail on dropped columns ... */
			appendPQExpBuffer(q, "SELECT a.attnum, a.attname, a.atttypmod, "
							  "a.attstattarget, a.attstorage, t.typstorage, "
							  "a.attnotnull, a.atthasdef, a.attisdropped, "
							  "a.attlen, a.attalign, a.attislocal, "
				  "pg_catalog.format_type(t.oid,a.atttypmod) AS atttypname, "
							  "'' AS attoptions "
			 "FROM pg_catalog.pg_attribute a LEFT JOIN pg_catalog.pg_type t "
							  "ON a.atttypid = t.oid "
							  "WHERE a.attrelid = '%u'::pg_catalog.oid "
							  "AND a.attnum > 0::pg_catalog.int2 "
							  "ORDER BY a.attrelid, a.attnum",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 70100)
		{
			/*
			 * attstattarget doesn't exist in 7.1.  It does exist in 7.2, but
			 * we don't dump it because we can't tell whether it's been
			 * explicitly set or was just a default.
			 */
			appendPQExpBuffer(q, "SELECT a.attnum, a.attname, a.atttypmod, "
							  "-1 AS attstattarget, a.attstorage, "
							  "t.typstorage, a.attnotnull, a.atthasdef, "
							  "false AS attisdropped, a.attlen, "
							  "a.attalign, false AS attislocal, "
							  "format_type(t.oid,a.atttypmod) AS atttypname, "
							  "'' AS attoptions "
							  "FROM pg_attribute a LEFT JOIN pg_type t "
							  "ON a.atttypid = t.oid "
							  "WHERE a.attrelid = '%u'::oid "
							  "AND a.attnum > 0::int2 "
							  "ORDER BY a.attrelid, a.attnum",
							  tbinfo->dobj.catId.oid);
		}
		else
		{
			/* format_type not available before 7.1 */
			appendPQExpBuffer(q, "SELECT attnum, attname, atttypmod, "
							  "-1 AS attstattarget, "
							  "attstorage, attstorage AS typstorage, "
							  "attnotnull, atthasdef, false AS attisdropped, "
							  "attlen, attalign, "
							  "false AS attislocal, "
							  "(SELECT typname FROM pg_type WHERE oid = atttypid) AS atttypname, "
							  "'' AS attoptions "
							  "FROM pg_attribute a "
							  "WHERE attrelid = '%u'::oid "
							  "AND attnum > 0::int2 "
							  "ORDER BY attrelid, attnum",
							  tbinfo->dobj.catId.oid);
		}

		res = PQexec(g_conn, q->data);
		check_sql_result(res, g_conn, q->data, PGRES_TUPLES_OK);

		ntups = PQntuples(res);

		i_attnum = PQfnumber(res, "attnum");
		i_attname = PQfnumber(res, "attname");
		i_atttypname = PQfnumber(res, "atttypname");
		i_atttypmod = PQfnumber(res, "atttypmod");
		i_attstattarget = PQfnumber(res, "attstattarget");
		i_attstorage = PQfnumber(res, "attstorage");
		i_typstorage = PQfnumber(res, "typstorage");
		i_attnotnull = PQfnumber(res, "attnotnull");
		i_atthasdef = PQfnumber(res, "atthasdef");
		i_attisdropped = PQfnumber(res, "attisdropped");
		i_attlen = PQfnumber(res, "attlen");
		i_attalign = PQfnumber(res, "attalign");
		i_attislocal = PQfnumber(res, "attislocal");
		i_attoptions = PQfnumber(res, "attoptions");

		tbinfo->numatts = ntups;
		tbinfo->attnames = (char **) malloc(ntups * sizeof(char *));
		tbinfo->atttypnames = (char **) malloc(ntups * sizeof(char *));
		tbinfo->atttypmod = (int *) malloc(ntups * sizeof(int));
		tbinfo->attstattarget = (int *) malloc(ntups * sizeof(int));
		tbinfo->attstorage = (char *) malloc(ntups * sizeof(char));
		tbinfo->typstorage = (char *) malloc(ntups * sizeof(char));
		tbinfo->attisdropped = (bool *) malloc(ntups * sizeof(bool));
		tbinfo->attlen = (int *) malloc(ntups * sizeof(int));
		tbinfo->attalign = (char *) malloc(ntups * sizeof(char));
		tbinfo->attislocal = (bool *) malloc(ntups * sizeof(bool));
		tbinfo->notnull = (bool *) malloc(ntups * sizeof(bool));
		tbinfo->attrdefs = (AttrDefInfo **) malloc(ntups * sizeof(AttrDefInfo *));
		tbinfo->attoptions = (char **) malloc(ntups * sizeof(char *));
		tbinfo->inhAttrs = (bool *) malloc(ntups * sizeof(bool));
		tbinfo->inhAttrDef = (bool *) malloc(ntups * sizeof(bool));
		tbinfo->inhNotNull = (bool *) malloc(ntups * sizeof(bool));
		hasdefaults = false;

		for (j = 0; j < ntups; j++)
		{
			if (j + 1 != atoi(PQgetvalue(res, j, i_attnum)))
			{
				write_msg(NULL, "invalid column numbering in table \"%s\"\n",
						  tbinfo->dobj.name);
				exit_nicely();
			}
			tbinfo->attnames[j] = strdup(PQgetvalue(res, j, i_attname));
			tbinfo->atttypnames[j] = strdup(PQgetvalue(res, j, i_atttypname));
			tbinfo->atttypmod[j] = atoi(PQgetvalue(res, j, i_atttypmod));
			tbinfo->attstattarget[j] = atoi(PQgetvalue(res, j, i_attstattarget));
			tbinfo->attstorage[j] = *(PQgetvalue(res, j, i_attstorage));
			tbinfo->typstorage[j] = *(PQgetvalue(res, j, i_typstorage));
			tbinfo->attisdropped[j] = (PQgetvalue(res, j, i_attisdropped)[0] == 't');
			tbinfo->attlen[j] = atoi(PQgetvalue(res, j, i_attlen));
			tbinfo->attalign[j] = *(PQgetvalue(res, j, i_attalign));
			tbinfo->attislocal[j] = (PQgetvalue(res, j, i_attislocal)[0] == 't');
			tbinfo->notnull[j] = (PQgetvalue(res, j, i_attnotnull)[0] == 't');
			tbinfo->attoptions[j] = strdup(PQgetvalue(res, j, i_attoptions));
			tbinfo->attrdefs[j] = NULL; /* fix below */
			if (PQgetvalue(res, j, i_atthasdef)[0] == 't')
				hasdefaults = true;
			/* these flags will be set in flagInhAttrs() */
			tbinfo->inhAttrs[j] = false;
			tbinfo->inhAttrDef[j] = false;
			tbinfo->inhNotNull[j] = false;
		}

		PQclear(res);

		/*
		 * Get info about column defaults
		 */
		if (hasdefaults)
		{
			AttrDefInfo *attrdefs;
			int			numDefaults;

			if (g_verbose)
				write_msg(NULL, "finding default expressions of table \"%s\"\n",
						  tbinfo->dobj.name);

			resetPQExpBuffer(q);
			if (g_fout->remoteVersion >= 70300)
			{
				appendPQExpBuffer(q, "SELECT tableoid, oid, adnum, "
						   "pg_catalog.pg_get_expr(adbin, adrelid) AS adsrc "
								  "FROM pg_catalog.pg_attrdef "
								  "WHERE adrelid = '%u'::pg_catalog.oid",
								  tbinfo->dobj.catId.oid);
			}
			else if (g_fout->remoteVersion >= 70200)
			{
				/* 7.2 did not have OIDs in pg_attrdef */
				appendPQExpBuffer(q, "SELECT tableoid, 0 AS oid, adnum, "
								  "pg_get_expr(adbin, adrelid) AS adsrc "
								  "FROM pg_attrdef "
								  "WHERE adrelid = '%u'::oid",
								  tbinfo->dobj.catId.oid);
			}
			else if (g_fout->remoteVersion >= 70100)
			{
				/* no pg_get_expr, so must rely on adsrc */
				appendPQExpBuffer(q, "SELECT tableoid, oid, adnum, adsrc "
								  "FROM pg_attrdef "
								  "WHERE adrelid = '%u'::oid",
								  tbinfo->dobj.catId.oid);
			}
			else
			{
				/* no pg_get_expr, no tableoid either */
				appendPQExpBuffer(q, "SELECT "
								  "(SELECT oid FROM pg_class WHERE relname = 'pg_attrdef') AS tableoid, "
								  "oid, adnum, adsrc "
								  "FROM pg_attrdef "
								  "WHERE adrelid = '%u'::oid",
								  tbinfo->dobj.catId.oid);
			}
			res = PQexec(g_conn, q->data);
			check_sql_result(res, g_conn, q->data, PGRES_TUPLES_OK);

			numDefaults = PQntuples(res);
			attrdefs = (AttrDefInfo *) malloc(numDefaults * sizeof(AttrDefInfo));

			for (j = 0; j < numDefaults; j++)
			{
				int			adnum;

				attrdefs[j].dobj.objType = DO_ATTRDEF;
				attrdefs[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, 0));
				attrdefs[j].dobj.catId.oid = atooid(PQgetvalue(res, j, 1));
				AssignDumpId(&attrdefs[j].dobj);
				attrdefs[j].adtable = tbinfo;
				attrdefs[j].adnum = adnum = atoi(PQgetvalue(res, j, 2));
				attrdefs[j].adef_expr = strdup(PQgetvalue(res, j, 3));

				attrdefs[j].dobj.name = strdup(tbinfo->dobj.name);
				attrdefs[j].dobj.namespace = tbinfo->dobj.namespace;

				attrdefs[j].dobj.dump = tbinfo->dobj.dump;

				/*
				 * Defaults on a VIEW must always be dumped as separate ALTER
				 * TABLE commands.	Defaults on regular tables are dumped as
				 * part of the CREATE TABLE if possible.  To check if it's
				 * safe, we mark the default as needing to appear before the
				 * CREATE.
				 */
				if (tbinfo->relkind == RELKIND_VIEW)
				{
					attrdefs[j].separate = true;
					/* needed in case pre-7.3 DB: */
					addObjectDependency(&attrdefs[j].dobj,
										tbinfo->dobj.dumpId);
				}
				else
				{
					attrdefs[j].separate = false;
					addObjectDependency(&tbinfo->dobj,
										attrdefs[j].dobj.dumpId);
				}

				if (adnum <= 0 || adnum > ntups)
				{
					write_msg(NULL, "invalid adnum value %d for table \"%s\"\n",
							  adnum, tbinfo->dobj.name);
					exit_nicely();
				}
				tbinfo->attrdefs[adnum - 1] = &attrdefs[j];
			}
			PQclear(res);
		}

		/*
		 * Get info about table CHECK constraints
		 */
		if (tbinfo->ncheck > 0)
		{
			ConstraintInfo *constrs;
			int			numConstrs;

			if (g_verbose)
				write_msg(NULL, "finding check constraints for table \"%s\"\n",
						  tbinfo->dobj.name);

			resetPQExpBuffer(q);
			if (g_fout->remoteVersion >= 80400)
			{
				appendPQExpBuffer(q, "SELECT tableoid, oid, conname, "
						   "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
								  "conislocal "
								  "FROM pg_catalog.pg_constraint "
								  "WHERE conrelid = '%u'::pg_catalog.oid "
								  "   AND contype = 'c' "
								  "ORDER BY conname",
								  tbinfo->dobj.catId.oid);
			}
			else if (g_fout->remoteVersion >= 70400)
			{
				appendPQExpBuffer(q, "SELECT tableoid, oid, conname, "
						   "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
								  "true AS conislocal "
								  "FROM pg_catalog.pg_constraint "
								  "WHERE conrelid = '%u'::pg_catalog.oid "
								  "   AND contype = 'c' "
								  "ORDER BY conname",
								  tbinfo->dobj.catId.oid);
			}
			else if (g_fout->remoteVersion >= 70300)
			{
				/* no pg_get_constraintdef, must use consrc */
				appendPQExpBuffer(q, "SELECT tableoid, oid, conname, "
								  "'CHECK (' || consrc || ')' AS consrc, "
								  "true AS conislocal "
								  "FROM pg_catalog.pg_constraint "
								  "WHERE conrelid = '%u'::pg_catalog.oid "
								  "   AND contype = 'c' "
								  "ORDER BY conname",
								  tbinfo->dobj.catId.oid);
			}
			else if (g_fout->remoteVersion >= 70200)
			{
				/* 7.2 did not have OIDs in pg_relcheck */
				appendPQExpBuffer(q, "SELECT tableoid, 0 AS oid, "
								  "rcname AS conname, "
								  "'CHECK (' || rcsrc || ')' AS consrc, "
								  "true AS conislocal "
								  "FROM pg_relcheck "
								  "WHERE rcrelid = '%u'::oid "
								  "ORDER BY rcname",
								  tbinfo->dobj.catId.oid);
			}
			else if (g_fout->remoteVersion >= 70100)
			{
				appendPQExpBuffer(q, "SELECT tableoid, oid, "
								  "rcname AS conname, "
								  "'CHECK (' || rcsrc || ')' AS consrc, "
								  "true AS conislocal "
								  "FROM pg_relcheck "
								  "WHERE rcrelid = '%u'::oid "
								  "ORDER BY rcname",
								  tbinfo->dobj.catId.oid);
			}
			else
			{
				/* no tableoid in 7.0 */
				appendPQExpBuffer(q, "SELECT "
								  "(SELECT oid FROM pg_class WHERE relname = 'pg_relcheck') AS tableoid, "
								  "oid, rcname AS conname, "
								  "'CHECK (' || rcsrc || ')' AS consrc, "
								  "true AS conislocal "
								  "FROM pg_relcheck "
								  "WHERE rcrelid = '%u'::oid "
								  "ORDER BY rcname",
								  tbinfo->dobj.catId.oid);
			}
			res = PQexec(g_conn, q->data);
			check_sql_result(res, g_conn, q->data, PGRES_TUPLES_OK);

			numConstrs = PQntuples(res);
			if (numConstrs != tbinfo->ncheck)
			{
				write_msg(NULL, ngettext("expected %d check constraint on table \"%s\" but found %d\n",
										 "expected %d check constraints on table \"%s\" but found %d\n",
										 tbinfo->ncheck),
						  tbinfo->ncheck, tbinfo->dobj.name, numConstrs);
				write_msg(NULL, "(The system catalogs might be corrupted.)\n");
				exit_nicely();
			}

			constrs = (ConstraintInfo *) malloc(numConstrs * sizeof(ConstraintInfo));
			tbinfo->checkexprs = constrs;

			for (j = 0; j < numConstrs; j++)
			{
				constrs[j].dobj.objType = DO_CONSTRAINT;
				constrs[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, 0));
				constrs[j].dobj.catId.oid = atooid(PQgetvalue(res, j, 1));
				AssignDumpId(&constrs[j].dobj);
				constrs[j].dobj.name = strdup(PQgetvalue(res, j, 2));
				constrs[j].dobj.namespace = tbinfo->dobj.namespace;
				constrs[j].contable = tbinfo;
				constrs[j].condomain = NULL;
				constrs[j].contype = 'c';
				constrs[j].condef = strdup(PQgetvalue(res, j, 3));
				constrs[j].confrelid = InvalidOid;
				constrs[j].conindex = 0;
				constrs[j].condeferrable = false;
				constrs[j].condeferred = false;
				constrs[j].conislocal = (PQgetvalue(res, j, 4)[0] == 't');
				constrs[j].separate = false;

				constrs[j].dobj.dump = tbinfo->dobj.dump;

				/*
				 * Mark the constraint as needing to appear before the table
				 * --- this is so that any other dependencies of the
				 * constraint will be emitted before we try to create the
				 * table.
				 */
				addObjectDependency(&tbinfo->dobj,
									constrs[j].dobj.dumpId);

				/*
				 * If the constraint is inherited, this will be detected later
				 * (in pre-8.4 databases).	We also detect later if the
				 * constraint must be split out from the table definition.
				 */
			}
			PQclear(res);
		}
	}

	destroyPQExpBuffer(q);
}


/*
 * getTSParsers:
 *	  read all text search parsers in the system catalogs and return them
 *	  in the TSParserInfo* structure
 *
 *	numTSParsers is set to the number of parsers read in
 */
TSParserInfo *
getTSParsers(int *numTSParsers)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	TSParserInfo *prsinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_prsname;
	int			i_prsnamespace;
	int			i_prsstart;
	int			i_prstoken;
	int			i_prsend;
	int			i_prsheadline;
	int			i_prslextype;

	/* Before 8.3, there is no built-in text search support */
	if (g_fout->remoteVersion < 80300)
	{
		*numTSParsers = 0;
		return NULL;
	}

	/*
	 * find all text search objects, including builtin ones; we filter out
	 * system-defined objects at dump-out time.
	 */

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT tableoid, oid, prsname, prsnamespace, "
					  "prsstart::oid, prstoken::oid, "
					  "prsend::oid, prsheadline::oid, prslextype::oid "
					  "FROM pg_ts_parser");

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numTSParsers = ntups;

	prsinfo = (TSParserInfo *) malloc(ntups * sizeof(TSParserInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_prsname = PQfnumber(res, "prsname");
	i_prsnamespace = PQfnumber(res, "prsnamespace");
	i_prsstart = PQfnumber(res, "prsstart");
	i_prstoken = PQfnumber(res, "prstoken");
	i_prsend = PQfnumber(res, "prsend");
	i_prsheadline = PQfnumber(res, "prsheadline");
	i_prslextype = PQfnumber(res, "prslextype");

	for (i = 0; i < ntups; i++)
	{
		prsinfo[i].dobj.objType = DO_TSPARSER;
		prsinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		prsinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&prsinfo[i].dobj);
		prsinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_prsname));
		prsinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_prsnamespace)),
												  prsinfo[i].dobj.catId.oid);
		prsinfo[i].prsstart = atooid(PQgetvalue(res, i, i_prsstart));
		prsinfo[i].prstoken = atooid(PQgetvalue(res, i, i_prstoken));
		prsinfo[i].prsend = atooid(PQgetvalue(res, i, i_prsend));
		prsinfo[i].prsheadline = atooid(PQgetvalue(res, i, i_prsheadline));
		prsinfo[i].prslextype = atooid(PQgetvalue(res, i, i_prslextype));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(prsinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return prsinfo;
}

/*
 * getTSDictionaries:
 *	  read all text search dictionaries in the system catalogs and return them
 *	  in the TSDictInfo* structure
 *
 *	numTSDicts is set to the number of dictionaries read in
 */
TSDictInfo *
getTSDictionaries(int *numTSDicts)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	TSDictInfo *dictinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_dictname;
	int			i_dictnamespace;
	int			i_rolname;
	int			i_dicttemplate;
	int			i_dictinitoption;

	/* Before 8.3, there is no built-in text search support */
	if (g_fout->remoteVersion < 80300)
	{
		*numTSDicts = 0;
		return NULL;
	}

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT tableoid, oid, dictname, "
					  "dictnamespace, (%s dictowner) AS rolname, "
					  "dicttemplate, dictinitoption "
					  "FROM pg_ts_dict",
					  username_subquery);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numTSDicts = ntups;

	dictinfo = (TSDictInfo *) malloc(ntups * sizeof(TSDictInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_dictname = PQfnumber(res, "dictname");
	i_dictnamespace = PQfnumber(res, "dictnamespace");
	i_rolname = PQfnumber(res, "rolname");
	i_dictinitoption = PQfnumber(res, "dictinitoption");
	i_dicttemplate = PQfnumber(res, "dicttemplate");

	for (i = 0; i < ntups; i++)
	{
		dictinfo[i].dobj.objType = DO_TSDICT;
		dictinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		dictinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&dictinfo[i].dobj);
		dictinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_dictname));
		dictinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_dictnamespace)),
												 dictinfo[i].dobj.catId.oid);
		dictinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		dictinfo[i].dicttemplate = atooid(PQgetvalue(res, i, i_dicttemplate));
		if (PQgetisnull(res, i, i_dictinitoption))
			dictinfo[i].dictinitoption = NULL;
		else
			dictinfo[i].dictinitoption = strdup(PQgetvalue(res, i, i_dictinitoption));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(dictinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return dictinfo;
}

/*
 * getTSTemplates:
 *	  read all text search templates in the system catalogs and return them
 *	  in the TSTemplateInfo* structure
 *
 *	numTSTemplates is set to the number of templates read in
 */
TSTemplateInfo *
getTSTemplates(int *numTSTemplates)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	TSTemplateInfo *tmplinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_tmplname;
	int			i_tmplnamespace;
	int			i_tmplinit;
	int			i_tmpllexize;

	/* Before 8.3, there is no built-in text search support */
	if (g_fout->remoteVersion < 80300)
	{
		*numTSTemplates = 0;
		return NULL;
	}

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT tableoid, oid, tmplname, "
					  "tmplnamespace, tmplinit::oid, tmpllexize::oid "
					  "FROM pg_ts_template");

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numTSTemplates = ntups;

	tmplinfo = (TSTemplateInfo *) malloc(ntups * sizeof(TSTemplateInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_tmplname = PQfnumber(res, "tmplname");
	i_tmplnamespace = PQfnumber(res, "tmplnamespace");
	i_tmplinit = PQfnumber(res, "tmplinit");
	i_tmpllexize = PQfnumber(res, "tmpllexize");

	for (i = 0; i < ntups; i++)
	{
		tmplinfo[i].dobj.objType = DO_TSTEMPLATE;
		tmplinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		tmplinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&tmplinfo[i].dobj);
		tmplinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_tmplname));
		tmplinfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_tmplnamespace)),
												 tmplinfo[i].dobj.catId.oid);
		tmplinfo[i].tmplinit = atooid(PQgetvalue(res, i, i_tmplinit));
		tmplinfo[i].tmpllexize = atooid(PQgetvalue(res, i, i_tmpllexize));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(tmplinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return tmplinfo;
}

/*
 * getTSConfigurations:
 *	  read all text search configurations in the system catalogs and return
 *	  them in the TSConfigInfo* structure
 *
 *	numTSConfigs is set to the number of configurations read in
 */
TSConfigInfo *
getTSConfigurations(int *numTSConfigs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	TSConfigInfo *cfginfo;
	int			i_tableoid;
	int			i_oid;
	int			i_cfgname;
	int			i_cfgnamespace;
	int			i_rolname;
	int			i_cfgparser;

	/* Before 8.3, there is no built-in text search support */
	if (g_fout->remoteVersion < 80300)
	{
		*numTSConfigs = 0;
		return NULL;
	}

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT tableoid, oid, cfgname, "
					  "cfgnamespace, (%s cfgowner) AS rolname, cfgparser "
					  "FROM pg_ts_config",
					  username_subquery);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numTSConfigs = ntups;

	cfginfo = (TSConfigInfo *) malloc(ntups * sizeof(TSConfigInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_cfgname = PQfnumber(res, "cfgname");
	i_cfgnamespace = PQfnumber(res, "cfgnamespace");
	i_rolname = PQfnumber(res, "rolname");
	i_cfgparser = PQfnumber(res, "cfgparser");

	for (i = 0; i < ntups; i++)
	{
		cfginfo[i].dobj.objType = DO_TSCONFIG;
		cfginfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		cfginfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&cfginfo[i].dobj);
		cfginfo[i].dobj.name = strdup(PQgetvalue(res, i, i_cfgname));
		cfginfo[i].dobj.namespace = findNamespace(atooid(PQgetvalue(res, i, i_cfgnamespace)),
												  cfginfo[i].dobj.catId.oid);
		cfginfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		cfginfo[i].cfgparser = atooid(PQgetvalue(res, i, i_cfgparser));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(cfginfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return cfginfo;
}

/*
 * getForeignDataWrappers:
 *	  read all foreign-data wrappers in the system catalogs and return
 *	  them in the FdwInfo* structure
 *
 *	numForeignDataWrappers is set to the number of fdws read in
 */
FdwInfo *
getForeignDataWrappers(int *numForeignDataWrappers)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	FdwInfo    *fdwinfo;
	int			i_oid;
	int			i_fdwname;
	int			i_rolname;
	int			i_fdwvalidator;
	int			i_fdwacl;
	int			i_fdwoptions;

	/* Before 8.4, there are no foreign-data wrappers */
	if (g_fout->remoteVersion < 80400)
	{
		*numForeignDataWrappers = 0;
		return NULL;
	}

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT oid, fdwname, "
		"(%s fdwowner) AS rolname, fdwvalidator::pg_catalog.regproc, fdwacl,"
					  "array_to_string(ARRAY("
		 "		SELECT option_name || ' ' || quote_literal(option_value) "
	   "		FROM pg_options_to_table(fdwoptions)), ', ') AS fdwoptions "
					  "FROM pg_foreign_data_wrapper",
					  username_subquery);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numForeignDataWrappers = ntups;

	fdwinfo = (FdwInfo *) malloc(ntups * sizeof(FdwInfo));

	i_oid = PQfnumber(res, "oid");
	i_fdwname = PQfnumber(res, "fdwname");
	i_rolname = PQfnumber(res, "rolname");
	i_fdwvalidator = PQfnumber(res, "fdwvalidator");
	i_fdwacl = PQfnumber(res, "fdwacl");
	i_fdwoptions = PQfnumber(res, "fdwoptions");

	for (i = 0; i < ntups; i++)
	{
		fdwinfo[i].dobj.objType = DO_FDW;
		fdwinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&fdwinfo[i].dobj);
		fdwinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_fdwname));
		fdwinfo[i].dobj.namespace = NULL;
		fdwinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		fdwinfo[i].fdwvalidator = strdup(PQgetvalue(res, i, i_fdwvalidator));
		fdwinfo[i].fdwoptions = strdup(PQgetvalue(res, i, i_fdwoptions));
		fdwinfo[i].fdwacl = strdup(PQgetvalue(res, i, i_fdwacl));


		/* Decide whether we want to dump it */
		selectDumpableObject(&(fdwinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return fdwinfo;
}

/*
 * getForeignServers:
 *	  read all foreign servers in the system catalogs and return
 *	  them in the ForeignServerInfo * structure
 *
 *	numForeignServers is set to the number of servers read in
 */
ForeignServerInfo *
getForeignServers(int *numForeignServers)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	ForeignServerInfo *srvinfo;
	int			i_oid;
	int			i_srvname;
	int			i_rolname;
	int			i_srvfdw;
	int			i_srvtype;
	int			i_srvversion;
	int			i_srvacl;
	int			i_srvoptions;

	/* Before 8.4, there are no foreign servers */
	if (g_fout->remoteVersion < 80400)
	{
		*numForeignServers = 0;
		return NULL;
	}

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT oid, srvname, "
					  "(%s srvowner) AS rolname, "
					  "srvfdw, srvtype, srvversion, srvacl,"
					  "array_to_string(ARRAY("
		 "		SELECT option_name || ' ' || quote_literal(option_value) "
	   "		FROM pg_options_to_table(srvoptions)), ', ') AS srvoptions "
					  "FROM pg_foreign_server",
					  username_subquery);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numForeignServers = ntups;

	srvinfo = (ForeignServerInfo *) malloc(ntups * sizeof(ForeignServerInfo));

	i_oid = PQfnumber(res, "oid");
	i_srvname = PQfnumber(res, "srvname");
	i_rolname = PQfnumber(res, "rolname");
	i_srvfdw = PQfnumber(res, "srvfdw");
	i_srvtype = PQfnumber(res, "srvtype");
	i_srvversion = PQfnumber(res, "srvversion");
	i_srvacl = PQfnumber(res, "srvacl");
	i_srvoptions = PQfnumber(res, "srvoptions");

	for (i = 0; i < ntups; i++)
	{
		srvinfo[i].dobj.objType = DO_FOREIGN_SERVER;
		srvinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&srvinfo[i].dobj);
		srvinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_srvname));
		srvinfo[i].dobj.namespace = NULL;
		srvinfo[i].rolname = strdup(PQgetvalue(res, i, i_rolname));
		srvinfo[i].srvfdw = atooid(PQgetvalue(res, i, i_srvfdw));
		srvinfo[i].srvtype = strdup(PQgetvalue(res, i, i_srvtype));
		srvinfo[i].srvversion = strdup(PQgetvalue(res, i, i_srvversion));
		srvinfo[i].srvoptions = strdup(PQgetvalue(res, i, i_srvoptions));
		srvinfo[i].srvacl = strdup(PQgetvalue(res, i, i_srvacl));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(srvinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return srvinfo;
}

/*
 * getDefaultACLs:
 *	  read all default ACL information in the system catalogs and return
 *	  them in the DefaultACLInfo structure
 *
 *	numDefaultACLs is set to the number of ACLs read in
 */
DefaultACLInfo *
getDefaultACLs(int *numDefaultACLs)
{
	DefaultACLInfo *daclinfo;
	PQExpBuffer query;
	PGresult   *res;
	int			i_oid;
	int			i_tableoid;
	int			i_defaclrole;
	int			i_defaclnamespace;
	int			i_defaclobjtype;
	int			i_defaclacl;
	int			i,
				ntups;

	if (g_fout->remoteVersion < 90000)
	{
		*numDefaultACLs = 0;
		return NULL;
	}

	query = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query, "SELECT oid, tableoid, "
					  "(%s defaclrole) AS defaclrole, "
					  "defaclnamespace, "
					  "defaclobjtype, "
					  "defaclacl "
					  "FROM pg_default_acl",
					  username_subquery);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numDefaultACLs = ntups;

	daclinfo = (DefaultACLInfo *) malloc(ntups * sizeof(DefaultACLInfo));

	i_oid = PQfnumber(res, "oid");
	i_tableoid = PQfnumber(res, "tableoid");
	i_defaclrole = PQfnumber(res, "defaclrole");
	i_defaclnamespace = PQfnumber(res, "defaclnamespace");
	i_defaclobjtype = PQfnumber(res, "defaclobjtype");
	i_defaclacl = PQfnumber(res, "defaclacl");

	for (i = 0; i < ntups; i++)
	{
		Oid			nspid = atooid(PQgetvalue(res, i, i_defaclnamespace));

		daclinfo[i].dobj.objType = DO_DEFAULT_ACL;
		daclinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		daclinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&daclinfo[i].dobj);
		/* cheesy ... is it worth coming up with a better object name? */
		daclinfo[i].dobj.name = strdup(PQgetvalue(res, i, i_defaclobjtype));

		if (nspid != InvalidOid)
			daclinfo[i].dobj.namespace = findNamespace(nspid,
												 daclinfo[i].dobj.catId.oid);
		else
			daclinfo[i].dobj.namespace = NULL;

		daclinfo[i].defaclrole = strdup(PQgetvalue(res, i, i_defaclrole));
		daclinfo[i].defaclobjtype = *(PQgetvalue(res, i, i_defaclobjtype));
		daclinfo[i].defaclacl = strdup(PQgetvalue(res, i, i_defaclacl));

		/* Decide whether we want to dump it */
		selectDumpableDefaultACL(&(daclinfo[i]));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return daclinfo;
}

/*
 * dumpComment --
 *
 * This routine is used to dump any comments associated with the
 * object handed to this routine. The routine takes a constant character
 * string for the target part of the comment-creation command, plus
 * the namespace and owner of the object (for labeling the ArchiveEntry),
 * plus catalog ID and subid which are the lookup key for pg_description,
 * plus the dump ID for the object (for setting a dependency).
 * If a matching pg_description entry is found, it is dumped.
 *
 * Note: although this routine takes a dumpId for dependency purposes,
 * that purpose is just to mark the dependency in the emitted dump file
 * for possible future use by pg_restore.  We do NOT use it for determining
 * ordering of the comment in the dump file, because this routine is called
 * after dependency sorting occurs.  This routine should be called just after
 * calling ArchiveEntry() for the specified object.
 */
static void
dumpComment(Archive *fout, const char *target,
			const char *namespace, const char *owner,
			CatalogId catalogId, int subid, DumpId dumpId)
{
	CommentItem *comments;
	int			ncomments;

	/* Comments are schema not data ... except blob comments are data */
	if (strncmp(target, "LARGE OBJECT ", 13) != 0)
	{
		if (dataOnly)
			return;
	}
	else
	{
		if (schemaOnly)
			return;
	}

	/* Search for comments associated with catalogId, using table */
	ncomments = findComments(fout, catalogId.tableoid, catalogId.oid,
							 &comments);

	/* Is there one matching the subid? */
	while (ncomments > 0)
	{
		if (comments->objsubid == subid)
			break;
		comments++;
		ncomments--;
	}

	/* If a comment exists, build COMMENT ON statement */
	if (ncomments > 0)
	{
		PQExpBuffer query = createPQExpBuffer();

		appendPQExpBuffer(query, "COMMENT ON %s IS ", target);
		appendStringLiteralAH(query, comments->descr, fout);
		appendPQExpBuffer(query, ";\n");

		/*
		 * We mark comments as SECTION_NONE because they really belong in the
		 * same section as their parent, whether that is pre-data or
		 * post-data.
		 */
		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 target, namespace, NULL, owner,
					 false, "COMMENT", SECTION_NONE,
					 query->data, "", NULL,
					 &(dumpId), 1,
					 NULL, NULL);

		destroyPQExpBuffer(query);
	}
}

/*
 * dumpTableComment --
 *
 * As above, but dump comments for both the specified table (or view)
 * and its columns.
 */
static void
dumpTableComment(Archive *fout, TableInfo *tbinfo,
				 const char *reltypename)
{
	CommentItem *comments;
	int			ncomments;
	PQExpBuffer query;
	PQExpBuffer target;

	/* Comments are SCHEMA not data */
	if (dataOnly)
		return;

	/* Search for comments associated with relation, using table */
	ncomments = findComments(fout,
							 tbinfo->dobj.catId.tableoid,
							 tbinfo->dobj.catId.oid,
							 &comments);

	/* If comments exist, build COMMENT ON statements */
	if (ncomments <= 0)
		return;

	query = createPQExpBuffer();
	target = createPQExpBuffer();

	while (ncomments > 0)
	{
		const char *descr = comments->descr;
		int			objsubid = comments->objsubid;

		if (objsubid == 0)
		{
			resetPQExpBuffer(target);
			appendPQExpBuffer(target, "%s %s", reltypename,
							  fmtId(tbinfo->dobj.name));

			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "COMMENT ON %s IS ", target->data);
			appendStringLiteralAH(query, descr, fout);
			appendPQExpBuffer(query, ";\n");

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 target->data,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL, tbinfo->rolname,
						 false, "COMMENT", SECTION_NONE,
						 query->data, "", NULL,
						 &(tbinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}
		else if (objsubid > 0 && objsubid <= tbinfo->numatts)
		{
			resetPQExpBuffer(target);
			appendPQExpBuffer(target, "COLUMN %s.",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(target, "%s",
							  fmtId(tbinfo->attnames[objsubid - 1]));

			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "COMMENT ON %s IS ", target->data);
			appendStringLiteralAH(query, descr, fout);
			appendPQExpBuffer(query, ";\n");

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 target->data,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL, tbinfo->rolname,
						 false, "COMMENT", SECTION_NONE,
						 query->data, "", NULL,
						 &(tbinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}

		comments++;
		ncomments--;
	}

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(target);
}

/*
 * findComments --
 *
 * Find the comment(s), if any, associated with the given object.  All the
 * objsubid values associated with the given classoid/objoid are found with
 * one search.
 */
static int
findComments(Archive *fout, Oid classoid, Oid objoid,
			 CommentItem **items)
{
	/* static storage for table of comments */
	static CommentItem *comments = NULL;
	static int	ncomments = -1;

	CommentItem *middle = NULL;
	CommentItem *low;
	CommentItem *high;
	int			nmatch;

	/* Get comments if we didn't already */
	if (ncomments < 0)
		ncomments = collectComments(fout, &comments);

	/*
	 * Pre-7.2, pg_description does not contain classoid, so collectComments
	 * just stores a zero.	If there's a collision on object OID, well, you
	 * get duplicate comments.
	 */
	if (fout->remoteVersion < 70200)
		classoid = 0;

	/*
	 * Do binary search to find some item matching the object.
	 */
	low = &comments[0];
	high = &comments[ncomments - 1];
	while (low <= high)
	{
		middle = low + (high - low) / 2;

		if (classoid < middle->classoid)
			high = middle - 1;
		else if (classoid > middle->classoid)
			low = middle + 1;
		else if (objoid < middle->objoid)
			high = middle - 1;
		else if (objoid > middle->objoid)
			low = middle + 1;
		else
			break;				/* found a match */
	}

	if (low > high)				/* no matches */
	{
		*items = NULL;
		return 0;
	}

	/*
	 * Now determine how many items match the object.  The search loop
	 * invariant still holds: only items between low and high inclusive could
	 * match.
	 */
	nmatch = 1;
	while (middle > low)
	{
		if (classoid != middle[-1].classoid ||
			objoid != middle[-1].objoid)
			break;
		middle--;
		nmatch++;
	}

	*items = middle;

	middle += nmatch;
	while (middle <= high)
	{
		if (classoid != middle->classoid ||
			objoid != middle->objoid)
			break;
		middle++;
		nmatch++;
	}

	return nmatch;
}

/*
 * collectComments --
 *
 * Construct a table of all comments available for database objects.
 * We used to do per-object queries for the comments, but it's much faster
 * to pull them all over at once, and on most databases the memory cost
 * isn't high.
 *
 * The table is sorted by classoid/objid/objsubid for speed in lookup.
 */
static int
collectComments(Archive *fout, CommentItem **items)
{
	PGresult   *res;
	PQExpBuffer query;
	int			i_description;
	int			i_classoid;
	int			i_objoid;
	int			i_objsubid;
	int			ntups;
	int			i;
	CommentItem *comments;

	/*
	 * Note we do NOT change source schema here; preserve the caller's
	 * setting, instead.
	 */

	query = createPQExpBuffer();

	if (fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT description, classoid, objoid, objsubid "
						  "FROM pg_catalog.pg_description "
						  "ORDER BY classoid, objoid, objsubid");
	}
	else if (fout->remoteVersion >= 70200)
	{
		appendPQExpBuffer(query, "SELECT description, classoid, objoid, objsubid "
						  "FROM pg_description "
						  "ORDER BY classoid, objoid, objsubid");
	}
	else
	{
		/* Note: this will fail to find attribute comments in pre-7.2... */
		appendPQExpBuffer(query, "SELECT description, 0 AS classoid, objoid, 0 AS objsubid "
						  "FROM pg_description "
						  "ORDER BY objoid");
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Construct lookup table containing OIDs in numeric form */

	i_description = PQfnumber(res, "description");
	i_classoid = PQfnumber(res, "classoid");
	i_objoid = PQfnumber(res, "objoid");
	i_objsubid = PQfnumber(res, "objsubid");

	ntups = PQntuples(res);

	comments = (CommentItem *) malloc(ntups * sizeof(CommentItem));

	for (i = 0; i < ntups; i++)
	{
		comments[i].descr = PQgetvalue(res, i, i_description);
		comments[i].classoid = atooid(PQgetvalue(res, i, i_classoid));
		comments[i].objoid = atooid(PQgetvalue(res, i, i_objoid));
		comments[i].objsubid = atoi(PQgetvalue(res, i, i_objsubid));
	}

	/* Do NOT free the PGresult since we are keeping pointers into it */
	destroyPQExpBuffer(query);

	*items = comments;
	return ntups;
}

/*
 * dumpDumpableObject
 *
 * This routine and its subsidiaries are responsible for creating
 * ArchiveEntries (TOC objects) for each object to be dumped.
 */
static void
dumpDumpableObject(Archive *fout, DumpableObject *dobj)
{
	switch (dobj->objType)
	{
		case DO_NAMESPACE:
			dumpNamespace(fout, (NamespaceInfo *) dobj);
			break;
		case DO_TYPE:
			dumpType(fout, (TypeInfo *) dobj);
			break;
		case DO_SHELL_TYPE:
			dumpShellType(fout, (ShellTypeInfo *) dobj);
			break;
		case DO_FUNC:
			dumpFunc(fout, (FuncInfo *) dobj);
			break;
		case DO_AGG:
			dumpAgg(fout, (AggInfo *) dobj);
			break;
		case DO_OPERATOR:
			dumpOpr(fout, (OprInfo *) dobj);
			break;
		case DO_OPCLASS:
			dumpOpclass(fout, (OpclassInfo *) dobj);
			break;
		case DO_OPFAMILY:
			dumpOpfamily(fout, (OpfamilyInfo *) dobj);
			break;
		case DO_CONVERSION:
			dumpConversion(fout, (ConvInfo *) dobj);
			break;
		case DO_TABLE:
			dumpTable(fout, (TableInfo *) dobj);
			break;
		case DO_ATTRDEF:
			dumpAttrDef(fout, (AttrDefInfo *) dobj);
			break;
		case DO_INDEX:
			dumpIndex(fout, (IndxInfo *) dobj);
			break;
		case DO_RULE:
			dumpRule(fout, (RuleInfo *) dobj);
			break;
		case DO_TRIGGER:
			dumpTrigger(fout, (TriggerInfo *) dobj);
			break;
		case DO_CONSTRAINT:
			dumpConstraint(fout, (ConstraintInfo *) dobj);
			break;
		case DO_FK_CONSTRAINT:
			dumpConstraint(fout, (ConstraintInfo *) dobj);
			break;
		case DO_PROCLANG:
			dumpProcLang(fout, (ProcLangInfo *) dobj);
			break;
		case DO_CAST:
			dumpCast(fout, (CastInfo *) dobj);
			break;
		case DO_TABLE_DATA:
			dumpTableData(fout, (TableDataInfo *) dobj);
			break;
		case DO_DUMMY_TYPE:
			/* table rowtypes and array types are never dumped separately */
			break;
		case DO_TSPARSER:
			dumpTSParser(fout, (TSParserInfo *) dobj);
			break;
		case DO_TSDICT:
			dumpTSDictionary(fout, (TSDictInfo *) dobj);
			break;
		case DO_TSTEMPLATE:
			dumpTSTemplate(fout, (TSTemplateInfo *) dobj);
			break;
		case DO_TSCONFIG:
			dumpTSConfig(fout, (TSConfigInfo *) dobj);
			break;
		case DO_FDW:
			dumpForeignDataWrapper(fout, (FdwInfo *) dobj);
			break;
		case DO_FOREIGN_SERVER:
			dumpForeignServer(fout, (ForeignServerInfo *) dobj);
			break;
		case DO_DEFAULT_ACL:
			dumpDefaultACL(fout, (DefaultACLInfo *) dobj);
			break;
		case DO_BLOB:
			dumpBlob(fout, (BlobInfo *) dobj);
			break;
		case DO_BLOB_DATA:
			ArchiveEntry(fout, dobj->catId, dobj->dumpId,
						 dobj->name, NULL, NULL, "",
						 false, "BLOBS", SECTION_DATA,
						 "", "", NULL,
						 dobj->dependencies, dobj->nDeps,
						 dumpBlobs, NULL);
			break;
	}
}

/*
 * dumpNamespace
 *	  writes out to fout the queries to recreate a user-defined namespace
 */
static void
dumpNamespace(Archive *fout, NamespaceInfo *nspinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	char	   *qnspname;

	/* Skip if not to be dumped */
	if (!nspinfo->dobj.dump || dataOnly)
		return;

	/* don't dump dummy namespace from pre-7.3 source */
	if (strlen(nspinfo->dobj.name) == 0)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qnspname = strdup(fmtId(nspinfo->dobj.name));

	appendPQExpBuffer(delq, "DROP SCHEMA %s;\n", qnspname);

	appendPQExpBuffer(q, "CREATE SCHEMA %s;\n", qnspname);

	ArchiveEntry(fout, nspinfo->dobj.catId, nspinfo->dobj.dumpId,
				 nspinfo->dobj.name,
				 NULL, NULL,
				 nspinfo->rolname,
				 false, "SCHEMA", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 nspinfo->dobj.dependencies, nspinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Schema Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "SCHEMA %s", qnspname);
	dumpComment(fout, q->data,
				NULL, nspinfo->rolname,
				nspinfo->dobj.catId, 0, nspinfo->dobj.dumpId);

	dumpACL(fout, nspinfo->dobj.catId, nspinfo->dobj.dumpId, "SCHEMA",
			qnspname, NULL, nspinfo->dobj.name, NULL,
			nspinfo->rolname, nspinfo->nspacl);

	free(qnspname);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpType
 *	  writes out to fout the queries to recreate a user-defined type
 */
static void
dumpType(Archive *fout, TypeInfo *tyinfo)
{
	/* Skip if not to be dumped */
	if (!tyinfo->dobj.dump || dataOnly)
		return;

	/* Dump out in proper style */
	if (tyinfo->typtype == TYPTYPE_BASE)
		dumpBaseType(fout, tyinfo);
	else if (tyinfo->typtype == TYPTYPE_DOMAIN)
		dumpDomain(fout, tyinfo);
	else if (tyinfo->typtype == TYPTYPE_COMPOSITE)
		dumpCompositeType(fout, tyinfo);
	else if (tyinfo->typtype == TYPTYPE_ENUM)
		dumpEnumType(fout, tyinfo);
}

/*
 * dumpEnumType
 *	  writes out to fout the queries to recreate a user-defined enum type
 */
static void
dumpEnumType(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			num,
				i;
	Oid			enum_oid;
	char	   *label;

	/* Set proper schema search path so regproc references list correctly */
	selectSourceSchema(tyinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(query, "SELECT oid, enumlabel "
					  "FROM pg_catalog.pg_enum "
					  "WHERE enumtypid = '%u'"
					  "ORDER BY oid",
					  tyinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	num = PQntuples(res);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog.
	 * CASCADE shouldn't be required here as for normal types since the I/O
	 * functions are generic and do not get dropped.
	 */
	appendPQExpBuffer(delq, "DROP TYPE %s.",
					  fmtId(tyinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s;\n",
					  fmtId(tyinfo->dobj.name));

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(q, tyinfo->dobj.catId.oid);

	appendPQExpBuffer(q, "CREATE TYPE %s AS ENUM (",
					  fmtId(tyinfo->dobj.name));

	if (!binary_upgrade)
	{
		/* Labels with server-assigned oids */
		for (i = 0; i < num; i++)
		{
			label = PQgetvalue(res, i, PQfnumber(res, "enumlabel"));
			if (i > 0)
				appendPQExpBuffer(q, ",");
			appendPQExpBuffer(q, "\n    ");
			appendStringLiteralAH(q, label, fout);
		}
	}

	appendPQExpBuffer(q, "\n);\n");

	if (binary_upgrade)
	{
		/* Labels with dump-assigned (preserved) oids */
		for (i = 0; i < num; i++)
		{
			enum_oid = atooid(PQgetvalue(res, i, PQfnumber(res, "oid")));
			label = PQgetvalue(res, i, PQfnumber(res, "enumlabel"));

			if (i == 0)
				appendPQExpBuffer(q, "\n-- For binary upgrade, must preserve pg_enum oids\n");
			appendPQExpBuffer(q,
			 "SELECT binary_upgrade.add_pg_enum_label('%u'::pg_catalog.oid, "
							  "'%u'::pg_catalog.oid, ",
							  enum_oid, tyinfo->dobj.catId.oid);
			appendStringLiteralAH(q, label, fout);
			appendPQExpBuffer(q, ");\n");
		}
		appendPQExpBuffer(q, "\n");
	}

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "TYPE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 tyinfo->dobj.dependencies, tyinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Type Comments */
	resetPQExpBuffer(q);

	appendPQExpBuffer(q, "TYPE %s", fmtId(tyinfo->dobj.name));
	dumpComment(fout, q->data,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpBaseType
 *	  writes out to fout the queries to recreate a user-defined base type
 */
static void
dumpBaseType(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	char	   *typlen;
	char	   *typinput;
	char	   *typoutput;
	char	   *typreceive;
	char	   *typsend;
	char	   *typmodin;
	char	   *typmodout;
	char	   *typanalyze;
	Oid			typinputoid;
	Oid			typoutputoid;
	Oid			typreceiveoid;
	Oid			typsendoid;
	Oid			typmodinoid;
	Oid			typmodoutoid;
	Oid			typanalyzeoid;
	char	   *typcategory;
	char	   *typispreferred;
	char	   *typdelim;
	char	   *typbyval;
	char	   *typalign;
	char	   *typstorage;
	char	   *typdefault;
	bool		typdefault_is_literal = false;

	/* Set proper schema search path so regproc references list correctly */
	selectSourceSchema(tyinfo->dobj.namespace->dobj.name);

	/* Fetch type-specific details */
	if (fout->remoteVersion >= 80400)
	{
		appendPQExpBuffer(query, "SELECT typlen, "
						  "typinput, typoutput, typreceive, typsend, "
						  "typmodin, typmodout, typanalyze, "
						  "typinput::pg_catalog.oid AS typinputoid, "
						  "typoutput::pg_catalog.oid AS typoutputoid, "
						  "typreceive::pg_catalog.oid AS typreceiveoid, "
						  "typsend::pg_catalog.oid AS typsendoid, "
						  "typmodin::pg_catalog.oid AS typmodinoid, "
						  "typmodout::pg_catalog.oid AS typmodoutoid, "
						  "typanalyze::pg_catalog.oid AS typanalyzeoid, "
						  "typcategory, typispreferred, "
						  "typdelim, typbyval, typalign, typstorage, "
						  "pg_catalog.pg_get_expr(typdefaultbin, 0) AS typdefaultbin, typdefault "
						  "FROM pg_catalog.pg_type "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  tyinfo->dobj.catId.oid);
	}
	else if (fout->remoteVersion >= 80300)
	{
		/* Before 8.4, pg_get_expr does not allow 0 for its second arg */
		appendPQExpBuffer(query, "SELECT typlen, "
						  "typinput, typoutput, typreceive, typsend, "
						  "typmodin, typmodout, typanalyze, "
						  "typinput::pg_catalog.oid AS typinputoid, "
						  "typoutput::pg_catalog.oid AS typoutputoid, "
						  "typreceive::pg_catalog.oid AS typreceiveoid, "
						  "typsend::pg_catalog.oid AS typsendoid, "
						  "typmodin::pg_catalog.oid AS typmodinoid, "
						  "typmodout::pg_catalog.oid AS typmodoutoid, "
						  "typanalyze::pg_catalog.oid AS typanalyzeoid, "
						  "'U' AS typcategory, false AS typispreferred, "
						  "typdelim, typbyval, typalign, typstorage, "
						  "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, typdefault "
						  "FROM pg_catalog.pg_type "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  tyinfo->dobj.catId.oid);
	}
	else if (fout->remoteVersion >= 80000)
	{
		appendPQExpBuffer(query, "SELECT typlen, "
						  "typinput, typoutput, typreceive, typsend, "
						  "'-' AS typmodin, '-' AS typmodout, "
						  "typanalyze, "
						  "typinput::pg_catalog.oid AS typinputoid, "
						  "typoutput::pg_catalog.oid AS typoutputoid, "
						  "typreceive::pg_catalog.oid AS typreceiveoid, "
						  "typsend::pg_catalog.oid AS typsendoid, "
						  "0 AS typmodinoid, 0 AS typmodoutoid, "
						  "typanalyze::pg_catalog.oid AS typanalyzeoid, "
						  "'U' AS typcategory, false AS typispreferred, "
						  "typdelim, typbyval, typalign, typstorage, "
						  "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, typdefault "
						  "FROM pg_catalog.pg_type "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  tyinfo->dobj.catId.oid);
	}
	else if (fout->remoteVersion >= 70400)
	{
		appendPQExpBuffer(query, "SELECT typlen, "
						  "typinput, typoutput, typreceive, typsend, "
						  "'-' AS typmodin, '-' AS typmodout, "
						  "'-' AS typanalyze, "
						  "typinput::pg_catalog.oid AS typinputoid, "
						  "typoutput::pg_catalog.oid AS typoutputoid, "
						  "typreceive::pg_catalog.oid AS typreceiveoid, "
						  "typsend::pg_catalog.oid AS typsendoid, "
						  "0 AS typmodinoid, 0 AS typmodoutoid, "
						  "0 AS typanalyzeoid, "
						  "'U' AS typcategory, false AS typispreferred, "
						  "typdelim, typbyval, typalign, typstorage, "
						  "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, typdefault "
						  "FROM pg_catalog.pg_type "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  tyinfo->dobj.catId.oid);
	}
	else if (fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT typlen, "
						  "typinput, typoutput, "
						  "'-' AS typreceive, '-' AS typsend, "
						  "'-' AS typmodin, '-' AS typmodout, "
						  "'-' AS typanalyze, "
						  "typinput::pg_catalog.oid AS typinputoid, "
						  "typoutput::pg_catalog.oid AS typoutputoid, "
						  "0 AS typreceiveoid, 0 AS typsendoid, "
						  "0 AS typmodinoid, 0 AS typmodoutoid, "
						  "0 AS typanalyzeoid, "
						  "'U' AS typcategory, false AS typispreferred, "
						  "typdelim, typbyval, typalign, typstorage, "
						  "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, typdefault "
						  "FROM pg_catalog.pg_type "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  tyinfo->dobj.catId.oid);
	}
	else if (fout->remoteVersion >= 70200)
	{
		/*
		 * Note: although pre-7.3 catalogs contain typreceive and typsend,
		 * ignore them because they are not right.
		 */
		appendPQExpBuffer(query, "SELECT typlen, "
						  "typinput, typoutput, "
						  "'-' AS typreceive, '-' AS typsend, "
						  "'-' AS typmodin, '-' AS typmodout, "
						  "'-' AS typanalyze, "
						  "typinput::oid AS typinputoid, "
						  "typoutput::oid AS typoutputoid, "
						  "0 AS typreceiveoid, 0 AS typsendoid, "
						  "0 AS typmodinoid, 0 AS typmodoutoid, "
						  "0 AS typanalyzeoid, "
						  "'U' AS typcategory, false AS typispreferred, "
						  "typdelim, typbyval, typalign, typstorage, "
						  "NULL AS typdefaultbin, typdefault "
						  "FROM pg_type "
						  "WHERE oid = '%u'::oid",
						  tyinfo->dobj.catId.oid);
	}
	else if (fout->remoteVersion >= 70100)
	{
		/*
		 * Ignore pre-7.2 typdefault; the field exists but has an unusable
		 * representation.
		 */
		appendPQExpBuffer(query, "SELECT typlen, "
						  "typinput, typoutput, "
						  "'-' AS typreceive, '-' AS typsend, "
						  "'-' AS typmodin, '-' AS typmodout, "
						  "'-' AS typanalyze, "
						  "typinput::oid AS typinputoid, "
						  "typoutput::oid AS typoutputoid, "
						  "0 AS typreceiveoid, 0 AS typsendoid, "
						  "0 AS typmodinoid, 0 AS typmodoutoid, "
						  "0 AS typanalyzeoid, "
						  "'U' AS typcategory, false AS typispreferred, "
						  "typdelim, typbyval, typalign, typstorage, "
						  "NULL AS typdefaultbin, NULL AS typdefault "
						  "FROM pg_type "
						  "WHERE oid = '%u'::oid",
						  tyinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT typlen, "
						  "typinput, typoutput, "
						  "'-' AS typreceive, '-' AS typsend, "
						  "'-' AS typmodin, '-' AS typmodout, "
						  "'-' AS typanalyze, "
						  "typinput::oid AS typinputoid, "
						  "typoutput::oid AS typoutputoid, "
						  "0 AS typreceiveoid, 0 AS typsendoid, "
						  "0 AS typmodinoid, 0 AS typmodoutoid, "
						  "0 AS typanalyzeoid, "
						  "'U' AS typcategory, false AS typispreferred, "
						  "typdelim, typbyval, typalign, "
						  "'p'::char AS typstorage, "
						  "NULL AS typdefaultbin, NULL AS typdefault "
						  "FROM pg_type "
						  "WHERE oid = '%u'::oid",
						  tyinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	typlen = PQgetvalue(res, 0, PQfnumber(res, "typlen"));
	typinput = PQgetvalue(res, 0, PQfnumber(res, "typinput"));
	typoutput = PQgetvalue(res, 0, PQfnumber(res, "typoutput"));
	typreceive = PQgetvalue(res, 0, PQfnumber(res, "typreceive"));
	typsend = PQgetvalue(res, 0, PQfnumber(res, "typsend"));
	typmodin = PQgetvalue(res, 0, PQfnumber(res, "typmodin"));
	typmodout = PQgetvalue(res, 0, PQfnumber(res, "typmodout"));
	typanalyze = PQgetvalue(res, 0, PQfnumber(res, "typanalyze"));
	typinputoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typinputoid")));
	typoutputoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typoutputoid")));
	typreceiveoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typreceiveoid")));
	typsendoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typsendoid")));
	typmodinoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typmodinoid")));
	typmodoutoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typmodoutoid")));
	typanalyzeoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typanalyzeoid")));
	typcategory = PQgetvalue(res, 0, PQfnumber(res, "typcategory"));
	typispreferred = PQgetvalue(res, 0, PQfnumber(res, "typispreferred"));
	typdelim = PQgetvalue(res, 0, PQfnumber(res, "typdelim"));
	typbyval = PQgetvalue(res, 0, PQfnumber(res, "typbyval"));
	typalign = PQgetvalue(res, 0, PQfnumber(res, "typalign"));
	typstorage = PQgetvalue(res, 0, PQfnumber(res, "typstorage"));
	if (!PQgetisnull(res, 0, PQfnumber(res, "typdefaultbin")))
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefaultbin"));
	else if (!PQgetisnull(res, 0, PQfnumber(res, "typdefault")))
	{
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefault"));
		typdefault_is_literal = true;	/* it needs quotes */
	}
	else
		typdefault = NULL;

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog.
	 * The reason we include CASCADE is that the circular dependency between
	 * the type and its I/O functions makes it impossible to drop the type any
	 * other way.
	 */
	appendPQExpBuffer(delq, "DROP TYPE %s.",
					  fmtId(tyinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s CASCADE;\n",
					  fmtId(tyinfo->dobj.name));

	/* We might already have a shell type, but setting pg_type_oid is harmless */
	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(q, tyinfo->dobj.catId.oid);

	appendPQExpBuffer(q,
					  "CREATE TYPE %s (\n"
					  "    INTERNALLENGTH = %s",
					  fmtId(tyinfo->dobj.name),
					  (strcmp(typlen, "-1") == 0) ? "variable" : typlen);

	if (fout->remoteVersion >= 70300)
	{
		/* regproc result is correctly quoted as of 7.3 */
		appendPQExpBuffer(q, ",\n    INPUT = %s", typinput);
		appendPQExpBuffer(q, ",\n    OUTPUT = %s", typoutput);
		if (OidIsValid(typreceiveoid))
			appendPQExpBuffer(q, ",\n    RECEIVE = %s", typreceive);
		if (OidIsValid(typsendoid))
			appendPQExpBuffer(q, ",\n    SEND = %s", typsend);
		if (OidIsValid(typmodinoid))
			appendPQExpBuffer(q, ",\n    TYPMOD_IN = %s", typmodin);
		if (OidIsValid(typmodoutoid))
			appendPQExpBuffer(q, ",\n    TYPMOD_OUT = %s", typmodout);
		if (OidIsValid(typanalyzeoid))
			appendPQExpBuffer(q, ",\n    ANALYZE = %s", typanalyze);
	}
	else
	{
		/* regproc delivers an unquoted name before 7.3 */
		/* cannot combine these because fmtId uses static result area */
		appendPQExpBuffer(q, ",\n    INPUT = %s", fmtId(typinput));
		appendPQExpBuffer(q, ",\n    OUTPUT = %s", fmtId(typoutput));
		/* receive/send/typmodin/typmodout/analyze need not be printed */
	}

	if (typdefault != NULL)
	{
		appendPQExpBuffer(q, ",\n    DEFAULT = ");
		if (typdefault_is_literal)
			appendStringLiteralAH(q, typdefault, fout);
		else
			appendPQExpBufferStr(q, typdefault);
	}

	if (OidIsValid(tyinfo->typelem))
	{
		char	   *elemType;

		/* reselect schema in case changed by function dump */
		selectSourceSchema(tyinfo->dobj.namespace->dobj.name);
		elemType = getFormattedTypeName(tyinfo->typelem, zeroAsOpaque);
		appendPQExpBuffer(q, ",\n    ELEMENT = %s", elemType);
		free(elemType);
	}

	if (strcmp(typcategory, "U") != 0)
	{
		appendPQExpBuffer(q, ",\n    CATEGORY = ");
		appendStringLiteralAH(q, typcategory, fout);
	}

	if (strcmp(typispreferred, "t") == 0)
		appendPQExpBuffer(q, ",\n    PREFERRED = true");

	if (typdelim && strcmp(typdelim, ",") != 0)
	{
		appendPQExpBuffer(q, ",\n    DELIMITER = ");
		appendStringLiteralAH(q, typdelim, fout);
	}

	if (strcmp(typalign, "c") == 0)
		appendPQExpBuffer(q, ",\n    ALIGNMENT = char");
	else if (strcmp(typalign, "s") == 0)
		appendPQExpBuffer(q, ",\n    ALIGNMENT = int2");
	else if (strcmp(typalign, "i") == 0)
		appendPQExpBuffer(q, ",\n    ALIGNMENT = int4");
	else if (strcmp(typalign, "d") == 0)
		appendPQExpBuffer(q, ",\n    ALIGNMENT = double");

	if (strcmp(typstorage, "p") == 0)
		appendPQExpBuffer(q, ",\n    STORAGE = plain");
	else if (strcmp(typstorage, "e") == 0)
		appendPQExpBuffer(q, ",\n    STORAGE = external");
	else if (strcmp(typstorage, "x") == 0)
		appendPQExpBuffer(q, ",\n    STORAGE = extended");
	else if (strcmp(typstorage, "m") == 0)
		appendPQExpBuffer(q, ",\n    STORAGE = main");

	if (strcmp(typbyval, "t") == 0)
		appendPQExpBuffer(q, ",\n    PASSEDBYVALUE");

	appendPQExpBuffer(q, "\n);\n");

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "TYPE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 tyinfo->dobj.dependencies, tyinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Type Comments */
	resetPQExpBuffer(q);

	appendPQExpBuffer(q, "TYPE %s", fmtId(tyinfo->dobj.name));
	dumpComment(fout, q->data,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpDomain
 *	  writes out to fout the queries to recreate a user-defined domain
 */
static void
dumpDomain(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			i;
	char	   *typnotnull;
	char	   *typdefn;
	char	   *typdefault;
	bool		typdefault_is_literal = false;

	/* Set proper schema search path so type references list correctly */
	selectSourceSchema(tyinfo->dobj.namespace->dobj.name);

	/* Fetch domain specific details */
	/* We assume here that remoteVersion must be at least 70300 */
	appendPQExpBuffer(query, "SELECT typnotnull, "
				"pg_catalog.format_type(typbasetype, typtypmod) AS typdefn, "
					  "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, typdefault "
					  "FROM pg_catalog.pg_type "
					  "WHERE oid = '%u'::pg_catalog.oid",
					  tyinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	typnotnull = PQgetvalue(res, 0, PQfnumber(res, "typnotnull"));
	typdefn = PQgetvalue(res, 0, PQfnumber(res, "typdefn"));
	if (!PQgetisnull(res, 0, PQfnumber(res, "typdefaultbin")))
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefaultbin"));
	else if (!PQgetisnull(res, 0, PQfnumber(res, "typdefault")))
	{
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefault"));
		typdefault_is_literal = true;	/* it needs quotes */
	}
	else
		typdefault = NULL;

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(q, tyinfo->dobj.catId.oid);

	appendPQExpBuffer(q,
					  "CREATE DOMAIN %s AS %s",
					  fmtId(tyinfo->dobj.name),
					  typdefn);

	if (typnotnull[0] == 't')
		appendPQExpBuffer(q, " NOT NULL");

	if (typdefault != NULL)
	{
		appendPQExpBuffer(q, " DEFAULT ");
		if (typdefault_is_literal)
			appendStringLiteralAH(q, typdefault, fout);
		else
			appendPQExpBufferStr(q, typdefault);
	}

	PQclear(res);

	/*
	 * Add any CHECK constraints for the domain
	 */
	for (i = 0; i < tyinfo->nDomChecks; i++)
	{
		ConstraintInfo *domcheck = &(tyinfo->domChecks[i]);

		if (!domcheck->separate)
			appendPQExpBuffer(q, "\n\tCONSTRAINT %s %s",
							  fmtId(domcheck->dobj.name), domcheck->condef);
	}

	appendPQExpBuffer(q, ";\n");

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP DOMAIN %s.",
					  fmtId(tyinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s;\n",
					  fmtId(tyinfo->dobj.name));

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "DOMAIN", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 tyinfo->dobj.dependencies, tyinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Domain Comments */
	resetPQExpBuffer(q);

	appendPQExpBuffer(q, "DOMAIN %s", fmtId(tyinfo->dobj.name));
	dumpComment(fout, q->data,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpCompositeType
 *	  writes out to fout the queries to recreate a user-defined stand-alone
 *	  composite type
 */
static void
dumpCompositeType(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			i_attname;
	int			i_atttypdefn;
	int			i_typrelid;
	int			i;

	/* Set proper schema search path so type references list correctly */
	selectSourceSchema(tyinfo->dobj.namespace->dobj.name);

	/* Fetch type specific details */
	/* We assume here that remoteVersion must be at least 70300 */

	appendPQExpBuffer(query, "SELECT a.attname, "
			"pg_catalog.format_type(a.atttypid, a.atttypmod) AS atttypdefn, "
					  "typrelid "
					  "FROM pg_catalog.pg_type t, pg_catalog.pg_attribute a "
					  "WHERE t.oid = '%u'::pg_catalog.oid "
					  "AND a.attrelid = t.typrelid "
					  "AND NOT a.attisdropped "
					  "ORDER BY a.attnum ",
					  tyinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting at least a single result */
	ntups = PQntuples(res);
	if (ntups < 1)
	{
		write_msg(NULL, "query returned no rows: %s\n", query->data);
		exit_nicely();
	}

	i_attname = PQfnumber(res, "attname");
	i_atttypdefn = PQfnumber(res, "atttypdefn");
	i_typrelid = PQfnumber(res, "typrelid");

	if (binary_upgrade)
	{
		Oid			typrelid = atooid(PQgetvalue(res, 0, i_typrelid));

		binary_upgrade_set_type_oids_by_type_oid(q, tyinfo->dobj.catId.oid);
		binary_upgrade_set_relfilenodes(q, typrelid, false);
	}

	appendPQExpBuffer(q, "CREATE TYPE %s AS (",
					  fmtId(tyinfo->dobj.name));

	for (i = 0; i < ntups; i++)
	{
		char	   *attname;
		char	   *atttypdefn;

		attname = PQgetvalue(res, i, i_attname);
		atttypdefn = PQgetvalue(res, i, i_atttypdefn);

		appendPQExpBuffer(q, "\n\t%s %s", fmtId(attname), atttypdefn);
		if (i < ntups - 1)
			appendPQExpBuffer(q, ",");
	}
	appendPQExpBuffer(q, "\n);\n");

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TYPE %s.",
					  fmtId(tyinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s;\n",
					  fmtId(tyinfo->dobj.name));

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "TYPE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 tyinfo->dobj.dependencies, tyinfo->dobj.nDeps,
				 NULL, NULL);


	/* Dump Type Comments */
	resetPQExpBuffer(q);

	appendPQExpBuffer(q, "TYPE %s", fmtId(tyinfo->dobj.name));
	dumpComment(fout, q->data,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);

	/* Dump any per-column comments */
	dumpCompositeTypeColComments(fout, tyinfo);
}

/*
 * dumpCompositeTypeColComments
 *	  writes out to fout the queries to recreate comments on the columns of
 *	  a user-defined stand-alone composite type
 */
static void
dumpCompositeTypeColComments(Archive *fout, TypeInfo *tyinfo)
{
	CommentItem *comments;
	int			ncomments;
	PGresult   *res;
	PQExpBuffer query;
	PQExpBuffer target;
	Oid			pgClassOid;
	int			i;
	int			ntups;
	int			i_attname;
	int			i_attnum;

	query = createPQExpBuffer();

	/* We assume here that remoteVersion must be at least 70300 */
	appendPQExpBuffer(query,
					  "SELECT c.tableoid, a.attname, a.attnum "
					  "FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a "
					  "WHERE c.oid = '%u' AND c.oid = a.attrelid "
					  "  AND NOT a.attisdropped "
					  "ORDER BY a.attnum ",
					  tyinfo->typrelid);

	/* Fetch column attnames */
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting at least a single result */
	ntups = PQntuples(res);
	if (ntups < 1)
	{
		write_msg(NULL, "query returned no rows: %s\n", query->data);
		exit_nicely();
	}

	pgClassOid = atooid(PQgetvalue(res, 0, PQfnumber(res, "tableoid")));

	/* Search for comments associated with type's pg_class OID */
	ncomments = findComments(fout,
							 pgClassOid,
							 tyinfo->typrelid,
							 &comments);

	/* If no comments exist, we're done */
	if (ncomments <= 0)
	{
		PQclear(res);
		destroyPQExpBuffer(query);
		return;
	}

	/* Build COMMENT ON statements */
	target = createPQExpBuffer();

	i_attnum = PQfnumber(res, "attnum");
	i_attname = PQfnumber(res, "attname");
	while (ncomments > 0)
	{
		const char *attname;

		attname = NULL;
		for (i = 0; i < ntups; i++)
		{
			if (atoi(PQgetvalue(res, i, i_attnum)) == comments->objsubid)
			{
				attname = PQgetvalue(res, i, i_attname);
				break;
			}
		}
		if (attname)			/* just in case we don't find it */
		{
			const char *descr = comments->descr;

			resetPQExpBuffer(target);
			appendPQExpBuffer(target, "COLUMN %s.",
							  fmtId(tyinfo->dobj.name));
			appendPQExpBuffer(target, "%s",
							  fmtId(attname));

			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "COMMENT ON %s IS ", target->data);
			appendStringLiteralAH(query, descr, fout);
			appendPQExpBuffer(query, ";\n");

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 target->data,
						 tyinfo->dobj.namespace->dobj.name,
						 NULL, tyinfo->rolname,
						 false, "COMMENT", SECTION_NONE,
						 query->data, "", NULL,
						 &(tyinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}

		comments++;
		ncomments--;
	}

	PQclear(res);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(target);
}

/*
 * dumpShellType
 *	  writes out to fout the queries to create a shell type
 *
 * We dump a shell definition in advance of the I/O functions for the type.
 */
static void
dumpShellType(Archive *fout, ShellTypeInfo *stinfo)
{
	PQExpBuffer q;

	/* Skip if not to be dumped */
	if (!stinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();

	/*
	 * Note the lack of a DROP command for the shell type; any required DROP
	 * is driven off the base type entry, instead.	This interacts with
	 * _printTocEntry()'s use of the presence of a DROP command to decide
	 * whether an entry needs an ALTER OWNER command.  We don't want to alter
	 * the shell type's owner immediately on creation; that should happen only
	 * after it's filled in, otherwise the backend complains.
	 */

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(q,
										   stinfo->baseType->dobj.catId.oid);

	appendPQExpBuffer(q, "CREATE TYPE %s;\n",
					  fmtId(stinfo->dobj.name));

	ArchiveEntry(fout, stinfo->dobj.catId, stinfo->dobj.dumpId,
				 stinfo->dobj.name,
				 stinfo->dobj.namespace->dobj.name,
				 NULL,
				 stinfo->baseType->rolname, false,
				 "SHELL TYPE", SECTION_PRE_DATA,
				 q->data, "", NULL,
				 stinfo->dobj.dependencies, stinfo->dobj.nDeps,
				 NULL, NULL);

	destroyPQExpBuffer(q);
}

/*
 * Determine whether we want to dump definitions for procedural languages.
 * Since the languages themselves don't have schemas, we can't rely on
 * the normal schema-based selection mechanism.  We choose to dump them
 * whenever neither --schema nor --table was given.  (Before 8.1, we used
 * the dump flag of the PL's call handler function, but in 8.1 this will
 * probably always be false since call handlers are created in pg_catalog.)
 *
 * For some backwards compatibility with the older behavior, we forcibly
 * dump a PL if its handler function (and validator if any) are in a
 * dumpable namespace.	That case is not checked here.
 */
static bool
shouldDumpProcLangs(void)
{
	if (!include_everything)
		return false;
	/* And they're schema not data */
	if (dataOnly)
		return false;
	return true;
}

/*
 * dumpProcLang
 *		  writes out to fout the queries to recreate a user-defined
 *		  procedural language
 */
static void
dumpProcLang(Archive *fout, ProcLangInfo *plang)
{
	PQExpBuffer defqry;
	PQExpBuffer delqry;
	bool		useParams;
	char	   *qlanname;
	char	   *lanschema;
	FuncInfo   *funcInfo;
	FuncInfo   *inlineInfo = NULL;
	FuncInfo   *validatorInfo = NULL;

	if (dataOnly)
		return;

	/*
	 * Try to find the support function(s).  It is not an error if we don't
	 * find them --- if the functions are in the pg_catalog schema, as is
	 * standard in 8.1 and up, then we won't have loaded them. (In this case
	 * we will emit a parameterless CREATE LANGUAGE command, which will
	 * require PL template knowledge in the backend to reload.)
	 */

	funcInfo = findFuncByOid(plang->lanplcallfoid);
	if (funcInfo != NULL && !funcInfo->dobj.dump)
		funcInfo = NULL;		/* treat not-dumped same as not-found */

	if (OidIsValid(plang->laninline))
	{
		inlineInfo = findFuncByOid(plang->laninline);
		if (inlineInfo != NULL && !inlineInfo->dobj.dump)
			inlineInfo = NULL;
	}

	if (OidIsValid(plang->lanvalidator))
	{
		validatorInfo = findFuncByOid(plang->lanvalidator);
		if (validatorInfo != NULL && !validatorInfo->dobj.dump)
			validatorInfo = NULL;
	}

	/*
	 * If the functions are dumpable then emit a traditional CREATE LANGUAGE
	 * with parameters.  Otherwise, dump only if shouldDumpProcLangs() says to
	 * dump it.
	 */
	useParams = (funcInfo != NULL &&
				 (inlineInfo != NULL || !OidIsValid(plang->laninline)) &&
				 (validatorInfo != NULL || !OidIsValid(plang->lanvalidator)));

	if (!useParams && !shouldDumpProcLangs())
		return;

	defqry = createPQExpBuffer();
	delqry = createPQExpBuffer();

	qlanname = strdup(fmtId(plang->dobj.name));

	/*
	 * If dumping a HANDLER clause, treat the language as being in the handler
	 * function's schema; this avoids cluttering the HANDLER clause. Otherwise
	 * it doesn't really have a schema.
	 */
	if (useParams)
		lanschema = funcInfo->dobj.namespace->dobj.name;
	else
		lanschema = NULL;

	appendPQExpBuffer(delqry, "DROP PROCEDURAL LANGUAGE %s;\n",
					  qlanname);

	if (useParams)
	{
		appendPQExpBuffer(defqry, "CREATE %sPROCEDURAL LANGUAGE %s",
						  plang->lanpltrusted ? "TRUSTED " : "",
						  qlanname);
		appendPQExpBuffer(defqry, " HANDLER %s",
						  fmtId(funcInfo->dobj.name));
		if (OidIsValid(plang->laninline))
		{
			appendPQExpBuffer(defqry, " INLINE ");
			/* Cope with possibility that inline is in different schema */
			if (inlineInfo->dobj.namespace != funcInfo->dobj.namespace)
				appendPQExpBuffer(defqry, "%s.",
							   fmtId(inlineInfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(defqry, "%s",
							  fmtId(inlineInfo->dobj.name));
		}
		if (OidIsValid(plang->lanvalidator))
		{
			appendPQExpBuffer(defqry, " VALIDATOR ");
			/* Cope with possibility that validator is in different schema */
			if (validatorInfo->dobj.namespace != funcInfo->dobj.namespace)
				appendPQExpBuffer(defqry, "%s.",
							fmtId(validatorInfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(defqry, "%s",
							  fmtId(validatorInfo->dobj.name));
		}
	}
	else
	{
		/*
		 * If not dumping parameters, then use CREATE OR REPLACE so that the
		 * command will not fail if the language is preinstalled in the target
		 * database.  We restrict the use of REPLACE to this case so as to
		 * eliminate the risk of replacing a language with incompatible
		 * parameter settings: this command will only succeed at all if there
		 * is a pg_pltemplate entry, and if there is one, the existing entry
		 * must match it too.
		 */
		appendPQExpBuffer(defqry, "CREATE OR REPLACE PROCEDURAL LANGUAGE %s",
						  qlanname);
	}
	appendPQExpBuffer(defqry, ";\n");

	ArchiveEntry(fout, plang->dobj.catId, plang->dobj.dumpId,
				 plang->dobj.name,
				 lanschema, NULL, plang->lanowner,
				 false, "PROCEDURAL LANGUAGE", SECTION_PRE_DATA,
				 defqry->data, delqry->data, NULL,
				 plang->dobj.dependencies, plang->dobj.nDeps,
				 NULL, NULL);

	/* Dump Proc Lang Comments */
	resetPQExpBuffer(defqry);
	appendPQExpBuffer(defqry, "LANGUAGE %s", qlanname);
	dumpComment(fout, defqry->data,
				NULL, "",
				plang->dobj.catId, 0, plang->dobj.dumpId);

	if (plang->lanpltrusted)
		dumpACL(fout, plang->dobj.catId, plang->dobj.dumpId, "LANGUAGE",
				qlanname, NULL, plang->dobj.name,
				lanschema,
				plang->lanowner, plang->lanacl);

	free(qlanname);

	destroyPQExpBuffer(defqry);
	destroyPQExpBuffer(delqry);
}

/*
 * format_function_arguments: generate function name and argument list
 *
 * This is used when we can rely on pg_get_function_arguments to format
 * the argument list.
 */
static char *
format_function_arguments(FuncInfo *finfo, char *funcargs)
{
	PQExpBufferData fn;

	initPQExpBuffer(&fn);
	appendPQExpBuffer(&fn, "%s(%s)", fmtId(finfo->dobj.name), funcargs);
	return fn.data;
}

/*
 * format_function_arguments_old: generate function name and argument list
 *
 * The argument type names are qualified if needed.  The function name
 * is never qualified.
 *
 * This is used only with pre-8.4 servers, so we aren't expecting to see
 * VARIADIC or TABLE arguments, nor are there any defaults for arguments.
 *
 * Any or all of allargtypes, argmodes, argnames may be NULL.
 */
static char *
format_function_arguments_old(FuncInfo *finfo, int nallargs,
							  char **allargtypes,
							  char **argmodes,
							  char **argnames)
{
	PQExpBufferData fn;
	int			j;

	initPQExpBuffer(&fn);
	appendPQExpBuffer(&fn, "%s(", fmtId(finfo->dobj.name));
	for (j = 0; j < nallargs; j++)
	{
		Oid			typid;
		char	   *typname;
		const char *argmode;
		const char *argname;

		typid = allargtypes ? atooid(allargtypes[j]) : finfo->argtypes[j];
		typname = getFormattedTypeName(typid, zeroAsOpaque);

		if (argmodes)
		{
			switch (argmodes[j][0])
			{
				case PROARGMODE_IN:
					argmode = "";
					break;
				case PROARGMODE_OUT:
					argmode = "OUT ";
					break;
				case PROARGMODE_INOUT:
					argmode = "INOUT ";
					break;
				default:
					write_msg(NULL, "WARNING: bogus value in proargmodes array\n");
					argmode = "";
					break;
			}
		}
		else
			argmode = "";

		argname = argnames ? argnames[j] : (char *) NULL;
		if (argname && argname[0] == '\0')
			argname = NULL;

		appendPQExpBuffer(&fn, "%s%s%s%s%s",
						  (j > 0) ? ", " : "",
						  argmode,
						  argname ? fmtId(argname) : "",
						  argname ? " " : "",
						  typname);
		free(typname);
	}
	appendPQExpBuffer(&fn, ")");
	return fn.data;
}

/*
 * format_function_signature: generate function name and argument list
 *
 * This is like format_function_arguments_old except that only a minimal
 * list of input argument types is generated; this is sufficient to
 * reference the function, but not to define it.
 *
 * If honor_quotes is false then the function name is never quoted.
 * This is appropriate for use in TOC tags, but not in SQL commands.
 */
static char *
format_function_signature(FuncInfo *finfo, bool honor_quotes)
{
	PQExpBufferData fn;
	int			j;

	initPQExpBuffer(&fn);
	if (honor_quotes)
		appendPQExpBuffer(&fn, "%s(", fmtId(finfo->dobj.name));
	else
		appendPQExpBuffer(&fn, "%s(", finfo->dobj.name);
	for (j = 0; j < finfo->nargs; j++)
	{
		char	   *typname;

		typname = getFormattedTypeName(finfo->argtypes[j], zeroAsOpaque);

		appendPQExpBuffer(&fn, "%s%s",
						  (j > 0) ? ", " : "",
						  typname);
		free(typname);
	}
	appendPQExpBuffer(&fn, ")");
	return fn.data;
}


/*
 * dumpFunc:
 *	  dump out one function
 */
static void
dumpFunc(Archive *fout, FuncInfo *finfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delqry;
	PQExpBuffer asPart;
	PGresult   *res;
	char	   *funcsig;		/* identity signature */
	char	   *funcfullsig;	/* full signature */
	char	   *funcsig_tag;
	int			ntups;
	char	   *proretset;
	char	   *prosrc;
	char	   *probin;
	char	   *funcargs;
	char	   *funciargs;
	char	   *funcresult;
	char	   *proallargtypes;
	char	   *proargmodes;
	char	   *proargnames;
	char	   *proiswindow;
	char	   *provolatile;
	char	   *proisstrict;
	char	   *prosecdef;
	char	   *proconfig;
	char	   *procost;
	char	   *prorows;
	char	   *lanname;
	char	   *rettypename;
	int			nallargs;
	char	  **allargtypes = NULL;
	char	  **argmodes = NULL;
	char	  **argnames = NULL;
	char	  **configitems = NULL;
	int			nconfigitems = 0;
	int			i;

	/* Skip if not to be dumped */
	if (!finfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delqry = createPQExpBuffer();
	asPart = createPQExpBuffer();

	/* Set proper schema search path so type references list correctly */
	selectSourceSchema(finfo->dobj.namespace->dobj.name);

	/* Fetch function-specific details */
	if (g_fout->remoteVersion >= 80400)
	{
		/*
		 * In 8.4 and up we rely on pg_get_function_arguments and
		 * pg_get_function_result instead of examining proallargtypes etc.
		 */
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
					"pg_catalog.pg_get_function_arguments(oid) AS funcargs, "
		  "pg_catalog.pg_get_function_identity_arguments(oid) AS funciargs, "
					 "pg_catalog.pg_get_function_result(oid) AS funcresult, "
						  "proiswindow, provolatile, proisstrict, prosecdef, "
						  "proconfig, procost, prorows, "
						  "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
						  "FROM pg_catalog.pg_proc "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  finfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 80300)
	{
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
						  "proallargtypes, proargmodes, proargnames, "
						  "false AS proiswindow, "
						  "provolatile, proisstrict, prosecdef, "
						  "proconfig, procost, prorows, "
						  "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
						  "FROM pg_catalog.pg_proc "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  finfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 80100)
	{
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
						  "proallargtypes, proargmodes, proargnames, "
						  "false AS proiswindow, "
						  "provolatile, proisstrict, prosecdef, "
						  "null AS proconfig, 0 AS procost, 0 AS prorows, "
						  "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
						  "FROM pg_catalog.pg_proc "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  finfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 80000)
	{
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
						  "null AS proallargtypes, "
						  "null AS proargmodes, "
						  "proargnames, "
						  "false AS proiswindow, "
						  "provolatile, proisstrict, prosecdef, "
						  "null AS proconfig, 0 AS procost, 0 AS prorows, "
						  "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
						  "FROM pg_catalog.pg_proc "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  finfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
						  "null AS proallargtypes, "
						  "null AS proargmodes, "
						  "null AS proargnames, "
						  "false AS proiswindow, "
						  "provolatile, proisstrict, prosecdef, "
						  "null AS proconfig, 0 AS procost, 0 AS prorows, "
						  "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname "
						  "FROM pg_catalog.pg_proc "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  finfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
						  "null AS proallargtypes, "
						  "null AS proargmodes, "
						  "null AS proargnames, "
						  "false AS proiswindow, "
			 "case when proiscachable then 'i' else 'v' end AS provolatile, "
						  "proisstrict, "
						  "false AS prosecdef, "
						  "null AS proconfig, 0 AS procost, 0 AS prorows, "
		  "(SELECT lanname FROM pg_language WHERE oid = prolang) AS lanname "
						  "FROM pg_proc "
						  "WHERE oid = '%u'::oid",
						  finfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
						  "null AS proallargtypes, "
						  "null AS proargmodes, "
						  "null AS proargnames, "
						  "false AS proiswindow, "
			 "CASE WHEN proiscachable THEN 'i' ELSE 'v' END AS provolatile, "
						  "false AS proisstrict, "
						  "false AS prosecdef, "
						  "NULL AS proconfig, 0 AS procost, 0 AS prorows, "
		  "(SELECT lanname FROM pg_language WHERE oid = prolang) AS lanname "
						  "FROM pg_proc "
						  "WHERE oid = '%u'::oid",
						  finfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	proretset = PQgetvalue(res, 0, PQfnumber(res, "proretset"));
	prosrc = PQgetvalue(res, 0, PQfnumber(res, "prosrc"));
	probin = PQgetvalue(res, 0, PQfnumber(res, "probin"));
	if (g_fout->remoteVersion >= 80400)
	{
		funcargs = PQgetvalue(res, 0, PQfnumber(res, "funcargs"));
		funciargs = PQgetvalue(res, 0, PQfnumber(res, "funciargs"));
		funcresult = PQgetvalue(res, 0, PQfnumber(res, "funcresult"));
		proallargtypes = proargmodes = proargnames = NULL;
	}
	else
	{
		proallargtypes = PQgetvalue(res, 0, PQfnumber(res, "proallargtypes"));
		proargmodes = PQgetvalue(res, 0, PQfnumber(res, "proargmodes"));
		proargnames = PQgetvalue(res, 0, PQfnumber(res, "proargnames"));
		funcargs = funciargs = funcresult = NULL;
	}
	proiswindow = PQgetvalue(res, 0, PQfnumber(res, "proiswindow"));
	provolatile = PQgetvalue(res, 0, PQfnumber(res, "provolatile"));
	proisstrict = PQgetvalue(res, 0, PQfnumber(res, "proisstrict"));
	prosecdef = PQgetvalue(res, 0, PQfnumber(res, "prosecdef"));
	proconfig = PQgetvalue(res, 0, PQfnumber(res, "proconfig"));
	procost = PQgetvalue(res, 0, PQfnumber(res, "procost"));
	prorows = PQgetvalue(res, 0, PQfnumber(res, "prorows"));
	lanname = PQgetvalue(res, 0, PQfnumber(res, "lanname"));

	/*
	 * See backend/commands/functioncmds.c for details of how the 'AS' clause
	 * is used.  In 8.4 and up, an unused probin is NULL (here ""); previous
	 * versions would set it to "-".  There are no known cases in which prosrc
	 * is unused, so the tests below for "-" are probably useless.
	 */
	if (probin[0] != '\0' && strcmp(probin, "-") != 0)
	{
		appendPQExpBuffer(asPart, "AS ");
		appendStringLiteralAH(asPart, probin, fout);
		if (strcmp(prosrc, "-") != 0)
		{
			appendPQExpBuffer(asPart, ", ");

			/*
			 * where we have bin, use dollar quoting if allowed and src
			 * contains quote or backslash; else use regular quoting.
			 */
			if (disable_dollar_quoting ||
			  (strchr(prosrc, '\'') == NULL && strchr(prosrc, '\\') == NULL))
				appendStringLiteralAH(asPart, prosrc, fout);
			else
				appendStringLiteralDQ(asPart, prosrc, NULL);
		}
	}
	else
	{
		if (strcmp(prosrc, "-") != 0)
		{
			appendPQExpBuffer(asPart, "AS ");
			/* with no bin, dollar quote src unconditionally if allowed */
			if (disable_dollar_quoting)
				appendStringLiteralAH(asPart, prosrc, fout);
			else
				appendStringLiteralDQ(asPart, prosrc, NULL);
		}
	}

	nallargs = finfo->nargs;	/* unless we learn different from allargs */

	if (proallargtypes && *proallargtypes)
	{
		int			nitems = 0;

		if (!parsePGArray(proallargtypes, &allargtypes, &nitems) ||
			nitems < finfo->nargs)
		{
			write_msg(NULL, "WARNING: could not parse proallargtypes array\n");
			if (allargtypes)
				free(allargtypes);
			allargtypes = NULL;
		}
		else
			nallargs = nitems;
	}

	if (proargmodes && *proargmodes)
	{
		int			nitems = 0;

		if (!parsePGArray(proargmodes, &argmodes, &nitems) ||
			nitems != nallargs)
		{
			write_msg(NULL, "WARNING: could not parse proargmodes array\n");
			if (argmodes)
				free(argmodes);
			argmodes = NULL;
		}
	}

	if (proargnames && *proargnames)
	{
		int			nitems = 0;

		if (!parsePGArray(proargnames, &argnames, &nitems) ||
			nitems != nallargs)
		{
			write_msg(NULL, "WARNING: could not parse proargnames array\n");
			if (argnames)
				free(argnames);
			argnames = NULL;
		}
	}

	if (proconfig && *proconfig)
	{
		if (!parsePGArray(proconfig, &configitems, &nconfigitems))
		{
			write_msg(NULL, "WARNING: could not parse proconfig array\n");
			if (configitems)
				free(configitems);
			configitems = NULL;
			nconfigitems = 0;
		}
	}

	if (funcargs)
	{
		/* 8.4 or later; we rely on server-side code for most of the work */
		funcfullsig = format_function_arguments(finfo, funcargs);
		funcsig = format_function_arguments(finfo, funciargs);
	}
	else
	{
		/* pre-8.4, do it ourselves */
		funcsig = format_function_arguments_old(finfo, nallargs, allargtypes,
												argmodes, argnames);
		funcfullsig = funcsig;
	}

	funcsig_tag = format_function_signature(finfo, false);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delqry, "DROP FUNCTION %s.%s;\n",
					  fmtId(finfo->dobj.namespace->dobj.name),
					  funcsig);

	appendPQExpBuffer(q, "CREATE FUNCTION %s ", funcfullsig);
	if (funcresult)
		appendPQExpBuffer(q, "RETURNS %s", funcresult);
	else
	{
		rettypename = getFormattedTypeName(finfo->prorettype, zeroAsOpaque);
		appendPQExpBuffer(q, "RETURNS %s%s",
						  (proretset[0] == 't') ? "SETOF " : "",
						  rettypename);
		free(rettypename);
	}

	appendPQExpBuffer(q, "\n    LANGUAGE %s", fmtId(lanname));

	if (proiswindow[0] == 't')
		appendPQExpBuffer(q, " WINDOW");

	if (provolatile[0] != PROVOLATILE_VOLATILE)
	{
		if (provolatile[0] == PROVOLATILE_IMMUTABLE)
			appendPQExpBuffer(q, " IMMUTABLE");
		else if (provolatile[0] == PROVOLATILE_STABLE)
			appendPQExpBuffer(q, " STABLE");
		else if (provolatile[0] != PROVOLATILE_VOLATILE)
		{
			write_msg(NULL, "unrecognized provolatile value for function \"%s\"\n",
					  finfo->dobj.name);
			exit_nicely();
		}
	}

	if (proisstrict[0] == 't')
		appendPQExpBuffer(q, " STRICT");

	if (prosecdef[0] == 't')
		appendPQExpBuffer(q, " SECURITY DEFINER");

	/*
	 * COST and ROWS are emitted only if present and not default, so as not to
	 * break backwards-compatibility of the dump without need.	Keep this code
	 * in sync with the defaults in functioncmds.c.
	 */
	if (strcmp(procost, "0") != 0)
	{
		if (strcmp(lanname, "internal") == 0 || strcmp(lanname, "c") == 0)
		{
			/* default cost is 1 */
			if (strcmp(procost, "1") != 0)
				appendPQExpBuffer(q, " COST %s", procost);
		}
		else
		{
			/* default cost is 100 */
			if (strcmp(procost, "100") != 0)
				appendPQExpBuffer(q, " COST %s", procost);
		}
	}
	if (proretset[0] == 't' &&
		strcmp(prorows, "0") != 0 && strcmp(prorows, "1000") != 0)
		appendPQExpBuffer(q, " ROWS %s", prorows);

	for (i = 0; i < nconfigitems; i++)
	{
		/* we feel free to scribble on configitems[] here */
		char	   *configitem = configitems[i];
		char	   *pos;

		pos = strchr(configitem, '=');
		if (pos == NULL)
			continue;
		*pos++ = '\0';
		appendPQExpBuffer(q, "\n    SET %s TO ", fmtId(configitem));

		/*
		 * Some GUC variable names are 'LIST' type and hence must not be
		 * quoted.
		 */
		if (pg_strcasecmp(configitem, "DateStyle") == 0
			|| pg_strcasecmp(configitem, "search_path") == 0)
			appendPQExpBuffer(q, "%s", pos);
		else
			appendStringLiteralAH(q, pos, fout);
	}

	appendPQExpBuffer(q, "\n    %s;\n", asPart->data);

	ArchiveEntry(fout, finfo->dobj.catId, finfo->dobj.dumpId,
				 funcsig_tag,
				 finfo->dobj.namespace->dobj.name,
				 NULL,
				 finfo->rolname, false,
				 "FUNCTION", SECTION_PRE_DATA,
				 q->data, delqry->data, NULL,
				 finfo->dobj.dependencies, finfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Function Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "FUNCTION %s", funcsig);
	dumpComment(fout, q->data,
				finfo->dobj.namespace->dobj.name, finfo->rolname,
				finfo->dobj.catId, 0, finfo->dobj.dumpId);

	dumpACL(fout, finfo->dobj.catId, finfo->dobj.dumpId, "FUNCTION",
			funcsig, NULL, funcsig_tag,
			finfo->dobj.namespace->dobj.name,
			finfo->rolname, finfo->proacl);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delqry);
	destroyPQExpBuffer(asPart);
	free(funcsig);
	free(funcsig_tag);
	if (allargtypes)
		free(allargtypes);
	if (argmodes)
		free(argmodes);
	if (argnames)
		free(argnames);
	if (configitems)
		free(configitems);
}


/*
 * Dump a user-defined cast
 */
static void
dumpCast(Archive *fout, CastInfo *cast)
{
	PQExpBuffer defqry;
	PQExpBuffer delqry;
	PQExpBuffer castsig;
	FuncInfo   *funcInfo = NULL;
	TypeInfo   *sourceInfo;
	TypeInfo   *targetInfo;

	if (dataOnly)
		return;

	if (OidIsValid(cast->castfunc))
	{
		funcInfo = findFuncByOid(cast->castfunc);
		if (funcInfo == NULL)
			return;
	}

	/*
	 * As per discussion we dump casts if one or more of the underlying
	 * objects (the conversion function and the two data types) are not
	 * builtin AND if all of the non-builtin objects are included in the dump.
	 * Builtin meaning, the namespace name does not start with "pg_".
	 */
	sourceInfo = findTypeByOid(cast->castsource);
	targetInfo = findTypeByOid(cast->casttarget);

	if (sourceInfo == NULL || targetInfo == NULL)
		return;

	/*
	 * Skip this cast if all objects are from pg_
	 */
	if ((funcInfo == NULL ||
		 strncmp(funcInfo->dobj.namespace->dobj.name, "pg_", 3) == 0) &&
		strncmp(sourceInfo->dobj.namespace->dobj.name, "pg_", 3) == 0 &&
		strncmp(targetInfo->dobj.namespace->dobj.name, "pg_", 3) == 0)
		return;

	/*
	 * Skip cast if function isn't from pg_ and is not to be dumped.
	 */
	if (funcInfo &&
		strncmp(funcInfo->dobj.namespace->dobj.name, "pg_", 3) != 0 &&
		!funcInfo->dobj.dump)
		return;

	/*
	 * Same for the source type
	 */
	if (strncmp(sourceInfo->dobj.namespace->dobj.name, "pg_", 3) != 0 &&
		!sourceInfo->dobj.dump)
		return;

	/*
	 * and the target type.
	 */
	if (strncmp(targetInfo->dobj.namespace->dobj.name, "pg_", 3) != 0 &&
		!targetInfo->dobj.dump)
		return;

	/* Make sure we are in proper schema (needed for getFormattedTypeName) */
	selectSourceSchema("pg_catalog");

	defqry = createPQExpBuffer();
	delqry = createPQExpBuffer();
	castsig = createPQExpBuffer();

	appendPQExpBuffer(delqry, "DROP CAST (%s AS %s);\n",
					  getFormattedTypeName(cast->castsource, zeroAsNone),
					  getFormattedTypeName(cast->casttarget, zeroAsNone));

	appendPQExpBuffer(defqry, "CREATE CAST (%s AS %s) ",
					  getFormattedTypeName(cast->castsource, zeroAsNone),
					  getFormattedTypeName(cast->casttarget, zeroAsNone));

	switch (cast->castmethod)
	{
		case COERCION_METHOD_BINARY:
			appendPQExpBuffer(defqry, "WITHOUT FUNCTION");
			break;
		case COERCION_METHOD_INOUT:
			appendPQExpBuffer(defqry, "WITH INOUT");
			break;
		case COERCION_METHOD_FUNCTION:

			/*
			 * Always qualify the function name, in case it is not in
			 * pg_catalog schema (format_function_signature won't qualify it).
			 */
			appendPQExpBuffer(defqry, "WITH FUNCTION %s.",
							  fmtId(funcInfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(defqry, "%s",
							  format_function_signature(funcInfo, true));
			break;
		default:
			write_msg(NULL, "WARNING: bogus value in pg_cast.castmethod field\n");
	}

	if (cast->castcontext == 'a')
		appendPQExpBuffer(defqry, " AS ASSIGNMENT");
	else if (cast->castcontext == 'i')
		appendPQExpBuffer(defqry, " AS IMPLICIT");
	appendPQExpBuffer(defqry, ";\n");

	appendPQExpBuffer(castsig, "CAST (%s AS %s)",
					  getFormattedTypeName(cast->castsource, zeroAsNone),
					  getFormattedTypeName(cast->casttarget, zeroAsNone));

	ArchiveEntry(fout, cast->dobj.catId, cast->dobj.dumpId,
				 castsig->data,
				 "pg_catalog", NULL, "",
				 false, "CAST", SECTION_PRE_DATA,
				 defqry->data, delqry->data, NULL,
				 cast->dobj.dependencies, cast->dobj.nDeps,
				 NULL, NULL);

	/* Dump Cast Comments */
	resetPQExpBuffer(defqry);
	appendPQExpBuffer(defqry, "CAST (%s AS %s)",
					  getFormattedTypeName(cast->castsource, zeroAsNone),
					  getFormattedTypeName(cast->casttarget, zeroAsNone));
	dumpComment(fout, defqry->data,
				NULL, "",
				cast->dobj.catId, 0, cast->dobj.dumpId);

	destroyPQExpBuffer(defqry);
	destroyPQExpBuffer(delqry);
	destroyPQExpBuffer(castsig);
}

/*
 * dumpOpr
 *	  write out a single operator definition
 */
static void
dumpOpr(Archive *fout, OprInfo *oprinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer oprid;
	PQExpBuffer details;
	const char *name;
	PGresult   *res;
	int			ntups;
	int			i_oprkind;
	int			i_oprcode;
	int			i_oprleft;
	int			i_oprright;
	int			i_oprcom;
	int			i_oprnegate;
	int			i_oprrest;
	int			i_oprjoin;
	int			i_oprcanmerge;
	int			i_oprcanhash;
	char	   *oprkind;
	char	   *oprcode;
	char	   *oprleft;
	char	   *oprright;
	char	   *oprcom;
	char	   *oprnegate;
	char	   *oprrest;
	char	   *oprjoin;
	char	   *oprcanmerge;
	char	   *oprcanhash;

	/* Skip if not to be dumped */
	if (!oprinfo->dobj.dump || dataOnly)
		return;

	/*
	 * some operators are invalid because they were the result of user
	 * defining operators before commutators exist
	 */
	if (!OidIsValid(oprinfo->oprcode))
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	oprid = createPQExpBuffer();
	details = createPQExpBuffer();

	/* Make sure we are in proper schema so regoperator works correctly */
	selectSourceSchema(oprinfo->dobj.namespace->dobj.name);

	if (g_fout->remoteVersion >= 80300)
	{
		appendPQExpBuffer(query, "SELECT oprkind, "
						  "oprcode::pg_catalog.regprocedure, "
						  "oprleft::pg_catalog.regtype, "
						  "oprright::pg_catalog.regtype, "
						  "oprcom::pg_catalog.regoperator, "
						  "oprnegate::pg_catalog.regoperator, "
						  "oprrest::pg_catalog.regprocedure, "
						  "oprjoin::pg_catalog.regprocedure, "
						  "oprcanmerge, oprcanhash "
						  "FROM pg_catalog.pg_operator "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  oprinfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT oprkind, "
						  "oprcode::pg_catalog.regprocedure, "
						  "oprleft::pg_catalog.regtype, "
						  "oprright::pg_catalog.regtype, "
						  "oprcom::pg_catalog.regoperator, "
						  "oprnegate::pg_catalog.regoperator, "
						  "oprrest::pg_catalog.regprocedure, "
						  "oprjoin::pg_catalog.regprocedure, "
						  "(oprlsortop != 0) AS oprcanmerge, "
						  "oprcanhash "
						  "FROM pg_catalog.pg_operator "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  oprinfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT oprkind, oprcode, "
						  "CASE WHEN oprleft = 0 THEN '-' "
						  "ELSE format_type(oprleft, NULL) END AS oprleft, "
						  "CASE WHEN oprright = 0 THEN '-' "
						  "ELSE format_type(oprright, NULL) END AS oprright, "
						  "oprcom, oprnegate, oprrest, oprjoin, "
						  "(oprlsortop != 0) AS oprcanmerge, "
						  "oprcanhash "
						  "FROM pg_operator "
						  "WHERE oid = '%u'::oid",
						  oprinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT oprkind, oprcode, "
						  "CASE WHEN oprleft = 0 THEN '-'::name "
						  "ELSE (SELECT typname FROM pg_type WHERE oid = oprleft) END AS oprleft, "
						  "CASE WHEN oprright = 0 THEN '-'::name "
						  "ELSE (SELECT typname FROM pg_type WHERE oid = oprright) END AS oprright, "
						  "oprcom, oprnegate, oprrest, oprjoin, "
						  "(oprlsortop != 0) AS oprcanmerge, "
						  "oprcanhash "
						  "FROM pg_operator "
						  "WHERE oid = '%u'::oid",
						  oprinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	i_oprkind = PQfnumber(res, "oprkind");
	i_oprcode = PQfnumber(res, "oprcode");
	i_oprleft = PQfnumber(res, "oprleft");
	i_oprright = PQfnumber(res, "oprright");
	i_oprcom = PQfnumber(res, "oprcom");
	i_oprnegate = PQfnumber(res, "oprnegate");
	i_oprrest = PQfnumber(res, "oprrest");
	i_oprjoin = PQfnumber(res, "oprjoin");
	i_oprcanmerge = PQfnumber(res, "oprcanmerge");
	i_oprcanhash = PQfnumber(res, "oprcanhash");

	oprkind = PQgetvalue(res, 0, i_oprkind);
	oprcode = PQgetvalue(res, 0, i_oprcode);
	oprleft = PQgetvalue(res, 0, i_oprleft);
	oprright = PQgetvalue(res, 0, i_oprright);
	oprcom = PQgetvalue(res, 0, i_oprcom);
	oprnegate = PQgetvalue(res, 0, i_oprnegate);
	oprrest = PQgetvalue(res, 0, i_oprrest);
	oprjoin = PQgetvalue(res, 0, i_oprjoin);
	oprcanmerge = PQgetvalue(res, 0, i_oprcanmerge);
	oprcanhash = PQgetvalue(res, 0, i_oprcanhash);

	appendPQExpBuffer(details, "    PROCEDURE = %s",
					  convertRegProcReference(oprcode));

	appendPQExpBuffer(oprid, "%s (",
					  oprinfo->dobj.name);

	/*
	 * right unary means there's a left arg and left unary means there's a
	 * right arg
	 */
	if (strcmp(oprkind, "r") == 0 ||
		strcmp(oprkind, "b") == 0)
	{
		if (g_fout->remoteVersion >= 70100)
			name = oprleft;
		else
			name = fmtId(oprleft);
		appendPQExpBuffer(details, ",\n    LEFTARG = %s", name);
		appendPQExpBuffer(oprid, "%s", name);
	}
	else
		appendPQExpBuffer(oprid, "NONE");

	if (strcmp(oprkind, "l") == 0 ||
		strcmp(oprkind, "b") == 0)
	{
		if (g_fout->remoteVersion >= 70100)
			name = oprright;
		else
			name = fmtId(oprright);
		appendPQExpBuffer(details, ",\n    RIGHTARG = %s", name);
		appendPQExpBuffer(oprid, ", %s)", name);
	}
	else
		appendPQExpBuffer(oprid, ", NONE)");

	name = convertOperatorReference(oprcom);
	if (name)
		appendPQExpBuffer(details, ",\n    COMMUTATOR = %s", name);

	name = convertOperatorReference(oprnegate);
	if (name)
		appendPQExpBuffer(details, ",\n    NEGATOR = %s", name);

	if (strcmp(oprcanmerge, "t") == 0)
		appendPQExpBuffer(details, ",\n    MERGES");

	if (strcmp(oprcanhash, "t") == 0)
		appendPQExpBuffer(details, ",\n    HASHES");

	name = convertRegProcReference(oprrest);
	if (name)
		appendPQExpBuffer(details, ",\n    RESTRICT = %s", name);

	name = convertRegProcReference(oprjoin);
	if (name)
		appendPQExpBuffer(details, ",\n    JOIN = %s", name);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP OPERATOR %s.%s;\n",
					  fmtId(oprinfo->dobj.namespace->dobj.name),
					  oprid->data);

	appendPQExpBuffer(q, "CREATE OPERATOR %s (\n%s\n);\n",
					  oprinfo->dobj.name, details->data);

	ArchiveEntry(fout, oprinfo->dobj.catId, oprinfo->dobj.dumpId,
				 oprinfo->dobj.name,
				 oprinfo->dobj.namespace->dobj.name,
				 NULL,
				 oprinfo->rolname,
				 false, "OPERATOR", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 oprinfo->dobj.dependencies, oprinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Operator Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "OPERATOR %s", oprid->data);
	dumpComment(fout, q->data,
				oprinfo->dobj.namespace->dobj.name, oprinfo->rolname,
				oprinfo->dobj.catId, 0, oprinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(oprid);
	destroyPQExpBuffer(details);
}

/*
 * Convert a function reference obtained from pg_operator
 *
 * Returns what to print, or NULL if function references is InvalidOid
 *
 * In 7.3 the input is a REGPROCEDURE display; we have to strip the
 * argument-types part.  In prior versions, the input is a REGPROC display.
 */
static const char *
convertRegProcReference(const char *proc)
{
	/* In all cases "-" means a null reference */
	if (strcmp(proc, "-") == 0)
		return NULL;

	if (g_fout->remoteVersion >= 70300)
	{
		char	   *name;
		char	   *paren;
		bool		inquote;

		name = strdup(proc);
		/* find non-double-quoted left paren */
		inquote = false;
		for (paren = name; *paren; paren++)
		{
			if (*paren == '(' && !inquote)
			{
				*paren = '\0';
				break;
			}
			if (*paren == '"')
				inquote = !inquote;
		}
		return name;
	}

	/* REGPROC before 7.3 does not quote its result */
	return fmtId(proc);
}

/*
 * Convert an operator cross-reference obtained from pg_operator
 *
 * Returns what to print, or NULL to print nothing
 *
 * In 7.3 and up the input is a REGOPERATOR display; we have to strip the
 * argument-types part, and add OPERATOR() decoration if the name is
 * schema-qualified.  In older versions, the input is just a numeric OID,
 * which we search our operator list for.
 */
static const char *
convertOperatorReference(const char *opr)
{
	OprInfo    *oprInfo;

	/* In all cases "0" means a null reference */
	if (strcmp(opr, "0") == 0)
		return NULL;

	if (g_fout->remoteVersion >= 70300)
	{
		char	   *name;
		char	   *oname;
		char	   *ptr;
		bool		inquote;
		bool		sawdot;

		name = strdup(opr);
		/* find non-double-quoted left paren, and check for non-quoted dot */
		inquote = false;
		sawdot = false;
		for (ptr = name; *ptr; ptr++)
		{
			if (*ptr == '"')
				inquote = !inquote;
			else if (*ptr == '.' && !inquote)
				sawdot = true;
			else if (*ptr == '(' && !inquote)
			{
				*ptr = '\0';
				break;
			}
		}
		/* If not schema-qualified, don't need to add OPERATOR() */
		if (!sawdot)
			return name;
		oname = malloc(strlen(name) + 11);
		sprintf(oname, "OPERATOR(%s)", name);
		free(name);
		return oname;
	}

	oprInfo = findOprByOid(atooid(opr));
	if (oprInfo == NULL)
	{
		write_msg(NULL, "WARNING: could not find operator with OID %s\n",
				  opr);
		return NULL;
	}
	return oprInfo->dobj.name;
}

/*
 * Convert a function OID obtained from pg_ts_parser or pg_ts_template
 *
 * It is sufficient to use REGPROC rather than REGPROCEDURE, since the
 * argument lists of these functions are predetermined.  Note that the
 * caller should ensure we are in the proper schema, because the results
 * are search path dependent!
 */
static const char *
convertTSFunction(Oid funcOid)
{
	char	   *result;
	char		query[128];
	PGresult   *res;
	int			ntups;

	snprintf(query, sizeof(query),
			 "SELECT '%u'::pg_catalog.regproc", funcOid);
	res = PQexec(g_conn, query);
	check_sql_result(res, g_conn, query, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query);
		exit_nicely();
	}

	result = strdup(PQgetvalue(res, 0, 0));

	PQclear(res);

	return result;
}


/*
 * dumpOpclass
 *	  write out a single operator class definition
 */
static void
dumpOpclass(Archive *fout, OpclassInfo *opcinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PGresult   *res;
	int			ntups;
	int			i_opcintype;
	int			i_opckeytype;
	int			i_opcdefault;
	int			i_opcfamily;
	int			i_opcfamilynsp;
	int			i_amname;
	int			i_amopstrategy;
	int			i_amopreqcheck;
	int			i_amopopr;
	int			i_amprocnum;
	int			i_amproc;
	char	   *opcintype;
	char	   *opckeytype;
	char	   *opcdefault;
	char	   *opcfamily;
	char	   *opcfamilynsp;
	char	   *amname;
	char	   *amopstrategy;
	char	   *amopreqcheck;
	char	   *amopopr;
	char	   *amprocnum;
	char	   *amproc;
	bool		needComma;
	int			i;

	/* Skip if not to be dumped */
	if (!opcinfo->dobj.dump || dataOnly)
		return;

	/*
	 * XXX currently we do not implement dumping of operator classes from
	 * pre-7.3 databases.  This could be done but it seems not worth the
	 * trouble.
	 */
	if (g_fout->remoteVersion < 70300)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Make sure we are in proper schema so regoperator works correctly */
	selectSourceSchema(opcinfo->dobj.namespace->dobj.name);

	/* Get additional fields from the pg_opclass row */
	if (g_fout->remoteVersion >= 80300)
	{
		appendPQExpBuffer(query, "SELECT opcintype::pg_catalog.regtype, "
						  "opckeytype::pg_catalog.regtype, "
						  "opcdefault, "
						  "opfname AS opcfamily, "
						  "nspname AS opcfamilynsp, "
						  "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opcmethod) AS amname "
						  "FROM pg_catalog.pg_opclass c "
				   "LEFT JOIN pg_catalog.pg_opfamily f ON f.oid = opcfamily "
			   "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = opfnamespace "
						  "WHERE c.oid = '%u'::pg_catalog.oid",
						  opcinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT opcintype::pg_catalog.regtype, "
						  "opckeytype::pg_catalog.regtype, "
						  "opcdefault, "
						  "NULL AS opcfamily, "
						  "NULL AS opcfamilynsp, "
		"(SELECT amname FROM pg_catalog.pg_am WHERE oid = opcamid) AS amname "
						  "FROM pg_catalog.pg_opclass "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  opcinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	i_opcintype = PQfnumber(res, "opcintype");
	i_opckeytype = PQfnumber(res, "opckeytype");
	i_opcdefault = PQfnumber(res, "opcdefault");
	i_opcfamily = PQfnumber(res, "opcfamily");
	i_opcfamilynsp = PQfnumber(res, "opcfamilynsp");
	i_amname = PQfnumber(res, "amname");

	opcintype = PQgetvalue(res, 0, i_opcintype);
	opckeytype = PQgetvalue(res, 0, i_opckeytype);
	opcdefault = PQgetvalue(res, 0, i_opcdefault);
	opcfamily = PQgetvalue(res, 0, i_opcfamily);
	opcfamilynsp = PQgetvalue(res, 0, i_opcfamilynsp);
	/* amname will still be needed after we PQclear res */
	amname = strdup(PQgetvalue(res, 0, i_amname));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP OPERATOR CLASS %s",
					  fmtId(opcinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s",
					  fmtId(opcinfo->dobj.name));
	appendPQExpBuffer(delq, " USING %s;\n",
					  fmtId(amname));

	/* Build the fixed portion of the CREATE command */
	appendPQExpBuffer(q, "CREATE OPERATOR CLASS %s\n    ",
					  fmtId(opcinfo->dobj.name));
	if (strcmp(opcdefault, "t") == 0)
		appendPQExpBuffer(q, "DEFAULT ");
	appendPQExpBuffer(q, "FOR TYPE %s USING %s",
					  opcintype,
					  fmtId(amname));
	if (strlen(opcfamily) > 0 &&
		(strcmp(opcfamily, opcinfo->dobj.name) != 0 ||
		 strcmp(opcfamilynsp, opcinfo->dobj.namespace->dobj.name) != 0))
	{
		appendPQExpBuffer(q, " FAMILY ");
		if (strcmp(opcfamilynsp, opcinfo->dobj.namespace->dobj.name) != 0)
			appendPQExpBuffer(q, "%s.", fmtId(opcfamilynsp));
		appendPQExpBuffer(q, "%s", fmtId(opcfamily));
	}
	appendPQExpBuffer(q, " AS\n    ");

	needComma = false;

	if (strcmp(opckeytype, "-") != 0)
	{
		appendPQExpBuffer(q, "STORAGE %s",
						  opckeytype);
		needComma = true;
	}

	PQclear(res);

	/*
	 * Now fetch and print the OPERATOR entries (pg_amop rows).
	 */
	resetPQExpBuffer(query);

	if (g_fout->remoteVersion >= 80400)
	{
		/*
		 * Print only those opfamily members that are tied to the opclass by
		 * pg_depend entries.
		 *
		 * XXX RECHECK is gone as of 8.4, but we'll still print it if dumping
		 * an older server's opclass in which it is used.  This is to avoid
		 * hard-to-detect breakage if a newer pg_dump is used to dump from an
		 * older server and then reload into that old version.	This can go
		 * away once 8.3 is so old as to not be of interest to anyone.
		 */
		appendPQExpBuffer(query, "SELECT amopstrategy, false AS amopreqcheck, "
						  "amopopr::pg_catalog.regoperator "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
		   "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				   "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opcinfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 80300)
	{
		/*
		 * Print only those opfamily members that are tied to the opclass by
		 * pg_depend entries.
		 */
		appendPQExpBuffer(query, "SELECT amopstrategy, amopreqcheck, "
						  "amopopr::pg_catalog.regoperator "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
		   "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				   "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opcinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, amopreqcheck, "
						  "amopopr::pg_catalog.regoperator "
						  "FROM pg_catalog.pg_amop "
						  "WHERE amopclaid = '%u'::pg_catalog.oid "
						  "ORDER BY amopstrategy",
						  opcinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_amopstrategy = PQfnumber(res, "amopstrategy");
	i_amopreqcheck = PQfnumber(res, "amopreqcheck");
	i_amopopr = PQfnumber(res, "amopopr");

	for (i = 0; i < ntups; i++)
	{
		amopstrategy = PQgetvalue(res, i, i_amopstrategy);
		amopreqcheck = PQgetvalue(res, i, i_amopreqcheck);
		amopopr = PQgetvalue(res, i, i_amopopr);

		if (needComma)
			appendPQExpBuffer(q, " ,\n    ");

		appendPQExpBuffer(q, "OPERATOR %s %s",
						  amopstrategy, amopopr);
		if (strcmp(amopreqcheck, "t") == 0)
			appendPQExpBuffer(q, " RECHECK");

		needComma = true;
	}

	PQclear(res);

	/*
	 * Now fetch and print the FUNCTION entries (pg_amproc rows).
	 */
	resetPQExpBuffer(query);

	if (g_fout->remoteVersion >= 80300)
	{
		/*
		 * Print only those opfamily members that are tied to the opclass by
		 * pg_depend entries.
		 */
		appendPQExpBuffer(query, "SELECT amprocnum, "
						  "amproc::pg_catalog.regprocedure "
						"FROM pg_catalog.pg_amproc ap, pg_catalog.pg_depend "
		   "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				 "AND classid = 'pg_catalog.pg_amproc'::pg_catalog.regclass "
						  "AND objid = ap.oid "
						  "ORDER BY amprocnum",
						  opcinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT amprocnum, "
						  "amproc::pg_catalog.regprocedure "
						  "FROM pg_catalog.pg_amproc "
						  "WHERE amopclaid = '%u'::pg_catalog.oid "
						  "ORDER BY amprocnum",
						  opcinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_amprocnum = PQfnumber(res, "amprocnum");
	i_amproc = PQfnumber(res, "amproc");

	for (i = 0; i < ntups; i++)
	{
		amprocnum = PQgetvalue(res, i, i_amprocnum);
		amproc = PQgetvalue(res, i, i_amproc);

		if (needComma)
			appendPQExpBuffer(q, " ,\n    ");

		appendPQExpBuffer(q, "FUNCTION %s %s",
						  amprocnum, amproc);

		needComma = true;
	}

	PQclear(res);

	appendPQExpBuffer(q, ";\n");

	ArchiveEntry(fout, opcinfo->dobj.catId, opcinfo->dobj.dumpId,
				 opcinfo->dobj.name,
				 opcinfo->dobj.namespace->dobj.name,
				 NULL,
				 opcinfo->rolname,
				 false, "OPERATOR CLASS", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 opcinfo->dobj.dependencies, opcinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Operator Class Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "OPERATOR CLASS %s",
					  fmtId(opcinfo->dobj.name));
	appendPQExpBuffer(q, " USING %s",
					  fmtId(amname));
	dumpComment(fout, q->data,
				NULL, opcinfo->rolname,
				opcinfo->dobj.catId, 0, opcinfo->dobj.dumpId);

	free(amname);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpOpfamily
 *	  write out a single operator family definition
 */
static void
dumpOpfamily(Archive *fout, OpfamilyInfo *opfinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PGresult   *res;
	PGresult   *res_ops;
	PGresult   *res_procs;
	int			ntups;
	int			i_amname;
	int			i_amopstrategy;
	int			i_amopreqcheck;
	int			i_amopopr;
	int			i_amprocnum;
	int			i_amproc;
	int			i_amproclefttype;
	int			i_amprocrighttype;
	char	   *amname;
	char	   *amopstrategy;
	char	   *amopreqcheck;
	char	   *amopopr;
	char	   *amprocnum;
	char	   *amproc;
	char	   *amproclefttype;
	char	   *amprocrighttype;
	bool		needComma;
	int			i;

	/* Skip if not to be dumped */
	if (!opfinfo->dobj.dump || dataOnly)
		return;

	/*
	 * We want to dump the opfamily only if (1) it contains "loose" operators
	 * or functions, or (2) it contains an opclass with a different name or
	 * owner.  Otherwise it's sufficient to let it be created during creation
	 * of the contained opclass, and not dumping it improves portability of
	 * the dump.  Since we have to fetch the loose operators/funcs anyway, do
	 * that first.
	 */

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Make sure we are in proper schema so regoperator works correctly */
	selectSourceSchema(opfinfo->dobj.namespace->dobj.name);

	/*
	 * Fetch only those opfamily members that are tied directly to the
	 * opfamily by pg_depend entries.
	 */
	if (g_fout->remoteVersion >= 80400)
	{
		/*
		 * XXX RECHECK is gone as of 8.4, but we'll still print it if dumping
		 * an older server's opclass in which it is used.  This is to avoid
		 * hard-to-detect breakage if a newer pg_dump is used to dump from an
		 * older server and then reload into that old version.	This can go
		 * away once 8.3 is so old as to not be of interest to anyone.
		 */
		appendPQExpBuffer(query, "SELECT amopstrategy, false AS amopreqcheck, "
						  "amopopr::pg_catalog.regoperator "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
		  "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				   "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opfinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, amopreqcheck, "
						  "amopopr::pg_catalog.regoperator "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
		  "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				   "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opfinfo->dobj.catId.oid);
	}

	res_ops = PQexec(g_conn, query->data);
	check_sql_result(res_ops, g_conn, query->data, PGRES_TUPLES_OK);

	resetPQExpBuffer(query);

	appendPQExpBuffer(query, "SELECT amprocnum, "
					  "amproc::pg_catalog.regprocedure, "
					  "amproclefttype::pg_catalog.regtype, "
					  "amprocrighttype::pg_catalog.regtype "
					  "FROM pg_catalog.pg_amproc ap, pg_catalog.pg_depend "
		  "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
					  "AND refobjid = '%u'::pg_catalog.oid "
				 "AND classid = 'pg_catalog.pg_amproc'::pg_catalog.regclass "
					  "AND objid = ap.oid "
					  "ORDER BY amprocnum",
					  opfinfo->dobj.catId.oid);

	res_procs = PQexec(g_conn, query->data);
	check_sql_result(res_procs, g_conn, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res_ops) == 0 && PQntuples(res_procs) == 0)
	{
		/* No loose members, so check contained opclasses */
		resetPQExpBuffer(query);

		appendPQExpBuffer(query, "SELECT 1 "
						  "FROM pg_catalog.pg_opclass c, pg_catalog.pg_opfamily f, pg_catalog.pg_depend "
						  "WHERE f.oid = '%u'::pg_catalog.oid "
			"AND refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
						  "AND refobjid = f.oid "
				"AND classid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "AND objid = c.oid "
						  "AND (opcname != opfname OR opcnamespace != opfnamespace OR opcowner != opfowner) "
						  "LIMIT 1",
						  opfinfo->dobj.catId.oid);

		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		if (PQntuples(res) == 0)
		{
			/* no need to dump it, so bail out */
			PQclear(res);
			PQclear(res_ops);
			PQclear(res_procs);
			destroyPQExpBuffer(query);
			destroyPQExpBuffer(q);
			destroyPQExpBuffer(delq);
			return;
		}

		PQclear(res);
	}

	/* Get additional fields from the pg_opfamily row */
	resetPQExpBuffer(query);

	appendPQExpBuffer(query, "SELECT "
	 "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opfmethod) AS amname "
					  "FROM pg_catalog.pg_opfamily "
					  "WHERE oid = '%u'::pg_catalog.oid",
					  opfinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	i_amname = PQfnumber(res, "amname");

	/* amname will still be needed after we PQclear res */
	amname = strdup(PQgetvalue(res, 0, i_amname));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP OPERATOR FAMILY %s",
					  fmtId(opfinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s",
					  fmtId(opfinfo->dobj.name));
	appendPQExpBuffer(delq, " USING %s;\n",
					  fmtId(amname));

	/* Build the fixed portion of the CREATE command */
	appendPQExpBuffer(q, "CREATE OPERATOR FAMILY %s",
					  fmtId(opfinfo->dobj.name));
	appendPQExpBuffer(q, " USING %s;\n",
					  fmtId(amname));

	PQclear(res);

	/* Do we need an ALTER to add loose members? */
	if (PQntuples(res_ops) > 0 || PQntuples(res_procs) > 0)
	{
		appendPQExpBuffer(q, "ALTER OPERATOR FAMILY %s",
						  fmtId(opfinfo->dobj.name));
		appendPQExpBuffer(q, " USING %s ADD\n    ",
						  fmtId(amname));

		needComma = false;

		/*
		 * Now fetch and print the OPERATOR entries (pg_amop rows).
		 */
		ntups = PQntuples(res_ops);

		i_amopstrategy = PQfnumber(res_ops, "amopstrategy");
		i_amopreqcheck = PQfnumber(res_ops, "amopreqcheck");
		i_amopopr = PQfnumber(res_ops, "amopopr");

		for (i = 0; i < ntups; i++)
		{
			amopstrategy = PQgetvalue(res_ops, i, i_amopstrategy);
			amopreqcheck = PQgetvalue(res_ops, i, i_amopreqcheck);
			amopopr = PQgetvalue(res_ops, i, i_amopopr);

			if (needComma)
				appendPQExpBuffer(q, " ,\n    ");

			appendPQExpBuffer(q, "OPERATOR %s %s",
							  amopstrategy, amopopr);
			if (strcmp(amopreqcheck, "t") == 0)
				appendPQExpBuffer(q, " RECHECK");

			needComma = true;
		}

		/*
		 * Now fetch and print the FUNCTION entries (pg_amproc rows).
		 */
		ntups = PQntuples(res_procs);

		i_amprocnum = PQfnumber(res_procs, "amprocnum");
		i_amproc = PQfnumber(res_procs, "amproc");
		i_amproclefttype = PQfnumber(res_procs, "amproclefttype");
		i_amprocrighttype = PQfnumber(res_procs, "amprocrighttype");

		for (i = 0; i < ntups; i++)
		{
			amprocnum = PQgetvalue(res_procs, i, i_amprocnum);
			amproc = PQgetvalue(res_procs, i, i_amproc);
			amproclefttype = PQgetvalue(res_procs, i, i_amproclefttype);
			amprocrighttype = PQgetvalue(res_procs, i, i_amprocrighttype);

			if (needComma)
				appendPQExpBuffer(q, " ,\n    ");

			appendPQExpBuffer(q, "FUNCTION %s (%s, %s) %s",
							  amprocnum, amproclefttype, amprocrighttype,
							  amproc);

			needComma = true;
		}

		appendPQExpBuffer(q, ";\n");
	}

	ArchiveEntry(fout, opfinfo->dobj.catId, opfinfo->dobj.dumpId,
				 opfinfo->dobj.name,
				 opfinfo->dobj.namespace->dobj.name,
				 NULL,
				 opfinfo->rolname,
				 false, "OPERATOR FAMILY", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 opfinfo->dobj.dependencies, opfinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Operator Family Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "OPERATOR FAMILY %s",
					  fmtId(opfinfo->dobj.name));
	appendPQExpBuffer(q, " USING %s",
					  fmtId(amname));
	dumpComment(fout, q->data,
				NULL, opfinfo->rolname,
				opfinfo->dobj.catId, 0, opfinfo->dobj.dumpId);

	free(amname);
	PQclear(res_ops);
	PQclear(res_procs);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpConversion
 *	  write out a single conversion definition
 */
static void
dumpConversion(Archive *fout, ConvInfo *convinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer details;
	PGresult   *res;
	int			ntups;
	int			i_conname;
	int			i_conforencoding;
	int			i_contoencoding;
	int			i_conproc;
	int			i_condefault;
	const char *conname;
	const char *conforencoding;
	const char *contoencoding;
	const char *conproc;
	bool		condefault;

	/* Skip if not to be dumped */
	if (!convinfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	details = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(convinfo->dobj.namespace->dobj.name);

	/* Get conversion-specific details */
	appendPQExpBuffer(query, "SELECT conname, "
		 "pg_catalog.pg_encoding_to_char(conforencoding) AS conforencoding, "
		   "pg_catalog.pg_encoding_to_char(contoencoding) AS contoencoding, "
					  "conproc, condefault "
					  "FROM pg_catalog.pg_conversion c "
					  "WHERE c.oid = '%u'::pg_catalog.oid",
					  convinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	i_conname = PQfnumber(res, "conname");
	i_conforencoding = PQfnumber(res, "conforencoding");
	i_contoencoding = PQfnumber(res, "contoencoding");
	i_conproc = PQfnumber(res, "conproc");
	i_condefault = PQfnumber(res, "condefault");

	conname = PQgetvalue(res, 0, i_conname);
	conforencoding = PQgetvalue(res, 0, i_conforencoding);
	contoencoding = PQgetvalue(res, 0, i_contoencoding);
	conproc = PQgetvalue(res, 0, i_conproc);
	condefault = (PQgetvalue(res, 0, i_condefault)[0] == 't');

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP CONVERSION %s",
					  fmtId(convinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(convinfo->dobj.name));

	appendPQExpBuffer(q, "CREATE %sCONVERSION %s FOR ",
					  (condefault) ? "DEFAULT " : "",
					  fmtId(convinfo->dobj.name));
	appendStringLiteralAH(q, conforencoding, fout);
	appendPQExpBuffer(q, " TO ");
	appendStringLiteralAH(q, contoencoding, fout);
	/* regproc is automatically quoted in 7.3 and above */
	appendPQExpBuffer(q, " FROM %s;\n", conproc);

	ArchiveEntry(fout, convinfo->dobj.catId, convinfo->dobj.dumpId,
				 convinfo->dobj.name,
				 convinfo->dobj.namespace->dobj.name,
				 NULL,
				 convinfo->rolname,
				 false, "CONVERSION", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 convinfo->dobj.dependencies, convinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Conversion Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "CONVERSION %s", fmtId(convinfo->dobj.name));
	dumpComment(fout, q->data,
				convinfo->dobj.namespace->dobj.name, convinfo->rolname,
				convinfo->dobj.catId, 0, convinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(details);
}

/*
 * format_aggregate_signature: generate aggregate name and argument list
 *
 * The argument type names are qualified if needed.  The aggregate name
 * is never qualified.
 */
static char *
format_aggregate_signature(AggInfo *agginfo, Archive *fout, bool honor_quotes)
{
	PQExpBufferData buf;
	int			j;

	initPQExpBuffer(&buf);
	if (honor_quotes)
		appendPQExpBuffer(&buf, "%s",
						  fmtId(agginfo->aggfn.dobj.name));
	else
		appendPQExpBuffer(&buf, "%s", agginfo->aggfn.dobj.name);

	if (agginfo->aggfn.nargs == 0)
		appendPQExpBuffer(&buf, "(*)");
	else
	{
		appendPQExpBuffer(&buf, "(");
		for (j = 0; j < agginfo->aggfn.nargs; j++)
		{
			char	   *typname;

			typname = getFormattedTypeName(agginfo->aggfn.argtypes[j], zeroAsOpaque);

			appendPQExpBuffer(&buf, "%s%s",
							  (j > 0) ? ", " : "",
							  typname);
			free(typname);
		}
		appendPQExpBuffer(&buf, ")");
	}
	return buf.data;
}

/*
 * dumpAgg
 *	  write out a single aggregate definition
 */
static void
dumpAgg(Archive *fout, AggInfo *agginfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer details;
	char	   *aggsig;
	char	   *aggsig_tag;
	PGresult   *res;
	int			ntups;
	int			i_aggtransfn;
	int			i_aggfinalfn;
	int			i_aggsortop;
	int			i_aggtranstype;
	int			i_agginitval;
	int			i_convertok;
	const char *aggtransfn;
	const char *aggfinalfn;
	const char *aggsortop;
	const char *aggtranstype;
	const char *agginitval;
	bool		convertok;

	/* Skip if not to be dumped */
	if (!agginfo->aggfn.dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	details = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(agginfo->aggfn.dobj.namespace->dobj.name);

	/* Get aggregate-specific details */
	if (g_fout->remoteVersion >= 80100)
	{
		appendPQExpBuffer(query, "SELECT aggtransfn, "
						  "aggfinalfn, aggtranstype::pg_catalog.regtype, "
						  "aggsortop::pg_catalog.regoperator, "
						  "agginitval, "
						  "'t'::boolean AS convertok "
					  "FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
						  "WHERE a.aggfnoid = p.oid "
						  "AND p.oid = '%u'::pg_catalog.oid",
						  agginfo->aggfn.dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT aggtransfn, "
						  "aggfinalfn, aggtranstype::pg_catalog.regtype, "
						  "0 AS aggsortop, "
						  "agginitval, "
						  "'t'::boolean AS convertok "
					  "FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
						  "WHERE a.aggfnoid = p.oid "
						  "AND p.oid = '%u'::pg_catalog.oid",
						  agginfo->aggfn.dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT aggtransfn, aggfinalfn, "
						  "format_type(aggtranstype, NULL) AS aggtranstype, "
						  "0 AS aggsortop, "
						  "agginitval, "
						  "'t'::boolean AS convertok "
						  "FROM pg_aggregate "
						  "WHERE oid = '%u'::oid",
						  agginfo->aggfn.dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT aggtransfn1 AS aggtransfn, "
						  "aggfinalfn, "
						  "(SELECT typname FROM pg_type WHERE oid = aggtranstype1) AS aggtranstype, "
						  "0 AS aggsortop, "
						  "agginitval1 AS agginitval, "
						  "(aggtransfn2 = 0 and aggtranstype2 = 0 and agginitval2 is null) AS convertok "
						  "FROM pg_aggregate "
						  "WHERE oid = '%u'::oid",
						  agginfo->aggfn.dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	i_aggtransfn = PQfnumber(res, "aggtransfn");
	i_aggfinalfn = PQfnumber(res, "aggfinalfn");
	i_aggsortop = PQfnumber(res, "aggsortop");
	i_aggtranstype = PQfnumber(res, "aggtranstype");
	i_agginitval = PQfnumber(res, "agginitval");
	i_convertok = PQfnumber(res, "convertok");

	aggtransfn = PQgetvalue(res, 0, i_aggtransfn);
	aggfinalfn = PQgetvalue(res, 0, i_aggfinalfn);
	aggsortop = PQgetvalue(res, 0, i_aggsortop);
	aggtranstype = PQgetvalue(res, 0, i_aggtranstype);
	agginitval = PQgetvalue(res, 0, i_agginitval);
	convertok = (PQgetvalue(res, 0, i_convertok)[0] == 't');

	aggsig = format_aggregate_signature(agginfo, fout, true);
	aggsig_tag = format_aggregate_signature(agginfo, fout, false);

	if (!convertok)
	{
		write_msg(NULL, "WARNING: aggregate function %s could not be dumped correctly for this database version; ignored\n",
				  aggsig);
		return;
	}

	if (g_fout->remoteVersion >= 70300)
	{
		/* If using 7.3's regproc or regtype, data is already quoted */
		appendPQExpBuffer(details, "    SFUNC = %s,\n    STYPE = %s",
						  aggtransfn,
						  aggtranstype);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		/* format_type quotes, regproc does not */
		appendPQExpBuffer(details, "    SFUNC = %s,\n    STYPE = %s",
						  fmtId(aggtransfn),
						  aggtranstype);
	}
	else
	{
		/* need quotes all around */
		appendPQExpBuffer(details, "    SFUNC = %s,\n",
						  fmtId(aggtransfn));
		appendPQExpBuffer(details, "    STYPE = %s",
						  fmtId(aggtranstype));
	}

	if (!PQgetisnull(res, 0, i_agginitval))
	{
		appendPQExpBuffer(details, ",\n    INITCOND = ");
		appendStringLiteralAH(details, agginitval, fout);
	}

	if (strcmp(aggfinalfn, "-") != 0)
	{
		appendPQExpBuffer(details, ",\n    FINALFUNC = %s",
						  aggfinalfn);
	}

	aggsortop = convertOperatorReference(aggsortop);
	if (aggsortop)
	{
		appendPQExpBuffer(details, ",\n    SORTOP = %s",
						  aggsortop);
	}

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP AGGREGATE %s.%s;\n",
					  fmtId(agginfo->aggfn.dobj.namespace->dobj.name),
					  aggsig);

	appendPQExpBuffer(q, "CREATE AGGREGATE %s (\n%s\n);\n",
					  aggsig, details->data);

	ArchiveEntry(fout, agginfo->aggfn.dobj.catId, agginfo->aggfn.dobj.dumpId,
				 aggsig_tag,
				 agginfo->aggfn.dobj.namespace->dobj.name,
				 NULL,
				 agginfo->aggfn.rolname,
				 false, "AGGREGATE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 agginfo->aggfn.dobj.dependencies, agginfo->aggfn.dobj.nDeps,
				 NULL, NULL);

	/* Dump Aggregate Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "AGGREGATE %s", aggsig);
	dumpComment(fout, q->data,
			agginfo->aggfn.dobj.namespace->dobj.name, agginfo->aggfn.rolname,
				agginfo->aggfn.dobj.catId, 0, agginfo->aggfn.dobj.dumpId);

	/*
	 * Since there is no GRANT ON AGGREGATE syntax, we have to make the ACL
	 * command look like a function's GRANT; in particular this affects the
	 * syntax for zero-argument aggregates.
	 */
	free(aggsig);
	free(aggsig_tag);

	aggsig = format_function_signature(&agginfo->aggfn, true);
	aggsig_tag = format_function_signature(&agginfo->aggfn, false);

	dumpACL(fout, agginfo->aggfn.dobj.catId, agginfo->aggfn.dobj.dumpId,
			"FUNCTION",
			aggsig, NULL, aggsig_tag,
			agginfo->aggfn.dobj.namespace->dobj.name,
			agginfo->aggfn.rolname, agginfo->aggfn.proacl);

	free(aggsig);
	free(aggsig_tag);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(details);
}

/*
 * dumpTSParser
 *	  write out a single text search parser
 */
static void
dumpTSParser(Archive *fout, TSParserInfo *prsinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;

	/* Skip if not to be dumped */
	if (!prsinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(prsinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH PARSER %s (\n",
					  fmtId(prsinfo->dobj.name));

	appendPQExpBuffer(q, "    START = %s,\n",
					  convertTSFunction(prsinfo->prsstart));
	appendPQExpBuffer(q, "    GETTOKEN = %s,\n",
					  convertTSFunction(prsinfo->prstoken));
	appendPQExpBuffer(q, "    END = %s,\n",
					  convertTSFunction(prsinfo->prsend));
	if (prsinfo->prsheadline != InvalidOid)
		appendPQExpBuffer(q, "    HEADLINE = %s,\n",
						  convertTSFunction(prsinfo->prsheadline));
	appendPQExpBuffer(q, "    LEXTYPES = %s );\n",
					  convertTSFunction(prsinfo->prslextype));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TEXT SEARCH PARSER %s",
					  fmtId(prsinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(prsinfo->dobj.name));

	ArchiveEntry(fout, prsinfo->dobj.catId, prsinfo->dobj.dumpId,
				 prsinfo->dobj.name,
				 prsinfo->dobj.namespace->dobj.name,
				 NULL,
				 "",
				 false, "TEXT SEARCH PARSER", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 prsinfo->dobj.dependencies, prsinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Parser Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "TEXT SEARCH PARSER %s",
					  fmtId(prsinfo->dobj.name));
	dumpComment(fout, q->data,
				NULL, "",
				prsinfo->dobj.catId, 0, prsinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpTSDictionary
 *	  write out a single text search dictionary
 */
static void
dumpTSDictionary(Archive *fout, TSDictInfo *dictinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer query;
	PGresult   *res;
	int			ntups;
	char	   *nspname;
	char	   *tmplname;

	/* Skip if not to be dumped */
	if (!dictinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	/* Fetch name and namespace of the dictionary's template */
	selectSourceSchema("pg_catalog");
	appendPQExpBuffer(query, "SELECT nspname, tmplname "
					  "FROM pg_ts_template p, pg_namespace n "
					  "WHERE p.oid = '%u' AND n.oid = tmplnamespace",
					  dictinfo->dicttemplate);
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}
	nspname = PQgetvalue(res, 0, 0);
	tmplname = PQgetvalue(res, 0, 1);

	/* Make sure we are in proper schema */
	selectSourceSchema(dictinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH DICTIONARY %s (\n",
					  fmtId(dictinfo->dobj.name));

	appendPQExpBuffer(q, "    TEMPLATE = ");
	if (strcmp(nspname, dictinfo->dobj.namespace->dobj.name) != 0)
		appendPQExpBuffer(q, "%s.", fmtId(nspname));
	appendPQExpBuffer(q, "%s", fmtId(tmplname));

	PQclear(res);

	/* the dictinitoption can be dumped straight into the command */
	if (dictinfo->dictinitoption)
		appendPQExpBuffer(q, ",\n    %s", dictinfo->dictinitoption);

	appendPQExpBuffer(q, " );\n");

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TEXT SEARCH DICTIONARY %s",
					  fmtId(dictinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(dictinfo->dobj.name));

	ArchiveEntry(fout, dictinfo->dobj.catId, dictinfo->dobj.dumpId,
				 dictinfo->dobj.name,
				 dictinfo->dobj.namespace->dobj.name,
				 NULL,
				 dictinfo->rolname,
				 false, "TEXT SEARCH DICTIONARY", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 dictinfo->dobj.dependencies, dictinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Dictionary Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "TEXT SEARCH DICTIONARY %s",
					  fmtId(dictinfo->dobj.name));
	dumpComment(fout, q->data,
				NULL, dictinfo->rolname,
				dictinfo->dobj.catId, 0, dictinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpTSTemplate
 *	  write out a single text search template
 */
static void
dumpTSTemplate(Archive *fout, TSTemplateInfo *tmplinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;

	/* Skip if not to be dumped */
	if (!tmplinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(tmplinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH TEMPLATE %s (\n",
					  fmtId(tmplinfo->dobj.name));

	if (tmplinfo->tmplinit != InvalidOid)
		appendPQExpBuffer(q, "    INIT = %s,\n",
						  convertTSFunction(tmplinfo->tmplinit));
	appendPQExpBuffer(q, "    LEXIZE = %s );\n",
					  convertTSFunction(tmplinfo->tmpllexize));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TEXT SEARCH TEMPLATE %s",
					  fmtId(tmplinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(tmplinfo->dobj.name));

	ArchiveEntry(fout, tmplinfo->dobj.catId, tmplinfo->dobj.dumpId,
				 tmplinfo->dobj.name,
				 tmplinfo->dobj.namespace->dobj.name,
				 NULL,
				 "",
				 false, "TEXT SEARCH TEMPLATE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 tmplinfo->dobj.dependencies, tmplinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Template Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "TEXT SEARCH TEMPLATE %s",
					  fmtId(tmplinfo->dobj.name));
	dumpComment(fout, q->data,
				NULL, "",
				tmplinfo->dobj.catId, 0, tmplinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpTSConfig
 *	  write out a single text search configuration
 */
static void
dumpTSConfig(Archive *fout, TSConfigInfo *cfginfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer query;
	PGresult   *res;
	char	   *nspname;
	char	   *prsname;
	int			ntups,
				i;
	int			i_tokenname;
	int			i_dictname;

	/* Skip if not to be dumped */
	if (!cfginfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	/* Fetch name and namespace of the config's parser */
	selectSourceSchema("pg_catalog");
	appendPQExpBuffer(query, "SELECT nspname, prsname "
					  "FROM pg_ts_parser p, pg_namespace n "
					  "WHERE p.oid = '%u' AND n.oid = prsnamespace",
					  cfginfo->cfgparser);
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}
	nspname = PQgetvalue(res, 0, 0);
	prsname = PQgetvalue(res, 0, 1);

	/* Make sure we are in proper schema */
	selectSourceSchema(cfginfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH CONFIGURATION %s (\n",
					  fmtId(cfginfo->dobj.name));

	appendPQExpBuffer(q, "    PARSER = ");
	if (strcmp(nspname, cfginfo->dobj.namespace->dobj.name) != 0)
		appendPQExpBuffer(q, "%s.", fmtId(nspname));
	appendPQExpBuffer(q, "%s );\n", fmtId(prsname));

	PQclear(res);

	resetPQExpBuffer(query);
	appendPQExpBuffer(query,
					  "SELECT \n"
					  "  ( SELECT alias FROM pg_catalog.ts_token_type('%u'::pg_catalog.oid) AS t \n"
					  "    WHERE t.tokid = m.maptokentype ) AS tokenname, \n"
					  "  m.mapdict::pg_catalog.regdictionary AS dictname \n"
					  "FROM pg_catalog.pg_ts_config_map AS m \n"
					  "WHERE m.mapcfg = '%u' \n"
					  "ORDER BY m.mapcfg, m.maptokentype, m.mapseqno",
					  cfginfo->cfgparser, cfginfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(res);

	i_tokenname = PQfnumber(res, "tokenname");
	i_dictname = PQfnumber(res, "dictname");

	for (i = 0; i < ntups; i++)
	{
		char	   *tokenname = PQgetvalue(res, i, i_tokenname);
		char	   *dictname = PQgetvalue(res, i, i_dictname);

		if (i == 0 ||
			strcmp(tokenname, PQgetvalue(res, i - 1, i_tokenname)) != 0)
		{
			/* starting a new token type, so start a new command */
			if (i > 0)
				appendPQExpBuffer(q, ";\n");
			appendPQExpBuffer(q, "\nALTER TEXT SEARCH CONFIGURATION %s\n",
							  fmtId(cfginfo->dobj.name));
			/* tokenname needs quoting, dictname does NOT */
			appendPQExpBuffer(q, "    ADD MAPPING FOR %s WITH %s",
							  fmtId(tokenname), dictname);
		}
		else
			appendPQExpBuffer(q, ", %s", dictname);
	}

	if (ntups > 0)
		appendPQExpBuffer(q, ";\n");

	PQclear(res);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TEXT SEARCH CONFIGURATION %s",
					  fmtId(cfginfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(cfginfo->dobj.name));

	ArchiveEntry(fout, cfginfo->dobj.catId, cfginfo->dobj.dumpId,
				 cfginfo->dobj.name,
				 cfginfo->dobj.namespace->dobj.name,
				 NULL,
				 cfginfo->rolname,
				 false, "TEXT SEARCH CONFIGURATION", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 cfginfo->dobj.dependencies, cfginfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Configuration Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "TEXT SEARCH CONFIGURATION %s",
					  fmtId(cfginfo->dobj.name));
	dumpComment(fout, q->data,
				NULL, cfginfo->rolname,
				cfginfo->dobj.catId, 0, cfginfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpForeignDataWrapper
 *	  write out a single foreign-data wrapper definition
 */
static void
dumpForeignDataWrapper(Archive *fout, FdwInfo *fdwinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	char	   *namecopy;

	/* Skip if not to be dumped */
	if (!fdwinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	appendPQExpBuffer(q, "CREATE FOREIGN DATA WRAPPER %s",
					  fmtId(fdwinfo->dobj.name));

	if (fdwinfo->fdwvalidator && strcmp(fdwinfo->fdwvalidator, "-") != 0)
		appendPQExpBuffer(q, " VALIDATOR %s",
						  fdwinfo->fdwvalidator);

	if (fdwinfo->fdwoptions && strlen(fdwinfo->fdwoptions) > 0)
		appendPQExpBuffer(q, " OPTIONS (%s)", fdwinfo->fdwoptions);

	appendPQExpBuffer(q, ";\n");

	appendPQExpBuffer(delq, "DROP FOREIGN DATA WRAPPER %s;\n",
					  fmtId(fdwinfo->dobj.name));

	ArchiveEntry(fout, fdwinfo->dobj.catId, fdwinfo->dobj.dumpId,
				 fdwinfo->dobj.name,
				 NULL,
				 NULL,
				 fdwinfo->rolname,
				 false, "FOREIGN DATA WRAPPER", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 fdwinfo->dobj.dependencies, fdwinfo->dobj.nDeps,
				 NULL, NULL);

	/* Handle the ACL */
	namecopy = strdup(fmtId(fdwinfo->dobj.name));
	dumpACL(fout, fdwinfo->dobj.catId, fdwinfo->dobj.dumpId,
			"FOREIGN DATA WRAPPER",
			namecopy, NULL, fdwinfo->dobj.name,
			NULL, fdwinfo->rolname,
			fdwinfo->fdwacl);
	free(namecopy);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpForeignServer
 *	  write out a foreign server definition
 */
static void
dumpForeignServer(Archive *fout, ForeignServerInfo *srvinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer query;
	PGresult   *res;
	int			ntups;
	char	   *namecopy;
	char	   *fdwname;

	/* Skip if not to be dumped */
	if (!srvinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	/* look up the foreign-data wrapper */
	selectSourceSchema("pg_catalog");
	appendPQExpBuffer(query, "SELECT fdwname "
					  "FROM pg_foreign_data_wrapper w "
					  "WHERE w.oid = '%u'",
					  srvinfo->srvfdw);
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}
	fdwname = PQgetvalue(res, 0, 0);

	appendPQExpBuffer(q, "CREATE SERVER %s", fmtId(srvinfo->dobj.name));
	if (srvinfo->srvtype && strlen(srvinfo->srvtype) > 0)
	{
		appendPQExpBuffer(q, " TYPE ");
		appendStringLiteralAH(q, srvinfo->srvtype, fout);
	}
	if (srvinfo->srvversion && strlen(srvinfo->srvversion) > 0)
	{
		appendPQExpBuffer(q, " VERSION ");
		appendStringLiteralAH(q, srvinfo->srvversion, fout);
	}

	appendPQExpBuffer(q, " FOREIGN DATA WRAPPER ");
	appendPQExpBuffer(q, "%s", fmtId(fdwname));

	if (srvinfo->srvoptions && strlen(srvinfo->srvoptions) > 0)
		appendPQExpBuffer(q, " OPTIONS (%s)", srvinfo->srvoptions);

	appendPQExpBuffer(q, ";\n");

	appendPQExpBuffer(delq, "DROP SERVER %s;\n",
					  fmtId(srvinfo->dobj.name));

	ArchiveEntry(fout, srvinfo->dobj.catId, srvinfo->dobj.dumpId,
				 srvinfo->dobj.name,
				 NULL,
				 NULL,
				 srvinfo->rolname,
				 false, "SERVER", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 srvinfo->dobj.dependencies, srvinfo->dobj.nDeps,
				 NULL, NULL);

	/* Handle the ACL */
	namecopy = strdup(fmtId(srvinfo->dobj.name));
	dumpACL(fout, srvinfo->dobj.catId, srvinfo->dobj.dumpId,
			"FOREIGN SERVER",
			namecopy, NULL, srvinfo->dobj.name,
			NULL, srvinfo->rolname,
			srvinfo->srvacl);
	free(namecopy);

	/* Dump user mappings */
	dumpUserMappings(fout,
					 srvinfo->dobj.name, NULL,
					 srvinfo->rolname,
					 srvinfo->dobj.catId, srvinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpUserMappings
 *
 * This routine is used to dump any user mappings associated with the
 * server handed to this routine. Should be called after ArchiveEntry()
 * for the server.
 */
static void
dumpUserMappings(Archive *fout,
				 const char *servername, const char *namespace,
				 const char *owner,
				 CatalogId catalogId, DumpId dumpId)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer query;
	PQExpBuffer tag;
	PGresult   *res;
	int			ntups;
	int			i_usename;
	int			i_umoptions;
	int			i;

	q = createPQExpBuffer();
	tag = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	/*
	 * We read from the publicly accessible view pg_user_mappings, so as not
	 * to fail if run by a non-superuser.  Note that the view will show
	 * umoptions as null if the user hasn't got privileges for the associated
	 * server; this means that pg_dump will dump such a mapping, but with no
	 * OPTIONS clause.  A possible alternative is to skip such mappings
	 * altogether, but it's not clear that that's an improvement.
	 */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query,
					  "SELECT usename, "
					  "array_to_string(ARRAY(SELECT option_name || ' ' || quote_literal(option_value) FROM pg_options_to_table(umoptions)), ', ') AS umoptions\n"
					  "FROM pg_user_mappings "
					  "WHERE srvid = %u",
					  catalogId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	i_usename = PQfnumber(res, "usename");
	i_umoptions = PQfnumber(res, "umoptions");

	for (i = 0; i < ntups; i++)
	{
		char	   *usename;
		char	   *umoptions;

		usename = PQgetvalue(res, i, i_usename);
		umoptions = PQgetvalue(res, i, i_umoptions);

		resetPQExpBuffer(q);
		appendPQExpBuffer(q, "CREATE USER MAPPING FOR %s", fmtId(usename));
		appendPQExpBuffer(q, " SERVER %s", fmtId(servername));

		if (umoptions && strlen(umoptions) > 0)
			appendPQExpBuffer(q, " OPTIONS (%s)", umoptions);

		appendPQExpBuffer(q, ";\n");

		resetPQExpBuffer(delq);
		appendPQExpBuffer(delq, "DROP USER MAPPING FOR %s", fmtId(usename));
		appendPQExpBuffer(delq, " SERVER %s;\n", fmtId(servername));

		resetPQExpBuffer(tag);
		appendPQExpBuffer(tag, "USER MAPPING %s SERVER %s",
						  usename, servername);

		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tag->data,
					 namespace,
					 NULL,
					 owner, false,
					 "USER MAPPING", SECTION_PRE_DATA,
					 q->data, delq->data, NULL,
					 &dumpId, 1,
					 NULL, NULL);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(q);
}

/*
 * Write out default privileges information
 */
static void
dumpDefaultACL(Archive *fout, DefaultACLInfo *daclinfo)
{
	PQExpBuffer q;
	PQExpBuffer tag;
	const char *type;

	/* Skip if not to be dumped */
	if (!daclinfo->dobj.dump || dataOnly || aclsSkip)
		return;

	q = createPQExpBuffer();
	tag = createPQExpBuffer();

	switch (daclinfo->defaclobjtype)
	{
		case DEFACLOBJ_RELATION:
			type = "TABLES";
			break;
		case DEFACLOBJ_SEQUENCE:
			type = "SEQUENCES";
			break;
		case DEFACLOBJ_FUNCTION:
			type = "FUNCTIONS";
			break;
		default:
			/* shouldn't get here */
			write_msg(NULL, "unknown object type (%d) in default privileges\n",
					  (int) daclinfo->defaclobjtype);
			exit_nicely();
			type = "";			/* keep compiler quiet */
	}

	appendPQExpBuffer(tag, "DEFAULT PRIVILEGES FOR %s", type);

	/* build the actual command(s) for this tuple */
	if (!buildDefaultACLCommands(type,
								 daclinfo->dobj.namespace != NULL ?
								 daclinfo->dobj.namespace->dobj.name : NULL,
								 daclinfo->defaclacl,
								 daclinfo->defaclrole,
								 fout->remoteVersion,
								 q))
	{
		write_msg(NULL, "could not parse default ACL list (%s)\n",
				  daclinfo->defaclacl);
		exit_nicely();
	}

	ArchiveEntry(fout, daclinfo->dobj.catId, daclinfo->dobj.dumpId,
				 tag->data,
	   daclinfo->dobj.namespace ? daclinfo->dobj.namespace->dobj.name : NULL,
				 NULL,
				 daclinfo->defaclrole,
				 false, "DEFAULT ACL", SECTION_NONE,
				 q->data, "", NULL,
				 daclinfo->dobj.dependencies, daclinfo->dobj.nDeps,
				 NULL, NULL);

	destroyPQExpBuffer(tag);
	destroyPQExpBuffer(q);
}

/*----------
 * Write out grant/revoke information
 *
 * 'objCatId' is the catalog ID of the underlying object.
 * 'objDumpId' is the dump ID of the underlying object.
 * 'type' must be one of
 *		TABLE, SEQUENCE, FUNCTION, LANGUAGE, SCHEMA, DATABASE, TABLESPACE,
 *		FOREIGN DATA WRAPPER, SERVER, or LARGE OBJECT.
 * 'name' is the formatted name of the object.	Must be quoted etc. already.
 * 'subname' is the formatted name of the sub-object, if any.  Must be quoted.
 * 'tag' is the tag for the archive entry (typ. unquoted name of object).
 * 'nspname' is the namespace the object is in (NULL if none).
 * 'owner' is the owner, NULL if there is no owner (for languages).
 * 'acls' is the string read out of the fooacl system catalog field;
 *		it will be parsed here.
 *----------
 */
static void
dumpACL(Archive *fout, CatalogId objCatId, DumpId objDumpId,
		const char *type, const char *name, const char *subname,
		const char *tag, const char *nspname, const char *owner,
		const char *acls)
{
	PQExpBuffer sql;

	/* Do nothing if ACL dump is not enabled */
	if (aclsSkip)
		return;

	/* --data-only skips ACLs *except* BLOB ACLs */
	if (dataOnly && strcmp(type, "LARGE OBJECT") != 0)
		return;

	sql = createPQExpBuffer();

	if (!buildACLCommands(name, subname, type, acls, owner,
						  "", fout->remoteVersion, sql))
	{
		write_msg(NULL, "could not parse ACL list (%s) for object \"%s\" (%s)\n",
				  acls, name, type);
		exit_nicely();
	}

	if (sql->len > 0)
		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tag, nspname,
					 NULL,
					 owner ? owner : "",
					 false, "ACL", SECTION_NONE,
					 sql->data, "", NULL,
					 &(objDumpId), 1,
					 NULL, NULL);

	destroyPQExpBuffer(sql);
}

/*
 * dumpTable
 *	  write out to fout the declarations (not data) of a user-defined table
 */
static void
dumpTable(Archive *fout, TableInfo *tbinfo)
{
	if (tbinfo->dobj.dump)
	{
		char	   *namecopy;

		if (tbinfo->relkind == RELKIND_SEQUENCE)
			dumpSequence(fout, tbinfo);
		else if (!dataOnly)
			dumpTableSchema(fout, tbinfo);

		/* Handle the ACL here */
		namecopy = strdup(fmtId(tbinfo->dobj.name));
		dumpACL(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
				(tbinfo->relkind == RELKIND_SEQUENCE) ? "SEQUENCE" : "TABLE",
				namecopy, NULL, tbinfo->dobj.name,
				tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
				tbinfo->relacl);

		/*
		 * Handle column ACLs, if any.	Note: we pull these with a separate
		 * query rather than trying to fetch them during getTableAttrs, so
		 * that we won't miss ACLs on system columns.
		 */
		if (g_fout->remoteVersion >= 80400)
		{
			PQExpBuffer query = createPQExpBuffer();
			PGresult   *res;
			int			i;

			appendPQExpBuffer(query,
					   "SELECT attname, attacl FROM pg_catalog.pg_attribute "
							  "WHERE attrelid = '%u' AND NOT attisdropped AND attacl IS NOT NULL "
							  "ORDER BY attnum",
							  tbinfo->dobj.catId.oid);
			res = PQexec(g_conn, query->data);
			check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

			for (i = 0; i < PQntuples(res); i++)
			{
				char	   *attname = PQgetvalue(res, i, 0);
				char	   *attacl = PQgetvalue(res, i, 1);
				char	   *attnamecopy;
				char	   *acltag;

				attnamecopy = strdup(fmtId(attname));
				acltag = malloc(strlen(tbinfo->dobj.name) + strlen(attname) + 2);
				sprintf(acltag, "%s.%s", tbinfo->dobj.name, attname);
				/* Column's GRANT type is always TABLE */
				dumpACL(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId, "TABLE",
						namecopy, attnamecopy, acltag,
						tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
						attacl);
				free(attnamecopy);
				free(acltag);
			}
			PQclear(res);
			destroyPQExpBuffer(query);
		}

		free(namecopy);
	}
}

/*
 * dumpTableSchema
 *	  write the declaration (not data) of one user-defined table or view
 */
static void
dumpTableSchema(Archive *fout, TableInfo *tbinfo)
{
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PGresult   *res;
	int			numParents;
	TableInfo **parents;
	int			actual_atts;	/* number of attrs in this CREATE statment */
	char	   *reltypename;
	char	   *storage;
	int			j,
				k;
	bool		toast_set = false;

	/* Make sure we are in proper schema */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	if (binary_upgrade)
		toast_set = binary_upgrade_set_type_oids_by_rel_oid(q,
													 tbinfo->dobj.catId.oid);

	/* Is it a table or a view? */
	if (tbinfo->relkind == RELKIND_VIEW)
	{
		char	   *viewdef;

		reltypename = "VIEW";

		/* Fetch the view definition */
		if (g_fout->remoteVersion >= 70300)
		{
			/* Beginning in 7.3, viewname is not unique; rely on OID */
			appendPQExpBuffer(query,
							  "SELECT pg_catalog.pg_get_viewdef('%u'::pg_catalog.oid) AS viewdef",
							  tbinfo->dobj.catId.oid);
		}
		else
		{
			appendPQExpBuffer(query, "SELECT definition AS viewdef "
							  "FROM pg_views WHERE viewname = ");
			appendStringLiteralAH(query, tbinfo->dobj.name, fout);
			appendPQExpBuffer(query, ";");
		}

		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		if (PQntuples(res) != 1)
		{
			if (PQntuples(res) < 1)
				write_msg(NULL, "query to obtain definition of view \"%s\" returned no data\n",
						  tbinfo->dobj.name);
			else
				write_msg(NULL, "query to obtain definition of view \"%s\" returned more than one definition\n",
						  tbinfo->dobj.name);
			exit_nicely();
		}

		viewdef = PQgetvalue(res, 0, 0);

		if (strlen(viewdef) == 0)
		{
			write_msg(NULL, "definition of view \"%s\" appears to be empty (length zero)\n",
					  tbinfo->dobj.name);
			exit_nicely();
		}

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP VIEW %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s;\n",
						  fmtId(tbinfo->dobj.name));

		if (binary_upgrade)
			binary_upgrade_set_relfilenodes(q, tbinfo->dobj.catId.oid, false);

		appendPQExpBuffer(q, "CREATE VIEW %s AS\n    %s\n",
						  fmtId(tbinfo->dobj.name), viewdef);

		PQclear(res);
	}
	else
	{
		reltypename = "TABLE";
		numParents = tbinfo->numParents;
		parents = tbinfo->parents;

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP TABLE %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s;\n",
						  fmtId(tbinfo->dobj.name));

		if (binary_upgrade)
			binary_upgrade_set_relfilenodes(q, tbinfo->dobj.catId.oid, false);

		appendPQExpBuffer(q, "CREATE TABLE %s",
						  fmtId(tbinfo->dobj.name));
		if (tbinfo->reloftype)
			appendPQExpBuffer(q, " OF %s", tbinfo->reloftype);
		actual_atts = 0;
		for (j = 0; j < tbinfo->numatts; j++)
		{
			/*
			 * Normally, dump if it's one of the table's own attrs, and not
			 * dropped.  But for binary upgrade, dump all the columns.
			 */
			if ((!tbinfo->inhAttrs[j] && !tbinfo->attisdropped[j]) ||
				binary_upgrade)
			{
				/*
				 * Default value --- suppress if inherited (except in
				 * binary-upgrade case, where we're not doing normal
				 * inheritance) or if it's to be printed separately.
				 */
				bool		has_default = (tbinfo->attrdefs[j] != NULL
								&& (!tbinfo->inhAttrDef[j] || binary_upgrade)
										   && !tbinfo->attrdefs[j]->separate);

				/*
				 * Not Null constraint --- suppress if inherited, except in
				 * binary-upgrade case.
				 */
				bool		has_notnull = (tbinfo->notnull[j]
							  && (!tbinfo->inhNotNull[j] || binary_upgrade));

				if (tbinfo->reloftype && !has_default && !has_notnull)
					continue;

				/* Format properly if not first attr */
				if (actual_atts == 0)
					appendPQExpBuffer(q, " (");
				else
					appendPQExpBuffer(q, ",");
				appendPQExpBuffer(q, "\n    ");
				actual_atts++;

				/* Attribute name */
				appendPQExpBuffer(q, "%s ",
								  fmtId(tbinfo->attnames[j]));

				if (tbinfo->attisdropped[j])
				{
					/*
					 * ALTER TABLE DROP COLUMN clears pg_attribute.atttypid,
					 * so we will not have gotten a valid type name; insert
					 * INTEGER as a stopgap.  We'll clean things up later.
					 */
					appendPQExpBuffer(q, "INTEGER /* dummy */");
					/* Skip all the rest, too */
					continue;
				}

				/* Attribute type */
				if (tbinfo->reloftype)
				{
					appendPQExpBuffer(q, "WITH OPTIONS");
				}
				else if (g_fout->remoteVersion >= 70100)
				{
					appendPQExpBuffer(q, "%s",
									  tbinfo->atttypnames[j]);
				}
				else
				{
					/* If no format_type, fake it */
					appendPQExpBuffer(q, "%s",
									  myFormatType(tbinfo->atttypnames[j],
												   tbinfo->atttypmod[j]));
				}

				if (has_default)
					appendPQExpBuffer(q, " DEFAULT %s",
									  tbinfo->attrdefs[j]->adef_expr);

				if (has_notnull)
					appendPQExpBuffer(q, " NOT NULL");
			}
		}

		/*
		 * Add non-inherited CHECK constraints, if any.
		 */
		for (j = 0; j < tbinfo->ncheck; j++)
		{
			ConstraintInfo *constr = &(tbinfo->checkexprs[j]);

			if (constr->separate || !constr->conislocal)
				continue;

			if (actual_atts == 0)
				appendPQExpBuffer(q, " (\n    ");
			else
				appendPQExpBuffer(q, ",\n    ");

			appendPQExpBuffer(q, "CONSTRAINT %s ",
							  fmtId(constr->dobj.name));
			appendPQExpBuffer(q, "%s", constr->condef);

			actual_atts++;
		}

		if (actual_atts)
			appendPQExpBuffer(q, "\n)");
		else if (!tbinfo->reloftype)
		{
			/*
			 * We must have a parenthesized attribute list, even though empty,
			 * when not using the OF TYPE syntax.
			 */
			appendPQExpBuffer(q, " (\n)");
		}

		if (numParents > 0 && !binary_upgrade)
		{
			appendPQExpBuffer(q, "\nINHERITS (");
			for (k = 0; k < numParents; k++)
			{
				TableInfo  *parentRel = parents[k];

				if (k > 0)
					appendPQExpBuffer(q, ", ");
				if (parentRel->dobj.namespace != tbinfo->dobj.namespace)
					appendPQExpBuffer(q, "%s.",
								fmtId(parentRel->dobj.namespace->dobj.name));
				appendPQExpBuffer(q, "%s",
								  fmtId(parentRel->dobj.name));
			}
			appendPQExpBuffer(q, ")");
		}

		if ((tbinfo->reloptions && strlen(tbinfo->reloptions) > 0) ||
		  (tbinfo->toast_reloptions && strlen(tbinfo->toast_reloptions) > 0))
		{
			bool		addcomma = false;

			appendPQExpBuffer(q, "\nWITH (");
			if (tbinfo->reloptions && strlen(tbinfo->reloptions) > 0)
			{
				addcomma = true;
				appendPQExpBuffer(q, "%s", tbinfo->reloptions);
			}
			if (tbinfo->toast_reloptions && strlen(tbinfo->toast_reloptions) > 0)
			{
				appendPQExpBuffer(q, "%s%s", addcomma ? ", " : "",
								  tbinfo->toast_reloptions);
			}
			appendPQExpBuffer(q, ")");
		}

		appendPQExpBuffer(q, ";\n");

		/*
		 * To create binary-compatible heap files, we have to ensure the same
		 * physical column order, including dropped columns, as in the
		 * original.  Therefore, we create dropped columns above and drop them
		 * here, also updating their attlen/attalign values so that the
		 * dropped column can be skipped properly.	(We do not bother with
		 * restoring the original attbyval setting.)  Also, inheritance
		 * relationships are set up by doing ALTER INHERIT rather than using
		 * an INHERITS clause --- the latter would possibly mess up the column
		 * order.  That also means we have to take care about setting
		 * attislocal correctly, plus fix up any inherited CHECK constraints.
		 */
		if (binary_upgrade)
		{
			for (j = 0; j < tbinfo->numatts; j++)
			{
				if (tbinfo->attisdropped[j])
				{
					appendPQExpBuffer(q, "\n-- For binary upgrade, recreate dropped column.\n");
					appendPQExpBuffer(q, "UPDATE pg_catalog.pg_attribute\n"
									  "SET attlen = %d, "
									  "attalign = '%c', attbyval = false\n"
									  "WHERE attname = ",
									  tbinfo->attlen[j],
									  tbinfo->attalign[j]);
					appendStringLiteralAH(q, tbinfo->attnames[j], fout);
					appendPQExpBuffer(q, "\n  AND attrelid = ");
					appendStringLiteralAH(q, fmtId(tbinfo->dobj.name), fout);
					appendPQExpBuffer(q, "::pg_catalog.regclass;\n");

					appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
									  fmtId(tbinfo->dobj.name));
					appendPQExpBuffer(q, "DROP COLUMN %s;\n",
									  fmtId(tbinfo->attnames[j]));
				}
				else if (!tbinfo->attislocal[j])
				{
					appendPQExpBuffer(q, "\n-- For binary upgrade, recreate inherited column.\n");
					appendPQExpBuffer(q, "UPDATE pg_catalog.pg_attribute\n"
									  "SET attislocal = false\n"
									  "WHERE attname = ");
					appendStringLiteralAH(q, tbinfo->attnames[j], fout);
					appendPQExpBuffer(q, "\n  AND attrelid = ");
					appendStringLiteralAH(q, fmtId(tbinfo->dobj.name), fout);
					appendPQExpBuffer(q, "::pg_catalog.regclass;\n");
				}
			}

			for (k = 0; k < tbinfo->ncheck; k++)
			{
				ConstraintInfo *constr = &(tbinfo->checkexprs[k]);

				if (constr->separate || constr->conislocal)
					continue;

				appendPQExpBuffer(q, "\n-- For binary upgrade, set up inherited constraint.\n");
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
								  fmtId(tbinfo->dobj.name));
				appendPQExpBuffer(q, " ADD CONSTRAINT %s ",
								  fmtId(constr->dobj.name));
				appendPQExpBuffer(q, "%s;\n", constr->condef);
				appendPQExpBuffer(q, "UPDATE pg_catalog.pg_constraint\n"
								  "SET conislocal = false\n"
								  "WHERE contype = 'c' AND conname = ");
				appendStringLiteralAH(q, constr->dobj.name, fout);
				appendPQExpBuffer(q, "\n  AND conrelid = ");
				appendStringLiteralAH(q, fmtId(tbinfo->dobj.name), fout);
				appendPQExpBuffer(q, "::pg_catalog.regclass;\n");
			}

			if (numParents > 0)
			{
				appendPQExpBuffer(q, "\n-- For binary upgrade, set up inheritance this way.\n");
				for (k = 0; k < numParents; k++)
				{
					TableInfo  *parentRel = parents[k];

					appendPQExpBuffer(q, "ALTER TABLE ONLY %s INHERIT ",
									  fmtId(tbinfo->dobj.name));
					if (parentRel->dobj.namespace != tbinfo->dobj.namespace)
						appendPQExpBuffer(q, "%s.",
								fmtId(parentRel->dobj.namespace->dobj.name));
					appendPQExpBuffer(q, "%s;\n",
									  fmtId(parentRel->dobj.name));
				}
			}

			appendPQExpBuffer(q, "\n-- For binary upgrade, set relfrozenxid.\n");
			appendPQExpBuffer(q, "UPDATE pg_catalog.pg_class\n"
							  "SET relfrozenxid = '%u'\n"
							  "WHERE oid = ",
							  tbinfo->frozenxid);
			appendStringLiteralAH(q, fmtId(tbinfo->dobj.name), fout);
			appendPQExpBuffer(q, "::pg_catalog.regclass;\n");
		}

		/* Loop dumping statistics and storage statements */
		for (j = 0; j < tbinfo->numatts; j++)
		{
			/*
			 * Dump per-column statistics information. We only issue an ALTER
			 * TABLE statement if the attstattarget entry for this column is
			 * non-negative (i.e. it's not the default value)
			 */
			if (tbinfo->attstattarget[j] >= 0 &&
				!tbinfo->attisdropped[j])
			{
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
								  fmtId(tbinfo->dobj.name));
				appendPQExpBuffer(q, "ALTER COLUMN %s ",
								  fmtId(tbinfo->attnames[j]));
				appendPQExpBuffer(q, "SET STATISTICS %d;\n",
								  tbinfo->attstattarget[j]);
			}

			/*
			 * Dump per-column storage information.  The statement is only
			 * dumped if the storage has been changed from the type's default.
			 */
			if (!tbinfo->attisdropped[j] && tbinfo->attstorage[j] != tbinfo->typstorage[j])
			{
				switch (tbinfo->attstorage[j])
				{
					case 'p':
						storage = "PLAIN";
						break;
					case 'e':
						storage = "EXTERNAL";
						break;
					case 'm':
						storage = "MAIN";
						break;
					case 'x':
						storage = "EXTENDED";
						break;
					default:
						storage = NULL;
				}

				/*
				 * Only dump the statement if it's a storage type we recognize
				 */
				if (storage != NULL)
				{
					appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
									  fmtId(tbinfo->dobj.name));
					appendPQExpBuffer(q, "ALTER COLUMN %s ",
									  fmtId(tbinfo->attnames[j]));
					appendPQExpBuffer(q, "SET STORAGE %s;\n",
									  storage);
				}
			}

			/*
			 * Dump per-column attributes.
			 */
			if (tbinfo->attoptions[j] && tbinfo->attoptions[j][0] != '\0')
			{
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
								  fmtId(tbinfo->dobj.name));
				appendPQExpBuffer(q, "ALTER COLUMN %s ",
								  fmtId(tbinfo->attnames[j]));
				appendPQExpBuffer(q, "SET (%s);\n",
								  tbinfo->attoptions[j]);
			}
		}
	}

	ArchiveEntry(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
				 tbinfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
			(tbinfo->relkind == RELKIND_VIEW) ? NULL : tbinfo->reltablespace,
				 tbinfo->rolname,
			   (strcmp(reltypename, "TABLE") == 0) ? tbinfo->hasoids : false,
				 reltypename, SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 tbinfo->dobj.dependencies, tbinfo->dobj.nDeps,
				 NULL, NULL);


	/* Dump Table Comments */
	dumpTableComment(fout, tbinfo, reltypename);

	/* Dump comments on inlined table constraints */
	for (j = 0; j < tbinfo->ncheck; j++)
	{
		ConstraintInfo *constr = &(tbinfo->checkexprs[j]);

		if (constr->separate || !constr->conislocal)
			continue;

		dumpTableConstraintComment(fout, constr);
	}

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpAttrDef --- dump an attribute's default-value declaration
 */
static void
dumpAttrDef(Archive *fout, AttrDefInfo *adinfo)
{
	TableInfo  *tbinfo = adinfo->adtable;
	int			adnum = adinfo->adnum;
	PQExpBuffer q;
	PQExpBuffer delq;

	/* Only print it if "separate" mode is selected */
	if (!tbinfo->dobj.dump || !adinfo->separate || dataOnly)
		return;

	/* Don't print inherited defaults, either, except for binary upgrade */
	if (tbinfo->inhAttrDef[adnum - 1] && !binary_upgrade)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	appendPQExpBuffer(q, "ALTER TABLE %s ",
					  fmtId(tbinfo->dobj.name));
	appendPQExpBuffer(q, "ALTER COLUMN %s SET DEFAULT %s;\n",
					  fmtId(tbinfo->attnames[adnum - 1]),
					  adinfo->adef_expr);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "ALTER TABLE %s.",
					  fmtId(tbinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s ",
					  fmtId(tbinfo->dobj.name));
	appendPQExpBuffer(delq, "ALTER COLUMN %s DROP DEFAULT;\n",
					  fmtId(tbinfo->attnames[adnum - 1]));

	ArchiveEntry(fout, adinfo->dobj.catId, adinfo->dobj.dumpId,
				 tbinfo->attnames[adnum - 1],
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname,
				 false, "DEFAULT", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 adinfo->dobj.dependencies, adinfo->dobj.nDeps,
				 NULL, NULL);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * getAttrName: extract the correct name for an attribute
 *
 * The array tblInfo->attnames[] only provides names of user attributes;
 * if a system attribute number is supplied, we have to fake it.
 * We also do a little bit of bounds checking for safety's sake.
 */
static const char *
getAttrName(int attrnum, TableInfo *tblInfo)
{
	if (attrnum > 0 && attrnum <= tblInfo->numatts)
		return tblInfo->attnames[attrnum - 1];
	switch (attrnum)
	{
		case SelfItemPointerAttributeNumber:
			return "ctid";
		case ObjectIdAttributeNumber:
			return "oid";
		case MinTransactionIdAttributeNumber:
			return "xmin";
		case MinCommandIdAttributeNumber:
			return "cmin";
		case MaxTransactionIdAttributeNumber:
			return "xmax";
		case MaxCommandIdAttributeNumber:
			return "cmax";
		case TableOidAttributeNumber:
			return "tableoid";
	}
	write_msg(NULL, "invalid column number %d for table \"%s\"\n",
			  attrnum, tblInfo->dobj.name);
	exit_nicely();
	return NULL;				/* keep compiler quiet */
}

/*
 * dumpIndex
 *	  write out to fout a user-defined index
 */
static void
dumpIndex(Archive *fout, IndxInfo *indxinfo)
{
	TableInfo  *tbinfo = indxinfo->indextable;
	PQExpBuffer q;
	PQExpBuffer delq;

	if (dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/*
	 * If there's an associated constraint, don't dump the index per se, but
	 * do dump any comment for it.	(This is safe because dependency ordering
	 * will have ensured the constraint is emitted first.)
	 */
	if (indxinfo->indexconstraint == 0)
	{
		if (binary_upgrade)
			binary_upgrade_set_relfilenodes(q, indxinfo->dobj.catId.oid, true);

		/* Plain secondary index */
		appendPQExpBuffer(q, "%s;\n", indxinfo->indexdef);

		/* If the index is clustered, we need to record that. */
		if (indxinfo->indisclustered)
		{
			appendPQExpBuffer(q, "\nALTER TABLE %s CLUSTER",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(q, " ON %s;\n",
							  fmtId(indxinfo->dobj.name));
		}

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP INDEX %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s;\n",
						  fmtId(indxinfo->dobj.name));

		ArchiveEntry(fout, indxinfo->dobj.catId, indxinfo->dobj.dumpId,
					 indxinfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 indxinfo->tablespace,
					 tbinfo->rolname, false,
					 "INDEX", SECTION_POST_DATA,
					 q->data, delq->data, NULL,
					 indxinfo->dobj.dependencies, indxinfo->dobj.nDeps,
					 NULL, NULL);
	}

	/* Dump Index Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "INDEX %s",
					  fmtId(indxinfo->dobj.name));
	dumpComment(fout, q->data,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				indxinfo->dobj.catId, 0, indxinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpConstraint
 *	  write out to fout a user-defined constraint
 */
static void
dumpConstraint(Archive *fout, ConstraintInfo *coninfo)
{
	TableInfo  *tbinfo = coninfo->contable;
	PQExpBuffer q;
	PQExpBuffer delq;

	/* Skip if not to be dumped */
	if (!coninfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	if (coninfo->contype == 'p' ||
		coninfo->contype == 'u' ||
		coninfo->contype == 'x')
	{
		/* Index-related constraint */
		IndxInfo   *indxinfo;
		int			k;

		indxinfo = (IndxInfo *) findObjectByDumpId(coninfo->conindex);

		if (indxinfo == NULL)
		{
			write_msg(NULL, "missing index for constraint \"%s\"\n",
					  coninfo->dobj.name);
			exit_nicely();
		}

		if (binary_upgrade && !coninfo->condef)
			binary_upgrade_set_relfilenodes(q, indxinfo->dobj.catId.oid, true);

		appendPQExpBuffer(q, "ALTER TABLE ONLY %s\n",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(q, "    ADD CONSTRAINT %s ",
						  fmtId(coninfo->dobj.name));

		if (coninfo->condef)
		{
			/* pg_get_constraintdef should have provided everything */
			appendPQExpBuffer(q, "%s;\n", coninfo->condef);
		}
		else
		{
			appendPQExpBuffer(q, "%s (",
						 coninfo->contype == 'p' ? "PRIMARY KEY" : "UNIQUE");
			for (k = 0; k < indxinfo->indnkeys; k++)
			{
				int			indkey = (int) indxinfo->indkeys[k];
				const char *attname;

				if (indkey == InvalidAttrNumber)
					break;
				attname = getAttrName(indkey, tbinfo);

				appendPQExpBuffer(q, "%s%s",
								  (k == 0) ? "" : ", ",
								  fmtId(attname));
			}

			appendPQExpBuffer(q, ")");

			if (indxinfo->options && strlen(indxinfo->options) > 0)
				appendPQExpBuffer(q, " WITH (%s)", indxinfo->options);

			if (coninfo->condeferrable)
			{
				appendPQExpBuffer(q, " DEFERRABLE");
				if (coninfo->condeferred)
					appendPQExpBuffer(q, " INITIALLY DEFERRED");
			}

			appendPQExpBuffer(q, ";\n");
		}

		/* If the index is clustered, we need to record that. */
		if (indxinfo->indisclustered)
		{
			appendPQExpBuffer(q, "\nALTER TABLE %s CLUSTER",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(q, " ON %s;\n",
							  fmtId(indxinfo->dobj.name));
		}

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "ALTER TABLE ONLY %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s ",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
						  fmtId(coninfo->dobj.name));

		ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
					 coninfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 indxinfo->tablespace,
					 tbinfo->rolname, false,
					 "CONSTRAINT", SECTION_POST_DATA,
					 q->data, delq->data, NULL,
					 coninfo->dobj.dependencies, coninfo->dobj.nDeps,
					 NULL, NULL);
	}
	else if (coninfo->contype == 'f')
	{
		/*
		 * XXX Potentially wrap in a 'SET CONSTRAINTS OFF' block so that the
		 * current table data is not processed
		 */
		appendPQExpBuffer(q, "ALTER TABLE ONLY %s\n",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
						  fmtId(coninfo->dobj.name),
						  coninfo->condef);

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "ALTER TABLE ONLY %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s ",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
						  fmtId(coninfo->dobj.name));

		ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
					 coninfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 NULL,
					 tbinfo->rolname, false,
					 "FK CONSTRAINT", SECTION_POST_DATA,
					 q->data, delq->data, NULL,
					 coninfo->dobj.dependencies, coninfo->dobj.nDeps,
					 NULL, NULL);
	}
	else if (coninfo->contype == 'c' && tbinfo)
	{
		/* CHECK constraint on a table */

		/* Ignore if not to be dumped separately */
		if (coninfo->separate)
		{
			/* not ONLY since we want it to propagate to children */
			appendPQExpBuffer(q, "ALTER TABLE %s\n",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
							  fmtId(coninfo->dobj.name),
							  coninfo->condef);

			/*
			 * DROP must be fully qualified in case same name appears in
			 * pg_catalog
			 */
			appendPQExpBuffer(delq, "ALTER TABLE %s.",
							  fmtId(tbinfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(delq, "%s ",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
							  fmtId(coninfo->dobj.name));

			ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
						 coninfo->dobj.name,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL,
						 tbinfo->rolname, false,
						 "CHECK CONSTRAINT", SECTION_POST_DATA,
						 q->data, delq->data, NULL,
						 coninfo->dobj.dependencies, coninfo->dobj.nDeps,
						 NULL, NULL);
		}
	}
	else if (coninfo->contype == 'c' && tbinfo == NULL)
	{
		/* CHECK constraint on a domain */
		TypeInfo   *tyinfo = coninfo->condomain;

		/* Ignore if not to be dumped separately */
		if (coninfo->separate)
		{
			appendPQExpBuffer(q, "ALTER DOMAIN %s\n",
							  fmtId(tyinfo->dobj.name));
			appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
							  fmtId(coninfo->dobj.name),
							  coninfo->condef);

			/*
			 * DROP must be fully qualified in case same name appears in
			 * pg_catalog
			 */
			appendPQExpBuffer(delq, "ALTER DOMAIN %s.",
							  fmtId(tyinfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(delq, "%s ",
							  fmtId(tyinfo->dobj.name));
			appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
							  fmtId(coninfo->dobj.name));

			ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
						 coninfo->dobj.name,
						 tyinfo->dobj.namespace->dobj.name,
						 NULL,
						 tyinfo->rolname, false,
						 "CHECK CONSTRAINT", SECTION_POST_DATA,
						 q->data, delq->data, NULL,
						 coninfo->dobj.dependencies, coninfo->dobj.nDeps,
						 NULL, NULL);
		}
	}
	else
	{
		write_msg(NULL, "unrecognized constraint type: %c\n", coninfo->contype);
		exit_nicely();
	}

	/* Dump Constraint Comments --- only works for table constraints */
	if (tbinfo && coninfo->separate)
		dumpTableConstraintComment(fout, coninfo);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpTableConstraintComment --- dump a constraint's comment if any
 *
 * This is split out because we need the function in two different places
 * depending on whether the constraint is dumped as part of CREATE TABLE
 * or as a separate ALTER command.
 */
static void
dumpTableConstraintComment(Archive *fout, ConstraintInfo *coninfo)
{
	TableInfo  *tbinfo = coninfo->contable;
	PQExpBuffer q = createPQExpBuffer();

	appendPQExpBuffer(q, "CONSTRAINT %s ",
					  fmtId(coninfo->dobj.name));
	appendPQExpBuffer(q, "ON %s",
					  fmtId(tbinfo->dobj.name));
	dumpComment(fout, q->data,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				coninfo->dobj.catId, 0,
			 coninfo->separate ? coninfo->dobj.dumpId : tbinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
}

/*
 * findLastBuiltInOid -
 * find the last built in oid
 *
 * For 7.1 and 7.2, we do this by retrieving datlastsysoid from the
 * pg_database entry for the current database
 */
static Oid
findLastBuiltinOid_V71(const char *dbname)
{
	PGresult   *res;
	int			ntups;
	Oid			last_oid;
	PQExpBuffer query = createPQExpBuffer();

	resetPQExpBuffer(query);
	appendPQExpBuffer(query, "SELECT datlastsysoid from pg_database where datname = ");
	appendStringLiteralAH(query, dbname, g_fout);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	if (ntups < 1)
	{
		write_msg(NULL, "missing pg_database entry for this database\n");
		exit_nicely();
	}
	if (ntups > 1)
	{
		write_msg(NULL, "found more than one pg_database entry for this database\n");
		exit_nicely();
	}
	last_oid = atooid(PQgetvalue(res, 0, PQfnumber(res, "datlastsysoid")));
	PQclear(res);
	destroyPQExpBuffer(query);
	return last_oid;
}

/*
 * findLastBuiltInOid -
 * find the last built in oid
 *
 * For 7.0, we do this by assuming that the last thing that initdb does is to
 * create the pg_indexes view.	This sucks in general, but seeing that 7.0.x
 * initdb won't be changing anymore, it'll do.
 */
static Oid
findLastBuiltinOid_V70(void)
{
	PGresult   *res;
	int			ntups;
	int			last_oid;

	res = PQexec(g_conn,
				 "SELECT oid FROM pg_class WHERE relname = 'pg_indexes'");
	check_sql_result(res, g_conn,
					 "SELECT oid FROM pg_class WHERE relname = 'pg_indexes'",
					 PGRES_TUPLES_OK);
	ntups = PQntuples(res);
	if (ntups < 1)
	{
		write_msg(NULL, "could not find entry for pg_indexes in pg_class\n");
		exit_nicely();
	}
	if (ntups > 1)
	{
		write_msg(NULL, "found more than one entry for pg_indexes in pg_class\n");
		exit_nicely();
	}
	last_oid = atooid(PQgetvalue(res, 0, PQfnumber(res, "oid")));
	PQclear(res);
	return last_oid;
}

static void
dumpSequence(Archive *fout, TableInfo *tbinfo)
{
	PGresult   *res;
	char	   *startv,
			   *last,
			   *incby,
			   *maxv = NULL,
			   *minv = NULL,
			   *cache;
	char		bufm[100],
				bufx[100];
	bool		cycled,
				called;
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer delqry = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	snprintf(bufm, sizeof(bufm), INT64_FORMAT, SEQ_MINVALUE);
	snprintf(bufx, sizeof(bufx), INT64_FORMAT, SEQ_MAXVALUE);

	if (g_fout->remoteVersion >= 80400)
	{
		appendPQExpBuffer(query,
						  "SELECT sequence_name, "
						  "start_value, last_value, increment_by, "
				   "CASE WHEN increment_by > 0 AND max_value = %s THEN NULL "
				   "     WHEN increment_by < 0 AND max_value = -1 THEN NULL "
						  "     ELSE max_value "
						  "END AS max_value, "
					"CASE WHEN increment_by > 0 AND min_value = 1 THEN NULL "
				   "     WHEN increment_by < 0 AND min_value = %s THEN NULL "
						  "     ELSE min_value "
						  "END AS min_value, "
						  "cache_value, is_cycled, is_called from %s",
						  bufx, bufm,
						  fmtId(tbinfo->dobj.name));
	}
	else
	{
		appendPQExpBuffer(query,
						  "SELECT sequence_name, "
						  "0 AS start_value, last_value, increment_by, "
				   "CASE WHEN increment_by > 0 AND max_value = %s THEN NULL "
				   "     WHEN increment_by < 0 AND max_value = -1 THEN NULL "
						  "     ELSE max_value "
						  "END AS max_value, "
					"CASE WHEN increment_by > 0 AND min_value = 1 THEN NULL "
				   "     WHEN increment_by < 0 AND min_value = %s THEN NULL "
						  "     ELSE min_value "
						  "END AS min_value, "
						  "cache_value, is_cycled, is_called from %s",
						  bufx, bufm,
						  fmtId(tbinfo->dobj.name));
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) != 1)
	{
		write_msg(NULL, ngettext("query to get data of sequence \"%s\" returned %d row (expected 1)\n",
								 "query to get data of sequence \"%s\" returned %d rows (expected 1)\n",
								 PQntuples(res)),
				  tbinfo->dobj.name, PQntuples(res));
		exit_nicely();
	}

	/* Disable this check: it fails if sequence has been renamed */
#ifdef NOT_USED
	if (strcmp(PQgetvalue(res, 0, 0), tbinfo->dobj.name) != 0)
	{
		write_msg(NULL, "query to get data of sequence \"%s\" returned name \"%s\"\n",
				  tbinfo->dobj.name, PQgetvalue(res, 0, 0));
		exit_nicely();
	}
#endif

	startv = PQgetvalue(res, 0, 1);
	last = PQgetvalue(res, 0, 2);
	incby = PQgetvalue(res, 0, 3);
	if (!PQgetisnull(res, 0, 4))
		maxv = PQgetvalue(res, 0, 4);
	if (!PQgetisnull(res, 0, 5))
		minv = PQgetvalue(res, 0, 5);
	cache = PQgetvalue(res, 0, 6);
	cycled = (strcmp(PQgetvalue(res, 0, 7), "t") == 0);
	called = (strcmp(PQgetvalue(res, 0, 8), "t") == 0);

	/*
	 * The logic we use for restoring sequences is as follows:
	 *
	 * Add a CREATE SEQUENCE statement as part of a "schema" dump (use
	 * last_val for start if called is false, else use min_val for start_val).
	 * Also, if the sequence is owned by a column, add an ALTER SEQUENCE OWNED
	 * BY command for it.
	 *
	 * Add a 'SETVAL(seq, last_val, iscalled)' as part of a "data" dump.
	 */
	if (!dataOnly)
	{
		resetPQExpBuffer(delqry);

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delqry, "DROP SEQUENCE %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delqry, "%s;\n",
						  fmtId(tbinfo->dobj.name));

		resetPQExpBuffer(query);

		if (binary_upgrade)
		{
			binary_upgrade_set_relfilenodes(query, tbinfo->dobj.catId.oid, false);
			binary_upgrade_set_type_oids_by_rel_oid(query, tbinfo->dobj.catId.oid);
		}

		appendPQExpBuffer(query,
						  "CREATE SEQUENCE %s\n",
						  fmtId(tbinfo->dobj.name));

		if (g_fout->remoteVersion >= 80400)
			appendPQExpBuffer(query, "    START WITH %s\n", startv);
		else
		{
			/*
			 * Versions before 8.4 did not remember the true start value.  If
			 * is_called is false then the sequence has never been incremented
			 * so we can use last_val.	Otherwise punt and let it default.
			 */
			if (!called)
				appendPQExpBuffer(query, "    START WITH %s\n", last);
		}

		appendPQExpBuffer(query, "    INCREMENT BY %s\n", incby);

		if (minv)
			appendPQExpBuffer(query, "    MINVALUE %s\n", minv);
		else
			appendPQExpBuffer(query, "    NO MINVALUE\n");

		if (maxv)
			appendPQExpBuffer(query, "    MAXVALUE %s\n", maxv);
		else
			appendPQExpBuffer(query, "    NO MAXVALUE\n");

		appendPQExpBuffer(query,
						  "    CACHE %s%s",
						  cache, (cycled ? "\n    CYCLE" : ""));

		appendPQExpBuffer(query, ";\n");

		/* binary_upgrade:	no need to clear TOAST table oid */

		ArchiveEntry(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
					 tbinfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 NULL,
					 tbinfo->rolname,
					 false, "SEQUENCE", SECTION_PRE_DATA,
					 query->data, delqry->data, NULL,
					 tbinfo->dobj.dependencies, tbinfo->dobj.nDeps,
					 NULL, NULL);

		/*
		 * If the sequence is owned by a table column, emit the ALTER for it
		 * as a separate TOC entry immediately following the sequence's own
		 * entry.  It's OK to do this rather than using full sorting logic,
		 * because the dependency that tells us it's owned will have forced
		 * the table to be created first.  We can't just include the ALTER in
		 * the TOC entry because it will fail if we haven't reassigned the
		 * sequence owner to match the table's owner.
		 *
		 * We need not schema-qualify the table reference because both
		 * sequence and table must be in the same schema.
		 */
		if (OidIsValid(tbinfo->owning_tab))
		{
			TableInfo  *owning_tab = findTableByOid(tbinfo->owning_tab);

			if (owning_tab && owning_tab->dobj.dump)
			{
				resetPQExpBuffer(query);
				appendPQExpBuffer(query, "ALTER SEQUENCE %s",
								  fmtId(tbinfo->dobj.name));
				appendPQExpBuffer(query, " OWNED BY %s",
								  fmtId(owning_tab->dobj.name));
				appendPQExpBuffer(query, ".%s;\n",
						fmtId(owning_tab->attnames[tbinfo->owning_col - 1]));

				ArchiveEntry(fout, nilCatalogId, createDumpId(),
							 tbinfo->dobj.name,
							 tbinfo->dobj.namespace->dobj.name,
							 NULL,
							 tbinfo->rolname,
							 false, "SEQUENCE OWNED BY", SECTION_PRE_DATA,
							 query->data, "", NULL,
							 &(tbinfo->dobj.dumpId), 1,
							 NULL, NULL);
			}
		}

		/* Dump Sequence Comments */
		resetPQExpBuffer(query);
		appendPQExpBuffer(query, "SEQUENCE %s", fmtId(tbinfo->dobj.name));
		dumpComment(fout, query->data,
					tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
					tbinfo->dobj.catId, 0, tbinfo->dobj.dumpId);
	}

	if (!schemaOnly)
	{
		resetPQExpBuffer(query);
		appendPQExpBuffer(query, "SELECT pg_catalog.setval(");
		appendStringLiteralAH(query, fmtId(tbinfo->dobj.name), fout);
		appendPQExpBuffer(query, ", %s, %s);\n",
						  last, (called ? "true" : "false"));

		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tbinfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 NULL,
					 tbinfo->rolname,
					 false, "SEQUENCE SET", SECTION_PRE_DATA,
					 query->data, "", NULL,
					 &(tbinfo->dobj.dumpId), 1,
					 NULL, NULL);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delqry);
}

static void
dumpTrigger(Archive *fout, TriggerInfo *tginfo)
{
	TableInfo  *tbinfo = tginfo->tgtable;
	PQExpBuffer query;
	PQExpBuffer delqry;
	char	   *tgargs;
	size_t		lentgargs;
	const char *p;
	int			findx;

	if (dataOnly)
		return;

	query = createPQExpBuffer();
	delqry = createPQExpBuffer();

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delqry, "DROP TRIGGER %s ",
					  fmtId(tginfo->dobj.name));
	appendPQExpBuffer(delqry, "ON %s.",
					  fmtId(tbinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delqry, "%s;\n",
					  fmtId(tbinfo->dobj.name));

	if (tginfo->tgdef)
	{
		appendPQExpBuffer(query, "%s;\n", tginfo->tgdef);
	}
	else
	{
		if (tginfo->tgisconstraint)
		{
			appendPQExpBuffer(query, "CREATE CONSTRAINT TRIGGER ");
			appendPQExpBufferStr(query, fmtId(tginfo->tgconstrname));
		}
		else
		{
			appendPQExpBuffer(query, "CREATE TRIGGER ");
			appendPQExpBufferStr(query, fmtId(tginfo->dobj.name));
		}
		appendPQExpBuffer(query, "\n    ");

		/* Trigger type */
		findx = 0;
		if (TRIGGER_FOR_BEFORE(tginfo->tgtype))
			appendPQExpBuffer(query, "BEFORE");
		else
			appendPQExpBuffer(query, "AFTER");
		if (TRIGGER_FOR_INSERT(tginfo->tgtype))
		{
			appendPQExpBuffer(query, " INSERT");
			findx++;
		}
		if (TRIGGER_FOR_DELETE(tginfo->tgtype))
		{
			if (findx > 0)
				appendPQExpBuffer(query, " OR DELETE");
			else
				appendPQExpBuffer(query, " DELETE");
			findx++;
		}
		if (TRIGGER_FOR_UPDATE(tginfo->tgtype))
		{
			if (findx > 0)
				appendPQExpBuffer(query, " OR UPDATE");
			else
				appendPQExpBuffer(query, " UPDATE");
			findx++;
		}
		if (TRIGGER_FOR_TRUNCATE(tginfo->tgtype))
		{
			if (findx > 0)
				appendPQExpBuffer(query, " OR TRUNCATE");
			else
				appendPQExpBuffer(query, " TRUNCATE");
			findx++;
		}
		appendPQExpBuffer(query, " ON %s\n",
						  fmtId(tbinfo->dobj.name));

		if (tginfo->tgisconstraint)
		{
			if (OidIsValid(tginfo->tgconstrrelid))
			{
				/* If we are using regclass, name is already quoted */
				if (g_fout->remoteVersion >= 70300)
					appendPQExpBuffer(query, "    FROM %s\n    ",
									  tginfo->tgconstrrelname);
				else
					appendPQExpBuffer(query, "    FROM %s\n    ",
									  fmtId(tginfo->tgconstrrelname));
			}
			if (!tginfo->tgdeferrable)
				appendPQExpBuffer(query, "NOT ");
			appendPQExpBuffer(query, "DEFERRABLE INITIALLY ");
			if (tginfo->tginitdeferred)
				appendPQExpBuffer(query, "DEFERRED\n");
			else
				appendPQExpBuffer(query, "IMMEDIATE\n");
		}

		if (TRIGGER_FOR_ROW(tginfo->tgtype))
			appendPQExpBuffer(query, "    FOR EACH ROW\n    ");
		else
			appendPQExpBuffer(query, "    FOR EACH STATEMENT\n    ");

		/* In 7.3, result of regproc is already quoted */
		if (g_fout->remoteVersion >= 70300)
			appendPQExpBuffer(query, "EXECUTE PROCEDURE %s(",
							  tginfo->tgfname);
		else
			appendPQExpBuffer(query, "EXECUTE PROCEDURE %s(",
							  fmtId(tginfo->tgfname));

		tgargs = (char *) PQunescapeBytea((unsigned char *) tginfo->tgargs,
										  &lentgargs);
		p = tgargs;
		for (findx = 0; findx < tginfo->tgnargs; findx++)
		{
			/* find the embedded null that terminates this trigger argument */
			size_t		tlen = strlen(p);

			if (p + tlen >= tgargs + lentgargs)
			{
				/* hm, not found before end of bytea value... */
				write_msg(NULL, "invalid argument string (%s) for trigger \"%s\" on table \"%s\"\n",
						  tginfo->tgargs,
						  tginfo->dobj.name,
						  tbinfo->dobj.name);
				exit_nicely();
			}

			if (findx > 0)
				appendPQExpBuffer(query, ", ");
			appendStringLiteralAH(query, p, fout);
			p += tlen + 1;
		}
		free(tgargs);
		appendPQExpBuffer(query, ");\n");
	}

	if (tginfo->tgenabled != 't' && tginfo->tgenabled != 'O')
	{
		appendPQExpBuffer(query, "\nALTER TABLE %s ",
						  fmtId(tbinfo->dobj.name));
		switch (tginfo->tgenabled)
		{
			case 'D':
			case 'f':
				appendPQExpBuffer(query, "DISABLE");
				break;
			case 'A':
				appendPQExpBuffer(query, "ENABLE ALWAYS");
				break;
			case 'R':
				appendPQExpBuffer(query, "ENABLE REPLICA");
				break;
			default:
				appendPQExpBuffer(query, "ENABLE");
				break;
		}
		appendPQExpBuffer(query, " TRIGGER %s;\n",
						  fmtId(tginfo->dobj.name));
	}

	ArchiveEntry(fout, tginfo->dobj.catId, tginfo->dobj.dumpId,
				 tginfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname, false,
				 "TRIGGER", SECTION_POST_DATA,
				 query->data, delqry->data, NULL,
				 tginfo->dobj.dependencies, tginfo->dobj.nDeps,
				 NULL, NULL);

	resetPQExpBuffer(query);
	appendPQExpBuffer(query, "TRIGGER %s ",
					  fmtId(tginfo->dobj.name));
	appendPQExpBuffer(query, "ON %s",
					  fmtId(tbinfo->dobj.name));

	dumpComment(fout, query->data,
				tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
				tginfo->dobj.catId, 0, tginfo->dobj.dumpId);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delqry);
}

/*
 * dumpRule
 *		Dump a rule
 */
static void
dumpRule(Archive *fout, RuleInfo *rinfo)
{
	TableInfo  *tbinfo = rinfo->ruletable;
	PQExpBuffer query;
	PQExpBuffer cmd;
	PQExpBuffer delcmd;
	PGresult   *res;

	/* Skip if not to be dumped */
	if (!rinfo->dobj.dump || dataOnly)
		return;

	/*
	 * If it is an ON SELECT rule that is created implicitly by CREATE VIEW,
	 * we do not want to dump it as a separate object.
	 */
	if (!rinfo->separate)
		return;

	/*
	 * Make sure we are in proper schema.
	 */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	query = createPQExpBuffer();
	cmd = createPQExpBuffer();
	delcmd = createPQExpBuffer();

	if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query,
						  "SELECT pg_catalog.pg_get_ruledef('%u'::pg_catalog.oid) AS definition",
						  rinfo->dobj.catId.oid);
	}
	else
	{
		/* Rule name was unique before 7.3 ... */
		appendPQExpBuffer(query,
						  "SELECT pg_get_ruledef('%s') AS definition",
						  rinfo->dobj.name);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) != 1)
	{
		write_msg(NULL, "query to get rule \"%s\" for table \"%s\" failed: wrong number of rows returned\n",
				  rinfo->dobj.name, tbinfo->dobj.name);
		exit_nicely();
	}

	printfPQExpBuffer(cmd, "%s\n", PQgetvalue(res, 0, 0));

	/*
	 * Add the command to alter the rules replication firing semantics if it
	 * differs from the default.
	 */
	if (rinfo->ev_enabled != 'O')
	{
		appendPQExpBuffer(cmd, "ALTER TABLE %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(cmd, "%s ",
						  fmtId(tbinfo->dobj.name));
		switch (rinfo->ev_enabled)
		{
			case 'A':
				appendPQExpBuffer(cmd, "ENABLE ALWAYS RULE %s;\n",
								  fmtId(rinfo->dobj.name));
				break;
			case 'R':
				appendPQExpBuffer(cmd, "ENABLE REPLICA RULE %s;\n",
								  fmtId(rinfo->dobj.name));
				break;
			case 'D':
				appendPQExpBuffer(cmd, "DISABLE RULE %s;\n",
								  fmtId(rinfo->dobj.name));
				break;
		}
	}

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delcmd, "DROP RULE %s ",
					  fmtId(rinfo->dobj.name));
	appendPQExpBuffer(delcmd, "ON %s.",
					  fmtId(tbinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delcmd, "%s;\n",
					  fmtId(tbinfo->dobj.name));

	ArchiveEntry(fout, rinfo->dobj.catId, rinfo->dobj.dumpId,
				 rinfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname, false,
				 "RULE", SECTION_POST_DATA,
				 cmd->data, delcmd->data, NULL,
				 rinfo->dobj.dependencies, rinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump rule comments */
	resetPQExpBuffer(query);
	appendPQExpBuffer(query, "RULE %s",
					  fmtId(rinfo->dobj.name));
	appendPQExpBuffer(query, " ON %s",
					  fmtId(tbinfo->dobj.name));
	dumpComment(fout, query->data,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				rinfo->dobj.catId, 0, rinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(cmd);
	destroyPQExpBuffer(delcmd);
}

/*
 * getDependencies --- obtain available dependency data
 */
static void
getDependencies(void)
{
	PQExpBuffer query;
	PGresult   *res;
	int			ntups,
				i;
	int			i_classid,
				i_objid,
				i_refclassid,
				i_refobjid,
				i_deptype;
	DumpableObject *dobj,
			   *refdobj;

	/* No dependency info available before 7.3 */
	if (g_fout->remoteVersion < 70300)
		return;

	if (g_verbose)
		write_msg(NULL, "reading dependency data\n");

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT "
					  "classid, objid, refclassid, refobjid, deptype "
					  "FROM pg_depend "
					  "WHERE deptype != 'p' "
					  "ORDER BY 1,2");

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_classid = PQfnumber(res, "classid");
	i_objid = PQfnumber(res, "objid");
	i_refclassid = PQfnumber(res, "refclassid");
	i_refobjid = PQfnumber(res, "refobjid");
	i_deptype = PQfnumber(res, "deptype");

	/*
	 * Since we ordered the SELECT by referencing ID, we can expect that
	 * multiple entries for the same object will appear together; this saves
	 * on searches.
	 */
	dobj = NULL;

	for (i = 0; i < ntups; i++)
	{
		CatalogId	objId;
		CatalogId	refobjId;
		char		deptype;

		objId.tableoid = atooid(PQgetvalue(res, i, i_classid));
		objId.oid = atooid(PQgetvalue(res, i, i_objid));
		refobjId.tableoid = atooid(PQgetvalue(res, i, i_refclassid));
		refobjId.oid = atooid(PQgetvalue(res, i, i_refobjid));
		deptype = *(PQgetvalue(res, i, i_deptype));

		if (dobj == NULL ||
			dobj->catId.tableoid != objId.tableoid ||
			dobj->catId.oid != objId.oid)
			dobj = findObjectByCatalogId(objId);

		/*
		 * Failure to find objects mentioned in pg_depend is not unexpected,
		 * since for example we don't collect info about TOAST tables.
		 */
		if (dobj == NULL)
		{
#ifdef NOT_USED
			fprintf(stderr, "no referencing object %u %u\n",
					objId.tableoid, objId.oid);
#endif
			continue;
		}

		refdobj = findObjectByCatalogId(refobjId);

		if (refdobj == NULL)
		{
#ifdef NOT_USED
			fprintf(stderr, "no referenced object %u %u\n",
					refobjId.tableoid, refobjId.oid);
#endif
			continue;
		}

		/*
		 * Ordinarily, table rowtypes have implicit dependencies on their
		 * tables.	However, for a composite type the implicit dependency goes
		 * the other way in pg_depend; which is the right thing for DROP but
		 * it doesn't produce the dependency ordering we need. So in that one
		 * case, we reverse the direction of the dependency.
		 */
		if (deptype == 'i' &&
			dobj->objType == DO_TABLE &&
			refdobj->objType == DO_TYPE)
			addObjectDependency(refdobj, dobj->dumpId);
		else
			/* normal case */
			addObjectDependency(dobj, refdobj->dumpId);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
}


/*
 * selectSourceSchema - make the specified schema the active search path
 * in the source database.
 *
 * NB: pg_catalog is explicitly searched after the specified schema;
 * so user names are only qualified if they are cross-schema references,
 * and system names are only qualified if they conflict with a user name
 * in the current schema.
 *
 * Whenever the selected schema is not pg_catalog, be careful to qualify
 * references to system catalogs and types in our emitted commands!
 */
static void
selectSourceSchema(const char *schemaName)
{
	static char *curSchemaName = NULL;
	PQExpBuffer query;

	/* Not relevant if fetching from pre-7.3 DB */
	if (g_fout->remoteVersion < 70300)
		return;
	/* Ignore null schema names */
	if (schemaName == NULL || *schemaName == '\0')
		return;
	/* Optimize away repeated selection of same schema */
	if (curSchemaName && strcmp(curSchemaName, schemaName) == 0)
		return;

	query = createPQExpBuffer();
	appendPQExpBuffer(query, "SET search_path = %s",
					  fmtId(schemaName));
	if (strcmp(schemaName, "pg_catalog") != 0)
		appendPQExpBuffer(query, ", pg_catalog");

	do_sql_command(g_conn, query->data);

	destroyPQExpBuffer(query);
	if (curSchemaName)
		free(curSchemaName);
	curSchemaName = strdup(schemaName);
}

/*
 * getFormattedTypeName - retrieve a nicely-formatted type name for the
 * given type name.
 *
 * NB: in 7.3 and up the result may depend on the currently-selected
 * schema; this is why we don't try to cache the names.
 */
static char *
getFormattedTypeName(Oid oid, OidOptions opts)
{
	char	   *result;
	PQExpBuffer query;
	PGresult   *res;
	int			ntups;

	if (oid == 0)
	{
		if ((opts & zeroAsOpaque) != 0)
			return strdup(g_opaque_type);
		else if ((opts & zeroAsAny) != 0)
			return strdup("'any'");
		else if ((opts & zeroAsStar) != 0)
			return strdup("*");
		else if ((opts & zeroAsNone) != 0)
			return strdup("NONE");
	}

	query = createPQExpBuffer();
	if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT pg_catalog.format_type('%u'::pg_catalog.oid, NULL)",
						  oid);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT format_type('%u'::oid, NULL)",
						  oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT typname "
						  "FROM pg_type "
						  "WHERE oid = '%u'::oid",
						  oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, query->data);
		exit_nicely();
	}

	if (g_fout->remoteVersion >= 70100)
	{
		/* already quoted */
		result = strdup(PQgetvalue(res, 0, 0));
	}
	else
	{
		/* may need to quote it */
		result = strdup(fmtId(PQgetvalue(res, 0, 0)));
	}

	PQclear(res);
	destroyPQExpBuffer(query);

	return result;
}

/*
 * myFormatType --- local implementation of format_type for use with 7.0.
 */
static char *
myFormatType(const char *typname, int32 typmod)
{
	char	   *result;
	bool		isarray = false;
	PQExpBuffer buf = createPQExpBuffer();

	/* Handle array types */
	if (typname[0] == '_')
	{
		isarray = true;
		typname++;
	}

	/* Show lengths on bpchar and varchar */
	if (!strcmp(typname, "bpchar"))
	{
		int			len = (typmod - VARHDRSZ);

		appendPQExpBuffer(buf, "character");
		if (len > 1)
			appendPQExpBuffer(buf, "(%d)",
							  typmod - VARHDRSZ);
	}
	else if (!strcmp(typname, "varchar"))
	{
		appendPQExpBuffer(buf, "character varying");
		if (typmod != -1)
			appendPQExpBuffer(buf, "(%d)",
							  typmod - VARHDRSZ);
	}
	else if (!strcmp(typname, "numeric"))
	{
		appendPQExpBuffer(buf, "numeric");
		if (typmod != -1)
		{
			int32		tmp_typmod;
			int			precision;
			int			scale;

			tmp_typmod = typmod - VARHDRSZ;
			precision = (tmp_typmod >> 16) & 0xffff;
			scale = tmp_typmod & 0xffff;
			appendPQExpBuffer(buf, "(%d,%d)",
							  precision, scale);
		}
	}

	/*
	 * char is an internal single-byte data type; Let's make sure we force it
	 * through with quotes. - thomas 1998-12-13
	 */
	else if (strcmp(typname, "char") == 0)
		appendPQExpBuffer(buf, "\"char\"");
	else
		appendPQExpBuffer(buf, "%s", fmtId(typname));

	/* Append array qualifier for array types */
	if (isarray)
		appendPQExpBuffer(buf, "[]");

	result = strdup(buf->data);
	destroyPQExpBuffer(buf);

	return result;
}

/*
 * fmtQualifiedId - convert a qualified name to the proper format for
 * the source database.
 *
 * Like fmtId, use the result before calling again.
 */
static const char *
fmtQualifiedId(const char *schema, const char *id)
{
	static PQExpBuffer id_return = NULL;

	if (id_return)				/* first time through? */
		resetPQExpBuffer(id_return);
	else
		id_return = createPQExpBuffer();

	/* Suppress schema name if fetching from pre-7.3 DB */
	if (g_fout->remoteVersion >= 70300 && schema && *schema)
	{
		appendPQExpBuffer(id_return, "%s.",
						  fmtId(schema));
	}
	appendPQExpBuffer(id_return, "%s",
					  fmtId(id));

	return id_return->data;
}

/*
 * Return a column list clause for the given relation.
 *
 * Special case: if there are no undropped columns in the relation, return
 * "", not an invalid "()" column list.
 */
static const char *
fmtCopyColumnList(const TableInfo *ti)
{
	static PQExpBuffer q = NULL;
	int			numatts = ti->numatts;
	char	  **attnames = ti->attnames;
	bool	   *attisdropped = ti->attisdropped;
	bool		needComma;
	int			i;

	if (q)						/* first time through? */
		resetPQExpBuffer(q);
	else
		q = createPQExpBuffer();

	appendPQExpBuffer(q, "(");
	needComma = false;
	for (i = 0; i < numatts; i++)
	{
		if (attisdropped[i])
			continue;
		if (needComma)
			appendPQExpBuffer(q, ", ");
		appendPQExpBuffer(q, "%s", fmtId(attnames[i]));
		needComma = true;
	}

	if (!needComma)
		return "";				/* no undropped columns */

	appendPQExpBuffer(q, ")");
	return q->data;
}

/*
 * Convenience subroutine to execute a SQL command and check for
 * COMMAND_OK status.
 */
static void
do_sql_command(PGconn *conn, const char *query)
{
	PGresult   *res;

	res = PQexec(conn, query);
	check_sql_result(res, conn, query, PGRES_COMMAND_OK);
	PQclear(res);
}

/*
 * Convenience subroutine to verify a SQL command succeeded,
 * and exit with a useful error message if not.
 */
static void
check_sql_result(PGresult *res, PGconn *conn, const char *query,
				 ExecStatusType expected)
{
	const char *err;

	if (res && PQresultStatus(res) == expected)
		return;					/* A-OK */

	write_msg(NULL, "SQL command failed\n");
	if (res)
		err = PQresultErrorMessage(res);
	else
		err = PQerrorMessage(conn);
	write_msg(NULL, "Error message from server: %s", err);
	write_msg(NULL, "The command was: %s\n", query);
	exit_nicely();
}
