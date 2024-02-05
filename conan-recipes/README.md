# Customized conan packages

## hiredis

Added `version = "1.2.0"` to conanfile.py and fixed an issue with
conan versions < 2.0 in combination with the with_ssl option by
changing variables to cache_variables.

## openssl

See the patches/ folder

## Export and upload

Go into the package folder (for exmaple hiredis) and
export the recipe, locally with:

`conan export . erp/stable`

Then, upload to the appropriate remote (here, erp):

`conan upload -r erp hiredis/1.2.0@erp/stable`

The reference `erp/stable` must match in both steps in order
to address the recipe from the proper remote.
