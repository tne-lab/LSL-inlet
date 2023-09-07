/*
 ------------------------------------------------------------------

 This file is part of the Open Ephys GUI
 Copyright (C) 2022 Open Ephys

 ------------------------------------------------------------------

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#ifndef LSLINLETTHREAD_H_DEFINED
#define LSLINLETTHREAD_H_DEFINED

#include <DataThreadHeaders.h>

#include <lsl_cpp.h>


#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <iomanip>  // For std::setprecision



class EventLogger {
private:
    std::string filename;
    std::ofstream file;

public:
    EventLogger(const std::string& filename) : filename(filename) {
        file.open(filename, std::ios::out);  // Open in write mode to overwrite
        file << std::setprecision(15);  // Set high precision for floating-point numbers
    }

    ~EventLogger() {
        file.close();
    }

    double log_event(const std::string& event_name) {
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
        file << event_name << "," << timestamp << std::endl;
        file.flush();  // Flush the buffer to write immediately
        return timestamp;
    }
};



const float DEFAULT_SAMPLE_RATE = 10000.0f;
const float DEFAULT_DATA_SCALE = 1.0f;
const int DEFAULT_NUM_SAMPLES = 256;
const int DEFAULT_NUM_CHANNELS = 1;;
const int MAX_CHANNELS = 64;

const int STREAM_SELECTION_UNDEFINED = -1;
const double TIMESTAMP_UNDEFINED = -1;

class LSLInletThread : public DataThread
{
public:
    /** The class constructor, used to initialize any members. */
    LSLInletThread(SourceNode *sn);

    /** The class destructor, used to deallocate memory */
    ~LSLInletThread();

    /** Index (in the discovered streams list) of the user selected LSL stream */
    int selectedDataStream;
    /** Index (in the discovered streams list) of the user selected LSL stream that will be used to read markers */
    int selectedMarkersStream;
    /** The list of discovered LSL streams */
    std::vector<lsl::stream_info> availableStreams;

    /** Perform LSL stream discovery */
    void discover();
    /** Define a file path that can be used to read the TTL mapping for the markers stream.
     * The content of the file should be a name-value pair in json format, for example:
     * {
     *   "Marker_1": 1,
     *   "Marker_2": 2
     * }
     */
    bool setMarkersMappingPath(std::string filePath);

    // ------------------------------------------------------------
    //                  PURE VIRTUAL METHODS
    //     (must be implemented by all DataThreads)
    // ------------------------------------------------------------

    /** Called repeatedly to add any available data to the buffer */
    bool updateBuffer() override;

    /** Returns true if the data source is connected, false otherwise.*/
    bool foundInputSource() override;

    /** Initializes data transfer.*/
    bool startAcquisition() override;

    /** Stops data transfer.*/
    bool stopAcquisition() override;

    int getNumDataOutputs(DataChannel::DataChannelTypes type, int subProcessor) const override;
    int getNumTTLOutputs(int subprocessor) const override;
    float getSampleRate(int subprocessor) const override;
    float getBitVolts(const DataChannel* chan) const override;
    int getNumChannels() const;
    bool isReady() override;

    // Called by the GUI to resize buffers. Simply calls reallocateBuffers.
    void resizeBuffers() override;
    
    // Called when the signal chain updates and before acquisiton starts to resize internal buffers
    bool reallocateBuffers();


    // ------------------------------------------------------------
    //                   VIRTUAL METHODS
    //       (can optionally be overriden by sub-classes)
    // ------------------------------------------------------------

    /** Create the DataThread custom editor */
    GenericEditor* createEditor(SourceNode *sn) override;

    // User defined
    std::string markerMapPath;
    float dataScale;
    
private:
    template <typename T>
    void printBuffer(const char *desc, T *buf, size_t size);
    void readMarkers(std::size_t samples_to_read);
    std::string trim(const std::string str);

    int64 totalSamples;
    lsl::stream_inlet *dataStream;
    lsl::stream_inlet *markersStream;

    // Temporary buffers
    float *dataBuffer;
    double *timestampBuffer;
    int64 *sampleNumbers;

    // Map of incoming LSL string events to Open Ephys event channels (typically 1-8)
    std::map<std::string, uint64> eventMap;
    
    int numChannels;
    double initialTimestamp;

    // Number of samples to pull from the LSL each iteration
    int numSamples;
    float sample_rate;

    EventLogger eventLogger;
};

#endif
