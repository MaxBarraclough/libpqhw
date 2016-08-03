libpq Hello World
=================

About
-----

A simple example program using Postgres's libpq library (Postgres client library in C).

It performs a simple SELECT query, does some checks on the schema of the returned table, and prints out that returned table.

It uses a hard-coded placeholder authentication configuration:
-  username: "testuser"
-  password: "testuser"


Dependencies
------------

On Mint (and presumably also Ubuntu), the following packages are necessary:

-  postgresql-server-dev-all
-  cmake
-  build-essential (or some other CMake-friendly C++ compiler suite)

PostgreSQL must be running (of course), and must have a database named "my_database", in which there must be a table in the "public" schema along the following lines:

    CREATE TABLE "my_table" (
      "number" integer NOT NULL,
      "english" character varying(11) NOT NULL
    );

TODO: The proper configuration is given in dump.sql, which includes example data.

It ought to be simple enough to get things to work on Windows, but this hasn't been tested.


Building and Running
--------------------

Provided your system meets the requirements specified above, to compile and run:

    cmake -G"Unix Makefiles" && make && ./hellolibpq

