/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "database-sqlite3.h"

#include "log.h"
#include "filesys.h"
#include "exceptions.h"
#include "settings.h"
#include "porting.h"
#include "util/string.h"
#include "util/thread.h"
#include "content_sao.h"
#include "remoteplayer.h"

#include <cassert>

// When to print messages when the database is being held locked by another process
// Note: I've seen occasional delays of over 250ms while running minetestmapper.
#define BUSY_INFO_TRESHOLD	100	// Print first informational message after 100ms.
#define BUSY_WARNING_TRESHOLD	250	// Print warning message after 250ms. Lag is increased.
#define BUSY_ERROR_TRESHOLD	1000	// Print error message after 1000ms. Significant lag.
#define BUSY_FATAL_TRESHOLD	3000	// Allow SQLITE_BUSY to be returned, which will cause a minetest crash.
#define BUSY_ERROR_INTERVAL	10000	// Safety net: report again every 10 seconds

// >> KIDSCODE - Threading
#define SQLFATALC(m, c) \
	throw DatabaseException(std::string(m) + ": " + sqlite3_errmsg(c));

#define SQLFATAL(m) SQLFATALC(m, getConnection())

#define SQLRESC(s, r, m, c) \
	if ((s) != (r)) SQLFATALC(m, c)

#define SQLOKC(s, m, c) SQLRESC(s, SQLITE_OK, m, c)
#define SQLRES(s, r, m) SQLRESC(s, r, m, getConnection())
#define SQLOK(s, m) SQLRES(s, SQLITE_OK, m)
#define Q(x) #x
#define QUOTE(x) Q(x)
#define PREPARE_STATEMENT(name, query) m_stmt_##name = statement(QUOTE(name), query);

#define SQLOK_ERRSTREAM(s, m)                           \
	if ((s) != SQLITE_OK) {                             \
		errorstream << (m) << ": "                      \
			<< sqlite3_errmsg(getConnection()) << std::endl; \
	}
/*
#define SQLRES(s, r, m) \
	if ((s) != (r)) { \
		throw DatabaseException(std::string(m) + ": " +\
				sqlite3_errmsg(m_database)); \
	}
#define SQLOK(s, m) SQLRES(s, SQLITE_OK, m)

#define PREPARE_STATEMENT(name, query) \
	SQLOK(sqlite3_prepare_v2(m_database, query, -1, &m_stmt_##name, NULL),\
		"Failed to prepare query '" query "'")

#define SQLOK_ERRSTREAM(s, m)                           \
	if ((s) != SQLITE_OK) {                             \
		errorstream << (m) << ": "                      \
			<< sqlite3_errmsg(m_database) << std::endl; \
	}

#define FINALIZE_STATEMENT(statement) SQLOK_ERRSTREAM(sqlite3_finalize(statement), \
	"Failed to finalize " #statement)
*/
// << KIDSCODE - Threading

int Database_SQLite3::busyHandler(void *data, int count)
{
	s64 &first_time = reinterpret_cast<s64 *>(data)[0];
	s64 &prev_time = reinterpret_cast<s64 *>(data)[1];
	s64 cur_time = porting::getTimeMs();

	if (count == 0) {
		first_time = cur_time;
		prev_time = first_time;
	} else {
		while (cur_time < prev_time)
			cur_time += s64(1)<<32;
	}

	if (cur_time - first_time < BUSY_INFO_TRESHOLD) {
		; // do nothing
	} else if (cur_time - first_time >= BUSY_INFO_TRESHOLD &&
			prev_time - first_time < BUSY_INFO_TRESHOLD) {
		infostream << "SQLite3 database has been locked for "
			<< cur_time - first_time << " ms." << std::endl;
	} else if (cur_time - first_time >= BUSY_WARNING_TRESHOLD &&
			prev_time - first_time < BUSY_WARNING_TRESHOLD) {
		warningstream << "SQLite3 database has been locked for "
			<< cur_time - first_time << " ms." << std::endl;
	} else if (cur_time - first_time >= BUSY_ERROR_TRESHOLD &&
			prev_time - first_time < BUSY_ERROR_TRESHOLD) {
		errorstream << "SQLite3 database has been locked for "
			<< cur_time - first_time << " ms; this causes lag." << std::endl;
	} else if (cur_time - first_time >= BUSY_FATAL_TRESHOLD &&
			prev_time - first_time < BUSY_FATAL_TRESHOLD) {
		errorstream << "SQLite3 database has been locked for "
			<< cur_time - first_time << " ms - giving up!" << std::endl;
	} else if ((cur_time - first_time) / BUSY_ERROR_INTERVAL !=
			(prev_time - first_time) / BUSY_ERROR_INTERVAL) {
		// Safety net: keep reporting every BUSY_ERROR_INTERVAL
		errorstream << "SQLite3 database has been locked for "
			<< (cur_time - first_time) / 1000 << " seconds!" << std::endl;
	}

	prev_time = cur_time;

	// Make sqlite transaction fail if delay exceeds BUSY_FATAL_TRESHOLD
	return cur_time - first_time < BUSY_FATAL_TRESHOLD;
}


Database_SQLite3::Database_SQLite3(const std::string &savedir, const std::string &dbname) :
	m_savedir(savedir),
	m_dbname(dbname)
{
}

// >> KIDSCODE - Threading - New functions

// Open a new connection to the database, eventually opens database.
void Database_SQLite3::openConnection(bool readwrite)
{
	openDatabase();
	if (m_connections.find(std::this_thread::get_id()) != m_connections.end()) {
		errorstream << "SQLite3 database has already a connection for thread "
				<< std::this_thread::get_id() << std::endl;
		return;
	}

	Database_SQLite3_Connection_Info connection_info;
	connection_info.readwrite = readwrite;

	std::string dbp = m_savedir + DIR_DELIM + m_dbname + ".sqlite";

	if (readwrite) { // Open read write database connection
		SQLOKC(sqlite3_open_v2(dbp.c_str(), &(connection_info.connection),
			SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX, NULL),
			std::string("Failed to open SQLite3 database file (read write)") + dbp,
			connection_info.connection);

		SQLOKC(sqlite3_busy_handler(connection_info.connection,
			Database_SQLite3::busyHandler,
			&(connection_info.busy_handler_data)),
			"Failed to set SQLite3 busy handler (read write)",
			connection_info.connection);

		SQLOKC(sqlite3_exec(connection_info.connection,
			"PRAGMA synchronous=NORMAL", NULL, NULL, NULL),
			"Failed to modify sqlite3 synchronous mode",
			connection_info.connection);
		SQLOKC(sqlite3_exec(connection_info.connection,
			"PRAGMA foreign_keys=ON", NULL, NULL, NULL),
			"Failed to enable sqlite3 foreign key support",
			connection_info.connection);
		SQLOKC(sqlite3_exec(connection_info.connection,
			"PRAGMA journal_mode=WAL", NULL, NULL, NULL),
			"Failed to modify sqlite3 journal mode",
			connection_info.connection);
	} else { // Open read only database connection
		SQLOKC(sqlite3_open_v2(dbp.c_str(), &(connection_info.connection),
			SQLITE_OPEN_READONLY|SQLITE_OPEN_FULLMUTEX, NULL),
			std::string("Failed to open SQLite3 database file (read only)") + dbp,
			connection_info.connection);

		SQLOKC(sqlite3_busy_handler(connection_info.connection,
			Database_SQLite3::busyHandler, &(connection_info.busy_handler_data)),
			"Failed to set SQLite3 busy handler (read only)",
			connection_info.connection);
	}

	printf("New Read %s connection on \"%s\" for thread %lx\n",
		readwrite?"Write":"Only", m_dbname.c_str(),
		std::this_thread::get_id());

	m_connections[std::this_thread::get_id()] = connection_info;

	initStatements();
}

