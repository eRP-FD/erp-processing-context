NOTE: this document is incomplete and a work-in-progress.


This directory contains the implementation of the TEE protocol (VAU Protokoll) for ePA 4 all aka ePA 3.0.

In short, the TEE protocol establishes a secure communication channel on top of TLS connections.
A TEE channel is established in two steps. Firstly, a handshake establishes encryption keys for all
following requests and responses. Secondly, an authentication of the client associates an ID-Token with
the channel.

Outcome of the handshake 
- encryption keys for messages 
  - from client to server (requests): `K2_c2s_app_data`
  - from server to client (responses): `K2_s2c_app_data`
- `KeyID`, which identifies the tee channel and is accessible in the outer requests and responses
- `VAU-CID` which is the URL path for all requests beginning with the second handshake request

Outcome of the authentication
- `VAU-NP` is a longer living value that helps the TEE router to find a matching TEE instance. Its use is
   optional for the server but when it is used and is returned to the client, the client must use it for
  all subsequent requests.
- `ID-Token` authenticates the client user and is a prerequisite for communication with the HSM

You can find the handshake API in classes ClientTeeHandshake and ServerTeeHandshake.
