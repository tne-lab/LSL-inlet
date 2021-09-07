#ifdef _WIN32
#include <Windows.h>
#endif

#include "LSLinlet.h"
#include "LSLinletEditor.h"
#include "LSLBrainAmp.h"

using namespace LSLinletNode;

DataThread* LSLinlet::createDataThread(SourceNode *sn)
{
    return new LSLinlet(sn);
}


LSLinlet::LSLinlet(SourceNode* sn) : DataThread(sn),
    num_channels(DEFAULT_NUM_CHANNELS),
    num_samp(DEFAULT_NUM_SAMPLES),
    data_scale(DEFAULT_DATA_SCALE),
    sample_rate(DEFAULT_SAMPLE_RATE)

{
        num_channels = 8;
        num_samp = 100;
        inlet = new LSLinletStream(&sample_rate, &num_channels, num_samp);
        if (inlet->success) {
            connected = true;

            sourceBuffers.add(new DataBuffer(num_channels, 10000));
            convbuf = (float*)malloc(num_channels * num_samp * sizeof(float));
        }
    
}

GenericEditor* LSLinlet::createEditor(SourceNode* sn)
{
    return new LSLinletEditor(sn, this);
}



LSLinlet::~LSLinlet()
{
    free(convbuf);
}


void LSLinlet::resizeChanSamp()
{
        sourceBuffers[0]->resize(num_channels, 10000);
        convbuf = (float*)realloc(convbuf, num_channels * num_samp * sizeof(float));
        timestamps.resize(num_samp);
        ttlEventWords.resize(num_samp);
}


// These are for other plugins to query the datathread (default OEPlugin functions)
int LSLinlet::getNumChannels() const
{
    return num_channels;
}

int LSLinlet::getNumDataOutputs(DataChannel::DataChannelTypes type, int subproc) const
{
    if (type == DataChannel::HEADSTAGE_CHANNEL)
        return num_channels;
    else
        return 0; 
}

int LSLinlet::getNumTTLOutputs(int subproc) const
{
    return 8;
}

float LSLinlet::getSampleRate(int subproc) const
{
    return sample_rate;
}

float LSLinlet::getBitVolts (const DataChannel* ch) const
{
    return data_scale;
}


bool LSLinlet::foundInputSource()
{
    return connected;
}

bool LSLinlet::startAcquisition()
{
    // most likely different for each type
    resizeChanSamp();

    total_samples = 0;

    startTimer(5000);

    startThread();
    return true;
}

void  LSLinlet::tryToConnect()
{       
        connected = inlet->connectToStream(&sample_rate, &num_channels, num_samp);
}

bool LSLinlet::stopAcquisition()
{
    // should always be the same
    if (isThreadRunning())
    {
        signalThreadShouldExit();
    }

    waitForThreadToExit(500);

    stopTimer();

    sourceBuffers[0]->clear();
    return true;
}

bool LSLinlet::updateBuffer()
{
        // create empty datastructs
        std::vector<std::vector<float>> recv_buf;
        std::vector<double> ts_buf;
        std::vector<std::string> eventVec;
        std::vector<int> eventIndsArray;

        // Pull data
        inlet->pullData(&recv_buf, &ts_buf, &eventVec, &eventIndsArray);

        // Add data to convbuf
        int k = 0;
        for (int i = 0; i < num_samp; i++) {
            for (int j = 0; j < num_channels; j++) {
                float curval = 0.195 * (float)(recv_buf[i][j]);
                convbuf[k++] = curval; 
                //std::cout << curval << std::endl;
            }
        }
       
        // Set timestamps and ttl events
        int curEvent = 0;
        for (int i = 0; i < num_samp; i++) {
            timestamps.set(i, total_samples+i);
            if (eventIndsArray[i]) {
                uint64 ttlEvent = std::stoi(eventVec[curEvent++]);
                if (ttlEvent <= 8) {
                    ttlEventWords.setUnchecked(i, ttlEvent);
                }
            }
        }

        
        int sampswrit = sourceBuffers[0]->addToBuffer(convbuf,
            timestamps.getRawDataPointer(),
            ttlEventWords.getRawDataPointer(),
            num_samp,
            1);
        
        // reset ttls. clearQuick() didn't work for whatever reason!
        for (int i = 0; i < num_samp; i++) {
            ttlEventWords.setUnchecked(i, 0);
        }

        //std::cout << "sampswrite: " << sampswrit << std::endl;
        total_samples += num_samp; // if needed

    return true;
}

void LSLinlet::timerCallback()
{
    //std::cout << "Expected samples: " << int(sample_rate * 5) << ", Actual samples: " << total_samples << std::endl;

    //relative_sample_rate = (sample_rate * 5) / float(total_samples);

    //total_samples = 0;
}

bool LSLinlet::usesCustomNames()
{
    return false;
}