# tabular
Reads a simple .CSV file and allows you to do operations on it

## Running the example
Still in heavy development, but you can already try it out.

### Acquiring the source
```
git clone https://github.com/thepsauce/tabular.git
cd tabular
```

### Now you have these options
- `./build.sh -x --`
- `./build.sh -x -- example.CSV --all --print`
- `./build.sh -x -- example.CSV --all --set-row Kiwi --print`
- `./build.sh -x -- example.CSV --all --set-column Sold --sum`
- `./build.sh -x -- example.CSV --all --view`
- `./build.sh -x -- example.CSV --all --setcolumn "Product*" --view`

This does not show all options, just the most interesting ones.

When using `--view`, you have a bunch of keys to use to navigate: `<TAB><BTAB>ijklIJKL<UP><DOWN><LEFT><RIGHT>` (some keys will do nothing because there might not be a reason to scroll)

## For the future

- Adding to the view to scroll the cell related to the `subColumn`
- Adding remove and add operations and then of course a write operation
