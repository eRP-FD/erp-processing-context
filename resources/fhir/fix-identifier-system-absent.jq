# find data absent reason:
path(.. | .identifier? | select(._system?.extension[]?.url == "http://hl7.org/fhir/StructureDefinition/data-absent-reason")) as $id
# remove _system
| delpaths([$id + ["_system"]])
# set correct system
| setpath($id + ["system"]; "http://fhir.de/sid/gkv/kvid-10")
