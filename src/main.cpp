#include <iostream>

#include <dv-processing/core/core.hpp>
#include <dv-processing/core/frame.hpp>
#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/io/mono_camera_writer.hpp>
#include <dv-processing/core/frame.hpp>

#include <opencv2/highgui.hpp>
using std::cout, std::endl;
using namespace std::chrono_literals;

// Paramters
// Generate Data
bool generate = false;
int generate_option = 1; // 0 == periodic motion, 1 == multi frequency

// Output
const int FPS = 1; 
const float SPF = 1000.0f / FPS; // milliseconds

// Frame
const std::chrono::milliseconds FRAME_LENGTH = 1ms; // 125ms for circle, 1ms for lights
const float PERCENT_OPEN_SHUTTER = 1; // TODO: Change to start stop (so not always at start of frame)

// Input
const char *file_path = "data/lights.aedat4";
// const char *file_path = "data/circle.aedat4";
// const char *file_path = "data/test_data.aedat4";

void generate_circle() {
    // Initialize writer
    const cv::Size resolution(15, 15);
    const auto config = dv::io::MonoCameraWriter::EventOnlyConfig("FakeCamera", resolution);
    dv::io::MonoCameraWriter writer("data/circle.aedat4", config);

    // Generate events
    dv::EventStore events;
    const int64_t timestamp = dv::now();

    const int64_t T = 1 * 1000000;
    int epl = 8; // events per loop
    const int64_t interval = T / epl; // Should divide evenly or will become slightly out of phase each oscillation
    int loops = 5;
    for (int i = 0; i < loops; ++i) {
        events.emplace_back(timestamp + interval * (0 + i * epl), 11, 7, true);
        events.emplace_back(timestamp + interval * (1 + i * epl), 9, 5, true);
        events.emplace_back(timestamp + interval * (2 + i * epl), 7, 3, true);
        events.emplace_back(timestamp + interval * (3 + i * epl), 5, 5, true);
        events.emplace_back(timestamp + interval * (4 + i * epl), 3, 7, true);
        events.emplace_back(timestamp + interval * (5 + i * epl), 5, 9, true);
        events.emplace_back(timestamp + interval * (6 + i * epl), 7, 11, true);
        events.emplace_back(timestamp + interval * (7 + i * epl), 9, 9, true);
    }

    events.emplace_back(timestamp + T * loops, 11, 7, true); // TODO: figure out why last event is cut off

    writer.writeEvents(events);
}

// Helper for generate_lights
void flicker(dv::EventStore *events, int64_t t, int start_x, int start_y = 2) {
    for (int i = 0; i < 9; ++i) {
        int x = start_x + (i % 3);
        int y = start_y + (i / 3);
        events->emplace_back(t, x, y, true);

    }
}

void generate_lights() {
    // Initialize writer
    const cv::Size resolution(19, 7);
    const auto config = dv::io::MonoCameraWriter::EventOnlyConfig("FakeCamera", resolution);
    dv::io::MonoCameraWriter writer("data/lights.aedat4", config);

    // Generate events
    dv::EventStore events;
    const int64_t f1 = 500, f2 = 200, f3 = 125; // Must be in descending order
    const int64_t t1 = 1000000 / f1, t2 = 1000000 / f2, t3 = 1000000 / f3; 

    int64_t timestamp = dv::now();
    int64_t last_t2 = timestamp - t1, last_t3 = timestamp - t1;
    const int64_t end = timestamp + t3 * 3; // inclusive
    while (timestamp <= end) {
        flicker(&events, timestamp, 2);

        if (timestamp + t1 - last_t2 >= t2 && last_t2 + t2 + t1 <= end) {
            last_t2 += t2;
            flicker(&events, last_t2, 8);
        }

        if (timestamp + t1 - last_t3 >= t3 && last_t3 + t3 + t1 <= end) {
            last_t3 += t3;
            flicker(&events, last_t3, 14);
        }
        timestamp += t1;

    }
    
    writer.writeEvents(events);
    
}

