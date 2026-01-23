def fixElement:
    if (.path|test("^Parameters.parameter(.part)+$"; "s")) then
        .contentReference = "http://hl7.org/fhir/StructureDefinition/Parameters#Parameters.parameter"|
        del(.type)
    else
        .
    end
;

.snapshot.element as $elements |
.snapshot.element = ($elements|map(fixElement))

