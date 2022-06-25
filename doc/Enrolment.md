# Enrolment

Enrolment is the process of storing blobs in a processing context's (PC) database that are required for
the PC's communication with the HSM.

# Blobs

The term blob is used with its meaning _binary large object_. It contains encrypted state information that
is only accessible to the HSM. However, as the HSM has no database and does not share data between different
HSMs in a HA (high availability) group, it falls to the PC to persist the state information, i.e. the blobs.

# Enrolment API

The enrolment API provides endpoints that allow the external enrolment manager to query for values
from the TPM and to store blobs in the blob database. The API has its own port number and is only intended 
for internal use: the enrolment manager which typically runs on a laptop which is to be used
on location in a data center, i.e. not via the internet. When there is more than one PC instance running
on a server, only one of them activates its enrolment API. The others can access the blob database but do
not support calls to the enrolment API.

# Configuration

There are currently three configuration values for use in production and one for development-only use.

## Production

Each value is described with its C++ enum value, the environment variable name and its JSON path:
- `ConfigurationKey::ENROLMENT_SERVER_PORT`,
  `ERP_ENROLMENT_SERVER_PORT`,
  `/erp/enrolment/server/port` <br>
The port number of the enrolment server.
- `ConfigurationKey::ENROLMENT_ACTIVATE_FOR_PORT`,
  `ERP_ENROLMENT_ACTIVATE_FOR_PORT`,
  `/erp/enrolment/server/activateForPort` <br>
The enrolment API is only activated when the value of `ERP_SERVER_PORT` is the same as `ERP_ENROLMENT_ACTIVATE_FOR_PORT`.
This allows all instances on one server to be configured with the same value for `ERP_ENROLMENT_ACTIVATE_FOR_PORT`. Only
the instance which has a matching `ERP_SERVER_PORT` will activate its enrolment API. The others will periodically check 
the blob database for updates. 
- `ConfigurationKey::ENROLMENT_API_CREDENTIALS`, `ERP_ENROLMENT_API_CREDENTIALS`, `/erp/enrolment/api/credentials` <br>
Credentials that are expected in the `Authorization` header of incoming requests. At the moment only `basic`
authentication is supported.
 
# Initialization

The blob database is initialized in production with a so-called key ceremony.  One representative from Gematik and one
representative from IBM prepare a new server for use by asking its TPM for values that uniquely identify the server.
When a PC on the new server presents these values to an HSM the PC gets authenticated and authorized.

In development the enrolment process is substituted with a simplified enrolment manager which creates some of the
required blobs depending on values from the TPM. Other blobs are taken from a static data set in the `vau-hsm` project. 
The tool that does this is called `blob-db-initialization` and is built automatically when the `erp-processing-context`
application is built. In order to connect it, it needs access to both a PC and a HSM in that environment.

Note that you may have to delete the blob database, even if you use the `-d` option with the `blob-db-initialization` 
command. For that you need access to the PostgreSQL for the respective environment and delete some or all
entries in the `blob` table. Be careful with this.
