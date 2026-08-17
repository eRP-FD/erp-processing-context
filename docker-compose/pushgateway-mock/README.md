# Push Gateway Mock - Docker Compose Integration

This directory contains the configuration files for the Push Gateway Mock service integrated into the erp-processing-context docker-compose setup.

## Service Configuration

The pushgateway-mock service is configured in the main `docker-compose.yml` with the following settings:

### Ports
- **19443**: HTTPS port (mapped from container port 8443)
- **19080**: HTTP port (mapped from container port 8080)

### Environment Variables
- `QUARKUS_HTTP_SSL_CERTIFICATE_KEY_STORE_FILE`: Path to keystore
- `QUARKUS_HTTP_SSL_CERTIFICATE_KEY_STORE_PASSWORD`: Keystore password
- `QUARKUS_HTTP_SSL_CERTIFICATE_TRUST_STORE_FILE`: Path to truststore
- `QUARKUS_HTTP_SSL_CERTIFICATE_TRUST_STORE_PASSWORD`: Truststore password
- `PUSHGATEWAY_CRL_FILE`: Path to Certificate Revocation List
- `PUSHGATEWAY_LCFG_FILE`: Path to LZ configuration
- `PUSHGATEWAY_TEMPLATE_PATH`: Path to response templates
- `PUSHGATEWAY_SRV_FILE`: Path to server configuration

### Security Features
- **TLS/SSL**: Enabled on port 8443 with server certificate
- **mTLS**: Mutual TLS authentication with client certificate validation
- **CRL**: Certificate revocation checking enabled

## Usage

### Starting the Service

From the `erp-processing-context/docker-compose` directory:

```bash
# Start all services including pushgateway-mock
docker-compose up -d

# Start only pushgateway-mock
docker-compose up -d pushgateway-mock

# View logs
docker-compose logs -f pushgateway-mock
```

### Stopping the Service

```bash
# Stop pushgateway-mock
docker-compose stop pushgateway-mock

# Stop and remove
docker-compose down pushgateway-mock
```

### Testing the Service

```bash
# Test HTTP endpoint
curl http://localhost:19080/q/health

# Test HTTPS endpoint (requires client certificate for mTLS)
curl --cert client.crt --key client.key --cacert rootCA.crt https://localhost:19443/q/health
```

## Configuration Files

### srv.yaml
Contains server-specific configuration:
- Port settings
- Keystore configuration
- Output settings
- Operation mode (default/chaos)
- Chaos ratio for error injection

### lz-config.json
Contains LZ-specific configuration for push notification rules and behavior.

### Templates
JSON templates for various response scenarios (success, errors).

## Updating Configuration

To update configuration files:

1. Edit the files in the `pushgateway-mock/` directory
2. Restart the service:
   ```bash
   docker-compose restart pushgateway-mock
   ```

## Troubleshooting

### Service won't start
- Check logs: `docker-compose logs pushgateway-mock`
- Verify TLS certificates exist and are valid
- Ensure ports 19080 and 19443 are not in use

### mTLS authentication fails
- Verify client certificate is signed by a CA in the truststore
- Check certificate is not revoked (not in CRL)
- Ensure certificate is valid (not expired)

### Configuration not applied
- Verify volume mounts in docker-compose.yml
- Check file permissions
- Restart the service after configuration changes

## Integration with erp-processing-context

The pushgateway-mock is now part of the local development environment and will start automatically with other services when running `docker-compose up`.

It follows the same patterns as other mock services (epa-test-mock, bfarm-test-mock) for consistency.