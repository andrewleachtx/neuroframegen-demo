#include <iostream>

#include <dv-processing/core/core.hpp>
#include <dv-processing/core/frame.hpp>
#include <dv-processing/io/mono_camera_recording.hpp>
<<<<<<< HEAD
#include <dv-processing/core/frame.hpp>

#include <opencv2/highgui.hpp>
=======

#include <opencv2/highgui.hpp>

>>>>>>> 8c269c085d019779034e0dab24e13e9cd81b8eca
using std::cout, std::endl;

const int FPS = 1; 
const int SPF = 1 / FPS * 1000; // milliseconds


// dv::EventStore binary_exposure() {
    
// }


int main() {
    using namespace std::chrono_literals;

    // Initalize data stream from file
    dv::io::MonoCameraRecording reader("data/test_data.aedat4");

    // Initalize accumulator (collects TODO)
    dv::Accumulator accumulator(*reader.getEventResolution());
    accumulator.setSynchronousDecay(true);

    dv::EventStreamSlicer slicer;
    slicer.doEveryTimeInterval(33ms, [&accumulator](const dv::EventStore &events) {
        accumulator.accept(events);
        dv::Frame frame = accumulator.generateFrame();

        cv::imshow("Preview", frame.image);
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