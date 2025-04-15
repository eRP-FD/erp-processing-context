# Customized conan packages

## openssl

See the patches/ folder

## Export and upload

Go into the package folder (for example hiredis) and
export the recipe, locally with:

`conan export . erp/stable`

openssl:
`conan create conanfile.py --version 3.1.8+erp --build=missing`
`conan upload -r erp-conan-2 openssl/3.1.8+erp`