// Get connection info, eventually open connection (in default rw or ro mode)
Database_SQLite3_Connection_Info *Database_SQLite3::getConnectionInfo()
{
	if (m_connections.find(std::this_thread::get_id()) == m_connections.end())
		openConnection(getDefaultReadWrite());

	return &(m_connections[std::this_thread::get_id()]);
}

sqlite3 *Database_SQLite3::getConnection()
{
	return getConnectionInfo()->connection;
}

sqlite3_stmt *Database_SQLite3::statement(std::string name) {
	Database_SQLite3_Connection_Info *connection_info = getConnectionInfo();
	auto it = connection_info->statements.find(name);
	if (it != connection_info->statements.end())
		return it->second;
	else
		return nullptr;
}

sqlite3_stmt *Database_SQLite3::statement(std::string name, std::string query)
{
	Database_SQLite3_Connection_Info *connection_info = getConnectionInfo();
	auto it = connection_info->statements.find(name);
	if (it != connection_info->statements.end())
		return it->second;

	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(connection_info->connection, query.c_str(), -1,
			&stmt, NULL) != SQLITE_OK) {
		errorstream << "SQLite3 failed to prepare query \"" <<
				query << "\"" << std::endl;
		return nullptr;
	}

	connection_info->statements[name] = stmt;
	return stmt;
}
// << KIDSCOCDE - Threading

// >> KIDSCODE - Threading - Removed and replaced functions
void Database_SQLite3::beginSave()
 {
	Database_SQLite3_Connection_Info *connection_info = getConnectionInfo();
	if (!connection_info->readwrite) {
		errorstream
		<< "SQLite3 database can't 'beginSave' on a read only connection."
		<< std::endl;
	}
	sqlite3_stmt *stmt = statement("begin", "BEGIN;");
	SQLRES(sqlite3_step(stmt), SQLITE_DONE,
		"Failed to start SQLite3 transaction");
	sqlite3_reset(stmt);
}

/*
void Database_SQLite3::beginSave()
{
	verifyDatabase();
	SQLRES(sqlite3_step(m_stmt_begin), SQLITE_DONE,
		"Failed to start SQLite3 transaction");
	sqlite3_reset(m_stmt_begin);
}
*/

void Database_SQLite3::endSave()
{
	Database_SQLite3_Connection_Info *connection_info = getConnectionInfo();
	if (!connection_info->readwrite) {
		errorstream
			<< "SQLite3 database can't 'endSave' on a read only connection."
			<< std::endl;
	}
	sqlite3_stmt *stmt = statement("end", "COMMIT;");
	SQLRES(sqlite3_step(stmt), SQLITE_DONE,
		"Failed to commit SQLite3 transaction");
	sqlite3_reset(stmt);
}

/*
void Database_SQLite3::endSave()
{
	verifyDatabase();
	SQLRES(sqlite3_step(m_stmt_end), SQLITE_DONE,
		"Failed to commit SQLite3 transaction");
	sqlite3_reset(m_stmt_end);
}
*/

void Database_SQLite3::openDatabase()
{
	m_open_database_mutex.lock();
	if (m_ready) {
		m_open_database_mutex.unlock();
		return;
	}

	sqlite3 *connection = nullptr;
	std::string dbp = m_savedir + DIR_DELIM + m_dbname + ".sqlite";

	// First do preparation : creation or update
	if (!fs::CreateAllDirs(m_savedir)) {
		infostream << "Database_SQLite3: Failed to create directory \""
				<< m_savedir << "\"" << std::endl;
		m_open_database_mutex.unlock();
		throw FileNotGoodException("Failed to create database save directory");
	}

	if (!fs::PathExists(dbp)) {
		// TODO : Is sqlerrm working when closing / openning connection ??
		SQLOKC(sqlite3_open_v2(dbp.c_str(), &connection,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL),
		std::string("Failed to open SQLite3 database file (initialization)") + dbp,
			connection);
		createDatabase(connection);
	} else {
		SQLOKC(sqlite3_open_v2(dbp.c_str(), &connection,
			SQLITE_OPEN_READWRITE, NULL),
		std::string("Failed to open SQLite3 database file (initialization)") + dbp,
			connection);
	}
	updateDatabase(connection);

	// TODO : Is sqlerrm working when closing / openning connection ??
	SQLOKC(sqlite3_close_v2(connection),
		std::string("Failed to close SQLite3 connection (initialization)"),
		connection);

	m_ready = true;
	m_open_database_mutex.unlock();
}

/*
void Database_SQLite3::openDatabase()
{
	if (m_database) return;

	std::string dbp = m_savedir + DIR_DELIM + m_dbname + ".sqlite";

	// Open the database connection

	if (!fs::CreateAllDirs(m_savedir)) {
		infostream << "Database_SQLite3: Failed to create directory \""
			<< m_savedir << "\"" << std::endl;
		throw FileNotGoodException("Failed to create database "
				"save directory");
	}

	bool needs_create = !fs::PathExists(dbp);

	SQLOK(sqlite3_open_v2(dbp.c_str(), &m_database,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL),
		std::string("Failed to open SQLite3 database file ") + dbp);

	SQLOK(sqlite3_busy_handler(m_database, Database_SQLite3::busyHandler,
		m_busy_handler_data), "Failed to set SQLite3 busy handler");

	if (needs_create) {
		createDatabase();
	}

	std::string query_str = std::string("PRAGMA synchronous = ")
			 + itos(g_settings->getU16("sqlite_synchronous"));
	SQLOK(sqlite3_exec(m_database, query_str.c_str(), NULL, NULL, NULL),
		"Failed to modify sqlite3 synchronous mode");
	SQLOK(sqlite3_exec(m_database, "PRAGMA foreign_keys = ON", NULL, NULL, NULL),
		"Failed to enable sqlite3 foreign key support");
}
*/

/* (Removed)
void Database_SQLite3::verifyDatabase()
{
	if (m_initialized) return;

	openDatabase();

	PREPARE_STATEMENT(begin, "BEGIN;");
	PREPARE_STATEMENT(end, "COMMIT;");

	initStatements();

	m_initialized = true;
}
*/

Database_SQLite3::~Database_SQLite3()
{
	// Finalize statements and close connections
	for (auto & cit : m_connections) {
		for (auto & sit : cit.second.statements)
			SQLOK_ERRSTREAM(sqlite3_finalize(sit.second), \
				std::string("Failed to finalize statement ") + sit.first)
		SQLOK_ERRSTREAM(sqlite3_close(cit.second.connection), \
			"Failed to close database connection");
	}
}
/*
Database_SQLite3::~Database_SQLite3()
{
	FINALIZE_STATEMENT(m_stmt_begin)
	FINALIZE_STATEMENT(m_stmt_end)

	SQLOK_ERRSTREAM(sqlite3_close(m_database), "Failed to close database");
}
*/
// << KIDSCODE - Threading

bool Database_SQLite3::tableExists(sqlite3 *connection, const std::string &table_name)
{
	sqlite3_stmt *stmt;

	assert(connection); // Pre-condition

	SQLOKC(sqlite3_prepare_v2(connection,
			"SELECT * FROM `sqlite_master` where type='table' and name=?;",
			-1, &stmt, NULL),
		"tableExists: Failed to verify existence of table (prepare)", connection);

	str_to_sqlite(stmt, 1, table_name);
	bool res = (sqlite3_step(stmt) == SQLITE_ROW);
	sqlite3_finalize(stmt);
	return res;
}

// >> KIDSCODE - Map versionning
/*
 * Map data purge thread
 */

