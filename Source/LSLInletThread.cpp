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

#include "LSLInletThread.h"
#include "LSLInletEditor.h"

#include <fstream>
#include <sstream>

LSLInletThread::LSLInletThread(SourceNode *sn) : DataThread(sn),
                                                 numSamples(DEFAULT_NUM_SAMPLES),
                                                 dataScale(DEFAULT_DATA_SCALE),
                                                 selectedDataStream(STREAM_SELECTION_UNDEFINED),
    num_samp(DEFAULT_NUM_SAMPLES),
    data_scale(DEFAULT_DATA_SCALE),
    sample_rate(DEFAULT_SAMPLE_RATE)
{
    numChannels = 1; // start with 1 channel, will resize the buffers as needed
    // calculate data buffer size instead of using a fixed value if the function of (numChannels, numSamples) can be determined
    sourceBuffers.add(new DataBuffer(numChannels, 100000));
    this->dataStream = NULL;
    this->markersStream = NULL;

    dataBuffer = (float *)malloc(numChannels * numSamples * sizeof(float));
    timestampBuffer = (double *)malloc(numSamples * sizeof(double));

    sampleNumbers = (int64 *)malloc(numSamples * sizeof(int64));
}

LSLInletThread::~LSLInletThread()
{
    free(dataBuffer);
    free(timestampBuffer);
    free(sampleNumbers);
}

void LSLInletThread::discover()
{
    availableStreams = lsl::resolve_streams(1.0);
}

bool LSLInletThread::updateBuffer()
{
    int max_samples_total = numChannels * numSamples;
    std::size_t multiplexed_samples_read = 0;

    try
    {
        multiplexed_samples_read = dataStream->pull_chunk_multiplexed(dataBuffer, timestampBuffer, max_samples_total, numSamples);
    }
    catch (const std::runtime_error &re)
    {
        std::cout << "Failed to read data samples with runtime error: " << re.what() << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cout << "Failed to read data samples with exception: " << ex.what() << std::endl;
    }

    if (multiplexed_samples_read <= 0)
    {
        // it probably doesn't make sense to read markers if we didn't find samples
        return true;
    }

    std::size_t data_samples_read = multiplexed_samples_read / numChannels;
    jassert(multiplexed_samples_read % numChannels == 0);

    readMarkers(data_samples_read);

    if (initialTimestamp == TIMESTAMP_UNDEFINED)
    {
        initialTimestamp = timestampBuffer[0];
    }

    // Compute sample numbers and convert time from absolute to relative
    for (int i = 0; i < data_samples_read; i++)
    {
        sampleNumbers[i] = totalSamples + i;
        timestampBuffer[i] -= initialTimestamp;
    }

    // Apply user defined scaling
    for (int i = 0; i < multiplexed_samples_read; i++)
    {
        dataBuffer[i] = (float)(dataScale * dataBuffer[i]);
    }

    // Add data to the GUI's source buffers
    sourceBuffers[0]->addToBuffer(
        dataBuffer,
        sampleNumbers,
        ttlEventWords.getRawDataPointer(),
        (int)data_samples_read,
        1);


    // Clear TTL buffer
    for (int i = 0; i < ttlEventWords.size(); i++) {
        ttlEventWords.set(i, 0);
    }

    totalSamples += data_samples_read;

    return true;
}

// expects timestampBuffer to be populated with sample timestamps
// in order to match markers with data samples
void LSLInletThread::readMarkers(std::size_t samples_to_read)
{
    if (markersStream == NULL)
    {
        return;
    }

    try
    {
        std::string eventSample;
        int i = 0;
        while (double ts = markersStream->pull_sample(&eventSample, 1, 0.0))
        {

            // try to find closest event timestamp among sample timestamps
            bool found_match = false;
            for (int j = i; samples_to_read; j++)
            {
                double abc = timestampBuffer[j];
                if (timestampBuffer[j] >= ts)
                {
                    i = j;
                    found_match = true;
                    break;
                }
            }
            if (!found_match)
            {
                std::cout << "Discarding marker because it couldn't be matched with data sample timestamp " << eventSample << std::endl;
                break;
            }

            // If we don't have a channel map, convert string to int and check if in range of OE event channels
            if (eventMap.size() == 0)
            {
                uint64 eventAsInt = std::stoi(eventSample);
                if (eventAsInt > 0 && eventAsInt <= 8)
                {
                    ttlEventWords.setUnchecked(0, eventAsInt);
                }
            } 
            else if (eventMap.find(eventSample) == eventMap.end())
            {
                std::cout << "Mapping not found for marker " << eventSample << std::endl;
            }
            else
            {
                uint64 eventAsInt = eventMap[eventSample];
                ttlEventWords.setUnchecked(0, eventAsInt);
            }

            if (++i >= samples_to_read)
            {
                break;
            }
        }
    }
    catch (const std::runtime_error &re)
    {
        std::cout << "Failed to read markers with runtime error: " << re.what() << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cout << "Failed to read markers with exception: " << ex.what() << std::endl;
    }
}

