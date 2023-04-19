# Database migrations

When a new database is set up, the migration scripts are executed in version order.
This means migration scripts must follow the given schema.

A new migration must increase the version number both in the database
and in `Database::expectedSchemaVersion`. An unexpected version will emit a warning.

Newer database versions must be compatible with older version, as the database is
upgraded before the processing-context is.