class PurgeDataSQLite3Thread : public Thread
{
public:
	PurgeDataSQLite3Thread(MapDatabaseSQLite3 *database):
		Thread("PurgeData"),
		m_database(database)
	{
		time(&m_lastpurge);
	}

	void *run();

private:
	MapDatabaseSQLite3 *m_database;
	time_t m_lastpurge;
};

void *PurgeDataSQLite3Thread::run() {
	m_database->setThreadWriteAccess(true);

	while (!stopRequested()) {

		// This is done to avoid waiting end of sleep time in case of thread
		// stop. Max wait in that case is 100ms, but purge atempts still occure
		// max each seconds.
		if (difftime(time(nullptr), m_lastpurge) < 1.0f) {
			sleep_ms(100);
			continue;
		}

		// Purge some blocks
		m_database->purgeUnreferencedBlocks(10000);

		// Actually count purge time in last purge time
		time(&m_lastpurge);
	}

	return nullptr;
}
// << KIDSCODE - Map versionning

/*
 * Map database
 */

// >> KIDSCODE
const char *c_qry_load_block = "SELECT `data` FROM `blocks` WHERE `pos` = ? LIMIT 1";
// << KIDSCODE

MapDatabaseSQLite3::MapDatabaseSQLite3(const std::string &savedir):
	Database_SQLite3(savedir, "map"),
	MapDatabase()
{
	// >> KIDSCODE - Map versionning
	// Map data purge thread
	m_purgethread = new PurgeDataSQLite3Thread(this);
	m_purgethread->start();
	// << KIDSCODE
}

MapDatabaseSQLite3::~MapDatabaseSQLite3()
{

	// >> KIDSCODE - Map versionning
	infostream<<"Sqlite3: Stopping and waiting purge thread"<<std::endl;
	m_purgethread->stop();
	m_purgethread->wait();
	delete m_purgethread;
	infostream<<"Sqlite3: Purge thread stopped"<<std::endl;
	// << KIDSCODE - Map versionning

	// >> KIDSCODE - Threads
	/*
	FINALIZE_STATEMENT(m_stmt_read)
	FINALIZE_STATEMENT(m_stmt_write)
	FINALIZE_STATEMENT(m_stmt_list)
	FINALIZE_STATEMENT(m_stmt_delete)
	*/
	// << KIDSCODE
}

// >> KIDSCODE - Map versionning
// This method creates or upgrades database structures
void MapDatabaseSQLite3::upgradeDatabaseStructure(sqlite3 *connection)
{
	printf("[SQLITE] Checking if database needs upgrade.\n");
	assert(connection);

	// Rely on this clue to determine if the database have been upgraded to
	// versioned blocks version.
	if (tableExists(connection, "versioned_blocks")) {
		printf("[SQLITE] Database does not need to be upgraded.\n");
		return;
	}

	printf("[SQLITE] Upgrading database, please wait, this may take some time.\n");

	SQLOKC(sqlite3_exec(connection, "BEGIN;", NULL, NULL, NULL),
		"upgradeDatabaseStructure: Failed to start SQLite3 transaction", connection);

	SQLOKC(sqlite3_exec(connection, R""""(
		CREATE TABLE versioned_blocks (
		  pos INT,
		  version_id INT,
			visible BOOLEAN,
			mtime INT,
		  data BLOB,
		  PRIMARY KEY(pos, version_id)
		);

		CREATE INDEX versioned_blocks_mtime_ix ON versioned_blocks(mtime);
		CREATE INDEX versioned_blocks_version_id_ix ON versioned_blocks(version_id);

		CREATE TABLE versions (
		  id INT PRIMARY KEY,
		  status CHAR(1),
		  name VARCHAR(50),
		  parent_id INT
		);

		INSERT INTO versions VALUES (0, '0', '.origin', NULL);
		INSERT INTO versions VALUES (1, 'A', '.init', 0);

		CREATE VIEW version_tree AS
		  WITH RECURSIVE r(id, parent_id, start_status, start_id) AS (
		    SELECT id, parent_id, status, id FROM versions
		     WHERE status IN ('0', 'A', 'C', 'D')
		    UNION ALL
		    SELECT v.id, v.parent_id, r.start_status, r.start_id FROM r, versions v
		     WHERE r.parent_id = v.id AND status IN ('0', 'A', 'C', 'D'))
		  SELECT * FROM r;

		CREATE TABLE current_versions AS
		  SELECT * FROM version_tree WHERE start_status = 'C';

		-- Migration if already populated map
		CREATE TABLE IF NOT EXISTS blocks(pos INT, data BLOB);
		INSERT INTO versioned_blocks SELECT pos, 0, 0, 0, data FROM blocks;
		DROP TABLE blocks;

		CREATE VIEW blocks AS
		  SELECT pos, data, mtime
		    FROM versioned_blocks
		   WHERE visible = 1;

		CREATE TRIGGER blocks_insert INSTEAD OF INSERT ON blocks FOR EACH ROW
		BEGIN
		  INSERT OR REPLACE INTO versioned_blocks(pos, version_id, visible, mtime, data)
		    SELECT new.pos, id, 1, strftime('%s', 'now'), new.data
		      FROM versions WHERE status = 'C';
		  UPDATE versioned_blocks SET visible = 0
		   WHERE pos = new.pos
		     AND version_id <> (SELECT id FROM versions WHERE status = 'C');
		END;

		CREATE TRIGGER blocks_update INSTEAD OF UPDATE ON blocks FOR EACH ROW
		BEGIN
		  INSERT OR REPLACE INTO versioned_blocks(pos, version_id, visible, mtime, data)
		    SELECT new.pos, id, 1, strftime('%s', 'now'), new.data
		      FROM versions WHERE status = 'C';
		  UPDATE versioned_blocks SET visible = 0
		   WHERE pos = new.pos
		  AND version_id <> (SELECT id FROM versions WHERE status = 'C');
		END;

		CREATE TRIGGER blocks_delete INSTEAD OF DELETE ON blocks FOR EACH ROW
		BEGIN
		  -- Not properly managed. Actually versionning cant "remove" a block from
		  -- a version without showing the previous one. So we delete all in all versions
		   DELETE FROM versioned_blocks WHERE pos = old.pos;
		END;
		)"""", NULL, NULL, NULL),
		"upgradeDatabaseStructure: Unable to upgrade database", connection);

	printf("[SQLITE] Database upgrade: creating first version.\n");

	setCurrentVersion(1, connection);

	SQLOKC(sqlite3_exec(connection, "COMMIT;", NULL, NULL, NULL),
		"upgradeDatabaseStructure: Failed to commit SQLite3 transaction", connection);

	printf("[SQLITE] Database upgrade done.\n");

}
// << KIDSCODE - Map versionning


// >> KIDSCODE - Threading
void MapDatabaseSQLite3::createDatabase(sqlite3 *connection)
//void MapDatabaseSQLite3::createDatabase()
// << KIDSCODE - Threading
{
	upgradeDatabaseStructure(connection); // KIDSCODE - Map versionning
}

// >> KIDSCODE - Threading (statements are per thread now)
/*
void MapDatabaseSQLite3::initStatements()
{
	upgradeDatabaseStructure(connection); // KIDSCODE - Map versionning

	PREPARE_STATEMENT(read, "SELECT `data` FROM `blocks` WHERE `pos` = ? LIMIT 1");
#ifdef __ANDROID__
	PREPARE_STATEMENT(write,  "INSERT INTO `blocks` (`pos`, `data`) VALUES (?, ?)");
#else
	PREPARE_STATEMENT(write, "REPLACE INTO `blocks` (`pos`, `data`) VALUES (?, ?)");
#endif
	PREPARE_STATEMENT(delete, "DELETE FROM `blocks` WHERE `pos` = ?");
	PREPARE_STATEMENT(list, "SELECT `pos` FROM `blocks`");

	verbosestream << "ServerMap: SQLite3 database opened." << std::endl;
}
*/
// << KIDSCODE - Threading

void MapDatabaseSQLite3::updateDatabase(sqlite3 *connection)
{
	upgradeDatabaseStructure(connection); // KIDSCODE - Map versionning
}

inline void MapDatabaseSQLite3::bindPos(sqlite3_stmt *stmt, const v3s16 &pos, int index)
{
	SQLOK(sqlite3_bind_int64(stmt, index, getBlockAsInteger(pos)),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));
}

