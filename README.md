# NoDB - PostgresRAW

EPFL NoDB code used in the Human Brain Project ([general information](http://dias.epfl.ch/nodb)). NoDB (a.k.a PostgresRAW) extends the PostgreSQL DBMS to support queries directly on csv files.

## Building PostgresRAW

To install PostgresRAW you have to follow the same steps as in PostgreSQL. This is done with the usual steps:

```sh
$ CFLAGS=-O0 ./configure --prefix=<installation_path>
$ make
$ make install
```
**Warning**: The PostgresRAW installation directory must use NO symbolic links. Otherwise, PostgresRAW fails to recognize the relative position of configuration files.
**Warning**: When building with GCC 4.8 or later, we have to disable optimisations otherwise PostgreSQL fails to run properly, which is why we have to add `-O0`.

## Using PostgresRAW

 1. Initialize PostgreSQL database cluster:

   ```sh
   $ <installation_path>/bin/initdb -D <PGDATA>
   ```

 2. Create a database:

   ```sh
   $ bin/pg_ctl start -D <PGDATA>
   $ bin/createdb <DBNAME>
   ```

 3. Create a table:

   ```sh
   $ bin/psql <DBNAME>
   $ <DBNAME>=# create table [...]
   ```
   For more information on the `create table` syntax, please refer to the [official documentation](http://www.postgresql.org/docs/9.1/static/sql-createtable.html).

For raw files, PostgresRAW assumes:
 1. that the schema is known **a priori** and registered as a table.
 2. the schema should *map* the structure of the file, as there is *no semi-structured data, nor schema discovery*.
 3. the exposed table will be used in read-only mode, no updates, insert nor delete operations.
 4. that modifications of the data are done directly in the file, in which case PostgresRAW will invalidate its caches as required. If the CSV layout changes, the table needs to be recreated to map to the new layout.

Unless the steps presented below to register the file are taken, the table will use the regular PostgreSQL storage, and will allow all usual operations on the table, even with the RAW file access backend enabled.

See below how to enable and configure PostgresRAW.

## Configuring PostgresRAW

PostgresRAW allows to access data in csv files through empty dummy tables defined in the database.

Each dummy table encodes a file's schema. When those dummy tables are queried, the data is read from the corresponding file directly (assuming the configuration further described here is completed).

### 1. Enabling Raw File support

The PostgreSQL configuration file `postgresql.conf` is found under `<PGDATA>`.

The NoDB parameters are found at the end of this file:

```
#------------------------------------------------------------------------------
# INVISIBLEDB OPTIONS
#------------------------------------------------------------------------------
conf_file                     = 'snoop.conf'
enable_invisible_db           = on

```

 * **conf_file**: Name of the NoDB configuration file:
Uncommenting this line allows the conf_file to be found and read. The conf_file should be stored under `<PGDATA>` (same folder as postgresql.conf).

 * **enable\_invisible\_db**: Enable/Disable NoDB

### 2. Registering files as tables

The conf_file (by default `snoop.conf`) must contain the following structure for each raw text file to register :

```
# Link to data file
filename-1 = '/home/NoDB/datafiles/load.txt'
# Table name (dummy table in the database)
relation-1 = 'persons'
# Delimiter for the file
delimiter-1 = ','
```

Similarly for more files...

```
filename-2 = '/home/NoDB/datafiles/load2.txt'
relation-2 = 'persons2'
delimiter-2 = ','
```

 * **Note 1:**
   For each file (filename-n parameter), the corresponding table name (relation-n parameter) refers to an empty table that must be created in the database, with columns mapping exactly the data in the file. When a query is performed on the empty table, the data will be read from the file directly (if noDB is enabled).

 * **Note 2:**
   For changes in `postgresql.conf` to be applied, you have to restart the DB.

 * **Note 3:**
   For changes in `snoop.conf` to be applied, you have to restart the interactive terminal.

 * **Note 4:**
   For maximum performance, an important action after running any query accessing a table for the first time, iis to subsequently run `ANALYZE <tablename>` where *<tablename>* is the name of the table accessed. This populates the statistics and improves the optimization in case the table is used in joins.
 * **Note 5:**
   For now only full line comments are supported, in other words line which start with `#`.
