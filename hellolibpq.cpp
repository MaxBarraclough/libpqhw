// We expect to user username "testuser", password "testuser", database "my_database",
// table "my_table" which should look like:
//
// CREATE TABLE "my_table" (
  // "number" integer NOT NULL,
  // "english" character varying(11) NOT NULL
// );


// required reading: https://www.postgresql.org/docs/current/static/libpq-exec.html and adjacent
// see also
// https://books.google.co.uk/books?id=gkQVL9pyFVYC&lpg=PA324&ots=E7xhWQIujW&dq=PQntuples&pg=PA324#v=onepage&q=PQntuples&f=false

#include <libpq-fe.h>

////////////////////////// Ugly #include trouble workaround //////////////////////////
//#include <9.3/server/catalog/pg_type.h> // trouble with indirectly included headers, possibly due to CMake


// Assuming copy-pasta is fragile and true values are liable to change between Postgre versions, let's be defensive:
#include <pg_config.h>
#if (PG_VERSION_NUM != 90313)
  #error Expected to build against exact version: PostgreSQL 9.3.13
#endif


#define INT4OID			23
#define VARCHAROID		1043
//////////////////////////////////////////////////////////////////////////////////////

#include <boost/assert.hpp>
#include <boost/static_assert.hpp>

#include <cstdlib>   // pedantic: strictly, this is needed for NULL
#include <iostream>
#include <iterator>  // for std::ostream_iterator

#include <string.h>  // for strcmp

// From Chromium via https://stackoverflow.com/a/4415646
#define COUNT_OF(x) ( (sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))) )


// TODO add link to GNU website that provides these (except they use unhelpful names)
#define STRINGIFY_WITH_EXPANSION(S) STRINGIFY_NO_EXPANSION(S)
#define STRINGIFY_NO_EXPANSION(S) #S

// print many dashes http://stackoverflow.com/a/11421689/2307853
void printHorizontalBar(std::ostream &os) {
  std::fill_n(std::ostream_iterator<char>(os), 30, '-');
}



// INT4OID => "integer" ("NOT NULL" is not reflected in type information proper, as it's a constraint)
// VARCHAROID => our table uses "character varying(11)"
static const int expectedTypes[] = { /*first col*/ INT4OID, /*second col*/ VARCHAROID };

// May have to update this manually if we change things:
#define EXPECTED_NUM_COLS 2
#define EXPECTED_NUM_COLS_STR STRINGIFY_WITH_EXPANSION(EXPECTED_NUM_COLS) // string literal like "2" (including quotes)
// This allows us to do:
  // "string literal here " EXPECTED_NUM_COLS_STR " another string literal"
// and it's equivalent to
  // "string literal here " << EXPECTED_NUM_COLS << " another string literal"
BOOST_STATIC_ASSERT(( EXPECTED_NUM_COLS == COUNT_OF(expectedTypes) ));


// returns true if checks passed, false if they failed
// We check the types of columns 0 and 1.
static bool checkColumnsTypes(PGresult * const result, int numCols) {
  BOOST_ASSERT_MSG( (EXPECTED_NUM_COLS == numCols), "Unexpected bad number of columns: this should have been handled earlier" );

  bool typesAreCorrect = true; // leave as true until we find a bad type

  for (int i = 0; i != EXPECTED_NUM_COLS; ++i) {

    const Oid typeForCol = PQftype(result, i);
    if (expectedTypes[i] == typeForCol) {
      std::cout << "Correct type found for column " << i << '\n';
    } else {
      typesAreCorrect = false;
      std::cerr << "Incorrect type for column " << i << ": OID number of " << typeForCol << '\n';
    }

  }

  return typesAreCorrect;
}

// unlike checkColumnsTypes(2), this is fully hard-coded to our schema.
// TODO FIX THAT by adding more globals arrays or maybe something nicer
static bool checkColumnsNames(PGresult * const result) {
  BOOST_ASSERT( NULL != result );

  char * const colName1 = PQfname(result, 0);
  BOOST_ASSERT_MSG( (NULL != colName1), "Unexpected too few columns: this should have been handled earlier" );

  char * const colName2 = PQfname(result, 1);
  BOOST_ASSERT_MSG( (NULL != colName2), "Unexpected too few columns: this should have been handled earlier" );

  // we use strcmp, which presumably means we can only handle 'ordinary' ASCII names TODO what are Postgres's rules on this?
  const bool column1NameCorrect = ( 0 == strcmp("number", colName1) );
  const bool column2NameCorrect = ( 0 == strcmp("english",colName2) );
  const bool bothColumnsNamesCorrect = (column1NameCorrect && column2NameCorrect);

  if (bothColumnsNamesCorrect) {
    std::cout << "Column names are correct\n";
  }

  return bothColumnsNamesCorrect;
}


// 'schema' as in DB's structure, not as in Postgres namespace
static bool verifyTheSchema(PGconn * const conn, PGresult * const result) {
  BOOST_ASSERT( NULL != conn );
  BOOST_ASSERT( NULL != result );

  const int numCols = PQnfields(result);
  bool allOk = false;

  if ( EXPECTED_NUM_COLS == numCols ) {
    std::cout << "As expected, we have " EXPECTED_NUM_COLS_STR " columns\n";
    allOk = checkColumnsTypes(result,numCols) && checkColumnsNames(result);
  } else {
    std::cerr << "Incorrect number of columns in table. Expected " EXPECTED_NUM_COLS_STR " but found " << numCols << '\n';
  }

  return allOk;
}