bool MapDatabaseSQLite3::deleteBlock(const v3s16 &pos)
{
	// >> KIDSCODE - Threading
	sqlite3_stmt *stmt = statement("delete",
		"DELETE FROM `blocks` WHERE `pos` = ?");

	bindPos(stmt, pos);

	bool good = sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_reset(stmt);

	/*
	verifyDatabase();

	bindPos(m_stmt_delete, pos);

	bool good = sqlite3_step(m_stmt_delete) == SQLITE_DONE;
	sqlite3_reset(m_stmt_delete);
	*/
	// << KIDSCODE - Threading

	if (!good) {
		warningstream << "deleteBlock: Block failed to delete "
			<< PP(pos) << ": " << sqlite3_errmsg(getConnection()) << std::endl; // KIDSCODE - Threading
//			<< PP(pos) << ": " << sqlite3_errmsg(m_database) << std::endl;
	}
	return good;
}

bool MapDatabaseSQLite3::saveBlock(const v3s16 &pos, const std::string &data)
{
	// >> KIDSCODE - Threading
	sqlite3_stmt *stmt;
	// verifyDatabase();
	// << KIDSCODE - Threading

#ifdef __ANDROID__
// TODO: TO BE UPGRADED AND TESTED, WONT WORK WITH 'versions' TABLE
	/**
	 * Note: For some unknown reason SQLite3 fails to REPLACE blocks on Android,
	 * deleting them and then inserting works.
	 */
	// >> KIDSCODE - Threading
	stmt = statement("load", c_qry_load_block);

	bindPos(stmt, pos);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		deleteBlock(pos);
	}
	sqlite3_reset(stmt);

	stmt = statement("save", "INSERT INTO `blocks`"
		"(`pos`, `data`) VALUES (?, ?)");

#else
	stmt = statement("save",
		"REPLACE INTO `blocks` (`pos`, `data`) VALUES (?, ?)");
	/*
	bindPos(m_stmt_read, pos);

	if (sqlite3_step(m_stmt_read) == SQLITE_ROW) {
		deleteBlock(pos);
	}
	sqlite3_reset(m_stmt_read);
	*/
	// << KIDSCODE - Threading
#endif
	// >> KIDSCODE - Threading
	bindPos(stmt, pos);
	SQLOK(sqlite3_bind_blob(stmt, 2, data.data(), data.size(), NULL),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));

	SQLRES(sqlite3_step(stmt), SQLITE_DONE, "Failed to save block")
	sqlite3_reset(stmt);
	/*
	bindPos(m_stmt_write, pos);
	SQLOK(sqlite3_bind_blob(m_stmt_write, 2, data.data(), data.size(), NULL),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));

	SQLRES(sqlite3_step(m_stmt_write), SQLITE_DONE, "Failed to save block")
	sqlite3_reset(m_stmt_write);
	*/
	// << KIDSCODE - Threading

	return true;
}

void MapDatabaseSQLite3::loadBlock(const v3s16 &pos, std::string *block)
{
	// >> KIDSCODE - Threading
	sqlite3_stmt *stmt = statement("load", c_qry_load_block);
	bindPos(stmt, pos);

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		sqlite3_reset(stmt);
		return;
	}

	const char *data = (const char *) sqlite3_column_blob(stmt, 0);
	size_t len = sqlite3_column_bytes(stmt, 0);

	*block = (data) ? std::string(data, len) : "";

	sqlite3_step(stmt);
	// We should never get more than 1 row, so ok to reset
	sqlite3_reset(stmt);
	/*
	verifyDatabase();

	bindPos(m_stmt_read, pos);

	if (sqlite3_step(m_stmt_read) != SQLITE_ROW) {
		sqlite3_reset(m_stmt_read);
		return;
	}

	const char *data = (const char *) sqlite3_column_blob(m_stmt_read, 0);
	size_t len = sqlite3_column_bytes(m_stmt_read, 0);

	*block = (data) ? std::string(data, len) : "";

	sqlite3_step(m_stmt_read);
	// We should never get more than 1 row, so ok to reset
	sqlite3_reset(m_stmt_read);
	*/
	// << KIDSCODE - Threading
}

void MapDatabaseSQLite3::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	// >> KIDSCODE - Threading
	sqlite3_stmt *stmt = statement("list", "SELECT `pos` FROM `blocks`");

	while (sqlite3_step(stmt) == SQLITE_ROW)
		dst.push_back(getIntegerAsBlock(sqlite3_column_int64(stmt, 0)));

	sqlite3_reset(stmt);

	/*
	verifyDatabase();

	while (sqlite3_step(m_stmt_list) == SQLITE_ROW)
		dst.push_back(getIntegerAsBlock(sqlite3_column_int64(m_stmt_list, 0)));

	sqlite3_reset(m_stmt_list);
	*/
	// << KIDSCODE - Threading
}

// >> KIDSCODE - Map versionning
// Should be called inside a transaction (between beginSave() and endSave())
void MapDatabaseSQLite3::setCurrentVersion(int id, sqlite3* connection)
{
	if (! connection) {
		connection = getConnection();
	}
	sqlite3_stmt *stmt;

	// Mark old current for purge
	SQLOK(sqlite3_exec(connection,
		"UPDATE versions SET status = 'P' WHERE status = 'C'", NULL, NULL, NULL),
		"setCurrentVersion: Failed to mark old current for purge");

	// Create new current
	SQLOK(sqlite3_prepare_v2(connection,
		"INSERT INTO versions SELECT MAX(id)+1, 'C', '.current', ? FROM versions",
		-1, &stmt, NULL),
		"setCurrentVersion: Failed to create new current version (prepare)");
	int_to_sqlite(stmt, 1, id);
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		throw DatabaseException(
			"setCurrentVersion: Failed to create new current version (step): " +
			std::string(sqlite3_errmsg(connection)));
	}

	// Rebuild current_versions table
	SQLOK(sqlite3_exec(connection,
		"DELETE FROM current_versions", NULL, NULL, NULL),
		"setCurrentVersion: Rebuild current_versions table (delete)");

	SQLOK(sqlite3_exec(connection,
		"INSERT INTO current_versions SELECT * FROM version_tree "
		"WHERE start_status = 'C'", NULL, NULL, NULL),
		"setCurrentVersion: Rebuild current_versions table (insert)");

	// Update blocks visibility
	SQLOK(sqlite3_exec(connection,
		"UPDATE versioned_blocks SET visible = 1, mtime = strftime('%s', 'now')"
		" WHERE visible = 0 AND version_id = ("
		"      SELECT version_id FROM versioned_blocks b, current_versions v"
		"       WHERE v.id = b.version_id AND b.pos = versioned_blocks.pos"
		"       ORDER BY b.version_id DESC LIMIT 1)", NULL, NULL, NULL),
		"setCurrentVersion: Unable to update blocks to invisible");

	SQLOK(sqlite3_exec(connection,
		"UPDATE versioned_blocks SET visible = 0"
		" WHERE visible = 1 AND version_id NOT IN( "
		"  SELECT version_id FROM current_versions)", NULL, NULL, NULL),
		"setCurrentVersion: Unable to update blocks to visible");

	SQLOK(sqlite3_exec(connection,
		"UPDATE versioned_blocks SET visible = 0"
		" WHERE visible = 1 AND version_id <> ("
		"      SELECT version_id FROM versioned_blocks b, current_versions v"
		"       WHERE v.id = b.version_id AND b.pos = versioned_blocks.pos"
		"       ORDER BY b.version_id DESC LIMIT 1)", NULL, NULL, NULL),
		"setCurrentVersion: Unable to update blocks to visible");

	sqlite3_finalize(stmt);
}

