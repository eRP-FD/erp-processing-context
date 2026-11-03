#!/bin/bash -e

# (C) Copyright IBM Deutschland GmbH 2021, 2026
# (C) Copyright IBM Corp. 2021, 2026
#
# non-exclusively licensed to gematik GmbH

die()
{
    echo "$@"
    exit 1
}

usage()
{
    echo "Usage: $0 --build-path=<dir> --source-path=<dir> --output=<file> [--clang-tidy-bin=<bin>]"
    echo "          [--change-target=<branch> --git-commit=<sha>] [--clang-tidy-config=<file>] [-- <clang-tidy args>...]"
}

readargs()
{
    clang_tidy_bin="$(which clang-tidy)" >&/dev/null ||:
    clang_tidy_args=()
    clang_tidy_config=".clang-tidy"
    build_path=
    source_path=
    output=
    change_target=
    git_commit=
    while [ $# -gt 0 ] ; do
        case "$1" in
            --)
                # everything after "--" is passed verbatim to clang-tidy
                shift
                clang_tidy_args=("$@")
                break
                ;;
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
                # Reject unknown arguments instead of silently forwarding them: a missing space between
                # two arguments must fail
                usage >&2
                die "Unknown argument: '$1'"
                ;;
        esac
        shift
    done

    [ -n "${build_path}" ] || { usage >&2; die "--build-path is required"; }
    [ -n "${source_path}" ] || { usage >&2; die "--source-path is required"; }
    [ -n "${output}" ] || { usage >&2; die "--output is required"; }
    [ -n "${clang_tidy_bin}" ] || die "clang-tidy binary not found, specify --clang-tidy-bin"
    command -v "${clang_tidy_bin}" >/dev/null || die "clang-tidy binary '${clang_tidy_bin}' not found"

    # change-target and git-commit must be given together (or "null"/empty change-target for a full run)
    if [ -n "${change_target}" ] && [ "${change_target}" != "null" ] && [ -z "${git_commit}" ]; then
        die "--change-target requires --git-commit"
    fi
    if [ -n "${git_commit}" ]; then
        (cd "${source_path}" && git rev-parse --verify --quiet "${git_commit}^{commit}" >/dev/null) \
            || die "--git-commit '${git_commit}' is not a valid commit"
    fi
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
    # compile_commands.json is generated for GCC and contains GCC-only warning flags
    # (e.g. -Wno-maybe-uninitialized) which clang does not know.
    echo "  command=\${clang-tidy} -p '${build_path}' --extra-arg-before=-Wno-unknown-warning-option \$in"
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

# Prints .cxx files (relative to the source path) that include the given headers (at most one .cxx file per header).
# The include graph is taken from the dependency database of the build ("ninja -t deps"), so headers are
# analysed in the context of real translation units (with proper compile flags) instead of standalone.
# This requires that the project has been built in ${build_path}.
includingSources() {
  local abs_source_path deps_file headers_file
  abs_source_path=$(realpath "${source_path}")
  deps_file=$(mktemp)
  headers_file=$(mktemp)
  printf '%s\n' "$@" | sed "s|^|${abs_source_path}/|" > "${headers_file}"
  ninja -C "${build_path}" -t deps > "${deps_file}" || die "'ninja -t deps' failed in '${build_path}'"
  [ -s "${deps_file}" ] || die "no dependency information in '${build_path}', the project must be built first"
  # Output format of "ninja -t deps":
  #   <target>: #deps <n>, deps mtime <time> (VALID)
  #       <main source file>
  #       <dependency>
  #       ...
  awk -v root="${abs_source_path}/" '
      NR == FNR { wanted[$0] = 1; count++; next }
      /^[^ \t]/ { src = ""; next }
      /^[ \t]/  {
          if (src == "") { src = $1; next }
          if (($1 in wanted) && index(src, root) == 1) {
              delete wanted[$1]
              count--
              if (!(src in seen)) {
                  seen[src] = 1
                  print substr(src, length(root) + 1)
              }
              if (count == 0) exit
          }
      }
  ' "${headers_file}" "${deps_file}" | sort -u
  rm -f "${deps_file}" "${headers_file}"
}

changedFiles() {
  local target_commit changed_files changed_headers including_sources
  target_commit=$(cd "$source_path" && git merge-base "remotes/origin/${change_target}" ${git_commit})
  echo "change_target=${change_target} git_commit=${git_commit} target_commit=${target_commit}"
  changed_files=$(cd "$source_path" && git diff --name-only --diff-filter=d ${target_commit} ${git_commit} | grep '\.[cht]xx') || true

  check_files=$(echo "${changed_files}" | grep '\.cxx$') || true

  # Headers are checked through the sources that include them.
  changed_headers=$(echo "${changed_files}" | grep '\.[ht]xx$') || true
  if [ -n "${changed_headers}" ]; then
      echo "changed_headers=${changed_headers}"
      including_sources=$(includingSources ${changed_headers})
      # only consider files tracked by git
      including_sources=$(echo "${including_sources}" | grep -Fxf <(cd "$source_path" && git ls-files)) || true
      check_files=$(printf '%s\n%s\n' "${check_files}" "${including_sources}" | sed '/^$/d' | sort -u)
  fi
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
