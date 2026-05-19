eRP-FHIR-Validator Web Docker Image
===================================

Building
--------

Before starting, acquire a conan authentication token from [IBM-Artifactory](https://artifactory-cpp-ce.ihc-devops.net/ui/repos/tree).
Log in and click on _Set Me Up_ on the top right under the navigation bar.
Then insert your authentication token into a file in the top-level source folder of `erp-processing-context` and
name it `conan_credentials.json` with the following contents (tokens should normaly be identical):

```JSON
{
    "credentials": [
        {
            "remote": "erp-conan-2",
            "user": "<user-name>",
            "password": "<authentication-token>"
        },
        {
            "remote": "erp-conan-internal",
            "user": "<user-name>",
            "password": "<authentication-token>"
        }
    ]
}
```

Then the docker image can be built from the top-level of the source tree using the following command:
(provide reasonable values for the `ERP_*_VERSION` variables.)
```sh
docker build \
    --file docker/erp-fhir-ws/Dockerfile \
    --build-arg ERP_BUILD_VERSION=LOCAL \
    --build-arg ERP_RELEASE_VERSION=1.22.0 \
    --secret id=conan_credentials.json \
    --tag de.icr.io/erp_dev/erp-fhir-ws .
```

Running
-------

After building, the Container can be started using:

```sh
docker run --detach --publish 127.0.0.1:8085:8085 --name erp-fhir-ws de.icr.io/erp_dev/erp-fhir-ws
```

After a few seconds the service should be accessible via: http://127.0.0.1:8085/

