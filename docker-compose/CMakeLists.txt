include(add_erp_config)

add_erp_config(docker-compose.yml
    TEMPLATE "docker-compose.yml.in"
    NO_INSTALL
)

add_erp_config(04_compose_ca.config.json
    TEMPLATE "04_compose_ca.config.json.in"
    NO_INSTALL
)