bool LSLInletThread::reallocateBuffers()
{
    if (selectedDataStream == STREAM_SELECTION_UNDEFINED || selectedDataStream >= availableStreams.size())
    {
        std::cout << "Skipping buffer re-allocation. Stream selection undefined" << std::endl;
        return false;
    }

    int newNumChannels = availableStreams[selectedDataStream].channel_count();

    numChannels = newNumChannels;
    sample_rate = (float)availableStreams[selectedDataStream].nominal_srate();
    sourceBuffers[0]->resize(newNumChannels, 100000);
    ttlEventWords.resize(num_samp);
    for (int i = 0; i < num_samp; i++)
    {
        ttlEventWords.set(i, 0);
    }

    if (auto newBuffer = (float*)realloc(dataBuffer, newNumChannels * numSamples * sizeof(float)))
    {
        dataBuffer = newBuffer;
    }
    else
    {
        std::cout << "Failed to allocate data buffer" << std::endl;
        return false;
    }
    if (auto newBuffer = (double*)realloc(timestampBuffer, numSamples * sizeof(double)))
    {
        timestampBuffer = newBuffer;
    }
    else
    {
        std::cout << "Failed to allocate timestamp buffer" << std::endl;
        return false;
    }

    if (auto newBuffer = (int64*)realloc(sampleNumbers, numSamples * sizeof(int64)))
    {
        sampleNumbers = newBuffer;
    }
    else
    {
        std::cout << "Failed to allocate sample numbers buffer" << std::endl;
        return false;
    }

    return true;
}

void LSLInletThread::resizeBuffers()
{
    reallocateBuffers();
}

bool LSLInletThread::foundInputSource()
{
    return selectedDataStream != STREAM_SELECTION_UNDEFINED;
}

bool LSLInletThread::isReady()
{
    return true;
}

bool LSLInletThread::startAcquisition()
{
    if (selectedDataStream == STREAM_SELECTION_UNDEFINED)
    {
        std::cout << "Not starting acquisition because no data stream was selected" << std::endl;
        return false;
    }

    totalSamples = 0;
    initialTimestamp = TIMESTAMP_UNDEFINED;

    this->dataStream = new lsl::stream_inlet(availableStreams[selectedDataStream]);

    if (!reallocateBuffers())
    {
        CoreServices::sendStatusMessage("Failed to allocate space for internal buffers. Please delete the plugin and try again.");
        return false;
    }

    if (selectedMarkersStream != STREAM_SELECTION_UNDEFINED)
    {
        this->markersStream = new lsl::stream_inlet(availableStreams[selectedMarkersStream]);
        jassert(this->markersStream->get_channel_count() == 1);
    }

    startThread();
    return true;
}

bool LSLInletThread::stopAcquisition()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
    }

    waitForThreadToExit(500);

    if (this->dataStream != NULL)
    {
        this->dataStream->close_stream();
        delete this->dataStream;
        this->dataStream = NULL;
    }
    if (this->markersStream != NULL)
    {
        this->markersStream->close_stream();
        delete this->markersStream;
        this->markersStream = NULL;
    }

    sourceBuffers[0]->clear();
    return true;
}


GenericEditor* LSLInletThread::createEditor(SourceNode *sn)
{
    return new LSLInletEditor(sn, this);
}

bool LSLInletThread::setMarkersMappingPath(std::string filePath)
{
    eventMap.clear();
    std::ifstream file(filePath);

    if (!file)
    {
        //LOGE("Cannot open file: ", filePath);
        std::cout << "Cannot open marker config file " << filePath << std::endl;
        CoreServices::sendStatusMessage("Cannot open marker config file");
        return false;
    }

    // Perform a naive JSON parsing
    try
    {
        std::stringstream ss;
        ss << file.rdbuf();
        file.close();

        // remove curly braces
        std::string str = ss.str();
        str.erase(std::remove(str.begin(), str.end(), '{'), str.end());
        str.erase(std::remove(str.begin(), str.end(), '}'), str.end());

        // split by comma
        std::vector<std::string> pairs;
        size_t pos = 0;
        std::string token;
        while ((pos = str.find(',')) != std::string::npos)
        {
            pairs.push_back(str.substr(0, pos));
            str.erase(0, pos + 1);
        }
        pairs.push_back(str);

        // update mapping for TTL
        for (const auto pair : pairs)
        {
            if ((pos = pair.find(':')) != std::string::npos)
            {
                std::string marker = trim(pair.substr(0, pos));
                uint64 ttl = std::stoul(trim(pair.substr(pos + 1, pair.size())));
                eventMap[marker] = ttl;
            }
        }
    }
    catch (const std::runtime_error &re)
    {
        std::cout << "Failed to read markers mapping file with runtime error: " << re.what() << std::endl;
        return false;
    }
    catch (const std::exception &ex)
    {
        std::cout << "Failed to read markers mapping file with exception: " << ex.what() << std::endl;
        return false;
    }

    return true;
}


int LSLInletThread::getNumChannels() const
{
    if (selectedDataStream == STREAM_SELECTION_UNDEFINED)
    {
        return 0;
    }

    return numChannels;
}

int LSLInletThread::getNumDataOutputs(DataChannel::DataChannelTypes type, int subproc) const
{
    if (type == DataChannel::HEADSTAGE_CHANNEL)
        return numChannels;
    else
        return 0;
}

int LSLInletThread::getNumTTLOutputs(int subproc) const
{
    return 8;
}

float LSLInletThread::getSampleRate(int subproc) const
{
    return sample_rate;
}

float LSLInletThread::getBitVolts(const DataChannel* ch) const
{
    return data_scale;
}

inline std::string LSLInletThread::trim(const std::string const_str)
{
    std::string str = const_str;
    str.erase(str.find_last_not_of(" \n\r\t\"") + 1);
    str.erase(0, str.find_first_not_of(" \n\r\t\""));
    return str;
}