int MapDatabaseSQLite3::getVersionByName(const std::string &name)
{
	sqlite3 *connection = getConnection();
	sqlite3_stmt *stmt;
	int id;

	SQLOK(sqlite3_prepare_v2(connection,
			"SELECT id FROM versions WHERE status = 'A' AND name = ?",
			-1, &stmt, NULL),
		"getVersionByName: Failed to find version (prepare)");
	str_to_sqlite(stmt, 1, name);
	switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			id = sqlite_to_int(stmt, 0);
			sqlite3_finalize(stmt);
			return id;
		case SQLITE_DONE:
			sqlite3_finalize(stmt);
			throw DatabaseException(
				"getVersionByName: Version '" + name + "' not found.");
		default:
			sqlite3_finalize(stmt);
			throw DatabaseException("getVersionByName: Failed to find version: " +
				std::string(sqlite3_errmsg(connection)));
	}
}

// Backups list
void MapDatabaseSQLite3::listBackups(std::vector<std::string> &dst)
{
	sqlite3 *connection = getConnection();
	sqlite3_stmt *stmt;

	SQLOK(sqlite3_prepare_v2(connection,
		"SELECT name FROM versions WHERE status = 'A'",
		-1, &stmt, NULL),
		"listSavepoversionningListBackupsints: Failed to get list of backups (prepare)");

	while (sqlite3_step(stmt) == SQLITE_ROW)
		dst.push_back(sqlite_to_string(stmt, 0));

	sqlite3_finalize(stmt);
}

bool MapDatabaseSQLite3::createBackup(const std::string &name)
{
	sqlite3 *connection = getConnection();
	sqlite3_stmt *stmt;
	int id;

	beginSave();

	SQLOK(sqlite3_prepare_v2(connection,
		"SELECT 1 FROM versions WHERE status = 'A' AND name = ?",
		-1, &stmt, NULL),
		"newBackup: Failed to check existing backup (prepare)");
	str_to_sqlite(stmt, 1, name);
	switch (sqlite3_step(stmt))
	{
		case SQLITE_DONE:
			break;
		case SQLITE_ROW:
			sqlite3_finalize(stmt);
			return false; // Savepoint already exists
		default:
			throw DatabaseException(
				"newBackup: Failed to check existing backup (step): " +
				std::string(sqlite3_errmsg(connection)));
	}
	sqlite3_finalize(stmt);

	SQLOK(sqlite3_prepare_v2(connection,
		"SELECT id FROM versions WHERE status = 'C'", -1, &stmt, NULL),
		"newBackup: Failed to get current version (prepare)");
	switch (sqlite3_step(stmt))
	{
		case SQLITE_ROW:
			id =  sqlite_to_int(stmt, 0);
			sqlite3_finalize(stmt);
			break;
		case SQLITE_DONE:
			sqlite3_finalize(stmt);
			throw DatabaseException("newBackup: Current version not found!");
		default:
			sqlite3_finalize(stmt);
			throw DatabaseException(
				"newBackup: Failed to get current version (step): " +
				std::string(sqlite3_errmsg(connection)));
	}

	SQLOK(sqlite3_prepare_v2(connection,
		"UPDATE versions SET status = 'A', name = ? WHERE id = ?;",
		-1, &stmt, NULL),
		"newBackup: Failed to change current version to backup (prepare)");
	str_to_sqlite(stmt, 1, name);
	int_to_sqlite(stmt, 2, id);
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		throw DatabaseException(
			"newCurrentVersion: Failed to activate versions (step): " +
			std::string(sqlite3_errmsg(connection)));
	}

	sqlite3_finalize(stmt);

	setCurrentVersion(id);

	endSave();

	return true;
}

void MapDatabaseSQLite3::restoreBackup(const std::string &name)
{
	beginSave();
	setCurrentVersion(getVersionByName(name));
	endSave();
}

void MapDatabaseSQLite3::deleteBackup(const std::string &name) {
	int id = getVersionByName(name);
	sqlite3 *connection = getConnection();
	sqlite3_stmt *stmt;
	SQLOK(sqlite3_prepare_v2(connection,
			"UPDATE versions SET status = 'D' WHERE id = ?",
			-1, &stmt, NULL),
		"deleteBackup: Failed mark version for deletion (prepare)");
	int_to_sqlite(stmt, 1, id);
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		throw DatabaseException("deleteBackup: Failed mark version for deletion (step): " +
			std::string(sqlite3_errmsg(connection)));
	}
	sqlite3_finalize(stmt);
}

void MapDatabaseSQLite3::purgeUnreferencedBlocks(int limit)
{
	sqlite3 *connection = getConnection();
	sqlite3_stmt *stmt;
	int res;

	beginSave();

	try {
		// Mark for purge deleted versions with no child
		if ((res = sqlite3_exec(connection,
				"UPDATE versions SET status = 'P' WHERE status = 'D'"
				" AND NOT EXISTS (SELECT 1 FROM versions AS v"
				"  WHERE v.status <> 'P' and v.parent_id = versions.id);",
				NULL, NULL, NULL)))
			throw(res);

		// Delete already purged versions
		if ((res = sqlite3_exec(connection,
				"DELETE FROM versions WHERE status = 'P'"
				" AND NOT EXISTS (SELECT 1 FROM versioned_blocks"
				"  WHERE versioned_blocks.version_id = versions.id);",
				NULL, NULL, NULL)))
			throw(res);

		// Get first version to purge
		if ((res = sqlite3_prepare_v2(connection,
				"SELECT id FROM versions WHERE status = 'P' LIMIT 1",
				-1, &stmt, NULL)))
			throw(res);

		res = sqlite3_step(stmt);
		if (res == SQLITE_DONE) {
			endSave();
			sqlite3_finalize(stmt);
			return;
		}
		if (res != SQLITE_ROW)
			throw(res);

		int version =  sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		if ((res = sqlite3_prepare_v2(connection,
				"DELETE FROM versioned_blocks WHERE version_id = ? "
				"  AND pos IN (SELECT pos FROM versioned_blocks"
				"    WHERE version_id = ? LIMIT ?)",
				-1, &stmt, NULL)))
			throw(res);

		if ((res = sqlite3_bind_int(stmt, 1, version)))
			throw(res);
		if ((res = sqlite3_bind_int(stmt, 2, version)))
			throw(res);
		if ((res = sqlite3_bind_int(stmt, 3, limit)))
			throw(res);

		res = sqlite3_step(stmt);
		if (res != SQLITE_DONE)
			throw(res);

	} catch (int res) {
		switch(res) {
		case SQLITE_LOCKED:
			printf("SQLITE_LOCKED");
			break;
		case SQLITE_BUSY:
			printf("SQLITE_BUSY");
			break;
		default:
			printf("SQLCODE %d\n", res);
			SQLFATAL("purgeUnreferencedBlocks");
		}
	}

	sqlite3_finalize(stmt);
	endSave();
}

// << KIDSCODE - Map versionning

/*
 * Player Database
 */

