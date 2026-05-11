# CMF HFT Market-Making Backtester

## Directory structure

```
.
├── 3rdparty                    # place holder for 3rd party libraries (downloaded during the build)
├── build                       # local build tree used by CMake
├    ├── bin                    # generated binaries
├    ├── lib                    # generated libs (including those, which are built from 3rd party sources)
├    ├── cfg                    # generated config files (if any)
├    └── include                # generated include files (installed during the build for 3rd party sources)
├── cmake                       # cmake helper scripts
├── config                      # example config files
├── scripts                     # shell (and other) maintenance scripts
├── src                         # source files
├    ├── common                 # common utility files
├    ├── ...                    # ...
├    └── main                   # main() for hft-market-making app
├── test                        # unit-tests and other tests
├── CMakeLists.txt              # main build script
└── README.md                   # this README
```

## OS

Our primary platform is Linux, but nothing prevents it to be built and run on other OS.
The following commands are for Linux users.
Other users are encouraged to add the corresponding instructions for required steps in this README.

## Build

Install dependencies once:

```
sudo apt install -y cmake g++ clang-format
```

Build using cmake:

```
cmake -B build -S .
cmake --build build -j
```

or

```
mkdir -p build
pushd build
cmake ..
make -j VERBOSE=1
popd
```

## Test

To run unit tests:

```
ctest --test-dir build -j
```

or

```
pushd build
ctest -j
popd
```

or

```
build/bin/test/hft-market-making-tests
```

## Run

HFT market-making app:

```
build/bin/hft-market-making
```

## Experiments

The backtester ships with three configs in `config/`. Place `lob.csv` and `trades.csv` in the
working directory (or update the `lob_path` / `trade_path` entries), then run:

```bash
# Vanilla Avellaneda-Stoikov (2008)
build/bin/hft-market-making config/baseline_as2008.conf

# Microprice-enhanced AS
build/bin/hft-market-making config/microprice_extension.conf
```

Each run writes a Markdown performance report to `reports/`. Pre-generated results and a
side-by-side comparison of both strategies are in `reports/EXPERIMENTS.md`.

For a full description of the engine design, strategy model, formula derivation, and
configuration reference see `docs/TECHNICAL.md`.

## Contributing

Install UV, create a virtual environment, and install the project dependencies:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv sync
```

Then activate the virtual environment and set up the git pre-commit hooks:

```bash
source .venv/bin/activate
pre-commit install
```

After that, formatting and linting will run automatically before each commit.
If the source code does not meet the required formatting rules, the hook will
modify the files and stop the commit, and you will need to stage the updated
changes manually.

To run formatting and linting yourself, use one of these commands:

```bash
pre-commit run --files file.py
pre-commit run --all-files
```

The current pre-commit hooks do the following:
- format and lint C++ code with `clang-format`;
- format and lint Python code with `ruff`;
- strip outputs from Jupyter notebooks.
