# Configuration
Configuration values can be provided as environment variables or in a JSON file. Environment 
variables take precendence. 

#Values
Each entry has the form
- [name in enum ConfigurationKey] ([environment-variable] | [json path] | {optional default value})
<br>Description<br><br>


- HSM_WORK_USERNAME ("ERP_HSM_WORK_USERNAME" | "/erp/hsm/work-username" | "ERP_WORK")<br/>
  Username of the HSM work identity. It is used during regular operation, i.e. by code called 
  by VAU endpoints.
- HSM_WORK_PASSWORD ("ERP_HSM_WORK_PASSWORD" | "/erp/hsm/work-password")
  Password for the HSM work identity. Only used when no key spec is set.
- HSM_WORK_KEYSPEC ("ERP_HSM_WORK_KEYSPEC" | "/erp/hsm/work-keyspec")
  Key spec (filename of a certificate) for the HSM work identity. When set, it takes precedence
  over the password.
- HSM_SETUP_USERNAME ("ERP_HSM_SETUP_USERNAME" | "/erp/hsm/setup-username" | "ERP_SETUP")
  Username of the HSM setup identity. It is used during enrolment, i.e. by code called
  by the enrolment request handlers.
- HSM_SETUP_PASSWORD ("ERP_HSM_SETUP_PASSWORD" | "/erp/hsm/setup-password"
  Password for the HSM setup identity. Only used when no key spec is set.
- HSM_SETUP_KEYSPEC ("ERP_HSM_SETUP_KEYSPEC" | "/erp/hsm/setup-keyspec")
  Key spec (filename of a certificate) for the HSM setup identity. When set, it takes precedence
  over the password.
- DEPRECATED_HSM_USERNAME ("ERP_HSM_USERNAME" | "/erp/hsm/username")
  For backward compatibility. The old way to configure the username of the HSM work identity.
- DEPRECATED_HSM_PASSWORD ("ERP_HSM_PASWORD" | "/erp/hsm/password")
  For backward compatibility. The old way to configure the password of the HSM work identity.
- HSM_IDLE_TIMEOUT_SECONDS ("ERP_HSM_IDLE_TIMEOUT_SECONDS" | "/erp/hsm/idle-timeout-seconds")
  Idle time after which the HSM closes a connection, measured in seconds.
