Updating Profiles
=================
This document describes how to update FHIR profile-packages.

Downloading Profiles
--------------------
It is important to note, that all profile files must have a snapshot. Make sure to always select the snapshot version, when given the choice.
All downloaded files should be named `<package>-<version>.tgz` and placed into the folder `resources/fhir`.

### Download locations
**ePA profiles** and related should be downloaded from the [gemSpec](https://gemspec.gematik.de/fhir/ig/) page of gematik. If unsure check this location first.

Profiles of **KBV and eRP** can be downloaded from [simplifier.net](https://simplifier.net/). To download snapshot versions you might need to create an account.

**KBV-Schlüsseltabelle** can be downloaded on [applications.kbv.de](https://applications.kbv.de/overview.xhtml).

Configuration
-------------
### Package installation
#### CMakeLists.txt

`resources/CMakeLists.txt` contains the package installation instruction.

Usually this should be a `fhir_install` entry as follows:
```cmake
fhir_install(<package> <version>)
```

The macro also supports the following parameters:
* `PACKAGE_FIX_SCRIPT` a cmake script that is run after extracting the package.
* `DEPENDS` allows adding dependencies. Usually files needed by the script.

When the script is executed the following variables will be set by default:
* `SOURCE_DIR` the top-level source folder.
* `BINARY_DIR` the top-level binary folder.
* `CURRENT_SOURCE_DIR` the folder where the `CMakeLists.txt` is located
* `CURRENT_BINARY_DIR` the same sub-folder, but in the binary tree.
* `FHIR_CURRENT_PACKAGE_DIR` the folder, where the archive has been extracted
* `FHIR_PACKAGE_VERSION` the version of the package

Additional variables can be set by passing `-D` arguments after the script name.

Example:
```cmake
fhir_install(de.gematik.epa.medication 1.1.5
        PACKAGE_FIX_SCRIPT "fhir/de.gematik.epa.medication-fix-part-type.cmake" "-DPatch_EXECUTABLE=${Patch_EXECUTABLE}"
        DEPENDS "fhir/de.gematik.epa.medication-1.1.5-fix-part-type.patch"
)
```
This command installs the package `de.gematik.epa.medication` in version `1.1.5`. The package is expected to be located in `resources/fhir` and named `de.gematik.epa.medication-1.1.5.tgz`. (see [Downloading Profiles](#downloading-profiles))

After extracting the package to a temporary folder, the cmake-script `fhir/de.gematik.epa.medication-fix-part-type.cmake` is executed. As the script is executed during build-process it has no access to variables defined here. Therefore the value of `Patch_EXECUTABLE` is forwarded using a separate `-D` option after the script-name.

Finally there is a `DEPENDS` parameter that ensures, that the package installation is repeated, when the file `fhir/de.gematik.epa.medication-1.1.5-fix-part-type.patch` has changed.

#### Package exclude files
Optionally every package can have an exclude file. `<package>-<version>.exclude.txt` containing a list of files that should be excluded from installation.

Each line may contain a filename relative to the package extraction location that should be ignored during installation.

There are various reasons, why files are ignored. Some of the reasons are:
* The file is an example file
* it is not a FHIR file
* or the file has an error and is not needed
* It might also be present in another package.

When omitting any `StructureDefinition`, `ValueSet` or `CodeSystem` that isn't provided otherwise, make sure to clarify with gematik first.

The `fhir_exclude_txt.sh` script (in scripts folder) can assists in generating the `.exclude.txt` file.

Run it as follows:
```sh
fhir_exclude_txt.sh <old_package_folder> <new_package_folder>
```
The exclude file is a template for the package in `<old_package_folder>`

### File list configuration
Once the packages are installed to the target folder, the Validator needs to know, that they should be loaded.
The file `resources/production/001_fhir-resource-groups.config.json.in` contains a list of files or folders to load in the section `fhir/structure-files`.

_Note: Folders are not read recursively. If there are sub-folders, an error is reported._

### Group configuration
The file `resources/production/001_fhir-resource-groups.config.json.in` also contains the _Resource-Group_ configuration in the section `fhir/resource-groups`

A _Resource-Group_ is very simmilar to a package. They group multiple profiles and assign it an ID.
In contrast to a package a group must not contain more than one profile with the same URL and a profile with same URL and version
can only be present in one group. Sometimes this makes it necessary to split packages into multiple groups.

This is particularly required, when an updated version of a package contains "identical" (for validation purposes) files.
Add a new group, that matches all definitions that are present in both versions and include it in both groups.

They can be configured as follows:

```json
{
    "id": <group-id>,
    "include": [ <included-group>,...  ],
    "extend": [ <extended-group>,... ],
    "map-versions": <should-map-versions>,
    "match": [
        {"url": <url> , "version": <version> },
        {"file": <file-name> },
        ...
    ]
}
```
The fields have the following meaning:

| field | decription |
| ----- | ---------- |
| `id` | Identifier of the defined group |
| `include` | include the contents of other groups. A resource contained in this group or in any included group must still be resolveable by its `url` only. (ignoring version) |
| `extend` | This group extends another group. Resources are first resolved in this group or the included groups and if not found in the extended-group. Use this only for terminology (ValueSets, CodeSystems) and only if it cannot be solved by splitting packages into multiple groups and composing via `include` |
| `map-versions` | (boolean) Set to true to apply version mapping inside this group. (see [Version Mapping Configuration](#version-mapping-configuration)) |
| `match` | Defines which profiles go into this group. There are two ways to specify the selected profiles: `url`/`version` or `file`. Each can be provided multiple times. |
| `match.url` | A regular expression matching the profile url |
| `match.version` | The version of the profile (no regular expression). It can also be `null` to refer to a profile where no version is defined. |
| `match.file` | A regular expression matching the source file name. (as converted by the installer) |

#### Configuring terminology groups
Make sure any Group, you consider a *Terminology Group*, only consists of `ValueSets`, `CodeSystems` and `NamingSystems` (not loaded).
If needed, split out any StructureDefiniton into a separate Group. (Naming convention `<package>-<version>-structures` and `<package>-<version>-terminology` respectively).

`ValueSet` and `CodeSystem` dependencies will be resolved inside the *View*, therefore it is not necessary to have all
dependencies in the same *Group* or its included/extended groups.

As a result *Terminology Groups* generally don't need to include other *Groups*. However for `hl7.terminology` gematik allows use of `extend`to extend and override terminology from earlier versions.



### View Configuration
The Files `resources/production/01_production.config.json.in` (for processing-context) and `resources/production/01_production-medication-exporter.config.json.in` (for exporter) contain the configuration for _Views_ in the section `fhir/resource-views`.

The views fuse the groups and define the validity dates for each of them.
A _View_ configuration has the following form:

```json
{
    "id": <view-id>,
    "valid-from": <first-valid-day>,
    "valid-until": <last-valid-day>,
    "groups": [
        <group-id>,...
    ]
}
```
The fields have the following meaning:

| field | description |
| ----- | ----------- |
| `id` | id of the view. |
| `valid-from` | first day of validity  period (inclusive). Can be omitted to indicate _since start of time_* |
| `valid-to` | last day of validity period (inclusive). Can be omitted to indicate _forever_* |
| `groups` | list of groups in the view |

* dates are provided in the form YYYY-MM-DD

### Schlüsseltabellen Configuration
The Schlüsseltabellen komplement the views. They have separate validity periods.

Every start and end date of a Schlüsseltabelle will split the configured views into sub-views with different groups mixed in.

Schlüsseltabellen are configured the same way as Views, but in the configuration path `fhir/schluesseltabellen`.

As an example we have three dates `A`, `B`, `C` in that order and a view with id `V`, that is valid from `A` until `C`. Then we have two Schlüsseltabellen. Schlüsseltabelle `S1` which is valid from `A` until `B` and Schlüsseltabelle `S2` which is valid from `B` until `C`.
Then the View `V` will be split into two views `V-S1` valid form `A` until `B` and `V-S2` valid from `B` until `C`. `V-S1` will have the groups of `V` and `S1` and for `V-S2` respectively.
Note that all resulting views must still be free of conflicts.

### Version mapping Configuration
It is possible to map versions of profiles to other versions.
This is generally needed, when profiles are referenced with only part of the version. (i.e. Version `1.5.2` is referenced as `1.5`).

_Note: Even though FHIR encourages the use of semantic versioning, it is not required. The Versions are merely strings (often also dates) with no defined order._

Version mapping is configured in `resources/production/01_production.config.json.in` (for processing-context) and `resources/production/01_production-medication-exporter.config.json.in` (for exporter).

The values in `fhir/version-mapping` are configured as follows:

```json
{
    "urlRegex": <url-regex>,
    "versionRegex": <version-regex>,
    "realVersion": <real-version>,
    "renderVersion": <render-version>
},
```

The fields have the following meaning:

 | field | description |
 | ----- | ------------|
 | `urlRegex` | apply version mapping to profiles whos url matches this regular expression. |
 | `versionRegex` | apply version mapping to profiles whos version matches this regular expression. |
 | `realVersion` | Actual version as read from the profile-file. |
 | `renderVersion` | version number to use, when creating a resource of a matching `url`/`version` for output. (not used for validation) |

### Synthesized CodeSystems and ValueSets
Some `ValueSet`s and `CodeSystem`s have no definition available. When these are used in a context, where their definition is required
, it is sometimes not possible to load some profiles. (The validator is very strict about these constraints to make sure no codes
are accidentaly forgotten.)

To allow validation of such profiles, it is possible to add _synthetic_ `ValueSet`s and `CodeSystem`s.
When encountering any _synthetic_ `ValueSet`s or `CodeSystem`s during Validation, the validator will emit a warning.

_Synthetic_ `ValueSet`s and `CodeSystem`s are configured in the sections `fhir/synthesize-valuesets` and `fhir/synthesize-codesystems`
respectively:

```
{ "url": <url>, "version": <version> }
```

The fields have the following meaning:

| field | description |
| ----- | ----------- |
| url   | The URL of the `CodeSystem` or `ValueSet` to synthesize |
| version | The version of the `CodeSystem` or `ValueSet` to synthesize. This may also be `null` to indicate, that the version is not defined. In C++ `null`-version will be `FhirVersion::notVersioned`. |

Note that _synthesized_ `ValueSet`s and `CodeSystem`s must also belong to a group. (see [Group configuration](#group-configuration))

### Example updating hl7.terminology 7.1.0
```
fhir_package_tmp$ ../../../scripts/fhir_exclude_txt.sh hl7.terminology.r4#7.0.1 hl7.terminology.r4#7.1.0>fhir_exclude_txt-hl7.terminology.r4#7.1.0_7.0.1
```

Supporting Tools
----------------
### fhirinstall
`fhirinstall` is the installer for packages. It also has a `--tree` function to show the package dependency tree highlighting any conflicting package versions.

e.g. `build/RelWithDebInfo/bin/fhirinstall -p build/RelWithDebInfo/fhir_package_tmp/ --tree de.gematik.erezept-workflow.r4@1.6.2-rc1`
### fhir_exclude_txt.sh
`fhir_exclude_txt.sh` is a script that can generate a template for an `.exclude.txt` file as used by `fhirinstall`.

### unit test
`FhirStructureRepositoryTest`


Glossar
-------
| term       | description |
| ---------- | ----------- |
| Definition | CodeSystem, ValueSet or StructureDefinition |
| Group      | When *Definitions* are loaded they are composed into a Group for easier Referencing. Dependencies from `StructureDefinition`  must always be resolvable in their Group (or `include`/`extend` Group) |
| Terminology Group | A group that contains only Terminology *Definitions* (i.e. `CodeSystem`, `ValueSet`)
| View       | Combines *Groups* and associates them with a validity date range |