// given that PQresultErrorMessage(1) exists, there's no real need for this
#if 0
static void printResultStatusError(const ExecStatusType es) {
  BOOST_ASSERT_MSG( ( (PGRES_TUPLES_OK != es) && (PGRES_COMMAND_OK != es) ), "Cannot print an error message for a non-error outcome" );
  const char * toPrint = NULL;
  switch(es) {
    case PGRES_EMPTY_QUERY:
      toPrint = "The string sent to the server was empty.";
      break;
    case PGRES_COPY_OUT:
      toPrint = "Copy Out (from server) data transfer started.";
      break;
    case PGRES_COPY_IN:
      toPrint = "Copy In (to server) data transfer started.";
      break;
    case PGRES_BAD_RESPONSE:
      toPrint = "The server's response was not understood.";
      break;
    case PGRES_NONFATAL_ERROR:
      toPrint = "A nonfatal error (a notice or warning) occurred.";
      break;
    case PGRES_FATAL_ERROR:
      toPrint = "A fatal error occurred.";
      break;
    case PGRES_COPY_BOTH:
      toPrint = "Copy In/Out (to and from server) data transfer started. This feature is currently used only for streaming replication, so this status should not occur in ordinary applications.";
      break;
    case PGRES_SINGLE_TUPLE:
      toPrint = "The PGresult contains a single result tuple from the current command. This status occurs only when single-row mode has been selected for the query.";
      break;
    default:
      BOOST_ASSERT_MSG( (false), "Invalid or unexpected error code" );
  }
  std::cout << toPrint;
}
#endif




static void doStuffWithConnection(PGconn * const conn) {

  struct H { // just for the helper function
    inline static void helper(PGconn * const conn, PGresult * const result) {

      const ExecStatusType es = PQresultStatus(result);

      if (PGRES_TUPLES_OK == es) {
        const bool schemaIsOk = verifyTheSchema(conn, result);

        if (schemaIsOk) {
          std::cout << "All schema checks passed\n";
        } // else rely on specific checks to print messages

        PQclear(result); // Despite name, is *not* just a memset-style 'clear'.
                         // It calls 'free' for us, see https://git.io/vKpNI
      } else { // then PGRES_TUPLES_OK != es
        if ( PGRES_COMMAND_OK == es ) {
          std::cerr << "Expected to run a command which returns column data\n";
        } else { // we have a real error
          char * const errorMessage = PQresultErrorMessage(result);
          BOOST_ASSERT(( NULL != errorMessage ));
          std::cerr << "Error attempting query:\n";
          printHorizontalBar(std::cerr);
          std::cerr << '\n' << errorMessage << '\n';
          printHorizontalBar(std::cerr);
          std::cerr << "\n\n";
        }
      }
    }
  };


  BOOST_ASSERT( NULL != conn );

  PGresult * const result = PQexec(conn, "SELECT * FROM my_table WHERE false;");

  if (NULL != result) {
    H::helper(conn,result);
  } else {
    std::cerr << "Error attempting SELECT query. Likely causes: failed network connection, server down, or out-of-memory\n";
  }
}



int main(int argc, const char *argv[]) {

  #ifdef _MSC_VER
    #error This code is untested on the Windows platform
    WSAStartup();
  #endif


// Note that specifying "host":"localhost" is *not* equivalent to the default behaviour,
// which is to use a 'peer' connection.
// Network connections and peer connections have different permissions properties.
// We assume testuser allows only network connections.

  const char * const keywords[] = { "application_name",
                                    "host",
                                    "user",
                                    "password",
                                    "dbname",
                                    NULL };

  const char * const values[]   = { "libpqhw",     // application_name // TODO use CMake config.h name string
                                    "localhost",   // host
                                    "testuser",    // user
                                    "testuser",    // password (yes, hard-coded)
                                    "my_database", // dbname
                                    NULL };


  BOOST_STATIC_ASSERT(( sizeof keywords == sizeof values )); // Postgres won't do the equivalent runtime check

  PGconn * const conn = PQconnectdbParams(keywords,values,0);

  BOOST_ASSERT_MSG( (NULL != conn), "Unexpected error attempting to initiate connection" );

  const ConnStatusType status = PQstatus(conn);

  switch(status) {

    case CONNECTION_OK:
      doStuffWithConnection(conn);
      break;

    case CONNECTION_BAD:
      std::cerr << "Unable to connect. Reason: \n";
      printHorizontalBar(std::cerr);
      std::cerr << '\n';
      std::cerr << PQerrorMessage(conn);
      std::cerr << '\n';
      printHorizontalBar(std::cerr);
      std::cerr << "\n\n";
      break;

    default:
      BOOST_ASSERT_MSG( false, "Invalid connection status" );
      break;

  }

  PQfinish(conn);

  #ifdef _MSC_VER
    WSACleanup();
  #endif

  std::cout << "Terminating\n";

}

