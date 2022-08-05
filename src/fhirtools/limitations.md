Limitations for the FHIRPath part of the FHIR validator
=

Unimplemented but needed Functions/Expressions:
-
- resolve() : collection
For each item in the collection, if it is a string that is a uri (or canonical or url), locate the target of the reference, and add it to the resulting collection. If the item does not resolve to a resource, the item is ignored and nothing is added to the output collection.
The items in the collection may also represent a Reference, in which case the Reference.reference is resolved.
  http://hl7.org/fhir/R4/fhirpath.html Can be implemented together with https://dth01.ibmgcloud.net/jira/browse/ERP-10520

Unimplemented Functions/Expressions that are currently not used
-
- HtmlChecks: returns always true, assuming the previous xsd check already validates this. The generic fhir.xsd is enough.
- $index, $total
- subsetOf, supersetOf, repeat
- toBoolean, convertsToBoolean, convertsToInteger, toDate, convertsToDate,
toDateTime, convertsToDateTime, toDecimal, convertsToDecimal, toQuantity, convertsToQuantity, convertsToString
toTime, convertsToTime
- endsWith, upper, lower, replace
- +, abs, ceiling, exp, floor, ln, log, power, round sqrt, truncate
- now, timeOfDay, today
- ~
- elementDefinition(), slice(), checkModifiers()
- memberOf, subsumes, subsumedBy

Datamodel
-
- Quantities are implemented without unit conversions, e.g. [m]->[cm]
- asString(): for partial dates and times, the result shall only be specified to the level of precision in the value being converted.
It is currently specified with full precision, as implemented in Timestamp class [ERP-10543]
- operator==: If one input has a value for the precision and the other does not, the comparison stops and the result is empty ({ })  [ERP-10543]

