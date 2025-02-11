#include <iostream>

#include <dv-processing/core/core.hpp>
#include <dv-processing/core/frame.hpp>
#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/core/frame.hpp>

#include <opencv2/highgui.hpp>
using std::cout, std::endl;
using namespace std::chrono_literals;

const int FPS = 1; 
const float SPF = 1000.0f / FPS; // milliseconds

const std::chrono::milliseconds WINDOW_LENGTH = 100ms;
const float PERCENT_OPEN_SHUTTER = 0.5;

// dv::EventStore binary_exposure(int percentage) {
//     return nullptr;

// }


int main() {

    // Initalize data stream from file
    dv::io::MonoCameraRecording reader("data/test_data.aedat4");

    // Initalize accumulator (collects TODO)
    // params: resolution, decay tyoe, decay param, synchronous Decay, eventContribution, max, neutral, min, ignore polarity
    // Provided implementation of dv::AccumulatorBase
    dv::Accumulator accumulator(*reader.getEventResolution());
    accumulator.setDecayFunction(dv::Accumulator::Decay::NONE);

    dv::EventStreamSlicer slicer;
    // Adds an element-timestmp-interval trigger job to the Slicer
    // Calls callback whenever timestamp difference of an incoming event to the last time the function was called is bigger than the interval
    // Only calls once gets packet "after" interval
    // controls frame length not window function
    slicer.doEveryTimeInterval(WINDOW_LENGTH, [&accumulator](const dv::EventStore &events) {

        // Every event contributes evenly to potential
        // Decay: none, linear, exponential, step
        

        // sliceRate() may be useful
        // isWithinStoreTimeRange() may be useful

        int64_t start = events.getLowestTime(); // Returned in microseconds?
        int64_t end = start + (int64_t) (WINDOW_LENGTH.count() * PERCENT_OPEN_SHUTTER * 1000);

        accumulator.accept(events.sliceTime(start, end)); // .sliceTime(start, stop) //front() or getLowestTime()
        dv::Frame frame = accumulator.generateFrame();

        cv::imshow("Preview", frame.image);

        // Controls frame rate
        cv::waitKey(SPF);

    });

    while (reader.isRunning()) {

        const auto events = reader.getNextEventBatch();
        if (events.has_value()) {
            slicer.accept(*events); 

        }

    }

    return 0;
}