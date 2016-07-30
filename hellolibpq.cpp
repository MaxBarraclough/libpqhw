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

// From Chromium via https://stackoverflow.com/a/4415646
#define COUNT_OF(x) ( (sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))) )


// TODO add link to GNU website that provides these (except they use unhelpful names)
#define STRINGIFY_WITH_EXPANSION(S) STRINGIFY_NO_EXPANSION(S)
#define STRINGIFY_NO_EXPANSION(S) #S



static const int expectedTypes[] = {INT4OID,VARCHAROID};
#define EXPECTED_NUMBER_OF_COLUMNS_VAL COUNT_OF(expectedTypes) // STRINGIFY_WITH_EXPANSION will give code, not a string of the number
#define EXPECTED_NUMBER_OF_COLUMNS 2 // this way it plays nice with STRINGIFY_WITH_EXPANSION
// so that in iostreams
  // "string literal here " << EXPECTED_NUMBER_OF_COLUMNS << " another string literal"
// is equivalent to
  // "string literal here " STRINGIFY_WITH_EXPANSION(EXPECTED_NUMBER_OF_COLUMNS) " another string literal"
BOOST_STATIC_ASSERT(( EXPECTED_NUMBER_OF_COLUMNS == EXPECTED_NUMBER_OF_COLUMNS_VAL ));



// returns true if checks passed, false if they failed
static bool checkColumnsTypes(PGresult * const result, int numCols) {

// We expect to user username "testuser", password "testuser", database "my_database",
// table "my_table" which should look like:
//
// CREATE TABLE "my_table" (
  // "number" integer NOT NULL,
  // "English" character varying(11) NOT NULL
// );

// INT4OID => "integer" ("NOT NULL" is not reflected in type information proper, as it's a constraint)
// VARCHAROID => our table uses "character varying(11)"

  BOOST_ASSERT_MSG( (EXPECTED_NUMBER_OF_COLUMNS == numCols), "Unexpected bad number of columns: this should have been handled earlier" );

  bool typesAreCorrect = true; // leave as true until we find a bad type

  for (int i = 0; i != EXPECTED_NUMBER_OF_COLUMNS; ++i) {

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


// 'schema' as in DB's structure, not as in Postgres namespace
static bool verifyTheSchema(PGconn * const conn, PGresult * const result) {
  BOOST_ASSERT( NULL != conn );
  BOOST_ASSERT( NULL != result );

  const int numCols = PQnfields(result);

  bool allOk  = false;

  if ( EXPECTED_NUMBER_OF_COLUMNS == numCols ) {

    std::cout << "As expected, we have " STRINGIFY_WITH_EXPANSION(EXPECTED_NUMBER_OF_COLUMNS) " columns\n";

    for (int i = 0; i != numCols; ++i) {
      std::cout << "Column " << i << " is called '" << PQfname(result,i) << "'\n";
    }


    if ( EXPECTED_NUMBER_OF_COLUMNS == numCols ) {
      allOk = checkColumnsTypes(result,numCols);
    } else {
      std::cerr << "Incorrect number of columns in table. Expected " STRINGIFY_WITH_EXPANSION(EXPECTED_NUMBER_OF_COLUMNS) " but found " << numCols << '\n';
    }

  } else {
    std::cerr << "Unexpected number of columns: " << numCols << '\n';
  }

  return allOk;
}


static void doStuffWithConnection(PGconn * const conn) {
   BOOST_ASSERT( NULL != conn );

   PGresult * const result = PQexec(conn, "SELECT * FROM my_table;");

   if (NULL != result) {
     bool schemaIsOk = verifyTheSchema(conn, result);

     if (schemaIsOk) {
       std::cout << "All schema checks passed\n";
     }

     PQclear(result); // Despite name, is *not* just a memset-style 'clear'.
                      // It calls 'free' for us, see https://git.io/vKpNI
   } else {
     std::cerr << "Error attempting SELECT query. Likely causes: failed network connection, server down, or out-of-memory\n";
     // TODO PQerrorMessage for actual cause
   }
}



int main(int argc, const char *argv[]) {

  #ifdef _MSC_VER
    UNTESTED UNTESTED UNTESTED 1111() // break the build
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
                                    "testuser",    // password
                                    "my_database", // dbname
                                    NULL };


// BOOST_STATIC_ASSERT(( sizeof keywords == sizeof values )); // oddly Postgres won't do the equivalent check

  PGconn * const conn = PQconnectdbParams(keywords,values,0);

  BOOST_ASSERT_MSG( (NULL != conn), "Unexpected error attempting to initiate connection" );

  const ConnStatusType status = PQstatus(conn);

  switch(status) {

    case CONNECTION_OK:
      doStuffWithConnection(conn);
      break;

    case CONNECTION_BAD:
      std::cerr << "Unable to connect. Reason: \n";
      std::fill_n(std::ostream_iterator<char>(std::cout), 30, '-'); // print 20 dashes http://stackoverflow.com/a/11421689/2307853
      std::cerr << '\n';
      std::cerr << PQerrorMessage(conn);
      std::fill_n(std::ostream_iterator<char>(std::cout), 30, '-');
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

