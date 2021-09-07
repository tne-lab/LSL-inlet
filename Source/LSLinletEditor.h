#ifndef __EPHYSSOCKETEDITORH__
#define __EPHYSSOCKETEDITORH__

#ifdef _WIN32
#include <Windows.h>
#endif

#include <VisualizerEditorHeaders.h>
//#include <EditorHeaders.h>

namespace LSLinletNode
{
    class LSLinlet;

    class LSLinletEditor : public GenericEditor, public Label::Listener
    {

    public:

        LSLinletEditor(GenericProcessor* parentNode, LSLinlet *node);

        /** Button listener callback, called by button when pressed. */
        void buttonEvent(Button* button);

        /** Called by processor graph in beginning of the acqusition, disables editor completly. */
        void startAcquisition();

        /** Called by processor graph at the end of the acqusition, reenables editor completly. */
        void stopAcquisition();

        /** Called when configuration is saved. Adds editors config to xml. */
        void saveCustomParameters(XmlElement* xml) override;

        /** Called when configuration is loaded. Reads editors config from xml. */
        void loadCustomParameters(XmlElement* xml) override;

        /** Called when label is changed */
        void labelTextChanged(Label* label);

    private:

        // Button that tried to connect to client
        ScopedPointer<UtilityButton> connectButton;

        // Buffer size
        ScopedPointer<Label> bufferSizeMainLabel;

        // x label
        ScopedPointer<Label> xLabel;

        // Chans
        ScopedPointer<Label> channelCountLabel;
        ScopedPointer<Label> channelCountInput;

        // Samples
        ScopedPointer<Label> bufferSizeLabel;
        ScopedPointer<Label> bufferSizeInput;

        // Fs
        ScopedPointer<Label> sampleRateLabel;
        ScopedPointer<Label> sampleRateInput;

        // Scale
        ScopedPointer<Label> scaleLabel;
        ScopedPointer<Label> scaleInput;

        // Parent node
        LSLinlet* node;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSLinletEditor);
    };
}

#endif