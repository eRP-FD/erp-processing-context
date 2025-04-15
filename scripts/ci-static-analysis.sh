#!/bin/bash -e

# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH

die()
{
    echo "$@"
    exit 1
}

readargs()
{
    clang_tidy_bin="$(which clang-tidy)" >&/dev/null ||:
    clang_tidy_args=()
    clang_tidy_config=".clang-tidy"
    output=
    change_target=
    git_commit=
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
            --change-target=*)
                change_target="${1#*=}"
                ;;
            --git-commit=*)
                git_commit="${1#*=}"
                ;;
              --clang-tidy-config=*)
                clang_tidy_config="${1#*=}"
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
  die "${build_path}/compile_commands.json not found. CMake must run first!"
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

allFiles() {
  check_files=$(cd "$source_path" && git ls-files | grep '\.cxx')
}

changedFiles() {
  local target_commit
  target_commit=$(cd "$source_path" && git merge-base "remotes/origin/${change_target}" ${git_commit})
  echo "change_target=${change_target} git_commit=${git_commit} target_commit=${target_commit}"
  check_files=$(cd "$source_path" && git diff --name-only --diff-filter=d ${target_commit} ${git_commit} | grep '\.[cht]xx' | grep -v '\.inc\.hxx' ) || true
  echo "check_files=${check_files}"
}

prepareClangTidyConfig() {
  if [ "${clang_tidy_config}" != ".clang-tidy" ]; then
      if [ -f "${clang_tidy_config}" ]; then
          cp ".clang-tidy" ".clang-tidy.bak"
          cp "${clang_tidy_config}" ".clang-tidy"
      else
          die "Specified clang-tidy config file '${clang_tidy_config}' does not exist!"
      fi
  fi
  echo "using clang-tidy config: '${clang_tidy_config}'"
}

eval prepareClangTidyConfig

if [ -z "${change_target}" ] || [ "${change_target}" == "null" ] || [ -z "${git_commit}" ]; then
  eval allFiles
else
  eval changedFiles
fi

if [ -z "${check_files}" ]; then
  echo "no files to be checked"
  exit 0
fi

eval ninja_build_file ${check_files} > "${build_path}/ninja_clang-tidy.build"
echo "$(${clang_tidy_bin} --version)"
NINJA_STATUS= ninja -C "${source_path}" -l$(nproc) -k0 -f "${build_path}/ninja_clang-tidy.build" | tee "${output}"
