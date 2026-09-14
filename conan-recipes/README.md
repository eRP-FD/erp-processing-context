# Customized conan packages

## openssl

See the patches/ folder

## Export and upload

Go into the package folder (for example `openssl`) and
export each recipe, locally with:
```sh
conan export . --version 3.5.8+erp --user erp  --channel stable
```

openssl:
```sh
conan create conanfile.py --version 3.5.8+erp --build=missing
conan upload -r erp-conan-2 openssl/3.5.8+erp
```
