fhir-tools library contains the following directories
-
- expression: implementation of the FHIRPath expressions http://hl7.org/fhirpath/N1/ and http://hl7.org/fhir/R4/fhirpath.html
- model: datamodel for the FHIR-validator runtime checks.
  - Collection: All Expressions work on collections
  - Element: A single node in the document tree (Interface)
  - ValueElement: implementation of the Element interface for nodes of the structure definition containing
fixed values or pattern values.
  - erp/ErpElement: implementation of the Element interface for the ERP datamodel based on rapidjson

- parser: FHIRPath parser/compiler for FHIRPath expressions. Implements a ANTLR generated visitor.
- repository: read and manage the FHIR profile definitions (StructureDefinions)
  - FhirCodeSystem: representation of a CodeSystem
  - FhirConstraint: representation of a FHIRPath contraint
  - FhirElement: representation of one element node in the StructureDefintion
  - FhirSlicing: representation of a silced element  in the StructureDefintion
  - FhirStructureDefinition: representation of one StructureDefinition
  - FhirStructureRepository: contains all FhirStructureDefinitions that belong together.
We will have one per supported profile version.
  - FhirValueSet: representation of a ValueSet.
- typemodel:
  - ProfiledElementTypeInfo: meta information for profiled Elements to allow child access
- util: Utilities
- validator: the runtime validation of FHIR resources

StructureDefintions are parsed at startup using the files under repository. Also during startup the fhir constraint
expressions are parsed and compiled by the FhirPathParser.
During runtime the validation of FHIR documents takes place in the FhirPathValidator under validator/