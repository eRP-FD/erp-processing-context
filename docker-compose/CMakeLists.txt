include(add_erp_config)

add_erp_config(docker-compose.yml
    TEMPLATE "docker-compose.yml.in"
    NO_INSTALL
)
