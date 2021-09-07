#ifndef __EPHYSSOCKETH__
#define __EPHYSSOCKETH__

#include <DataThreadHeaders.h>
#include "SocketLSLBrainAmp.h"

const float DEFAULT_SAMPLE_RATE = 30000.0f;
const float DEFAULT_DATA_SCALE = 0.195f;
const int DEFAULT_NUM_SAMPLES = 256;
const int DEFAULT_NUM_CHANNELS = 64;

namespace LSLinletNode
{
    class LSLinlet : public DataThread, public Timer
    {

    public:
        LSLinlet(SourceNode* sn);
        ~LSLinlet();

        // Interface fulfillment
        bool foundInputSource() override;
        int getNumDataOutputs(DataChannel::DataChannelTypes type, int subProcessor) const override;
        int getNumTTLOutputs(int subprocessor) const override;
        float getSampleRate(int subprocessor) const override;
        float getBitVolts(const DataChannel* chan) const override;
        bool usesCustomNames(); 
        int getNumChannels() const;

        // User defined
        float sample_rate;
        float data_scale;
        int num_samp;
        int num_channels;

        int64 total_samples;
        float relative_sample_rate;

        void resizeChanSamp();
        void tryToConnect();

        GenericEditor* createEditor(SourceNode* sn);
        static DataThread* createDataThread(SourceNode* sn);

    private:
        bool updateBuffer() override;
        bool startAcquisition() override;
        bool stopAcquisition()  override;
        void timerCallback() override;


        bool connected = false;

       ScopedPointer<LSLinletStream> inlet;

        float *convbuf;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSLinlet);
    };
}
#endif