int main() {
    // Generate file for input
    if (generate) {
        if (generate_option == 0) { // periodic motion
            generate_circle();

        }
        else if (generate_option == 1) { // multi-frequency
            generate_lights();

        }

    }

    // Initalize data stream
    dv::io::MonoCameraRecording reader(file_path);
    cv::Size resolution = *reader.getEventResolution();

    // Provided implementation of dv::AccumulatorBase
    dv::Accumulator accumulator(resolution);
    accumulator.setDecayFunction(dv::Accumulator::Decay::STEP);

    dv::EventStreamSlicer slicer;
    // Adds an element-timestmp-interval trigger job to the Slicer
    // Calls callback whenever timestamp difference of an incoming event to the last time the function was called is bigger than the interval
    // Only calls once gets packet "after" interval
    // controls frame length not window function
    slicer.doEveryTimeInterval(FRAME_LENGTH, [&accumulator](const dv::EventStore &events) {
        // Review sliceRate() and isWithinStoreTimeRange

        int64_t start = events.getLowestTime(); // Returned in microseconds
        int64_t end = start + (int64_t) (FRAME_LENGTH.count() * PERCENT_OPEN_SHUTTER * 1000);

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


/*
Abstract: nothing
Intro:
-1: event occurs when brightness of pixel undergoes a change that exceeds a preset threshold. Contains timestamp, location, polarity
-2: stropboscopic effect and flutter shutter
-3: frame generation and frame rate/exposure are very important for some algorithms
-4: summary of paper (digital flutter shutter to control motion blur)
Digital coded exposure to emulate the frame formation of a conventional camera:
-1: traditional camera tradeoff of shutter speed and brightness
-2:  frame length != shutter length
-3: (prior works) flutter shutter and coded exposure implementations (for traditional cameras)
-4: (prior works) coded aperture????
-5: Approaches to forming frames. Longer windows = more motion blur. Also dynamic range is good (u is actually mew)
-6: virtual shutter. Since each pixel is asynchronous and software processed can apply arbitrary exposure function
-7: equation for summation of events
-8: Frames equation in terms of frequency (thinking of basis of frequencies where shutter is emphasizing some elements)
-9: Visualization of boxcar side lobes. Just pay attention to width of first oscillation (the rest is side lobes)
-10: Problems with DV and jAER (accidently applying a box filter). Need to sum smarter
Digital coded exposure as a virtual shutter for conventional frame formation wavelet digital coded exposure
-1: Digital shutter isn't necessarily binary
-2: Virtual shutter is a window function (can specify frequency slectivity and sidelobe leakage)
-3: Description of Morlet function: combination of frequency and time (f vs t)
-4,5,6: explain each variable in the quatin
-7: point-wise multiplication
-8: morlet wavelet is good for frequency selection (leads to passive digital stroboscopy)
Digital Cpded Exposure for emulating the shutter of a conventional camera
-1: Describes experiment set up (piston, f= 5.38 Hz)
-2: Describes event driven data frame of piston
-3: Introduces using box car shutter
-4: Specifies parameters (frame length, start, and stop)
-5: Discusses effect of window width (brightness/motionblur)
-5: Discusses effect of changing start time (slightly different phases)
-6: Combines these two parameters
Morlet Wavelet Digital Coded Exposure for Temporal Pattern Extraction
-1: Describes light setup (3 different frequency lights (small typo in 300 should be 500))
-2: Using morlet wavelet for lights (can select the right frequency)
-3: Discusses results (mostly works for selecting but others are visible)
Wavelet digital coded exposure for spatio-temporal pattern extraction for digial stroboscopy
-1: Describe experiment to capture vibrating aluminum beam without additional hardware (useful for morlet parameters)
Discussion
-1: Uses of digial coded exposure
-2: Uses of these frames in post-processing algorithms. Possible expansions (shutter on pixel-by-pixel basis, log-exposure)
-3: Specific fields this could be used
Conclusion:
-1: summary

TODO:
implement morlet functionality
gui
*/