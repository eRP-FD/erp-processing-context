# Customized conan packages

## openssl

See the patches/ folder

## xmlsec

* Picks upstream commit `a7e8464f2a2826820b94cc641ac0aae345641fc6` for RSA-PSS key support.

## Export and upload

Go into the package folder (for example `openssl`) and
export each recipe, locally with:

`conan export . erp/stable`

openssl:
`conan create conanfile.py --version 3.1.8+erp --build=missing`
`conan upload -r erp-conan-2 openssl/3.1.8+erp`

xmlsec:
```sh
conan create conanfile.py --version 1.3.7+erp --build=missing
conan upload -r erp-conan-2 xmlsec/1.3.7+erp
```
