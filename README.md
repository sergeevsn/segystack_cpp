# SEG-Y Stacking Utility

This program performs stacking of seismic traces from SEG-Y files with NMO (Normal Moveout) correction, using velocity information and configurable parameters.

## Features
- Reads input, output, and velocity SEG-Y files specified in a config file
- Applies NMO correction with configurable stretch muting percent
- Supports velocity tables in SEG-Y or text format
- Outputs stacked SEG-Y file

## Configuration
All parameters are set in a simple key=value text file (e.g., `config.txt`). Example:

```
input_file=data/pr02_pstm_inv_nmo.segy
output_file=data/pr02_stack.sgy
velocity_file=data/pr02_horvel.sgy
nmo_stretch_muting_percent=30.0
```

- `input_file`: Path to the input SEG-Y file
- `output_file`: Path for the stacked output SEG-Y file
- `velocity_file`: Path to the velocity file (SEG-Y or text table)
- `nmo_stretch_muting_percent`: NMO stretch muting threshold (float, percent)

## Build Instructions

This project uses CMake. You need a C++17 compiler.

```
git clone <repo_url>
cd segystack
mkdir build
cd build
cmake ..
make
```

## Usage

Run the program from the `build` directory, providing the path to your config file:

```
./segystack ../config.txt
```

## Velocity File Format
- **SEG-Y**: Standard SEG-Y file with velocity traces per CDP
- **Text Table**: ASCII file with columns: `CDP  TIME(ms)  VELOCITY(m/s)`
  - The first line may be a header and will be skipped automatically

## Error Handling
- The program checks for the existence and accessibility of all files before processing.
- If a file cannot be opened, a clear error message is printed and the program exits.

## License
Specify your license here. 