# LSL inlet plugin [![DOI](https://zenodo.org/badge/404116274.svg)](https://zenodo.org/badge/latestdoi/404116274)
A simple plugin to recieve from one LSL EEG and one LSL Markers stream on the network.

## Usage
Note that an EEG and marker stream must be present on the network, otherwise the plugin will hang. This is a downfall of the lsl api that I couldn't find a workaround for.

### Windows
This is currently built for windows only. I believe you can download the latest lsl libraries for your system, put them in the libs folder and update the CMakeLists accordingly. Contact @markschatza for assistance. 

### Building the plugins
Building the plugins requires [CMake](https://cmake.org/). Detailed instructions on how to build open ephys plugins with CMake can be found in [the Open Ephys GUI documentation](https://open-ephys.github.io/gui-docs/Developer-Guide/Compiling-plugins.html).

## Attribution
Developed by Mark Schatza (@markschatza).
