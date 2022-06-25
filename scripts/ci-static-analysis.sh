#!/bin/bash -e

die()
{
    echo "$@"
    exit 1
}

readargs()
{
    erp_build_path=
    erp_source_path=
    clang_tidy_bin="$(which clang-tidy)" >&/dev/null ||:
    clang_tidy_args=()
    output=
    while [ $# -gt 0 ] ; do
        case "$1" in
            --build-path=*)
                build_path="${1#*=}"
                ;;
            --source-path=*)
                source_path="${1#*=}"
                ;;
            --clang-tidy-bin=*)
                clang_tidy_bin="${1#*=}"
                ;;
            --output=*)
                output="${1#*=}"
                ;;
            *)
                clang_tidy_args=("${clang_tidy_args[@]}" "$1")
                ;;
        esac
        shift
    done
}


readargs "$@"



if [ ! -e "${build_path}/compile_commands.json" ] ; then
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DERP_WITH_HSM_MOCK=ON \
        -S "${source_path}" -B "${build_path}"
fi


ninja_build_file() {
    local file
    echo -n "clang-tidy=${clang_tidy_bin}"
    for arg in "${clang_tidy_args[@]}" ; do
        printf " %q" "$arg"
    done
    echo ""
    echo "rule clang-tidy"
    echo "  command=\${clang-tidy} -p '${build_path}' \$in"
    echo "  description=clang-tidy \$in"
    while [ $# -gt 0 ] ; do
        file="$1"
        echo "build $(uuidgen): clang-tidy ${file}"
        shift
    done
}

eval ninja_build_file $(jq ".[].file" "${build_path}/compile_commands.json") > "${build_path}/ninja_clang-tidy.build"
NINJA_STATUS= ninja -C "${build_path}" -l$(nproc) -k0 -f ninja_clang-tidy.build | tee "${output}"