PlayerDatabaseSQLite3::PlayerDatabaseSQLite3(const std::string &savedir):
	Database_SQLite3(savedir, "players"),
	PlayerDatabase()
{
	// >> KIDSCODE - Threading
	// Open read write connection for this thread
	openConnection(true);
	// << KIDSCODE - Threading
}

PlayerDatabaseSQLite3::~PlayerDatabaseSQLite3()
{
	// >> KIDSCODE - Threading
	/*
	FINALIZE_STATEMENT(m_stmt_player_load)
	FINALIZE_STATEMENT(m_stmt_player_add)
	FINALIZE_STATEMENT(m_stmt_player_update)
	FINALIZE_STATEMENT(m_stmt_player_remove)
	FINALIZE_STATEMENT(m_stmt_player_list)
	FINALIZE_STATEMENT(m_stmt_player_add_inventory)
	FINALIZE_STATEMENT(m_stmt_player_add_inventory_items)
	FINALIZE_STATEMENT(m_stmt_player_remove_inventory)
	FINALIZE_STATEMENT(m_stmt_player_remove_inventory_items)
	FINALIZE_STATEMENT(m_stmt_player_load_inventory)
	FINALIZE_STATEMENT(m_stmt_player_load_inventory_items)
	FINALIZE_STATEMENT(m_stmt_player_metadata_load)
	FINALIZE_STATEMENT(m_stmt_player_metadata_add)
	FINALIZE_STATEMENT(m_stmt_player_metadata_remove)
	*/
	// << KIDSCODE - Threading
};


void PlayerDatabaseSQLite3::createDatabase(sqlite3 *connection) // KIDSCODE - Threading
// void PlayerDatabaseSQLite3::createDatabase()
{
	assert(connection); // Pre-condition // KIDSCODE - Threading
//	assert(m_database); // Pre-condition

	SQLOK(sqlite3_exec(connection, // KIDSCODE - Threading
//	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `player` ("
			"`name` VARCHAR(50) NOT NULL,"
			"`pitch` NUMERIC(11, 4) NOT NULL,"
			"`yaw` NUMERIC(11, 4) NOT NULL,"
			"`posX` NUMERIC(11, 4) NOT NULL,"
			"`posY` NUMERIC(11, 4) NOT NULL,"
			"`posZ` NUMERIC(11, 4) NOT NULL,"
			"`hp` INT NOT NULL,"
			"`breath` INT NOT NULL,"
			"`creation_date` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
			"`modification_date` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
			"PRIMARY KEY (`name`));",
		NULL, NULL, NULL),
		"Failed to create player table");

	SQLOK(sqlite3_exec(connection, // KIDSCODE - Threading
//	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `player_metadata` ("
			"    `player` VARCHAR(50) NOT NULL,"
			"    `metadata` VARCHAR(256) NOT NULL,"
			"    `value` TEXT,"
			"    PRIMARY KEY(`player`, `metadata`),"
			"    FOREIGN KEY (`player`) REFERENCES player (`name`) ON DELETE CASCADE );",
		NULL, NULL, NULL),
		"Failed to create player metadata table");

	SQLOK(sqlite3_exec(connection, // KIDSCODE - Threading
//	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `player_inventories` ("
			"   `player` VARCHAR(50) NOT NULL,"
			"	`inv_id` INT NOT NULL,"
			"	`inv_width` INT NOT NULL,"
			"	`inv_name` TEXT NOT NULL DEFAULT '',"
			"	`inv_size` INT NOT NULL,"
			"	PRIMARY KEY(player, inv_id),"
			"   FOREIGN KEY (`player`) REFERENCES player (`name`) ON DELETE CASCADE );",
		NULL, NULL, NULL),
		"Failed to create player inventory table");

	SQLOK(sqlite3_exec(connection, // KIDSCODE - Threading
//	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE `player_inventory_items` ("
			"   `player` VARCHAR(50) NOT NULL,"
			"	`inv_id` INT NOT NULL,"
			"	`slot_id` INT NOT NULL,"
			"	`item` TEXT NOT NULL DEFAULT '',"
			"	PRIMARY KEY(player, inv_id, slot_id),"
			"   FOREIGN KEY (`player`) REFERENCES player (`name`) ON DELETE CASCADE );",
		NULL, NULL, NULL),
		"Failed to create player inventory items table");
}

