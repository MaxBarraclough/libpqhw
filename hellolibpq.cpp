//////////////////////////////////////////////////////////////
////////////////////////// PROLOGUE //////////////////////////
//////////////////////////////////////////////////////////////

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


#include <libpq-fe.h> // Postgres's "libpq" client library

/************************* Ugly #include trouble workaround *************************/
//#include <9.3/server/catalog/pg_type.h> // trouble with indirectly included headers, possibly due to CMake

// Assuming copy-pasta is fragile and true values are liable to change between Postgre versions, let's be defensive:
#include <pg_config.h>
#if (PG_VERSION_NUM != 90313)
  #error Expected to build against exact version: PostgreSQL 9.3.13
#endif

#define INT4OID			23
#define VARCHAROID		1043
/************************************************************************************/

// Handling endianness: in https://www.postgresql.org/docs/current/static/libpq-example.html
// we see netinet/in.h and arpa/inet.h but this doesn't let us work with signed types,
// and I can't see a way forward without breaking the strict aliasing rule.
// See also http://dbp-consulting.com/tutorials/StrictAliasing.html
// #include <boost/endian/conversion.hpp> // requires Boost 1.58, which I lack,
// so we just use GCC intrinsics for now: no #include necessary

#include <boost/type_traits/is_same.hpp>
#include <boost/assert.hpp>
#include <boost/static_assert.hpp>

#include <iostream>
#include <iterator>  // for std::ostream_iterator

#include <string.h>  // for strcmp
#include <cstdlib>   // pedantic: strictly, this is needed for NULL
// Leave this one out and just hope int32_t exists anyway (which it will in GCC)
// #include <cstdint>   // for int32_t etc



////////////////////////////////////////////////////////////////////////
////////////////////////// GLOBALS AND MACROS //////////////////////////
////////////////////////////////////////////////////////////////////////

// From Chromium via https://stackoverflow.com/a/4415646
#define COUNT_OF(x) ( (sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))) )

// TODO add link to GNU website that provides these (except they use unhelpful names)
#define STRINGIFY_WITH_EXPANSION(S) STRINGIFY_NO_EXPANSION(S)
#define STRINGIFY_NO_EXPANSION(S) #S

// INT4OID    => "integer" ("NOT NULL" is not reflected in type information proper; it's a constraint)
// VARCHAROID => our table uses "character varying(11)"
static const int expectedTypes[] = { /*first col*/ INT4OID, /*second col*/ VARCHAROID };

// May have to update this manually if we change things:
#define EXPECTED_NUM_COLS 2
#define EXPECTED_NUM_COLS_STR STRINGIFY_WITH_EXPANSION(EXPECTED_NUM_COLS) // string literal like "2" (including quotes)
// This allows us to do:
inline static void toy1() {std::cout << "Number of columns: "    EXPECTED_NUM_COLS_STR "\n";}
// and it's equivalent to
inline static void toy2() {std::cout << "Number of columns: " << EXPECTED_NUM_COLS <<  "\n";}
BOOST_STATIC_ASSERT(( EXPECTED_NUM_COLS == COUNT_OF(expectedTypes) ));

const int NUMBER_COL_NUM  = 0;
const int ENGLISH_COL_NUM = 1;



///////////////////////////////////////////////////////////////
////////////////////////// FUNCTIONS //////////////////////////
///////////////////////////////////////////////////////////////

// print many dashes http://stackoverflow.com/a/11421689/2307853
inline static void printHorizontalBar(std::ostream &os) {
  std::fill_n(std::ostream_iterator<char>(os), 30, '-');
}


// returns true if checks passed, false if they failed
// We check the types of columns 0 and 1.
static bool checkColumnsTypes(PGresult * const result, const int numCols) {
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
  } // else rely on specific checks to print messages

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


static void processReturnedTable(PGresult * const result) {

  const int numRows = PQntuples(result);

  for (int rowNum = 0; rowNum != numRows; ++rowNum) {

    char * const number_bs = PQgetvalue(result,rowNum,NUMBER_COL_NUM); // binary 'string', in network endianness
    int number = *( (int*)number_bs ); // still in network endianness
    BOOST_STATIC_ASSERT_MSG(( boost::is_same<int32_t,int>::value ), "We depend on int being 32-bit");
    number = __builtin_bswap32(number);        // now in host endianness, ready to use as an int
    // boost::big_to_native_inplace( number ); // now in host endianness, ready to use as an int

    char * const english = PQgetvalue(result,rowNum,ENGLISH_COL_NUM); // don't care about text/binary format: both '\0' terminated
    std::cout << number << " -> " << english << '\n';
  }
}


static void doStuffWithConnection(PGconn * const conn) {

  struct H { // just for the helper function
    inline static void helper(PGconn * const conn, PGresult * const result) {

      const ExecStatusType es = PQresultStatus(result);

      if (PGRES_TUPLES_OK == es) {
        const bool schemaIsOk = verifyTheSchema(conn, result);

        if (schemaIsOk) {
          std::cout << "All schema checks passed\n";
          processReturnedTable(result);
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

//PGresult * const result = PQexec(conn, "SELECT * FROM my_table WHERE ((number = 1) or (english='Twenty'));");
//PGresult * const result = PQexec(conn, "SELECT * FROM my_table WHERE ((number = 1) or (lower(english)='two'));");

  PGresult * const result =
    PQexecParams(conn,
                "SELECT * FROM my_table WHERE ((number = 1) or (english='Twenty'));",
                0,
                NULL,
                NULL,
                NULL,
                NULL,
                1); // request binary format outputs

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

  return 0;
}

