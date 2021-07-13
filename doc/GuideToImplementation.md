# Guide to the Implementation

## Directory Structure
The source code can be found in four separate trees.
- **src/erp**
  <br>
  The actual eRp processing context. It contains new implementation or modified files that where copied over from the 
ePA repository.

- **src/library**
  <br>
  A library of (mostly) unmodified files that where copied from the ePA repository.
  Update: the separation between modified and unmodified files copied over from ePa did not lead to a satisfactory result.
  Therefore the files under `src/library` will eventually be moved to `src/erp`.

- **test/erp**
  <br>
  Tests (unit tests, functional tests, workflow tests).

- **test/mock**
  <br>
  Mock implementation of some external services. To be used only for testing.

## HTTPS Server
The HTTPS server is based on boost::beast and boost::asio.
It is a template over the service context.

### Service Context 
The service context is a container of information that are not personalized and therefore can shared between
all request processing sessions. It is important that no information is stored here that can be associated with a single insurant or service provider.

There are two different implementations, one for the processing context and one for the enrolment server. This distinction
makes it necessary to keep it as a template parameter an classes like HttpsServer, ServerSession or SessionContext.
The two implementations are [PcServiceContext](src/erp/pc/PcServiceContext.hxx) for the processing context and
[EnrlomentServiceContext](src/erp/enrolment/EnrolmentServiceContext.hxx). Examples for values that are stored in either
or both of them are the Configuration, HSM (factory), Database (connection factory).
 

### Session Context
For each request that is processed by the server, a new instance of `SessionContext` is created. Its life time ends
when the response has been sent. Any sensitive information is then cleared before its memory is released.

It does not hold a mutex by design to make it clear that the SessionContext's data is not to be shared between threads or sessions.

### Request Handling
The HttpsServer connects a ServerSocketHandler instance with the server socket so that the later waits for incoming 
socket connections. When one is received its `ServerSocketHandler::on_accept()` method is called, together with the
client socket. At this time the TLS handshake has been successfully performed.

Control is passed to a new ServerSession instance. 
- `ServerSession::handleRequest()` reads the request header with the help of the `ServerRequestReader` (but not yet its body)
- look up the outer handler. This is either the handler for TEE requests (VauRequestHandler) or for HSM/Registration calls (TBD).
  - The VauRequestHandler decrypts the request body and extracts the inner request, which is a regular HTTP request including request line, header and body. 
  - from the inner request, extract the requests target (URL path)
  - look up a specialized request handler for the given target and method (GET, POST, DELETE)
  - extract parameters from the URL's path, the URL's query and (not used by us) the URL's fragment 
  - read and unbox the inner HTTP request body
  - hand over control to the inner request handler
  - with the response body create the inner response  
  - box and encrypt the response
- writes the response to the client socket with the help of `ServerResponseWriter`

### Request Handlers

Due to the VAU (TEE) protocol there are outer and inner request handlers. Both are 
managed by the `RequestHandlerManager` class which manages a mapping between endpoints and request handlers.
An endpoint is specified by request method (GET, POST, DELETE) and the request target (URL path). 
See `ErpProcessingContext::addEndpoints()` for the registration of the eRp endpoints.
 
The current set of request handles can be found in `src/erp/service/`.


### Multithreading

Whether we are allowed to use multiple threads to process more than a single request at a time is being discussed with
Gematik. The current implementation is prepared for using multiple threads. The use of threads for request handling is
controlled by boost::asio. We only have to provide a set of threads, see class `ThreadPool`, and run 
`boost::asio::io_context::run()` in each of them.


## Configuration

A simple configuration class exists with `Configuration`. Each configurable value has an entry in its `Key` enum. Values can be
specified as environment variables or via a JSON configuration file.

## HSM

As the interface of the HSM (hardware service module) is not yet defined we have a stand in with the classes `HsmInterface`
and mocked implementation in class `Hsm`. This will be replaced with a production implementation. A mock implementation will
be required for unit tests and be placed in `test/mock/`.

## Database

Access to the PostgreSQL database can be found in `src/erp/database/`. It is based on the `pqxx` library. The table structure
is defined by `scripts/sql/create_tables.sql`. It is not yet final and will have to undergo one more update to conform to
https://dth01.ibmgcloud.net/confluence/display/ERP/Postgres+Table+definition.

## VAU / TEE

The VAU (vertrauenswürdige Anwendungsumgebung) Protokoll or English TEE (trusted execution environment) protocol can be found
in `src/erp/service/VauRequestHandler`. The client side has been implemented for tests in `test/mock/ClientTeeProtocol`.

## Client

A client for HTTP and HTTPS connections exists in `src/erp/client`. It can be used for unit tests as well as for communication
with external services. This will include a registration service where the erp pc will register itself on startup and will send
periodically heartbeat requests to. It may be used for communication with the HSM.
 