void PlayerDatabaseSQLite3::initStatements()
{
	PREPARE_STATEMENT(player_load, "SELECT `pitch`, `yaw`, `posX`, `posY`, `posZ`, `hp`, "
		"`breath`"
		"FROM `player` WHERE `name` = ?")
	PREPARE_STATEMENT(player_add, "INSERT INTO `player` (`name`, `pitch`, `yaw`, `posX`, "
		"`posY`, `posZ`, `hp`, `breath`) VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
	PREPARE_STATEMENT(player_update, "UPDATE `player` SET `pitch` = ?, `yaw` = ?, "
			"`posX` = ?, `posY` = ?, `posZ` = ?, `hp` = ?, `breath` = ?, "
			"`modification_date` = CURRENT_TIMESTAMP WHERE `name` = ?")
	PREPARE_STATEMENT(player_remove, "DELETE FROM `player` WHERE `name` = ?")
	PREPARE_STATEMENT(player_list, "SELECT `name` FROM `player`")

	PREPARE_STATEMENT(player_add_inventory, "INSERT INTO `player_inventories` "
		"(`player`, `inv_id`, `inv_width`, `inv_name`, `inv_size`) VALUES (?, ?, ?, ?, ?)")
	PREPARE_STATEMENT(player_add_inventory_items, "INSERT INTO `player_inventory_items` "
		"(`player`, `inv_id`, `slot_id`, `item`) VALUES (?, ?, ?, ?)")
	PREPARE_STATEMENT(player_remove_inventory, "DELETE FROM `player_inventories` "
		"WHERE `player` = ?")
	PREPARE_STATEMENT(player_remove_inventory_items, "DELETE FROM `player_inventory_items` "
		"WHERE `player` = ?")
	PREPARE_STATEMENT(player_load_inventory, "SELECT `inv_id`, `inv_width`, `inv_name`, "
		"`inv_size` FROM `player_inventories` WHERE `player` = ? ORDER BY inv_id")
	PREPARE_STATEMENT(player_load_inventory_items, "SELECT `slot_id`, `item` "
		"FROM `player_inventory_items` WHERE `player` = ? AND `inv_id` = ?")

	PREPARE_STATEMENT(player_metadata_load, "SELECT `metadata`, `value` FROM "
		"`player_metadata` WHERE `player` = ?")
	PREPARE_STATEMENT(player_metadata_add, "INSERT INTO `player_metadata` "
		"(`player`, `metadata`, `value`) VALUES (?, ?, ?)")
	PREPARE_STATEMENT(player_metadata_remove, "DELETE FROM `player_metadata` "
		"WHERE `player` = ?")
	verbosestream << "ServerEnvironment: SQLite3 database opened (players)." << std::endl;
}

bool PlayerDatabaseSQLite3::playerDataExists(const std::string &name)
{
//	verifyDatabase(); // KIDSCODE - Threading
	str_to_sqlite(m_stmt_player_load, 1, name);
	bool res = (sqlite3_step(m_stmt_player_load) == SQLITE_ROW);
	sqlite3_reset(m_stmt_player_load);
	return res;
}

void PlayerDatabaseSQLite3::savePlayer(RemotePlayer *player)
{
	PlayerSAO* sao = player->getPlayerSAO();
	sanity_check(sao);

	const v3f &pos = sao->getBasePosition();
	// Begin save in brace is mandatory
	if (!playerDataExists(player->getName())) {
		beginSave();
		str_to_sqlite(m_stmt_player_add, 1, player->getName());
		double_to_sqlite(m_stmt_player_add, 2, sao->getLookPitch());
		double_to_sqlite(m_stmt_player_add, 3, sao->getRotation().Y);
		double_to_sqlite(m_stmt_player_add, 4, pos.X);
		double_to_sqlite(m_stmt_player_add, 5, pos.Y);
		double_to_sqlite(m_stmt_player_add, 6, pos.Z);
		int64_to_sqlite(m_stmt_player_add, 7, sao->getHP());
		int64_to_sqlite(m_stmt_player_add, 8, sao->getBreath());

		sqlite3_vrfy(sqlite3_step(m_stmt_player_add), SQLITE_DONE);
		sqlite3_reset(m_stmt_player_add);
	} else {
		beginSave();
		double_to_sqlite(m_stmt_player_update, 1, sao->getLookPitch());
		double_to_sqlite(m_stmt_player_update, 2, sao->getRotation().Y);
		double_to_sqlite(m_stmt_player_update, 3, pos.X);
		double_to_sqlite(m_stmt_player_update, 4, pos.Y);
		double_to_sqlite(m_stmt_player_update, 5, pos.Z);
		int64_to_sqlite(m_stmt_player_update, 6, sao->getHP());
		int64_to_sqlite(m_stmt_player_update, 7, sao->getBreath());
		str_to_sqlite(m_stmt_player_update, 8, player->getName());

		sqlite3_vrfy(sqlite3_step(m_stmt_player_update), SQLITE_DONE);
		sqlite3_reset(m_stmt_player_update);
	}

	// Write player inventories
	str_to_sqlite(m_stmt_player_remove_inventory, 1, player->getName());
	sqlite3_vrfy(sqlite3_step(m_stmt_player_remove_inventory), SQLITE_DONE);
	sqlite3_reset(m_stmt_player_remove_inventory);

	str_to_sqlite(m_stmt_player_remove_inventory_items, 1, player->getName());
	sqlite3_vrfy(sqlite3_step(m_stmt_player_remove_inventory_items), SQLITE_DONE);
	sqlite3_reset(m_stmt_player_remove_inventory_items);

	std::vector<const InventoryList*> inventory_lists = sao->getInventory()->getLists();
	for (u16 i = 0; i < inventory_lists.size(); i++) {
		const InventoryList* list = inventory_lists[i];

		str_to_sqlite(m_stmt_player_add_inventory, 1, player->getName());
		int_to_sqlite(m_stmt_player_add_inventory, 2, i);
		int_to_sqlite(m_stmt_player_add_inventory, 3, list->getWidth());
		str_to_sqlite(m_stmt_player_add_inventory, 4, list->getName());
		int_to_sqlite(m_stmt_player_add_inventory, 5, list->getSize());
		sqlite3_vrfy(sqlite3_step(m_stmt_player_add_inventory), SQLITE_DONE);
		sqlite3_reset(m_stmt_player_add_inventory);

		for (u32 j = 0; j < list->getSize(); j++) {
			std::ostringstream os;
			list->getItem(j).serialize(os);
			std::string itemStr = os.str();

			str_to_sqlite(m_stmt_player_add_inventory_items, 1, player->getName());
			int_to_sqlite(m_stmt_player_add_inventory_items, 2, i);
			int_to_sqlite(m_stmt_player_add_inventory_items, 3, j);
			str_to_sqlite(m_stmt_player_add_inventory_items, 4, itemStr);
			sqlite3_vrfy(sqlite3_step(m_stmt_player_add_inventory_items), SQLITE_DONE);
			sqlite3_reset(m_stmt_player_add_inventory_items);
		}
	}

	str_to_sqlite(m_stmt_player_metadata_remove, 1, player->getName());
	sqlite3_vrfy(sqlite3_step(m_stmt_player_metadata_remove), SQLITE_DONE);
	sqlite3_reset(m_stmt_player_metadata_remove);

	const StringMap &attrs = sao->getMeta().getStrings();
	for (const auto &attr : attrs) {
		str_to_sqlite(m_stmt_player_metadata_add, 1, player->getName());
		str_to_sqlite(m_stmt_player_metadata_add, 2, attr.first);
		str_to_sqlite(m_stmt_player_metadata_add, 3, attr.second);
		sqlite3_vrfy(sqlite3_step(m_stmt_player_metadata_add), SQLITE_DONE);
		sqlite3_reset(m_stmt_player_metadata_add);
	}

	endSave();

	player->onSuccessfulSave();
}

bool PlayerDatabaseSQLite3::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
//	verifyDatabase(); // KIDSCODE - Threading

	str_to_sqlite(m_stmt_player_load, 1, player->getName());
	if (sqlite3_step(m_stmt_player_load) != SQLITE_ROW) {
		sqlite3_reset(m_stmt_player_load);
		return false;
	}
	sao->setLookPitch(sqlite_to_float(m_stmt_player_load, 0));
	sao->setPlayerYaw(sqlite_to_float(m_stmt_player_load, 1));
	sao->setBasePosition(sqlite_to_v3f(m_stmt_player_load, 2));
	sao->setHPRaw((u16) MYMIN(sqlite_to_int(m_stmt_player_load, 5), U16_MAX));
	sao->setBreath((u16) MYMIN(sqlite_to_int(m_stmt_player_load, 6), U16_MAX), false);
	sqlite3_reset(m_stmt_player_load);

	// Load inventory
	str_to_sqlite(m_stmt_player_load_inventory, 1, player->getName());
	while (sqlite3_step(m_stmt_player_load_inventory) == SQLITE_ROW) {
		InventoryList *invList = player->inventory.addList(
			sqlite_to_string(m_stmt_player_load_inventory, 2),
			sqlite_to_uint(m_stmt_player_load_inventory, 3));
		invList->setWidth(sqlite_to_uint(m_stmt_player_load_inventory, 1));

		u32 invId = sqlite_to_uint(m_stmt_player_load_inventory, 0);

		str_to_sqlite(m_stmt_player_load_inventory_items, 1, player->getName());
		int_to_sqlite(m_stmt_player_load_inventory_items, 2, invId);
		while (sqlite3_step(m_stmt_player_load_inventory_items) == SQLITE_ROW) {
			const std::string itemStr = sqlite_to_string(m_stmt_player_load_inventory_items, 1);
			if (itemStr.length() > 0) {
				ItemStack stack;
				stack.deSerialize(itemStr);
				invList->changeItem(sqlite_to_uint(m_stmt_player_load_inventory_items, 0), stack);
			}
		}
		sqlite3_reset(m_stmt_player_load_inventory_items);
	}

	sqlite3_reset(m_stmt_player_load_inventory);

	str_to_sqlite(m_stmt_player_metadata_load, 1, sao->getPlayer()->getName());
	while (sqlite3_step(m_stmt_player_metadata_load) == SQLITE_ROW) {
		std::string attr = sqlite_to_string(m_stmt_player_metadata_load, 0);
		std::string value = sqlite_to_string(m_stmt_player_metadata_load, 1);

		sao->getMeta().setString(attr, value);
	}
	sao->getMeta().setModified(false);
	sqlite3_reset(m_stmt_player_metadata_load);
	return true;
}

bool PlayerDatabaseSQLite3::removePlayer(const std::string &name)
{
	if (!playerDataExists(name))
		return false;

	str_to_sqlite(m_stmt_player_remove, 1, name);
	sqlite3_vrfy(sqlite3_step(m_stmt_player_remove), SQLITE_DONE);
	sqlite3_reset(m_stmt_player_remove);
	return true;
}

