#!/bin/bash -e


ignore_fields=(
    .contact
    .copyright
    .date
    .description
    .expansion.timestamp
    .immutable
    .meta
    .property
    .text.div
    .title
    .version
    .language
    .text.status
)
IFS=","
jq_expression="del((.entry[]?.resource, .)|(${ignore_fields[*]}))"
IFS=" "

old_folder="$1"
new_folder="$2"
shift 2

list_files()
{
    find  "$old_folder" -type f -name '*.json' -printf '%P\0'
}

is_fhir_definition()
{
    local file="$1"
    local resource_type="$(jq ".resourceType?" "${file}")"
    test "${resource_type}" == '"StructureDefinition"' \
        -o "${resource_type}" == '"ValueSet"' \
        -o "${resource_type}" == '"CodeSystem"'
}

compare_file()
{
    local old_file="$1"
    local new_file="$2"
    cmp --quiet <(remove_ignored "$old_file") <(remove_ignored "$new_file")
}

remove_ignored()
{
    local file="$1"
    jq "${jq_expression}" "${file}"
}

version_changed()
{
    local old_file="$1"
    local new_file="$2"
    local old_version="$(jq ".version" "${old_file}")"
    local new_version="$(jq ".version" "${new_file}")"
    test "${old_version}" != "${new_version}"
}

non_fhir=()
removed=()
changed=()
unchanged=()
version_only=()
version_conflict=()
updated=()

while read -d "" file_name ; do
    new_file="${new_folder}/${file_name}"
    old_file="${old_folder}/${file_name}"
    if ! is_fhir_definition "${old_file}" ; then
        non_fhir=("${non_fhir[@]}" "${file_name}")
    elif [ ! -e "${new_folder}/${file_name}" ] ; then
        removed=("${removed[@]}" "${file_name}")
    elif compare_file "${old_file}" "${new_file}" ; then
        if version_changed "${old_file}" "${new_file}" ; then
            version_only=("${version_only[@]}" "${file_name}")
        else
            unchanged=("${unchanged[@]}" "${file_name}")
        fi
    else
        if version_changed "${old_file}" "${new_file}" ; then
            changed=("${changed[@]}" "${file_name}")
        else
            version_conflict=("${version_conflict[@]}" "${file_name}")
        fi
    fi
done < <(list_files)

dump()
{
    local title="$1"
    local commented="$2"
    shift 2
    if [ $# -gt 0 ] ; then
        echo "###############################################################"
        echo "# ${title}"
        echo "###############################################################"
        for f in "${@}" ; do
            if "${commented}" ; then
                echo "# ${f}"
            else
                echo "${f}"
            fi
        done
        echo
    fi
}

dump "not a FHIR definition" true "${non_fhir[@]}"
dump "files with version conflict to ${new_folder}" true "${version_conflict[@]}"
dump "files unchanged in ${new_folder}" false "${unchanged[@]}"
dump "only version changed in ${new_folder}" true "${version_only[@]}"
dump "files changed in ${new_folder}" true "${changed[@]}"
dump "removed in ${new_folder}" true "${removed[@]}"
