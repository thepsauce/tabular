# tabular
Reads a simple .CSV file and allows you to do operations on it

## Running the example

### Acquiring the source
```
git clone https://github.com/thepsauce/tabular.git
cd tabular
```

### Building
```
./build.sh
```

### Now you have these options
Show usage:
- `./tabular`
Print the whole file column by column:
- `./tabular example.CSV --all --print`
Print the "Kiwi" column:
- `./tabular example.CSV --all --set-row Kiwi --print`
View the table in a TUI:
- `./tabular example.CSV --all --view`
View all columns matching "Product\*":
- `./tabular example.CSV --all --set-column "Product*" --view`

This does not show all options, just the most interesting ones.

When using `--view`, you have a bunch of keys to use to navigate: `<TAB><BTAB>ijklIJKL<UP><DOWN><LEFT><RIGHT>` (some keys will do nothing because there might not be a reason to scroll)

