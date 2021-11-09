/*
------------------------------------------------------------------

This file is part of a library for the Open Ephys GUI
Copyright (C) 2017 Translational NeuroEngineering Laboratory, MGH

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

/*
LSL input from Brain amp


Output from brain amp specificaly
The hardware itself sends raw data that is encoded as 16-bit integers. 
These values must be converted into floating point values corresponding to the actually measured microvoltages at the electrodes. 
If checked, the LSL stream outlet will send the BrainAmp's raw data. If not, it will convert the data appropriately.


Events
If this box is checked, an extra channel will be added to the EEG stream corresponding to the device triggers in. 
Rather than unsampled markers, these channels will output -1 if no trigger is available,
else the value corresponding to the value at the trigger input when it changes value.
*/

#ifndef OEP_LSL_H_INCLUDED
#define OEP_LSL_H_INCLUDED

#include <CommonLibHeader.h>
#include <lsl_cpp.h>

namespace LSLinletNode
{
	/*
	Inlet stream for lsl
	*/
	class LSLinletStream
	{
	public:
		/*
		* Creates an inlet that connects to an EEG stream on the network.
		* @oaram sr pointer to a float to hold sampling rate of the stream
		* @param nChans pointer to an int to hold number of channels (crucial to correct size data vector)
		* @param nSamps how many samples per buffer pull. Should be equivalent to Open Ephys buffer size (regardless of sampling rate?) Not exactly sure how these interact
		*/
		LSLinletStream(float* sr, int* nChans, int nSampsIn):
			results(lsl::resolve_stream("type", "EEG")),
			inlet(lsl::stream_inlet(lsl::stream_info())),
			resultsEvents(lsl::resolve_stream("type", "Markers")),
			inletEvents(lsl::stream_inlet(lsl::stream_info())),
			initTs(-1)
		{
			results = lsl::resolve_stream("type", "EEG");
			std::cout << "results: " << results[0].name() << std::endl;
			if (results.empty()) {
				success =  false;
			}
			*sr = results[0].nominal_srate(); //sampling rate
			*nChans = results[0].channel_count();
			nSamps = nSampsIn;
			inlet = lsl::stream_inlet(results[0]); //stream_info, num_seconds (we don't want this, skip somehow??), nSamps determined from processor};


			resultsEvents = lsl::resolve_stream("type", "Markers");
			std::cout << "resultsEvents: " << resultsEvents[0].name() << std::endl;
			if (resultsEvents.empty()) {
				success = false;
			}
			inletEvents = lsl::stream_inlet(resultsEvents[0]); //stream_info, num_seconds (we don't want this, skip somehow??), nSamps determined from processor};

			success = true;
		}
		/*
		* Close stream on exit
		*/
		~LSLinletStream() {
			inlet.close_stream();
			inletEvents.close_stream();
		}

		bool connectToStream(float *sr, int *nChans, int nSampsIn)
		{
			results = lsl::resolve_stream("type", "EEG");
			if (results.empty()) {
				return false;
			}
			*sr = results[0].nominal_srate(); //sampling rate
			*nChans = results[0].channel_count();
			nSamps = nSampsIn;
			inlet = lsl::stream_inlet(results[0], 100, nSamps); //stream_info, num_seconds (we don't want this, skip somehow??), nSamps determined from processor
	
			return true;
		}

		/*
		* Pull most recent buffer of data. Parameters are pointers for data to get written to.
		* @oaram dataBuf vector<vector> with size = chan x nSamps or nSamps x chan
		* @param ts vector with size = nSamps
		*/
		void pullData(std::vector<std::vector<float>> *dataBuf, std::vector<double> *tsBuf, std::vector<std::string> *eventStr, std::vector<int>*eventIndArray) {
			//lsl::pull_chunk_multiplexed(dataBuf, tsBuf, nTotalSamps, nTotalSamps) // faster if needed
			std::vector<std::vector<float>> tempDataBuf(nSamps, std::vector<float>(results[0].channel_count()));
			std::vector<double> tempTsBuf(nSamps);
			std::vector<std::string> eventVec = std::vector<std::string>();
			std::string event;
			std::vector<int> eventIndsArray(nSamps, 0);

			size_t sampsAv = 0;
			for (int i = 0; i < nSamps; i++)
			{
				tempTsBuf[i] = inlet.pull_sample(tempDataBuf[i]); // this works???
				if (initTs == -1) {
					initTs = tempTsBuf[i];
				}
				int isEvent = inletEvents.pull_sample(&event, 1, 0.0);
				if (isEvent != 0) {
					isEvent = 0;
					eventVec.push_back(event);
					//event[i] = 
					//*eventStr = event;
					std::cout << "event found: " << event << std::endl;
					eventIndsArray[i] = 1;
				}
				tempTsBuf[i] -= initTs;
				//inlet.pull_sample(tempDataBuf[i]); // this works???
				//std::cout << "ts: "  << tempTsBuf[i] << std::endl;
				//for (int j = 0; j < results[0].channel_count(); j++)
				//{
					//std::cout << tempDataBuf[i][j] << std::endl;
				//}
				
			}
			//std::cout << "looking for events" << std::endl;

			
			//std::cout << "done looking" << std::endl;
			//std::cout << "NEW BUFFER" << std::endl;
			//while (sampsAv == 0) {
			//	inlet.pull_sample(tempTsBuf); // this works???
			//	sampsAv = inlet.samples_available();
			//	std::cout << tempTsBuf[0] << std::endl;
			//}
			//inlet.pull_chunk(tempDataBuf, tempTsBuf);
			*eventStr = eventVec;
			*eventIndArray = eventIndsArray;
			*dataBuf = tempDataBuf;
			*tsBuf = tempTsBuf;
		}

		/*
		* Change buffer size of inlet when pulling data
		* @param nSamps Number of samples per buffer (be sure to change data vector size accordingly)
		*/
		void setNumSamps(int nSamps) {
			//inlet.close_stream();
			//inlet = lsl::stream_inlet(results[0], 100, nSamps); //stream_info, num_seconds (we don't want this, skip somehow??), nSamps determined from processor
		}

		bool success;


	private:
		lsl::stream_inlet inlet;
		std::vector<lsl::stream_info> results;
		lsl::stream_inlet inletEvents;
		std::vector<lsl::stream_info> resultsEvents;

		int nSamps;
		float initTs;

		JUCE_LEAK_DETECTOR(LSLinletStream);
	};
}


#endif // OEP_LSL_H_INCLUDED