void PlayerDatabaseSQLite3::listPlayers(std::vector<std::string> &res)
{
//	verifyDatabase(); // KIDSCODE - Threading

	while (sqlite3_step(m_stmt_player_list) == SQLITE_ROW)
		res.push_back(sqlite_to_string(m_stmt_player_list, 0));

	sqlite3_reset(m_stmt_player_list);
}

/*
 * Auth database
 */

AuthDatabaseSQLite3::AuthDatabaseSQLite3(const std::string &savedir) :
		Database_SQLite3(savedir, "auth"), AuthDatabase()
{
	// >> KIDSCODE - Threading
	// Open read write connection for this thread
	openConnection(true);
	// << KIDSCODE - Threading
}

AuthDatabaseSQLite3::~AuthDatabaseSQLite3()
{
	// >> KIDSCODE - Threading
	/*
	FINALIZE_STATEMENT(m_stmt_read)
	FINALIZE_STATEMENT(m_stmt_write)
	FINALIZE_STATEMENT(m_stmt_create)
	FINALIZE_STATEMENT(m_stmt_delete)
	FINALIZE_STATEMENT(m_stmt_list_names)
	FINALIZE_STATEMENT(m_stmt_read_privs)
	FINALIZE_STATEMENT(m_stmt_write_privs)
	FINALIZE_STATEMENT(m_stmt_delete_privs)
	FINALIZE_STATEMENT(m_stmt_last_insert_rowid)
	*/
	// << KIDSCODE - Threading
}

void AuthDatabaseSQLite3::createDatabase(sqlite3 *connection) // KIDSCODE - Threading
//void AuthDatabaseSQLite3::createDatabase()
{
	assert(connection); // Pre-condition // KIDSCODE - Threading
//	assert(m_database); // Pre-condition

	SQLOK(sqlite3_exec(connection, // KIDSCODE - Threading
	//	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `auth` ("
			"`id` INTEGER PRIMARY KEY AUTOINCREMENT,"
			"`name` VARCHAR(32) UNIQUE,"
			"`password` VARCHAR(512),"
			"`last_login` INTEGER"
		");",
		NULL, NULL, NULL),
		"Failed to create auth table");

	SQLOK(sqlite3_exec(connection, // KIDSCODE - Threading
//	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `user_privileges` ("
			"`id` INTEGER,"
			"`privilege` VARCHAR(32),"
			"PRIMARY KEY (id, privilege)"
			"CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES auth (id) ON DELETE CASCADE"
		");",
		NULL, NULL, NULL),
		"Failed to create auth privileges table");
}

void AuthDatabaseSQLite3::initStatements()
{
	PREPARE_STATEMENT(read, "SELECT id, name, password, last_login FROM auth WHERE name = ?");
	PREPARE_STATEMENT(write, "UPDATE auth set name = ?, password = ?, last_login = ? WHERE id = ?");
	PREPARE_STATEMENT(create, "INSERT INTO auth (name, password, last_login) VALUES (?, ?, ?)");
	PREPARE_STATEMENT(delete, "DELETE FROM auth WHERE name = ?");

	PREPARE_STATEMENT(list_names, "SELECT name FROM auth ORDER BY name DESC");

	PREPARE_STATEMENT(read_privs, "SELECT privilege FROM user_privileges WHERE id = ?");
	PREPARE_STATEMENT(write_privs, "INSERT OR IGNORE INTO user_privileges (id, privilege) VALUES (?, ?)");
	PREPARE_STATEMENT(delete_privs, "DELETE FROM user_privileges WHERE id = ?");

	PREPARE_STATEMENT(last_insert_rowid, "SELECT last_insert_rowid()");
}

bool AuthDatabaseSQLite3::getAuth(const std::string &name, AuthEntry &res)
{
//	verifyDatabase(); // KIDSCODE - Threading
	str_to_sqlite(m_stmt_read, 1, name);
	if (sqlite3_step(m_stmt_read) != SQLITE_ROW) {
		sqlite3_reset(m_stmt_read);
		return false;
	}
	res.id = sqlite_to_uint(m_stmt_read, 0);
	res.name = sqlite_to_string(m_stmt_read, 1);
	res.password = sqlite_to_string(m_stmt_read, 2);
	res.last_login = sqlite_to_int64(m_stmt_read, 3);
	sqlite3_reset(m_stmt_read);

	int64_to_sqlite(m_stmt_read_privs, 1, res.id);
	while (sqlite3_step(m_stmt_read_privs) == SQLITE_ROW) {
		res.privileges.emplace_back(sqlite_to_string(m_stmt_read_privs, 0));
	}
	sqlite3_reset(m_stmt_read_privs);

	return true;
}

bool AuthDatabaseSQLite3::saveAuth(const AuthEntry &authEntry)
{
	beginSave();

	str_to_sqlite(m_stmt_write, 1, authEntry.name);
	str_to_sqlite(m_stmt_write, 2, authEntry.password);
	int64_to_sqlite(m_stmt_write, 3, authEntry.last_login);
	int64_to_sqlite(m_stmt_write, 4, authEntry.id);
	sqlite3_vrfy(sqlite3_step(m_stmt_write), SQLITE_DONE);
	sqlite3_reset(m_stmt_write);

	writePrivileges(authEntry);

	endSave();
	return true;
}

bool AuthDatabaseSQLite3::createAuth(AuthEntry &authEntry)
{
	beginSave();

	// id autoincrements
	str_to_sqlite(m_stmt_create, 1, authEntry.name);
	str_to_sqlite(m_stmt_create, 2, authEntry.password);
	int64_to_sqlite(m_stmt_create, 3, authEntry.last_login);
	sqlite3_vrfy(sqlite3_step(m_stmt_create), SQLITE_DONE);
	sqlite3_reset(m_stmt_create);

	// obtain id and write back to original authEntry
	sqlite3_step(m_stmt_last_insert_rowid);
	authEntry.id = sqlite_to_uint(m_stmt_last_insert_rowid, 0);
	sqlite3_reset(m_stmt_last_insert_rowid);

	writePrivileges(authEntry);

	endSave();
	return true;
}

bool AuthDatabaseSQLite3::deleteAuth(const std::string &name)
{
//	verifyDatabase(); // KIDSCODE - Threading

	str_to_sqlite(m_stmt_delete, 1, name);
	sqlite3_vrfy(sqlite3_step(m_stmt_delete), SQLITE_DONE);
	int changes = sqlite3_changes(getConnection());
	sqlite3_reset(m_stmt_delete);

	// privileges deleted by foreign key on delete cascade

	return changes > 0;
}

void AuthDatabaseSQLite3::listNames(std::vector<std::string> &res)
{
//	verifyDatabase(); // KIDSCODE - Threading

	while (sqlite3_step(m_stmt_list_names) == SQLITE_ROW) {
		res.push_back(sqlite_to_string(m_stmt_list_names, 0));
	}
	sqlite3_reset(m_stmt_list_names);
}

void AuthDatabaseSQLite3::reload()
{
	// noop for SQLite
}

void AuthDatabaseSQLite3::writePrivileges(const AuthEntry &authEntry)
{
	int64_to_sqlite(m_stmt_delete_privs, 1, authEntry.id);
	sqlite3_vrfy(sqlite3_step(m_stmt_delete_privs), SQLITE_DONE);
	sqlite3_reset(m_stmt_delete_privs);
	for (const std::string &privilege : authEntry.privileges) {
		int64_to_sqlite(m_stmt_write_privs, 1, authEntry.id);
		str_to_sqlite(m_stmt_write_privs, 2, privilege);
		sqlite3_vrfy(sqlite3_step(m_stmt_write_privs), SQLITE_DONE);
		sqlite3_reset(m_stmt_write_privs);
	}
}
