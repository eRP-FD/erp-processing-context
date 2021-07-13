# Integration with PostgreSQL, HSM and TPM
Running tests in a setup that resembles a cloud or production setup requires integration of the 
processing context with external services. 

#PostgreSQL
There are several ways of running PostgreSQL, including local installation, docker and kubernetes.

##PostgreSQL in a Docker container
Run
```
docker run -d --name local-postgres \
       -e POSTGRES_HOST_AUTH_METHOD=trust \
       -e POSTGRES_DB=erp-dev \
       -e POSTGRES_USER=erp_admin \
       -e POSTGRES_PASSWORD=erp0815 \
       -p 5430:5432 \
       de.icr.io/erp_dev/erp-postgres-db:v-0.0.4 \
       -c ssl=on \
       -c ssl_cert_file=/erp/secrets/server.crt \
       -c ssl_key_file=/erp/secrets/server.key
```

#HSM
Clone `git@github.ibmgcloud.net:eRp/vau-hsm.git` and build the firmware sub-project. As this process seems
to contain errors, use this script to be ready to delete the build directory and repeat the whole build process.
The whole build is done by this script
```
#!/usr/bin/env bash

if [[ ! -d firmware ]]; then
    echo "please cd to the top-level directory of the hsm-vau repository"
    exit 1
fi

echo "preparing"
if [[ ! -d firmware/build ]]; then
    mkdir firmware/build
    pushd firmware/build
    conan install ..
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    popd
fi


echo "building"
cd firmware/build
make
```
Start the HSM simulator with 
```
firmware/build/simulator/bin/bl_sim5 -h -o
```
The HSM is now listening on local port 3001. Get its status with
```
firmware/build/simulator/bin/csadm dev=3001@localhost GetState
```
Check that the users have been correctly set up with
```
firmware/build/simulator/bin/csadm dev=3001@localhost ListUsers
```
This list should at least contain `ERP_WORK`.

#TPM
Clone the tpm client repository `git clone git@github.ibmgcloud.net:eRp/tpm-client.git`. Then setup conan and cmake with
```
mkdir build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=1 ..
```
This will provide the tpm simulator `build/simulator/bin/tpm_server`.
Build the repository with `make` to get `build/bin/tpmclient-test`.

Start the simulator `build/simulator/bin/tpm_server` (no arguments necessary) and run the test `build/bin/tpmclient-test` 
to verify that the setup is OK.

#Access DEV

If you want to access any or all of the services on DEV (or another cloud environment) then connect to that
environment's kubernetes and establish these port forwards
- PostgreSQL
    ```
    kubectl port-forward -n erp-system deployment/erp-postgres-db 5432:5432
- TPM simulator (don't get fooled by the deployment name)
    ```
    kubectl port-forward -n erp-system deployment/tpm-client 2321:2321
    ```
- HSM simulator
    ```
    kubectl port-forward deployment/vau-hsm-simulator -n erp-system 3001:3001
    ```
- Enrolment API of the processing context
    ```
    kubectl port-forward deployment/erp-processing-context-1 -n erp-system 9191:9191
    ```
 
If you are on Windows and are using WSL2 and want to access to forwarded service not only from the Linux side
but also from the Windows side then use local port numbers (on the left side) above 49152, which are tunneled 
"to the other side".
