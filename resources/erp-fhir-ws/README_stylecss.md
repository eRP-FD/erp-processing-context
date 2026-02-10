Updating `style.css` file
=========================

The file `public/style.css` is generated using [postcss](https://postcss.org/) from [tailwindcss](https://tailwindcss.com/) styles.

_postcss_ uses `.css` files and `.html` files as input. The `.html` files are configured in `postcss.config.mjs` and the `.css` are passed to the command line.

To install _postcss_ and _tailwindcss_ run the following command in this folder:

```sh
npm install
```

To generate the `public/style.css` use (in this folder):
```sh
npx postcss src/style.css -o public/style.css
```

You can also add `--watch` to make _postcss_ watch the input files and update `public/style.css` automaticaly.


