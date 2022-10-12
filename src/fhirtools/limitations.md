Limitations for the FHIRPath part of the FHIR validator
=

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